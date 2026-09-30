#pragma once

namespace water_mask_packing_checks
{
WaterMaskLevels PatternMask(uint32_t size, uint32_t seed)
{
    WaterMaskLevels mask = {};
    mask.info.size = size;
    for (uint32_t side = size; side >= 1; side /= 2)
    {
        std::vector<uint8_t> level(static_cast<size_t>(side) * side);
        for (size_t i = 0; i < level.size(); ++i)
            level[i] = static_cast<uint8_t>((i * 37 + seed * 101 + side) & 0xFF);
        mask.levels.push_back(level);
        if (side == 1)
            break;
    }
    mask.info.mipCount = static_cast<uint32_t>(mask.levels.size());
    return mask;
}

void CheckWaveFoamPacking()
{
    const std::vector<WaterMaskLevels> masks = {PatternMask(8, 1), PatternMask(8, 2), PatternMask(8, 3),
                                                PatternMask(16, 4)};
    const WaterPackedMask all = PackWaveFoamMasks(masks, {0, 1, 2});
    bool exact = all.size == 8 && all.levels.size() == masks[0].levels.size();
    for (size_t level = 0; exact && level < all.levels.size(); ++level)
        for (size_t i = 0; exact && i < all.levels[level].size(); ++i)
            exact = all.levels[level][i] == (kPackedMaskOpaque | masks[0].levels[level][i] << 16 |
                                             masks[1].levels[level][i] << 8 | masks[2].levels[level][i]);
    Check(exact && all.present[0] && all.present[1] && all.present[2],
          "the packed wave-foam texture holds the high, mid and low foam masks unchanged in red, green and blue at "
          "every mip level");
    const WaterPackedMask gap = PackWaveFoamMasks(masks, {0, kWaterNoIndex, 2});
    const WaterPackedMask mismatched = PackWaveFoamMasks(masks, {0, 3, 2});
    bool emptyGreen = !gap.levels.empty();
    for (uint32_t texel : gap.levels.empty() ? std::vector<uint32_t>() : gap.levels[0])
        emptyGreen = emptyGreen && ((texel >> 8) & 0xFF) == 0;
    Check(emptyGreen && !gap.present[1] && gap.present[2] && !mismatched.present[1] && mismatched.present[2] &&
              PackWaveFoamMasks(masks, {kWaterNoIndex, kWaterNoIndex, kWaterNoIndex}).size == 0,
          "a missing wave-foam mask, or one whose size differs from the first, leaves its channel empty and marks it "
          "absent");
}

struct TexturedScreenVertex
{
    float x, y, z, rhw;
    float u, v;
};

constexpr UINT kFilterProbeWidth = 256;
constexpr UINT kFilterProbeHeight = 256;
constexpr float kFilterProbeFarRhw = 0.05f;
constexpr float kFilterProbeRepeats = 24.0f;
constexpr uint32_t kFilterProbeMaskSide = 64;

IDirect3DTexture9* LuminanceMaskTexture(IDirect3DDevice9* dev, const WaterMaskLevels& mask)
{
    IDirect3DTexture9* texture = nullptr;
    const UINT levels = static_cast<UINT>(mask.levels.size());
    if (FAILED(dev->CreateTexture(mask.info.size, mask.info.size, levels, 0, D3DFMT_L8, D3DPOOL_MANAGED, &texture,
                                  nullptr)))
        return nullptr;
    for (UINT level = 0; level < levels; ++level)
    {
        D3DLOCKED_RECT locked = {};
        const UINT side = std::max(mask.info.size >> level, 1u);
        if (FAILED(texture->LockRect(level, &locked, nullptr, 0)))
            continue;
        for (UINT y = 0; y < side; ++y)
            std::memcpy(static_cast<BYTE*>(locked.pBits) + static_cast<size_t>(y) * locked.Pitch,
                        mask.levels[level].data() + static_cast<size_t>(y) * side, side);
        texture->UnlockRect(level);
    }
    return texture;
}

IDirect3DTexture9* PackedMaskTexture(IDirect3DDevice9* dev, const WaterPackedMask& packed)
{
    IDirect3DTexture9* texture = nullptr;
    const UINT levels = static_cast<UINT>(packed.levels.size());
    if (FAILED(dev->CreateTexture(packed.size, packed.size, levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture,
                                  nullptr)))
        return nullptr;
    for (UINT level = 0; level < levels; ++level)
    {
        D3DLOCKED_RECT locked = {};
        const UINT side = std::max(packed.size >> level, 1u);
        if (FAILED(texture->LockRect(level, &locked, nullptr, 0)))
            continue;
        for (UINT y = 0; y < side; ++y)
            std::memcpy(static_cast<BYTE*>(locked.pBits) + static_cast<size_t>(y) * locked.Pitch,
                        packed.levels[level].data() + static_cast<size_t>(y) * side, side * sizeof(uint32_t));
        texture->UnlockRect(level);
    }
    return texture;
}

Image DrawRecedingPlane(IDirect3DDevice9* dev, IDirect3DTexture9* texture)
{
    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
        return Image();
    dev->SetRenderTarget(0, backBuffer);
    backBuffer->Release();
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    dev->SetTexture(0, texture);
    const struct
    {
        D3DSAMPLERSTATETYPE state;
        DWORD value;
    } trilinearWrap[] = {
        {D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP}, {D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP},
        {D3DSAMP_MAGFILTER, D3DTEXF_LINEAR},  {D3DSAMP_MINFILTER, D3DTEXF_LINEAR},
        {D3DSAMP_MIPFILTER, D3DTEXF_LINEAR},  {D3DSAMP_MAXMIPLEVEL, 0},
        {D3DSAMP_MIPMAPLODBIAS, 0},           {D3DSAMP_SRGBTEXTURE, FALSE},
    };
    for (const auto& setting : trilinearWrap)
        dev->SetSamplerState(0, setting.state, setting.value);
    const D3DVIEWPORT9 viewport = {0, 0, kFilterProbeWidth, kFilterProbeHeight, 0.0f, 1.0f};
    dev->SetViewport(&viewport);
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
    const float w = static_cast<float>(kFilterProbeWidth);
    const float h = static_cast<float>(kFilterProbeHeight);
    const float distant = kFilterProbeFarRhw;
    const float repeats = kFilterProbeRepeats;
    const TexturedScreenVertex farLeft = {0.4f * w, 0.0f, 0.5f, distant, 0.0f, repeats * distant};
    const TexturedScreenVertex farRight = {0.6f * w, 0.0f, 0.5f, distant, repeats * distant, repeats * distant};
    const TexturedScreenVertex nearLeft = {0.0f, h, 0.5f, 1.0f, 0.0f, 0.0f};
    const TexturedScreenVertex nearRight = {w, h, 0.5f, 1.0f, repeats, 0.0f};
    const TexturedScreenVertex plane[6] = {farLeft, farRight, nearLeft, farRight, nearRight, nearLeft};
    dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, plane, sizeof(TexturedScreenVertex));
    dev->SetTexture(0, nullptr);
    return Capture(dev);
}

void CheckPackedMasksFilterLikeSeparateMasks(IDirect3DDevice9* dev)
{
    const std::vector<WaterMaskLevels> masks = {PatternMask(kFilterProbeMaskSide, 5),
                                                PatternMask(kFilterProbeMaskSide, 6),
                                                PatternMask(kFilterProbeMaskSide, 7)};
    IDirect3DTexture9* packed = PackedMaskTexture(dev, PackWaveFoamMasks(masks, {0, 1, 2}));
    const Image packedImage = packed ? DrawRecedingPlane(dev, packed) : Image();
    int worst = packed ? 0 : 255;
    for (int slot = 0; slot < kWaveFoamMaskSlots && packed; ++slot)
    {
        IDirect3DTexture9* single = LuminanceMaskTexture(dev, masks[slot]);
        const Image singleImage = single ? DrawRecedingPlane(dev, single) : Image();
        const int bgraChannel = 2 - slot;
        for (UINT y = 0; single && y < kFilterProbeHeight; ++y)
            for (UINT x = 0; x < kFilterProbeWidth; ++x)
                worst = std::max(worst, std::abs(static_cast<int>(packedImage.At(x, y)[bgraChannel]) -
                                                 static_cast<int>(singleImage.At(x, y)[0])));
        if (single)
            single->Release();
        else
            worst = 255;
    }
    if (packed)
        packed->Release();
    std::printf("     packed wave-foam masks against separate L8 masks, trilinear over a receding plane: worst "
                "difference %d/255\n",
                worst);
    Check(worst == 0, "each channel of the packed wave-foam texture filters exactly like the separate L8 mask it "
                      "replaces at every level of detail");
}

void CheckWaveFoamMaskPacking(IDirect3DDevice9* dev)
{
    CheckWaveFoamPacking();
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* target = nullptr;
    dev->CreateStateBlock(D3DSBT_ALL, &state);
    dev->GetRenderTarget(0, &target);
    if (SUCCEEDED(dev->BeginScene()))
    {
        CheckPackedMasksFilterLikeSeparateMasks(dev);
        dev->EndScene();
    }
    if (state)
    {
        state->Apply();
        state->Release();
    }
    dev->SetRenderTarget(0, target);
    if (target)
        target->Release();
}
}
