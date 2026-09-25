#include "fixed_function_material.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <memory>

namespace
{
constexpr DWORD kRgb = D3DSP_WRITEMASK_0 | D3DSP_WRITEMASK_1 | D3DSP_WRITEMASK_2;
constexpr DWORD kAlpha = D3DSP_WRITEMASK_3;
constexpr DWORD kAlphaSwizzle = 0x00FF0000u;
constexpr DWORD kOneSwizzle = 0x00550000u;
constexpr DWORD kHalfSwizzle = 0x00AA0000u;
constexpr UINT kVertexLiterals = kFixedFunctionVertexConstantCount;
constexpr UINT kPixelLiterals = kFixedFunctionPixelConstantCount;

struct ReleaseShader
{
    void operator()(IUnknown* shader) const { shader->Release(); }
};

bool Fail(std::string& failure, const std::string& reason)
{
    failure = "fixed function " + reason;
    return false;
}

bool FailStage(std::string& failure, UINT stage, const std::string& reason)
{
    return Fail(failure, "stage " + std::to_string(stage) + " " + reason);
}

DWORD Register(unsigned type, unsigned index, DWORD components)
{
    return 0x80000000u | ((type << D3DSP_REGTYPE_SHIFT) & D3DSP_REGTYPE_MASK) |
           ((type << D3DSP_REGTYPE_SHIFT2) & D3DSP_REGTYPE_MASK2) | index | components;
}

DWORD Destination(unsigned type, unsigned index, DWORD mask = D3DSP_WRITEMASK_ALL)
{
    return Register(type, index, mask);
}

DWORD Source(unsigned type, unsigned index, DWORD swizzle = D3DSP_NOSWIZZLE)
{
    return Register(type, index, swizzle);
}

DWORD Temporary(unsigned index, DWORD swizzle = D3DSP_NOSWIZZLE)
{
    return Source(D3DSPR_TEMP, index, swizzle);
}

void Emit(std::vector<DWORD>& code, DWORD opcode, std::initializer_list<DWORD> parameters)
{
    const bool subtract = opcode == D3DSIO_SUB;
    code.push_back((subtract ? D3DSIO_ADD : opcode) |
                   (static_cast<DWORD>(parameters.size()) << D3DSI_INSTLENGTH_SHIFT));
    code.insert(code.end(), parameters.begin(), parameters.end());
    if (subtract)
        code.back() |= D3DSPSM_NEG;
}

void Declare(std::vector<DWORD>& code, unsigned type, unsigned reg, unsigned usage, unsigned index = 0)
{
    Emit(code, D3DSIO_DCL,
         {0x80000000u | usage | (index << D3DSP_DCL_USAGEINDEX_SHIFT), Destination(type, reg)});
}

void Move(std::vector<DWORD>& code, unsigned type, unsigned reg, DWORD source, DWORD mask = D3DSP_WRITEMASK_ALL,
          bool saturate = false)
{
    Emit(code, D3DSIO_MOV, {Destination(type, reg, mask) | (saturate ? D3DSPDM_SATURATE : 0), source});
}

void Colour(DWORD packed, float* rgba)
{
    rgba[0] = static_cast<float>((packed >> 16) & 255) / 255.0f;
    rgba[1] = static_cast<float>((packed >> 8) & 255) / 255.0f;
    rgba[2] = static_cast<float>(packed & 255) / 255.0f;
    rgba[3] = static_cast<float>(packed >> 24) / 255.0f;
}

D3DMATRIX Multiply(const D3DMATRIX& first, const D3DMATRIX& second)
{
    D3DMATRIX product = {};
    for (UINT row = 0; row < 4; ++row)
        for (UINT column = 0; column < 4; ++column)
            for (UINT index = 0; index < 4; ++index)
                product.m[row][column] += first.m[row][index] * second.m[index][column];
    return product;
}

bool Transpose(const D3DMATRIX& matrix, float (*constants)[4])
{
    for (UINT row = 0; row < 4; ++row)
        for (UINT column = 0; column < 4; ++column)
        {
            if (!std::isfinite(matrix.m[column][row]))
                return false;
            constants[row][column] = matrix.m[column][row];
        }
    return true;
}

const D3DVERTEXELEMENT9* FindElement(const FixedFunctionMaterialState& state, BYTE usage, BYTE index)
{
    for (const auto& element : state.declaration)
        if (element.Stream != 0xFF && element.Usage == usage && element.UsageIndex == index)
            return &element;
    return nullptr;
}

bool ReadDeclaration(IDirect3DDevice9* device, FixedFunctionMaterialState& state, std::string& failure)
{
    IDirect3DVertexDeclaration9* declaration = nullptr;
    if (FAILED(device->GetVertexDeclaration(&declaration)) || !declaration)
        return Fail(failure, "vertex declaration is unavailable");
    D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH + 1] = {};
    UINT count = MAXD3DDECLLENGTH + 1;
    const HRESULT result = declaration->GetDeclaration(elements, &count);
    declaration->Release();
    if (FAILED(result) || count < 2 || count > MAXD3DDECLLENGTH + 1 || elements[count - 1].Stream != 0xFF)
        return Fail(failure, "vertex declaration is invalid");
    state.declaration.assign(elements, elements + count);
    return true;
}

bool ValidateVertex(IDirect3DDevice9* device, FixedFunctionMaterialState& state, std::string& failure)
{
    constexpr D3DRENDERSTATETYPE disabled[] = {
        D3DRS_LIGHTING, D3DRS_VERTEXBLEND, D3DRS_INDEXEDVERTEXBLENDENABLE, D3DRS_POINTSCALEENABLE,
        D3DRS_POINTSPRITEENABLE, D3DRS_CLIPPLANEENABLE,
    };
    constexpr const char* names[] = {
        "lighting", "vertex blending", "indexed vertex blending", "point scaling", "point sprites", "clip planes",
    };
    for (UINT index = 0; index < sizeof(disabled) / sizeof(disabled[0]); ++index)
    {
        DWORD value = 0;
        if (FAILED(device->GetRenderState(disabled[index], &value)))
            return Fail(failure, std::string(names[index]) + " state is unavailable");
        if (value != 0)
            return Fail(failure, std::string(names[index]) + " is unsupported (" + std::to_string(value) + ")");
    }
    if (!ReadDeclaration(device, state, failure))
        return false;
    const auto* position = FindElement(state, D3DDECLUSAGE_POSITION, 0);
    if (FindElement(state, D3DDECLUSAGE_POSITIONT, 0))
        return Fail(failure, "POSITIONT vertices have no reliable view-depth contract");
    if (!position || position->Type != D3DDECLTYPE_FLOAT3)
        return Fail(failure, "vertices require FLOAT3 POSITION0");
    for (const auto& element : state.declaration)
    {
        if (element.Stream == 0xFF)
            break;
        if (element.Method != D3DDECLMETHOD_DEFAULT || element.Usage == D3DDECLUSAGE_BLENDWEIGHT ||
            element.Usage == D3DDECLUSAGE_BLENDINDICES || element.Usage == D3DDECLUSAGE_PSIZE)
            return Fail(failure, "skinned, point-size or nondefault vertex elements are unsupported");
        UINT frequency = 0;
        if (FAILED(device->GetStreamSourceFreq(element.Stream, &frequency)) || frequency != 1)
            return Fail(failure, "instanced vertex streams are unsupported");
        if (element.Usage == D3DDECLUSAGE_COLOR &&
            (element.UsageIndex > 1 || element.Type != D3DDECLTYPE_D3DCOLOR))
            return Fail(failure, "vertex colour requires packed COLOR0/COLOR1");
        if (element.Usage == D3DDECLUSAGE_TEXCOORD &&
            (element.UsageIndex >= 8 || element.Type > D3DDECLTYPE_FLOAT4))
            return Fail(failure, "texture coordinates require FLOAT1 through FLOAT4 TEXCOORD0..7");
    }
    state.diffuse = FindElement(state, D3DDECLUSAGE_COLOR, 0) != nullptr;
    state.specular = FindElement(state, D3DDECLUSAGE_COLOR, 1) != nullptr;
    D3DMATRIX world = {}, view = {}, projection = {};
    if (FAILED(device->GetTransform(D3DTS_WORLD, &world)) || FAILED(device->GetTransform(D3DTS_VIEW, &view)) ||
        FAILED(device->GetTransform(D3DTS_PROJECTION, &projection)))
        return Fail(failure, "world/view/projection transforms are unavailable");
    if (std::fabs(projection._14) > 1e-6f || std::fabs(projection._24) > 1e-6f ||
        std::fabs(projection._34 - 1.0f) > 1e-6f || std::fabs(projection._44) > 1e-6f)
        return Fail(failure, "projection must preserve clip.w = view depth");
    const D3DMATRIX worldView = Multiply(world, view);
    if (!Transpose(Multiply(worldView, projection), state.vertexConstants) ||
        !Transpose(worldView, state.vertexConstants + 36))
        return Fail(failure, "world/view/projection transforms are nonfinite");
    state.vertexConstantCount = kFixedFunctionVertexConstantCount;
    return true;
}

unsigned RegisterType(DWORD token)
{
    return ((token & D3DSP_REGTYPE_MASK) >> D3DSP_REGTYPE_SHIFT) |
           ((token & D3DSP_REGTYPE_MASK2) >> D3DSP_REGTYPE_SHIFT2);
}

bool ReadVertexOutputs(IDirect3DDevice9* device, FixedFunctionMaterialState& state, bool* coordinates,
                       std::string& failure)
{
    IDirect3DVertexShader9* shader = nullptr;
    if (FAILED(device->GetVertexShader(&shader)) || !shader)
        return Fail(failure, "native vertex shader is unavailable");
    const std::unique_ptr<IDirect3DVertexShader9, ReleaseShader> owned(shader);
    UINT bytes = 0;
    HRESULT result = shader->GetFunction(nullptr, &bytes);
    std::vector<DWORD> code;
    if (SUCCEEDED(result) && bytes >= 8 && bytes <= 65536 && bytes % sizeof(DWORD) == 0)
    {
        code.resize(bytes / sizeof(DWORD));
        result = shader->GetFunction(code.data(), &bytes);
    }
    if (FAILED(result) || code.empty())
        return Fail(failure, "native vertex outputs could not be read");
    const UINT major = D3DSHADER_VERSION_MAJOR(code[0]);
    if (major != 2 && major != 3)
        return Fail(failure, "native vertex shader must be model2 or model3");
    for (size_t offset = 1; offset < code.size();)
    {
        const DWORD instruction = code[offset++];
        const DWORD opcode = instruction & D3DSI_OPCODE_MASK;
        if (opcode == D3DSIO_END)
            return true;
        const UINT length = opcode == D3DSIO_COMMENT
                                ? (instruction & D3DSI_COMMENTSIZE_MASK) >> D3DSI_COMMENTSIZE_SHIFT
                                : (instruction & D3DSI_INSTLENGTH_MASK) >> D3DSI_INSTLENGTH_SHIFT;
        if (length > code.size() - offset)
            return Fail(failure, "native vertex bytecode is truncated");
        if (major == 3 && opcode == D3DSIO_DCL && length == 2 && RegisterType(code[offset + 1]) == D3DSPR_OUTPUT)
        {
            const DWORD usage = code[offset] & D3DSP_DCL_USAGE_MASK;
            const DWORD index = (code[offset] & D3DSP_DCL_USAGEINDEX_MASK) >> D3DSP_DCL_USAGEINDEX_SHIFT;
            if (usage == D3DDECLUSAGE_COLOR && index == 0)
                state.diffuse = true;
            if (usage == D3DDECLUSAGE_COLOR && index == 1)
                state.specular = true;
            if (usage == D3DDECLUSAGE_TEXCOORD && index < 8)
                coordinates[index] = true;
        }
        if (major == 2 && opcode != D3DSIO_DCL && opcode != D3DSIO_COMMENT && length > 0)
        {
            const unsigned type = RegisterType(code[offset]);
            const DWORD index = code[offset] & D3DSP_REGNUM_MASK;
            if (type == D3DSPR_ATTROUT && index == 0)
                state.diffuse = true;
            if (type == D3DSPR_ATTROUT && index == 1)
                state.specular = true;
            if (type == D3DSPR_TEXCRDOUT && index < 8)
                coordinates[index] = true;
        }
        offset += length;
    }
    return Fail(failure, "native vertex bytecode has no end");
}

bool ValidatePixelInputs(IDirect3DDevice9* device, std::string& failure)
{
    IDirect3DPixelShader9* shader = nullptr;
    if (FAILED(device->GetPixelShader(&shader)) || !shader)
        return Fail(failure, "native pixel shader is unavailable");
    const std::unique_ptr<IDirect3DPixelShader9, ReleaseShader> owned(shader);
    UINT bytes = 0;
    if (FAILED(shader->GetFunction(nullptr, &bytes)) || bytes < 8 || bytes > 65536 || bytes % sizeof(DWORD))
        return Fail(failure, "native pixel bytecode is unavailable");
    std::vector<DWORD> code(bytes / sizeof(DWORD));
    if (FAILED(shader->GetFunction(code.data(), &bytes)))
        return Fail(failure, "native pixel bytecode is unavailable");
    const UINT major = D3DSHADER_VERSION_MAJOR(code[0]);
    if (major != 2)
        return Fail(failure, "native pixel shader requires model2 with fixed function vertices");
    for (size_t offset = 1; offset < code.size();)
    {
        const DWORD instruction = code[offset++];
        const DWORD opcode = instruction & D3DSI_OPCODE_MASK;
        if (opcode == D3DSIO_END)
            return true;
        const UINT length = opcode == D3DSIO_COMMENT
                                ? (instruction & D3DSI_COMMENTSIZE_MASK) >> D3DSI_COMMENTSIZE_SHIFT
                                : (instruction & D3DSI_INSTLENGTH_MASK) >> D3DSI_INSTLENGTH_SHIFT;
        if (length > code.size() - offset)
            return Fail(failure, "native pixel bytecode is truncated");
        if (opcode == D3DSIO_DCL && length == 2)
        {
            const unsigned type = RegisterType(code[offset + 1]);
            const DWORD reg = code[offset + 1] & D3DSP_REGNUM_MASK;
            if (type == D3DSPR_TEXTURE && reg >= 7)
                return Fail(failure, "native pixel TEXCOORD7 is reserved for own-depth fog");
        }
        offset += length;
    }
    return Fail(failure, "native pixel bytecode has no end");
}

bool SupportedOperation(DWORD operation, bool alpha)
{
    return (operation >= D3DTOP_SELECTARG1 && operation <= D3DTOP_BLENDCURRENTALPHA) ||
           (!alpha && operation >= D3DTOP_MODULATEALPHA_ADDCOLOR &&
            operation <= D3DTOP_MODULATEINVCOLOR_ADDALPHA) ||
           operation == D3DTOP_MULTIPLYADD || operation == D3DTOP_LERP;
}

bool UsesArgument(DWORD operation, UINT argument)
{
    if (argument == 0)
        return operation == D3DTOP_MULTIPLYADD || operation == D3DTOP_LERP;
    return operation != static_cast<DWORD>(argument == 1 ? D3DTOP_SELECTARG2 : D3DTOP_SELECTARG1);
}

bool UsesTexture(const FixedFunctionTextureStage& stage)
{
    for (UINT argument = 0; argument < 3; ++argument)
        if ((UsesArgument(stage.colourOperation, argument) &&
             (stage.colourArguments[argument] & D3DTA_SELECTMASK) == D3DTA_TEXTURE) ||
            (UsesArgument(stage.alphaOperation, argument) &&
             (stage.alphaArguments[argument] & D3DTA_SELECTMASK) == D3DTA_TEXTURE))
            return true;
    return stage.colourOperation == D3DTOP_BLENDTEXTUREALPHA ||
           stage.colourOperation == D3DTOP_BLENDTEXTUREALPHAPM ||
           stage.alphaOperation == D3DTOP_BLENDTEXTUREALPHA ||
           stage.alphaOperation == D3DTOP_BLENDTEXTUREALPHAPM;
}

bool CaptureStage(IDirect3DDevice9* device, UINT index, FixedFunctionMaterialState& state,
                  const bool* nativeCoordinates, std::string& failure)
{
    auto& stage = state.stages[index];
    constexpr D3DTEXTURESTAGESTATETYPE fields[] = {
        D3DTSS_COLOROP, D3DTSS_ALPHAOP, D3DTSS_COLORARG0, D3DTSS_COLORARG1, D3DTSS_COLORARG2,
        D3DTSS_ALPHAARG0, D3DTSS_ALPHAARG1, D3DTSS_ALPHAARG2, D3DTSS_RESULTARG,
        D3DTSS_TEXCOORDINDEX, D3DTSS_TEXTURETRANSFORMFLAGS,
    };
    DWORD* values[] = {
        &stage.colourOperation, &stage.alphaOperation, &stage.colourArguments[0], &stage.colourArguments[1],
        &stage.colourArguments[2], &stage.alphaArguments[0], &stage.alphaArguments[1], &stage.alphaArguments[2],
        &stage.result, &stage.coordinateIndex, &stage.transform,
    };
    for (UINT field = 0; field < sizeof(fields) / sizeof(fields[0]); ++field)
        if (FAILED(device->GetTextureStageState(index, fields[field], values[field])))
            return FailStage(failure, index, "state is unavailable");
    if (state.pixel && stage.colourOperation == D3DTOP_DISABLE)
        return true;
    IDirect3DBaseTexture9* texture = nullptr;
    if (FAILED(device->GetTexture(index, &texture)))
        return FailStage(failure, index, "texture is unavailable");
    const D3DRESOURCETYPE textureType = texture ? texture->GetType() : D3DRTYPE_TEXTURE;
    stage.texture = texture != nullptr;
    if (texture)
        texture->Release();
    if (state.pixel && !stage.texture && stage.colourArguments[1] == D3DTA_TEXTURE &&
        UsesArgument(stage.colourOperation, 1))
    {
        stage.colourOperation = D3DTOP_DISABLE;
        return true;
    }
    if (state.pixel)
    {
        if (!SupportedOperation(stage.colourOperation, false) || !SupportedOperation(stage.alphaOperation, true))
            return FailStage(failure, index, "combiner is unsupported (colour=" +
                             std::to_string(stage.colourOperation) + ", alpha=" +
                             std::to_string(stage.alphaOperation) + ")");
        if (stage.result != D3DTA_CURRENT && stage.result != D3DTA_TEMP)
            return FailStage(failure, index, "result register is unsupported");
        for (UINT argument = 0; argument < 3; ++argument)
            if ((stage.colourArguments[argument] & ~(D3DTA_SELECTMASK | D3DTA_COMPLEMENT | D3DTA_ALPHAREPLICATE)) ||
                (stage.alphaArguments[argument] & ~(D3DTA_SELECTMASK | D3DTA_COMPLEMENT | D3DTA_ALPHAREPLICATE)) ||
                (stage.colourArguments[argument] & D3DTA_SELECTMASK) > D3DTA_CONSTANT ||
                (stage.alphaArguments[argument] & D3DTA_SELECTMASK) > D3DTA_CONSTANT)
                return FailStage(failure, index, "argument is unsupported");
        if (UsesTexture(stage) && textureType != D3DRTYPE_TEXTURE)
            return FailStage(failure, index, "requires a 2D texture");
        DWORD constant = 0;
        if (FAILED(device->GetTextureStageState(index, D3DTSS_CONSTANT, &constant)))
            return FailStage(failure, index, "constant is unavailable");
        Colour(constant, state.pixelConstants[index + 1]);
    }
    if (state.vertex)
    {
        const DWORD generation = stage.coordinateIndex & 0xFFFF0000u;
        const DWORD coordinate = stage.coordinateIndex & 0xFFFFu;
        if (generation != D3DTSS_TCI_PASSTHRU && generation != D3DTSS_TCI_CAMERASPACEPOSITION)
            return FailStage(failure, index, "coordinate generation is unsupported (" +
                             std::to_string(generation) + ")");
        if (coordinate >= 8)
            return FailStage(failure, index, "coordinate index is unsupported");
        const DWORD count = stage.transform & 255;
        if ((stage.transform & ~(255u | D3DTTFF_PROJECTED)) || count == 1 || count > 4 ||
            ((stage.transform & D3DTTFF_PROJECTED) && count < 3))
            return FailStage(failure, index, "texture transform is unsupported");
        if (!state.pixel && (stage.transform & D3DTTFF_PROJECTED))
            return FailStage(failure, index, "projected coordinates with a native pixel shader are unsupported");
        if (count)
        {
            D3DMATRIX transform = {};
            if (FAILED(device->GetTransform(static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + index), &transform)) ||
                !Transpose(transform, state.vertexConstants + 4 + index * 4))
                return FailStage(failure, index, "texture matrix is unavailable or nonfinite");
        }
        stage.coordinates = true;
    }
    else
    {
        if (stage.transform != D3DTTFF_DISABLE)
            return FailStage(failure, index, "texture transform with a native vertex shader is unsupported");
        stage.coordinates = nativeCoordinates[index];
    }
    DWORD wrapping = 0;
    if (FAILED(device->GetRenderState(static_cast<D3DRENDERSTATETYPE>(D3DRS_WRAP0 + index), &wrapping)) || wrapping)
        return FailStage(failure, index, "rasterizer texture wrapping is unsupported");
    return true;
}

void MakeKey(FixedFunctionMaterialState& state)
{
    state.key = {state.vertex, state.pixel, state.diffuse, state.specular, state.addSpecular, state.stageCount};
    for (const auto& element : state.declaration)
    {
        state.key.push_back(element.Stream | (static_cast<DWORD>(element.Offset) << 16));
        state.key.push_back(element.Type | (static_cast<DWORD>(element.Method) << 8) |
                            (static_cast<DWORD>(element.Usage) << 16) |
                            (static_cast<DWORD>(element.UsageIndex) << 24));
    }
    for (UINT index = 0; index < state.stageCount; ++index)
    {
        const auto& stage = state.stages[index];
        state.key.insert(state.key.end(), {stage.colourOperation, stage.alphaOperation, stage.result,
                                           stage.coordinateIndex, stage.transform, stage.texture, stage.coordinates});
        state.key.insert(state.key.end(), stage.colourArguments, stage.colourArguments + 3);
        state.key.insert(state.key.end(), stage.alphaArguments, stage.alphaArguments + 3);
    }
}

void GenerateVertex(const FixedFunctionMaterialState& state, std::vector<DWORD>& code)
{
    code = {D3DVS_VERSION(3, 0)};
    Declare(code, D3DSPR_INPUT, 0, D3DDECLUSAGE_POSITION);
    Declare(code, D3DSPR_OUTPUT, 0, D3DDECLUSAGE_POSITION);
    Declare(code, D3DSPR_OUTPUT, 1, D3DDECLUSAGE_COLOR, 0);
    Declare(code, D3DSPR_OUTPUT, 2, D3DDECLUSAGE_COLOR, 1);
    UINT input = 1;
    UINT diffuse = 0, specular = 0;
    UINT coordinates[8] = {};
    if (state.diffuse)
    {
        diffuse = input++;
        Declare(code, D3DSPR_INPUT, diffuse, D3DDECLUSAGE_COLOR, 0);
    }
    if (state.specular)
    {
        specular = input++;
        Declare(code, D3DSPR_INPUT, specular, D3DDECLUSAGE_COLOR, 1);
    }
    for (UINT index = 0; index < 8; ++index)
        if (FindElement(state, D3DDECLUSAGE_TEXCOORD, static_cast<BYTE>(index)))
        {
            coordinates[index] = input++;
            Declare(code, D3DSPR_INPUT, coordinates[index], D3DDECLUSAGE_TEXCOORD, index);
        }
    for (UINT stage = 0; stage < state.stageCount; ++stage)
        Declare(code, D3DSPR_OUTPUT, 3 + stage, D3DDECLUSAGE_TEXCOORD, stage);
    Emit(code, D3DSIO_DEF, {Destination(D3DSPR_CONST, kVertexLiterals), 0, 0x3F800000u, 0, 0x3F800000u});
    Emit(code, D3DSIO_M4x4, {Destination(D3DSPR_OUTPUT, 0), Source(D3DSPR_INPUT, 0), Source(D3DSPR_CONST, 0)});
    Move(code, D3DSPR_OUTPUT, 1, state.diffuse ? Source(D3DSPR_INPUT, diffuse)
                                             : Source(D3DSPR_CONST, kVertexLiterals, kOneSwizzle),
         D3DSP_WRITEMASK_ALL, true);
    Move(code, D3DSPR_OUTPUT, 2, state.specular ? Source(D3DSPR_INPUT, specular)
                                              : Source(D3DSPR_CONST, kVertexLiterals, 0),
         D3DSP_WRITEMASK_ALL, true);
    for (UINT index = 0; index < state.stageCount; ++index)
    {
        const auto& stage = state.stages[index];
        const DWORD coordinate = stage.coordinateIndex & 0xFFFFu;
        if ((stage.coordinateIndex & 0xFFFF0000u) == D3DTSS_TCI_CAMERASPACEPOSITION)
            Emit(code, D3DSIO_M4x4,
                 {Destination(D3DSPR_TEMP, 0), Source(D3DSPR_INPUT, 0), Source(D3DSPR_CONST, 36)});
        else if (coordinates[coordinate])
            Move(code, D3DSPR_TEMP, 0, Source(D3DSPR_INPUT, coordinates[coordinate]));
        else
            Move(code, D3DSPR_TEMP, 0, Source(D3DSPR_CONST, kVertexLiterals, 0x00400000u));
        if ((stage.transform & 255) && (stage.coordinateIndex & 0xFFFF0000u) == D3DTSS_TCI_PASSTHRU)
        {
            const auto* element = FindElement(state, D3DDECLUSAGE_TEXCOORD, static_cast<BYTE>(coordinate));
            const UINT dimensions = element ? element->Type + 1 : 2;
            if (dimensions < 3)
            {
                const DWORD oneMask = D3DSP_WRITEMASK_0 << dimensions;
                Move(code, D3DSPR_TEMP, 0, Source(D3DSPR_CONST, kVertexLiterals, kOneSwizzle), oneMask);
                const DWORD zeroMask = D3DSP_WRITEMASK_ALL & ~((oneMask << 1) - D3DSP_WRITEMASK_0);
                Move(code, D3DSPR_TEMP, 0, Source(D3DSPR_CONST, kVertexLiterals, 0), zeroMask);
            }
        }
        if (stage.transform & 255)
            Emit(code, D3DSIO_M4x4,
                 {Destination(D3DSPR_TEMP, 1), Temporary(0), Source(D3DSPR_CONST, 4 + index * 4)});
        else
            Move(code, D3DSPR_TEMP, 1, Temporary(0));
        Move(code, D3DSPR_OUTPUT, 3 + index, Temporary(1));
    }
    code.push_back(D3DSIO_END);
}

DWORD ArgumentSource(UINT stage, DWORD argument)
{
    DWORD source = 0;
    switch (argument & D3DTA_SELECTMASK)
    {
    case D3DTA_DIFFUSE: source = Temporary(9); break;
    case D3DTA_CURRENT: source = Temporary(0); break;
    case D3DTA_TEXTURE: source = Temporary(2); break;
    case D3DTA_TFACTOR: source = Source(D3DSPR_CONST, 0); break;
    case D3DTA_SPECULAR: source = Temporary(10); break;
    case D3DTA_TEMP: source = Temporary(1); break;
    case D3DTA_CONSTANT: source = Source(D3DSPR_CONST, stage + 1); break;
    }
    if (argument & D3DTA_ALPHAREPLICATE)
        source = (source & ~D3DSP_SWIZZLE_MASK) | kAlphaSwizzle;
    return source;
}

void LoadArgument(std::vector<DWORD>& code, UINT stage, UINT destination, DWORD argument)
{
    const DWORD source = ArgumentSource(stage, argument);
    if (argument & D3DTA_COMPLEMENT)
        Emit(code, D3DSIO_SUB, {Destination(D3DSPR_TEMP, destination),
                              Source(D3DSPR_CONST, kPixelLiterals, kOneSwizzle), source});
    else
        Move(code, D3DSPR_TEMP, destination, source);
}

void Combine(std::vector<DWORD>& code, DWORD operation, DWORD mask)
{
    const DWORD destination = Destination(D3DSPR_TEMP, 6, mask);
    const DWORD first = Temporary(4), second = Temporary(5), third = Temporary(3);
    const DWORD one = Source(D3DSPR_CONST, kPixelLiterals, kOneSwizzle);
    switch (operation)
    {
    case D3DTOP_SELECTARG1: Move(code, D3DSPR_TEMP, 6, first, mask); break;
    case D3DTOP_SELECTARG2: Move(code, D3DSPR_TEMP, 6, second, mask); break;
    case D3DTOP_MODULATE:
    case D3DTOP_MODULATE2X:
    case D3DTOP_MODULATE4X:
        Emit(code, D3DSIO_MUL, {destination, first, second});
        if (operation != D3DTOP_MODULATE)
            Emit(code, D3DSIO_MUL,
                 {destination, Temporary(6), operation == D3DTOP_MODULATE2X
                                                 ? Source(D3DSPR_CONST, kPixelLiterals, kAlphaSwizzle)
                                                 : Source(D3DSPR_CONST, kPixelLiterals + 1, 0)});
        break;
    case D3DTOP_ADD: Emit(code, D3DSIO_ADD, {destination, first, second}); break;
    case D3DTOP_SUBTRACT: Emit(code, D3DSIO_SUB, {destination, first, second}); break;
    case D3DTOP_ADDSIGNED:
    case D3DTOP_ADDSIGNED2X:
        Emit(code, D3DSIO_ADD, {destination, first, second});
        Emit(code, D3DSIO_SUB, {destination, Temporary(6), Source(D3DSPR_CONST, kPixelLiterals, kHalfSwizzle)});
        if (operation == D3DTOP_ADDSIGNED2X)
            Emit(code, D3DSIO_MUL,
                 {destination, Temporary(6), Source(D3DSPR_CONST, kPixelLiterals, kAlphaSwizzle)});
        break;
    case D3DTOP_ADDSMOOTH:
        Emit(code, D3DSIO_SUB, {Destination(D3DSPR_TEMP, 7), one, first});
        Emit(code, D3DSIO_MAD, {destination, Temporary(7), second, first});
        break;
    case D3DTOP_BLENDDIFFUSEALPHA:
    case D3DTOP_BLENDTEXTUREALPHA:
    case D3DTOP_BLENDFACTORALPHA:
    case D3DTOP_BLENDCURRENTALPHA:
    {
        const DWORD factor = operation == D3DTOP_BLENDDIFFUSEALPHA ? Temporary(9, kAlphaSwizzle)
                                 : operation == D3DTOP_BLENDTEXTUREALPHA ? Temporary(2, kAlphaSwizzle)
                                 : operation == D3DTOP_BLENDFACTORALPHA ? Source(D3DSPR_CONST, 0, kAlphaSwizzle)
                                                                      : Temporary(0, kAlphaSwizzle);
        Emit(code, D3DSIO_LRP, {destination, factor, first, second});
        break;
    }
    case D3DTOP_BLENDTEXTUREALPHAPM:
        Emit(code, D3DSIO_SUB, {Destination(D3DSPR_TEMP, 7), one, Temporary(2, kAlphaSwizzle)});
        Emit(code, D3DSIO_MAD, {destination, Temporary(7), second, first});
        break;
    case D3DTOP_MODULATEALPHA_ADDCOLOR:
        Emit(code, D3DSIO_MAD, {destination, Temporary(4, kAlphaSwizzle), second, first});
        break;
    case D3DTOP_MODULATECOLOR_ADDALPHA:
        Emit(code, D3DSIO_MAD, {destination, first, second, Temporary(4, kAlphaSwizzle)});
        break;
    case D3DTOP_MODULATEINVALPHA_ADDCOLOR:
        Emit(code, D3DSIO_SUB, {Destination(D3DSPR_TEMP, 7), one, Temporary(4, kAlphaSwizzle)});
        Emit(code, D3DSIO_MAD, {destination, Temporary(7), second, first});
        break;
    case D3DTOP_MODULATEINVCOLOR_ADDALPHA:
        Emit(code, D3DSIO_SUB, {Destination(D3DSPR_TEMP, 7), one, first});
        Emit(code, D3DSIO_MAD, {destination, Temporary(7), second, Temporary(4, kAlphaSwizzle)});
        break;
    case D3DTOP_MULTIPLYADD: Emit(code, D3DSIO_MAD, {destination, first, second, third}); break;
    case D3DTOP_LERP: Emit(code, D3DSIO_LRP, {destination, third, first, second}); break;
    }
}

void GeneratePixel(const FixedFunctionMaterialState& state, std::vector<DWORD>& code)
{
    code = {D3DPS_VERSION(3, 0)};
    if (state.diffuse)
        Declare(code, D3DSPR_INPUT, 0, D3DDECLUSAGE_COLOR, 0);
    if (state.specular)
        Declare(code, D3DSPR_INPUT, 1, D3DDECLUSAGE_COLOR, 1);
    for (UINT stage = 0; stage < state.stageCount; ++stage)
        if (UsesTexture(state.stages[stage]))
        {
            if (state.stages[stage].coordinates)
                Declare(code, D3DSPR_INPUT, 2 + stage, D3DDECLUSAGE_TEXCOORD, stage);
            Emit(code, D3DSIO_DCL, {0x80000000u | D3DSTT_2D,
                                  Destination(D3DSPR_SAMPLER, stage)});
        }
    Emit(code, D3DSIO_DEF, {Destination(D3DSPR_CONST, kPixelLiterals), 0, 0x3F800000u, 0x3F000000u, 0x40000000u});
    Emit(code, D3DSIO_DEF, {Destination(D3DSPR_CONST, kPixelLiterals + 1), 0x40800000u, 0, 0, 0});
    Move(code, D3DSPR_TEMP, 9, state.diffuse ? Source(D3DSPR_INPUT, 0)
                                           : Source(D3DSPR_CONST, kPixelLiterals, kOneSwizzle),
         D3DSP_WRITEMASK_ALL, true);
    Move(code, D3DSPR_TEMP, 10, state.specular ? Source(D3DSPR_INPUT, 1)
                                            : Source(D3DSPR_CONST, kPixelLiterals, 0),
         D3DSP_WRITEMASK_ALL, true);
    Move(code, D3DSPR_TEMP, 0, Temporary(9));
    Move(code, D3DSPR_TEMP, 1, Source(D3DSPR_CONST, kPixelLiterals, 0));
    for (UINT stage = 0; stage < state.stageCount; ++stage)
    {
        const auto& current = state.stages[stage];
        if (UsesTexture(current))
        {
            const DWORD coordinates = current.coordinates ? Source(D3DSPR_INPUT, 2 + stage)
                                                           : Source(D3DSPR_CONST, kPixelLiterals, 0);
            if (current.transform & D3DTTFF_PROJECTED)
            {
                const DWORD divisor = (current.transform & 255) == 3 ? kHalfSwizzle : kAlphaSwizzle;
                Emit(code, D3DSIO_RCP,
                     {Destination(D3DSPR_TEMP, 8), (coordinates & ~D3DSP_SWIZZLE_MASK) | divisor});
                Emit(code, D3DSIO_MUL, {Destination(D3DSPR_TEMP, 8), coordinates, Temporary(8)});
            }
            Emit(code, D3DSIO_TEX, {Destination(D3DSPR_TEMP, 2),
                                   current.transform & D3DTTFF_PROJECTED ? Temporary(8) : coordinates,
                                   Source(D3DSPR_SAMPLER, stage)});
        }
        for (UINT argument = 0; argument < 3; ++argument)
            if (UsesArgument(current.colourOperation, argument))
                LoadArgument(code, stage, 3 + argument, current.colourArguments[argument]);
        Combine(code, current.colourOperation, kRgb);
        for (UINT argument = 0; argument < 3; ++argument)
            if (UsesArgument(current.alphaOperation, argument))
                LoadArgument(code, stage, 3 + argument, current.alphaArguments[argument]);
        Combine(code, current.alphaOperation, kAlpha);
        Move(code, D3DSPR_TEMP, current.result == D3DTA_TEMP ? 1 : 0, Temporary(6), D3DSP_WRITEMASK_ALL, true);
    }
    if (state.addSpecular && state.specular)
        Emit(code, D3DSIO_ADD, {Destination(D3DSPR_TEMP, 0, kRgb) | D3DSPDM_SATURATE, Temporary(0), Temporary(10)});
    Move(code, D3DSPR_COLOROUT, 0, Temporary(0));
    code.push_back(D3DSIO_END);
}
}

bool CaptureFixedFunctionMaterial(IDirect3DDevice9* device, bool vertex, bool pixel,
                                  FixedFunctionMaterialState& state, std::string& failure)
{
    state = {};
    failure.clear();
    state.vertex = vertex;
    state.pixel = pixel;
    if (!device || (!vertex && !pixel))
        return Fail(failure, "capture requires at least one missing shader");
    DWORD shade = 0, specular = 0;
    if (FAILED(device->GetRenderState(D3DRS_SHADEMODE, &shade)) || shade != D3DSHADE_GOURAUD)
        return Fail(failure, "flat shading is unsupported");
    if (FAILED(device->GetRenderState(D3DRS_SPECULARENABLE, &specular)))
        return Fail(failure, "specular state is unavailable");
    state.addSpecular = pixel && specular != 0;
    bool nativeCoordinates[8] = {};
    if (vertex ? !ValidateVertex(device, state, failure)
               : !ReadVertexOutputs(device, state, nativeCoordinates, failure))
        return false;
    if (!pixel && !ValidatePixelInputs(device, failure))
        return false;
    if (pixel)
    {
        DWORD factor = 0;
        if (FAILED(device->GetRenderState(D3DRS_TEXTUREFACTOR, &factor)))
            return Fail(failure, "texture factor is unavailable");
        Colour(factor, state.pixelConstants[0]);
        state.pixelConstantCount = kFixedFunctionPixelConstantCount;
    }
    for (UINT stage = 0; stage < (pixel ? 8u : 7u); ++stage)
    {
        if (!CaptureStage(device, stage, state, nativeCoordinates, failure))
            return false;
        if (pixel && state.stages[stage].colourOperation == D3DTOP_DISABLE)
            break;
        ++state.stageCount;
    }
    if (state.stageCount > 7)
        return Fail(failure, "eight texture stages leave no TEXCOORD7 for own-depth fog");
    MakeKey(state);
    return true;
}

bool BuildFixedFunctionMaterialShaders(const FixedFunctionMaterialState& state, std::vector<DWORD>& vertex,
                                       std::vector<DWORD>& pixel, std::string& failure)
{
    failure.clear();
    if ((!state.vertex && !state.pixel) || state.stageCount > 7)
        return Fail(failure, "shader generation requires a captured supported state");
    if (state.vertex)
        GenerateVertex(state, vertex);
    if (state.pixel)
        GeneratePixel(state, pixel);
    return true;
}
