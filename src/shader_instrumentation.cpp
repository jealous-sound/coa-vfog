#include "shader_instrumentation.h"

#include "ps_material_footer.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <map>
#include <set>
#include <utility>

namespace
{
constexpr DWORD kParameterToken = 0x80000000u;
constexpr DWORD kFullWriteMask = D3DSP_WRITEMASK_ALL;
constexpr DWORD kIdentitySwizzle = D3DSP_NOSWIZZLE;
constexpr DWORD kShaderVersionMask = 0xFFFF0000u;
constexpr DWORD kVertexShaderVersion = 0xFFFE0000u;
constexpr DWORD kPixelShaderVersion = 0xFFFF0000u;
constexpr unsigned kMaximumShaderWords = 16384;
constexpr unsigned kPixelConstants = 224;
constexpr unsigned kShaderTemporaries = 32;
constexpr unsigned kPixelInputs = 10;
constexpr unsigned kVertexOutputs = 12;
constexpr unsigned kPixelSamplers = 16;
constexpr DWORD kPackedClipMask = D3DSP_WRITEMASK_1 | D3DSP_WRITEMASK_2 | D3DSP_WRITEMASK_3;
constexpr DWORD kPackedClipSourceSwizzle = 0x00D00000u;

struct Register
{
    unsigned type;
    unsigned index;

    bool operator<(const Register& other) const
    {
        return std::make_pair(type, index) < std::make_pair(other.type, other.index);
    }

    bool operator==(const Register& other) const { return type == other.type && index == other.index; }
};

struct Instruction
{
    DWORD token;
    unsigned opcode;
    std::vector<DWORD> parameters;
    std::vector<size_t> registers;
};

struct Program
{
    bool vertex = false;
    unsigned major = 0;
    std::vector<Instruction> instructions;
    std::set<Register> used;
    std::map<Register, DWORD> declarations;
    std::map<Register, DWORD> declarationMasks;
    bool relativeConstants = false;
};

Register DecodeRegister(DWORD token)
{
    return {((token & D3DSP_REGTYPE_MASK) >> D3DSP_REGTYPE_SHIFT) |
                ((token & D3DSP_REGTYPE_MASK2) >> D3DSP_REGTYPE_SHIFT2),
            token & D3DSP_REGNUM_MASK};
}

DWORD EncodeRegister(Register reg)
{
    return ((reg.type << D3DSP_REGTYPE_SHIFT) & D3DSP_REGTYPE_MASK) |
           ((reg.type << D3DSP_REGTYPE_SHIFT2) & D3DSP_REGTYPE_MASK2) | reg.index;
}

DWORD RelocateRegister(DWORD token, Register reg)
{
    return (token & ~(D3DSP_REGTYPE_MASK | D3DSP_REGTYPE_MASK2 | D3DSP_REGNUM_MASK)) | EncodeRegister(reg);
}

bool Fail(std::string& failure, const char* reason)
{
    failure = reason;
    return false;
}

bool IsDefinition(unsigned opcode)
{
    return opcode == D3DSIO_DEF || opcode == D3DSIO_DEFI || opcode == D3DSIO_DEFB;
}

bool ParseProgram(const DWORD* code, size_t words, bool vertex, Program& program, std::string& failure)
{
    if (!code || words < 2 || words > kMaximumShaderWords ||
        (code[0] & kShaderVersionMask) != (vertex ? kVertexShaderVersion : kPixelShaderVersion))
        return Fail(failure, "invalid material shader bytecode");
    program.vertex = vertex;
    program.major = D3DSHADER_VERSION_MAJOR(code[0]);
    if ((program.major != 2 && program.major != 3) || D3DSHADER_VERSION_MINOR(code[0]) != 0)
        return Fail(failure, "material shader model is not 2.0 or 3.0");
    size_t position = 1;
    std::vector<unsigned> flow;
    bool ended = false;
    while (position < words)
    {
        const DWORD token = code[position++];
        const unsigned opcode = token & D3DSI_OPCODE_MASK;
        if (opcode == D3DSIO_END)
        {
            ended = position == words && flow.empty();
            break;
        }
        const size_t count = opcode == D3DSIO_COMMENT
                                 ? (token & D3DSI_COMMENTSIZE_MASK) >> D3DSI_COMMENTSIZE_SHIFT
                                 : (token & D3DSI_INSTLENGTH_MASK) >> D3DSI_INSTLENGTH_SHIFT;
        if (count > words - position)
            return Fail(failure, "truncated material shader instruction");
        if (opcode == D3DSIO_COMMENT)
        {
            position += count;
            continue;
        }
        if (opcode == D3DSIO_CALL || opcode == D3DSIO_CALLNZ || opcode == D3DSIO_LABEL || opcode == D3DSIO_RET ||
            opcode == D3DSIO_PHASE)
            return Fail(failure, "material shader subroutines are unsupported");
        if (opcode > D3DSIO_BREAKP)
            return Fail(failure, "unknown material shader instruction");
        if (opcode == D3DSIO_IF || opcode == D3DSIO_IFC || opcode == D3DSIO_LOOP || opcode == D3DSIO_REP)
            flow.push_back(opcode);
        if (opcode == D3DSIO_ELSE && (flow.empty() || (flow.back() != D3DSIO_IF && flow.back() != D3DSIO_IFC)))
            return Fail(failure, "invalid material shader conditional");
        if (opcode == D3DSIO_ENDIF || opcode == D3DSIO_ENDLOOP || opcode == D3DSIO_ENDREP)
        {
            if (flow.empty() ||
                (opcode == D3DSIO_ENDIF && flow.back() != D3DSIO_IF && flow.back() != D3DSIO_IFC) ||
                (opcode == D3DSIO_ENDLOOP && flow.back() != D3DSIO_LOOP) ||
                (opcode == D3DSIO_ENDREP && flow.back() != D3DSIO_REP))
                return Fail(failure, "invalid material shader control flow");
            flow.pop_back();
        }
        Instruction instruction = {token, opcode, {code + position, code + position + count}, {}};
        if (opcode == D3DSIO_DCL)
        {
            if (count != 2)
                return Fail(failure, "invalid material shader declaration");
            if (program.declarations.count(DecodeRegister(instruction.parameters[1])) != 0)
                return Fail(failure, "prepacked material input or output declarations are unsupported");
            instruction.registers.push_back(1);
            program.declarations[DecodeRegister(instruction.parameters[1])] = instruction.parameters[0];
            program.declarationMasks[DecodeRegister(instruction.parameters[1])] =
                instruction.parameters[1] & (D3DSP_WRITEMASK_ALL | D3DSPDM_MSAMPCENTROID);
        }
        else if (IsDefinition(opcode))
        {
            if (count != (opcode == D3DSIO_DEFB ? 2u : 5u))
                return Fail(failure, "invalid material shader constant definition");
            instruction.registers.push_back(0);
        }
        else
        {
            for (size_t operand = 0; operand < count; ++operand)
            {
                if (!(instruction.parameters[operand] & kParameterToken))
                    return Fail(failure, "invalid material shader register token");
                instruction.registers.push_back(operand);
            }
        }
        for (size_t operand : instruction.registers)
        {
            const DWORD parameter = instruction.parameters[operand];
            const Register reg = DecodeRegister(parameter);
            program.used.insert(reg);
            if ((parameter & D3DSHADER_ADDRMODE_RELATIVE) && reg.type == D3DSPR_CONST)
                program.relativeConstants = true;
            if ((parameter & D3DSHADER_ADDRMODE_RELATIVE) &&
                (reg.type == D3DSPR_OUTPUT || reg.type == D3DSPR_INPUT))
                return Fail(failure, "relative material shader inputs or outputs are unsupported");
        }
        program.instructions.push_back(std::move(instruction));
        position += count;
    }
    return ended ? true : Fail(failure, "material shader has no complete final instruction");
}

unsigned Usage(DWORD declaration) { return declaration & D3DSP_DCL_USAGE_MASK; }

unsigned UsageIndex(DWORD declaration)
{
    return (declaration & D3DSP_DCL_USAGEINDEX_MASK) >> D3DSP_DCL_USAGEINDEX_SHIFT;
}

DWORD Semantic(unsigned usage, unsigned index)
{
    return kParameterToken | usage | (index << D3DSP_DCL_USAGEINDEX_SHIFT);
}

void Declare(std::vector<DWORD>& code, Register reg, unsigned usage, unsigned index, DWORD mask = kFullWriteMask)
{
    code.push_back(D3DSIO_DCL | (2u << D3DSI_INSTLENGTH_SHIFT));
    code.push_back(Semantic(usage, index));
    code.push_back(kParameterToken | EncodeRegister(reg) | mask);
}

void Move(std::vector<DWORD>& code, Register destination, Register source, bool saturate = false,
          DWORD mask = kFullWriteMask, DWORD swizzle = kIdentitySwizzle)
{
    code.push_back(D3DSIO_MOV | (2u << D3DSI_INSTLENGTH_SHIFT));
    code.push_back(kParameterToken | EncodeRegister(destination) | mask |
                   (saturate ? D3DSPDM_SATURATE : 0));
    code.push_back(kParameterToken | EncodeRegister(source) | swizzle);
}

int Allocate(std::set<Register>& used, unsigned type, unsigned count)
{
    for (unsigned index = count; index-- > 0;)
        if (used.insert({type, index}).second)
            return static_cast<int>(index);
    return -1;
}

int AllocateConstants(const Program& pixel, unsigned count)
{
    if (pixel.relativeConstants || count > kPixelConstants)
        return -1;
    for (unsigned base = kPixelConstants - count + 1; base-- > 0;)
    {
        bool available = true;
        for (unsigned index = 0; index < count; ++index)
            available = available && pixel.used.count({D3DSPR_CONST, base + index}) == 0;
        if (available)
            return static_cast<int>(base);
    }
    return -1;
}

bool HasClipSemantic(const Program& program)
{
    if (program.major == 2)
        return program.used.count({static_cast<unsigned>(program.vertex ? D3DSPR_TEXCRDOUT : D3DSPR_TEXTURE),
                                    kFogMaterialClipTexcoord}) != 0;
    for (const auto& declaration : program.declarations)
        if (declaration.first.type == static_cast<unsigned>(program.vertex ? D3DSPR_OUTPUT : D3DSPR_INPUT) &&
            Usage(declaration.second) == D3DDECLUSAGE_TEXCOORD &&
            UsageIndex(declaration.second) == kFogMaterialClipTexcoord)
            return true;
    return false;
}

bool MapVertexOutputs(const Program& vertex, std::map<Register, Register>& mapping,
                      std::map<Register, DWORD>& declarations, Register& position, std::string& failure)
{
    bool foundPosition = false;
    if (vertex.major == 3)
    {
        declarations = vertex.declarations;
        for (const auto& declaration : declarations)
            if (declaration.first.type == D3DSPR_OUTPUT && Usage(declaration.second) == D3DDECLUSAGE_POSITION)
            {
                if (foundPosition || UsageIndex(declaration.second) != 0)
                    return Fail(failure, "material vertex position declaration is ambiguous");
                position = declaration.first;
                foundPosition = true;
            }
        return foundPosition ? true : Fail(failure, "material vertex shader does not declare position");
    }
    unsigned nextOutput = 0;
    for (Register reg : vertex.used)
    {
        unsigned usage;
        unsigned index = reg.index;
        if (reg.type == D3DSPR_RASTOUT)
        {
            if (index > 2)
                return Fail(failure, "unknown material raster output");
            const unsigned usages[] = {D3DDECLUSAGE_POSITION, D3DDECLUSAGE_FOG, D3DDECLUSAGE_PSIZE};
            usage = usages[index];
            index = 0;
        }
        else if (reg.type == D3DSPR_ATTROUT)
            usage = D3DDECLUSAGE_COLOR;
        else if (reg.type == D3DSPR_TEXCRDOUT)
            usage = D3DDECLUSAGE_TEXCOORD;
        else
            continue;
        if (nextOutput >= kVertexOutputs)
            return Fail(failure, "material vertex shader has too many outputs");
        const Register replacement = {D3DSPR_OUTPUT, nextOutput++};
        mapping[reg] = replacement;
        declarations[replacement] = Semantic(usage, index);
        if (usage == D3DDECLUSAGE_POSITION)
        {
            position = replacement;
            foundPosition = true;
        }
    }
    return foundPosition ? true : Fail(failure, "material vertex shader does not write position");
}

bool MapPixelInputs(const Program& pixel, std::map<Register, Register>& mapping,
                    std::map<Register, DWORD>& declarations, std::string& failure)
{
    if (pixel.major == 3)
    {
        declarations = pixel.declarations;
        return true;
    }
    unsigned nextInput = 0;
    for (Register reg : pixel.used)
    {
        if (reg.type != D3DSPR_INPUT && reg.type != D3DSPR_TEXTURE)
            continue;
        if (nextInput >= kPixelInputs)
            return Fail(failure, "material pixel shader has too many inputs");
        const Register replacement = {D3DSPR_INPUT, nextInput++};
        mapping[reg] = replacement;
        declarations[replacement] = Semantic(reg.type == D3DSPR_INPUT ? D3DDECLUSAGE_COLOR : D3DDECLUSAGE_TEXCOORD,
                                               reg.index);
    }
    return true;
}

void CopyInstruction(const Instruction& instruction, const std::map<Register, Register>& mapping,
                      std::vector<DWORD>& output, bool upgrade, int packedClipInput = -1)
{
    DWORD token = instruction.token;
    size_t count = instruction.parameters.size();
    if (upgrade && (instruction.opcode == D3DSIO_SINCOS || instruction.opcode == D3DSIO_SGN) && count == 4)
    {
        count = 2;
        token = (token & ~D3DSI_INSTLENGTH_MASK) | (2u << D3DSI_INSTLENGTH_SHIFT);
    }
    output.push_back(token);
    const size_t first = output.size();
    output.insert(output.end(), instruction.parameters.begin(), instruction.parameters.begin() + count);
    for (size_t operand : instruction.registers)
    {
        if (operand >= count)
            continue;
        const auto replacement = mapping.find(DecodeRegister(output[first + operand]));
        if (replacement != mapping.end())
        {
            const Register original = DecodeRegister(output[first + operand]);
            output[first + operand] = RelocateRegister(output[first + operand], replacement->second);
            if (upgrade && original.type == D3DSPR_RASTOUT && original.index == 1)
                output[first + operand] = (output[first + operand] & ~D3DSP_WRITEMASK_ALL) | D3DSP_WRITEMASK_0;
            if (original.type == D3DSPR_INPUT && static_cast<int>(original.index) == packedClipInput)
            {
                const unsigned components[] = {1, 2, 0, 3};
                DWORD swizzle = 0;
                for (unsigned channel = 0; channel < 4; ++channel)
                {
                    const unsigned shift = 16 + channel * 2;
                    swizzle |= components[(output[first + operand] >> shift) & 3u] << shift;
                }
                output[first + operand] = (output[first + operand] & ~D3DSP_SWIZZLE_MASK) | swizzle;
            }
        }
    }
}

void CopyDefinitions(const Program& program, const std::map<Register, Register>& mapping,
                     std::vector<DWORD>& code)
{
    for (const Instruction& instruction : program.instructions)
        if (IsDefinition(instruction.opcode))
            CopyInstruction(instruction, mapping, code, false);
}

void CopyDeclarations(const Program& program, const std::map<Register, Register>& mapping,
                      std::vector<DWORD>& code, unsigned excludedType)
{
    for (const Instruction& instruction : program.instructions)
        if (instruction.opcode == D3DSIO_DCL &&
            DecodeRegister(instruction.parameters[1]).type != excludedType)
            CopyInstruction(instruction, mapping, code, false);
}

unsigned InterpolatorCount(const Program& program)
{
    unsigned count = 0;
    if (program.major == 2)
    {
        for (Register reg : program.used)
            if (program.vertex ? (reg.type == D3DSPR_TEXCRDOUT || reg.type == D3DSPR_ATTROUT ||
                                  (reg.type == D3DSPR_RASTOUT && reg.index == 1))
                               : (reg.type == D3DSPR_INPUT || reg.type == D3DSPR_TEXTURE))
                ++count;
        return count;
    }
    for (const auto& declaration : program.declarations)
        if (declaration.first.type == static_cast<unsigned>(program.vertex ? D3DSPR_OUTPUT : D3DSPR_INPUT) &&
            Usage(declaration.second) != D3DDECLUSAGE_POSITION && Usage(declaration.second) != D3DDECLUSAGE_PSIZE)
            ++count;
    return count;
}

int AvailableFogComponents(const Program& program, const std::map<Register, DWORD>& declarations)
{
    for (const auto& declaration : declarations)
        if (declaration.first.type == static_cast<unsigned>(program.vertex ? D3DSPR_OUTPUT : D3DSPR_INPUT) &&
            Usage(declaration.second) == D3DDECLUSAGE_FOG)
        {
            const auto mask = program.declarationMasks.find(declaration.first);
            if (program.major == 2 ||
                (mask != program.declarationMasks.end() && !(mask->second & kPackedClipMask)))
                return static_cast<int>(declaration.first.index);
        }
    return -1;
}

bool BuildVertex(const Program& vertex, bool packedClip, FogMaterialShaders& result, std::string& failure)
{
    std::map<Register, Register> mapping;
    std::map<Register, DWORD> declarations;
    Register position = {};
    if (!MapVertexOutputs(vertex, mapping, declarations, position, failure))
        return false;
    auto used = vertex.used;
    if (vertex.major == 2)
    {
        for (auto item = used.begin(); item != used.end();)
            item = item->type == D3DSPR_TEXCRDOUT ? used.erase(item) : std::next(item);
        for (const auto& declaration : declarations)
            used.insert(declaration.first);
    }
    const int clipOutput = packedClip ? AvailableFogComponents(vertex, declarations)
                                     : Allocate(used, D3DSPR_OUTPUT, kVertexOutputs);
    const int positionTemporary = Allocate(used, D3DSPR_TEMP, kShaderTemporaries);
    if (clipOutput < 0 || positionTemporary < 0)
        return Fail(failure, "material vertex shader has no spare output or temporary");
    const Register temporary = {D3DSPR_TEMP, static_cast<unsigned>(positionTemporary)};
    auto& code = result.vertex;
    code.push_back(D3DVS_VERSION(3, 0));
    CopyDefinitions(vertex, {}, code);
    CopyDeclarations(vertex, {}, code, D3DSPR_OUTPUT);
    for (const auto& declaration : declarations)
        if (declaration.first.type == D3DSPR_OUTPUT)
        {
            DWORD mask = kFullWriteMask;
            const unsigned usage = Usage(declaration.second);
            const auto existingMask = vertex.declarationMasks.find(declaration.first);
            if (vertex.major == 3 && existingMask != vertex.declarationMasks.end())
                mask = existingMask->second;
            else if (usage == D3DDECLUSAGE_FOG)
                mask = D3DSP_WRITEMASK_0;
            Declare(code, declaration.first, usage, UsageIndex(declaration.second), mask);
        }
    Declare(code, {D3DSPR_OUTPUT, static_cast<unsigned>(clipOutput)}, D3DDECLUSAGE_TEXCOORD,
            kFogMaterialClipTexcoord, packedClip ? kPackedClipMask : kFullWriteMask);
    if (vertex.major == 2)
    {
        for (auto& replacement : mapping)
            if (replacement.second == position)
                replacement.second = temporary;
    }
    else
        mapping[position] = temporary;
    for (const Instruction& instruction : vertex.instructions)
        if (instruction.opcode != D3DSIO_DCL && !IsDefinition(instruction.opcode))
            CopyInstruction(instruction, mapping, code, vertex.major == 2);
    Move(code, position, temporary);
    Move(code, {D3DSPR_OUTPUT, static_cast<unsigned>(clipOutput)}, temporary, false,
          packedClip ? kPackedClipMask : kFullWriteMask,
          packedClip ? kPackedClipSourceSwizzle : kIdentitySwizzle);
    code.push_back(D3DSIO_END);
    return true;
}

bool BuildPixel(const Program& pixel, const Program& footer, bool packedClip, FogMaterialShaders& result,
                 std::string& failure)
{
    std::map<Register, Register> mapping;
    std::map<Register, DWORD> declarations;
    if (!MapPixelInputs(pixel, mapping, declarations, failure))
        return false;
    auto used = pixel.used;
    if (pixel.major == 2)
    {
        for (auto item = used.begin(); item != used.end();)
            item = item->type == D3DSPR_INPUT ? used.erase(item) : std::next(item);
        for (const auto& declaration : declarations)
            used.insert(declaration.first);
    }
    int clipInput = Allocate(used, D3DSPR_INPUT, kPixelInputs);
    if (clipInput < 0 && packedClip)
        clipInput = AvailableFogComponents(pixel, declarations);
    const int sourceTemporary = Allocate(used, D3DSPR_TEMP, kShaderTemporaries);
    const int sampler = Allocate(used, D3DSPR_SAMPLER, kPixelSamplers);
    unsigned constantCount = kFogMaterialConstantCount;
    for (Register reg : footer.used)
        if (reg.type == D3DSPR_CONST)
            constantCount = std::max(constantCount, reg.index + 1);
    const int constantBase = AllocateConstants(pixel, constantCount);
    if (clipInput < 0 || sourceTemporary < 0 || sampler < 0 || constantBase < 0)
        return Fail(failure, "material pixel shader has no safely available fog resources");
    const Register source = {D3DSPR_TEMP, static_cast<unsigned>(sourceTemporary)};
    mapping[{D3DSPR_COLOROUT, 0}] = source;
    std::map<Register, Register> footerMapping;
    int footerClipInput = -1;
    for (Register reg : footer.used)
    {
        if (reg.type == D3DSPR_TEMP)
        {
            const int temporary = Allocate(used, D3DSPR_TEMP, kShaderTemporaries);
            if (temporary < 0)
                return Fail(failure, "material pixel shader has no spare fog temporaries");
            footerMapping[reg] = {D3DSPR_TEMP, static_cast<unsigned>(temporary)};
        }
        else if (reg.type == D3DSPR_CONST)
            footerMapping[reg] = {D3DSPR_CONST, static_cast<unsigned>(constantBase) + reg.index};
        else if (reg.type == D3DSPR_SAMPLER)
            footerMapping[reg] = {D3DSPR_SAMPLER, static_cast<unsigned>(sampler)};
        else if (reg.type == D3DSPR_INPUT)
        {
            const auto semantic = footer.declarations.find(reg);
            if (semantic == footer.declarations.end())
                return Fail(failure, "fog footer has an undeclared input");
            footerMapping[reg] = Usage(semantic->second) == D3DDECLUSAGE_COLOR
                                     ? source
                                     : Register{D3DSPR_INPUT, static_cast<unsigned>(clipInput)};
            if (Usage(semantic->second) == D3DDECLUSAGE_TEXCOORD)
                footerClipInput = static_cast<int>(reg.index);
        }
    }
    auto& code = result.pixel;
    code.push_back(D3DPS_VERSION(3, 0));
    CopyDefinitions(pixel, {}, code);
    CopyDefinitions(footer, footerMapping, code);
    if (pixel.major == 3)
        CopyDeclarations(pixel, {}, code, D3DSPR_INPUT);
    else
        for (const Instruction& instruction : pixel.instructions)
            if (instruction.opcode == D3DSIO_DCL &&
                DecodeRegister(instruction.parameters[1]).type == D3DSPR_SAMPLER)
                CopyInstruction(instruction, {}, code, false);
    for (const auto& declaration : declarations)
        if (declaration.first.type == D3DSPR_INPUT)
        {
            Register original = declaration.first;
            if (pixel.major == 2)
                for (const auto& replacement : mapping)
                    if (replacement.second == declaration.first)
                        original = replacement.first;
            const auto existingMask = pixel.declarationMasks.find(original);
            const DWORD mask = existingMask == pixel.declarationMasks.end() ? kFullWriteMask : existingMask->second;
            Declare(code, declaration.first, Usage(declaration.second), UsageIndex(declaration.second), mask);
        }
    Declare(code, {D3DSPR_INPUT, static_cast<unsigned>(clipInput)}, D3DDECLUSAGE_TEXCOORD,
            kFogMaterialClipTexcoord, packedClip ? kPackedClipMask : kFullWriteMask);
    for (const Instruction& instruction : footer.instructions)
        if (instruction.opcode == D3DSIO_DCL &&
            DecodeRegister(instruction.parameters[1]).type == D3DSPR_SAMPLER)
            CopyInstruction(instruction, footerMapping, code, false);
    if (pixel.major == 2)
        for (auto& replacement : mapping)
            if (replacement.first.type == D3DSPR_INPUT)
            {
                const int temporary = Allocate(used, D3DSPR_TEMP, kShaderTemporaries);
                if (temporary < 0)
                    return Fail(failure, "material pixel shader has no spare colour temporary");
                const Register clamped = {D3DSPR_TEMP, static_cast<unsigned>(temporary)};
                const auto declarationMask = pixel.declarationMasks.find(replacement.first);
                const DWORD mask = declarationMask == pixel.declarationMasks.end()
                                       ? kFullWriteMask : declarationMask->second & kFullWriteMask;
                Move(code, clamped, replacement.second, true, mask);
                replacement.second = clamped;
            }
    for (const Instruction& instruction : pixel.instructions)
        if (instruction.opcode != D3DSIO_DCL && !IsDefinition(instruction.opcode))
            CopyInstruction(instruction, mapping, code, pixel.major == 2);
    for (const Instruction& instruction : footer.instructions)
        if (instruction.opcode != D3DSIO_DCL && !IsDefinition(instruction.opcode))
            CopyInstruction(instruction, footerMapping, code, false, packedClip ? footerClipInput : -1);
    code.push_back(D3DSIO_END);
    result.resources.sampler = static_cast<UINT>(sampler);
    result.resources.constantBase = static_cast<UINT>(constantBase);
    return true;
}
}

bool InstrumentFogMaterialShaders(const DWORD* vertex, size_t vertexWords, const DWORD* pixel, size_t pixelWords,
                                  FogMaterialShaders& result, std::string& failure)
{
    result = {};
    failure.clear();
    Program vertexProgram;
    Program pixelProgram;
    Program footer;
    if (!ParseProgram(vertex, vertexWords, true, vertexProgram, failure) ||
        !ParseProgram(pixel, pixelWords, false, pixelProgram, failure) ||
        !ParseProgram(reinterpret_cast<const DWORD*>(g_ps_material_footer), sizeof(g_ps_material_footer) / 4,
                       false, footer, failure))
        return false;
    if (HasClipSemantic(vertexProgram) || HasClipSemantic(pixelProgram))
        return Fail(failure, "material TEXCOORD7 is already occupied");
    if (!pixelProgram.used.count({D3DSPR_COLOROUT, 0}))
        return Fail(failure, "material pixel shader does not write colour");
    const bool packedClip = InterpolatorCount(vertexProgram) >= kPixelInputs ||
                            InterpolatorCount(pixelProgram) >= kPixelInputs;
    if (!BuildVertex(vertexProgram, packedClip, result, failure) ||
        !BuildPixel(pixelProgram, footer, packedClip, result, failure))
    {
        result = {};
        return false;
    }
    return true;
}
