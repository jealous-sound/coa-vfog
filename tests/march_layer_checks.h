#pragma once

namespace march_layers
{
constexpr UINT kSize = 8;
constexpr int kConstantRegisters = 99;
constexpr int kFirstLayerRegister = 12;
constexpr int kRegistersPerLayer = 6;
constexpr float kDepthAtInfiniteViewZ = 0.940376f;
constexpr float kDepthPerInverseViewZ = -0.3761504f;
constexpr float kDeepestWorldDepth = 0.94f;
constexpr float kSkyDepth = 1.0f;
constexpr float kMaxFogDistance = 5000.0f;
constexpr float kHorizonBlendStart = 700.0f;
constexpr float kFarClip = 800.0f;
constexpr float kLastLayerLimit = 900.0f;
constexpr float kMaxDifference = 1.0e-4f;
constexpr float kPartialOpacity[2] = {0.02f, 0.9f};
constexpr int kVariants = 3;
constexpr int kMinPartiallyFogged = kVariants * kSize * kSize / 2;

struct Layer
{
    float start;
    float density;
    float g;
    float isotropic;
    float emissive[3];
    float strength;
    float diffuse[3];
    float exponent;
    float upperHeight;
    float upperFalloff;
    float lowerHeight;
    float lowerFalloff;
    float shadowEmissive[3];
    float shadowDensity;
    float shadowed;
    float skyFalloff;
    float limit;
    float densityVariation;
};

static_assert(sizeof(Layer) == kRegistersPerLayer * 4 * sizeof(float), "a march layer is six float4 registers");

constexpr Layer kLayers[4] = {
    {0, 0.001f, 0.6f, 0.3f, {0.2f, 0.3f, 0.4f}, 0, {0.8f, 0.7f, 0.6f}, 1, 60, 0.01f, 0, 0,
     {0.2f, 0.3f, 0.4f}, 1, 0, 0, 300, 1},
    {23, 0.0005f, 0.3f, 0.5f, {0.3f, 0.3f, 0.3f}, 3, {0.5f, 0.5f, 0.6f}, 1.5f, 200, 0.004f, -40, 0.02f,
     {0.1f, 0.1f, 0.2f}, 0.5f, 1, 2, 700, 1},
    {0, 0, 0.5f, 0.5f, {5, 5, 5}, 0, {5, 5, 5}, 1, 0, 0, 0, 0, {5, 5, 5}, 1, 0, 0, kMaxFogDistance, 1},
    {150, 0.0003f, 0.2f, 0.7f, {0.2f, 0.25f, 0.3f}, 2, {0.4f, 0.35f, 0.3f}, 2, 0, 0, 0, 0,
     {0.2f, 0.25f, 0.3f}, 1, 0, 10, kLastLayerLimit, 0},
};

struct Resources
{
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* readback = nullptr;
    IDirect3DTexture9* depth = nullptr;
    IDirect3DVolumeTexture9* noise = nullptr;
    IDirect3DSurface9* previousTarget = nullptr;
    IDirect3DSurface9* previousDepth = nullptr;
    IDirect3DStateBlock9* previousState = nullptr;

    ~Resources()
    {
        for (IUnknown* resource : std::initializer_list<IUnknown*>{target, readback, depth, noise, previousTarget,
                                                                   previousDepth, previousState})
            if (resource)
                resource->Release();
    }
};

float ViewDistanceAt(UINT x, UINT y)
{
    if (y < 2)
        return 0.0f;
    if (y == 2)
        return 740.0f + 15.0f * x;
    return 6.0f * std::pow(1.9f, static_cast<float>(x + y - 3) * 0.75f);
}

bool FillDepth(IDirect3DTexture9* depth)
{
    D3DLOCKED_RECT locked = {};
    if (FAILED(depth->LockRect(0, &locked, nullptr, 0)))
        return false;
    for (UINT y = 0; y < kSize; ++y)
    {
        auto* row = reinterpret_cast<float*>(static_cast<BYTE*>(locked.pBits) + y * locked.Pitch);
        for (UINT x = 0; x < kSize; ++x)
        {
            const float viewZ = ViewDistanceAt(x, y);
            row[x] = viewZ > 0.0f ? kDepthAtInfiniteViewZ + kDepthPerInverseViewZ / viewZ : kSkyDepth;
        }
    }
    return SUCCEEDED(depth->UnlockRect(0));
}

bool CreateResources(IDirect3DDevice9* device, Resources& resources)
{
    return SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &resources.previousState)) &&
           SUCCEEDED(device->GetRenderTarget(0, &resources.previousTarget)) &&
           SUCCEEDED(device->GetDepthStencilSurface(&resources.previousDepth)) &&
           SUCCEEDED(device->CreateRenderTarget(kSize, kSize, D3DFMT_A32B32G32R32F, D3DMULTISAMPLE_NONE, 0, FALSE,
                                                &resources.target, nullptr)) &&
           SUCCEEDED(device->CreateOffscreenPlainSurface(kSize, kSize, D3DFMT_A32B32G32R32F, D3DPOOL_SYSTEMMEM,
                                                         &resources.readback, nullptr)) &&
           SUCCEEDED(device->CreateTexture(kSize, kSize, 1, 0, D3DFMT_R32F, D3DPOOL_MANAGED, &resources.depth,
                                           nullptr)) &&
           FillDepth(resources.depth) && CreateDensityNoise(device, &resources.noise);
}

void BindResources(IDirect3DDevice9* device, const Resources& resources)
{
    const D3DVIEWPORT9 viewport = {0, 0, kSize, kSize, 0.0f, 1.0f};
    device->SetDepthStencilSurface(nullptr);
    device->SetRenderTarget(0, resources.target);
    device->SetViewport(&viewport);
    device->SetVertexShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW);
    device->SetTexture(0, resources.depth);
    device->SetTexture(8, nullptr);
    device->SetTexture(9, resources.noise);
    for (DWORD stage : {0u, 9u})
    {
        const D3DTEXTUREFILTERTYPE filter = stage == 0 ? D3DTEXF_POINT : D3DTEXF_LINEAR;
        const D3DTEXTUREADDRESS address = stage == 0 ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP;
        device->SetSamplerState(stage, D3DSAMP_MINFILTER, filter);
        device->SetSamplerState(stage, D3DSAMP_MAGFILTER, filter);
        device->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
        device->SetSamplerState(stage, D3DSAMP_ADDRESSU, address);
        device->SetSamplerState(stage, D3DSAMP_ADDRESSV, address);
        device->SetSamplerState(stage, D3DSAMP_ADDRESSW, address);
    }
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
}

void FillConstants(float constants[kConstantRegisters][4], bool jitter, float frame)
{
    std::memset(constants, 0, sizeof(float) * kConstantRegisters * 4);
    constants[0][2] = constants[0][3] = static_cast<float>(kSize);
    constants[1][0] = 1.0f;
    constants[1][1] = frame;
    constants[1][2] = constants[1][3] = 1.0f / kSize;
    constants[2][0] = constants[2][1] = 1.0f;
    constants[3][0] = kDepthAtInfiniteViewZ;
    constants[3][1] = kDepthPerInverseViewZ;
    constants[3][2] = kMaxFogDistance;
    constants[3][3] = kDeepestWorldDepth;
    const float viewToWorld[4][4] = {{0, -1, 0, 0}, {0, 0, 1, 0}, {1, 0, 0, 0}, {100, -50, 20, 1}};
    std::memcpy(&constants[4][0], viewToWorld, sizeof(viewToWorld));
    constants[8][0] = constants[8][1] = static_cast<float>(kSize);
    constants[8][2] = constants[8][3] = 1.0f / kSize;
    const float light[4] = {0.3f, 0.5f, 0.81f, 0.4f};
    std::memcpy(constants[9], light, sizeof(light));
    const float march[4] = {jitter ? 1.0f : 0.0f, 1000.0f, kHorizonBlendStart, kFarClip};
    std::memcpy(constants[11], march, sizeof(march));
    std::memcpy(constants[kFirstLayerRegister], kLayers, sizeof(kLayers));
    const float variation[4] = {0.6f, 0.2f, 3.5f, -2.0f};
    std::memcpy(constants[78], variation, sizeof(variation));
}

bool RenderMarch(IDirect3DDevice9* device, const Resources& resources, IDirect3DPixelShader9* shader,
                 const float constants[kConstantRegisters][4], float pixels[kSize * kSize * 4])
{
    const float quad[4][4] = {{-0.5f, -0.5f, 0, 1}, {kSize - 0.5f, -0.5f, 0, 1},
                             {-0.5f, kSize - 0.5f, 0, 1}, {kSize - 0.5f, kSize - 0.5f, 0, 1}};
    device->SetPixelShader(shader);
    device->SetPixelShaderConstantF(0, &constants[0][0], kConstantRegisters);
    if (FAILED(device->BeginScene()))
        return false;
    const HRESULT draw = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
    device->EndScene();
    D3DLOCKED_RECT locked = {};
    if (FAILED(draw) || FAILED(device->GetRenderTargetData(resources.target, resources.readback)) ||
        FAILED(resources.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return false;
    for (UINT y = 0; y < kSize; ++y)
        std::memcpy(pixels + y * kSize * 4, static_cast<const BYTE*>(locked.pBits) + y * locked.Pitch,
                    kSize * 4 * sizeof(float));
    return SUCCEEDED(resources.readback->UnlockRect());
}

void CheckUnrolledMarchMatchesLoopedMarch(IDirect3DDevice9* device)
{
    Resources resources;
    const bool ready = CreateResources(device, resources);
    Check(ready, "march layer comparison targets, depth and density noise created");
    if (!ready)
        return;
    BindResources(device, resources);
    const BYTE* unrolled[3] = {g_ps_march_low, g_ps_march_mid, g_ps_march_high};
    const BYTE* looped[3] = {g_ps_lit_march_low, g_ps_lit_march_mid, g_ps_lit_march_high};
    for (int quality = 0; quality < 3; ++quality)
    {
        IDirect3DPixelShader9* shaders[2] = {};
        const bool created =
            SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(unrolled[quality]), &shaders[0])) &&
            SUCCEEDED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(looped[quality]), &shaders[1]));
        float worst = 0.0f;
        int partial = 0;
        bool rendered = created;
        for (int variant = 0; variant < kVariants && rendered; ++variant)
        {
            float constants[kConstantRegisters][4];
            FillConstants(constants, variant > 0, variant == 2 ? 17.0f : 0.0f);
            float pixels[2][kSize * kSize * 4];
            rendered = RenderMarch(device, resources, shaders[0], constants, pixels[0]) &&
                       RenderMarch(device, resources, shaders[1], constants, pixels[1]);
            for (UINT i = 0; i < kSize * kSize * 4 && rendered; ++i)
                worst = std::max(worst, std::fabs(pixels[0][i] - pixels[1][i]) / std::max(1.0f, pixels[1][i]));
            for (UINT i = 0; i < kSize * kSize && rendered; ++i)
                partial += pixels[1][i * 4 + 3] > kPartialOpacity[0] && pixels[1][i * 4 + 3] < kPartialOpacity[1];
        }
        for (IDirect3DPixelShader9* shader : shaders)
            if (shader)
                shader->Release();
        char label[224];
        std::snprintf(label, sizeof(label),
                      "quality %d: the unrolled march (shared step noise, stop past the last layer) matches the "
                      "per-layer loop with clipped, empty and shadowed layers, sky and horizon rays (%.2g, %d "
                      "partly fogged)",
                      quality + 1, worst, partial);
        Check(rendered && worst <= kMaxDifference && partial >= kMinPartiallyFogged, label);
    }
    device->SetRenderTarget(0, resources.previousTarget);
    device->SetDepthStencilSurface(resources.previousDepth);
    resources.previousState->Apply();
}
}
