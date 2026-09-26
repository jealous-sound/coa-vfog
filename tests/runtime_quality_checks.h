#pragma once

void CheckDepthWriteStateBlockRestore(IDirect3DDevice9* device)
{
    DWORD initial = TRUE;
    device->GetRenderState(D3DRS_ZWRITEENABLE, &initial);
    for (DWORD desired : {static_cast<DWORD>(FALSE), static_cast<DWORD>(TRUE)})
    {
        IDirect3DStateBlock9* state = nullptr;
        device->SetRenderState(D3DRS_ZWRITEENABLE, desired);
        bool ok = SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &state)) && state;
        if (ok)
        {
            device->SetRenderState(D3DRS_ZWRITEENABLE, !desired);
            ok = SUCCEEDED(state->Apply());
            DWORD forced = FALSE;
            DWORD restored = !desired;
            vf_test_force_depth_write(1);
            device->GetRenderState(D3DRS_ZWRITEENABLE, &forced);
            vf_test_force_depth_write(0);
            device->GetRenderState(D3DRS_ZWRITEENABLE, &restored);
            ok = ok && forced == TRUE && restored == desired;
            vf_test_suppress_depth_write(1);
            device->GetRenderState(D3DRS_ZWRITEENABLE, &forced);
            vf_test_suppress_depth_write(0);
            device->GetRenderState(D3DRS_ZWRITEENABLE, &restored);
            ok = ok && forced == FALSE && restored == desired;
        }
        if (state)
            state->Release();
        Check(ok, desired ? "depth overrides restore enabled writes applied by a state block" :
                            "depth overrides restore disabled writes applied by a state block");
    }
    device->SetRenderState(D3DRS_ZWRITEENABLE, initial);
}

void CheckWorldTextDepthIsolation(Harness& h)
{
    IDirect3DDevice9* device = h.dev;
    IDirect3DStateBlock9* state = nullptr;
    const bool ready = SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &state));
    Check(ready, "world text depth regression captures device state");
    if (!ready)
        return;
    const D3DVIEWPORT9 viewport = {0, 0, h.pp.BackBufferWidth, h.pp.BackBufferHeight, 0, 1};
    device->SetViewport(&viewport);
    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
    device->SetTexture(0, nullptr);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    for (int suppress = 0; suppress < 2; ++suppress)
    {
        device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF000000, 0.8f, 0);
        device->BeginScene();
        h.DrawPretransformedQuadAtRawDepth(0, 0, 16, 32, 0.1f, 0xFFFFFFFF);
        vf_test_suppress_depth_write(suppress);
        device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        h.DrawPretransformedQuadAtRawDepth(8, 8, 32, 24, 0.2f, 0xFF0000FF);
        vf_test_suppress_depth_write(0);
        device->EndScene();
        const Image text = Capture(device);
        if (suppress)
        {
            Check(text.At(24, 16)[0] == 255 && text.At(24, 16)[1] == 0,
                  "world text still draws its visible colour with depth writes suppressed");
            Check(text.At(12, 16)[1] == 255, "world text still tests against foreground geometry");
        }
        device->BeginScene();
        h.DrawPretransformedQuadAtRawDepth(16, 8, 32, 24, 0.5f, 0xFF00FF00);
        device->EndScene();
        const Image probe = Capture(device);
        Check(probe.At(24, 16)[1] == (suppress ? 255 : 0),
              suppress ? "world text leaves the world depth behind its glyphs intact" :
                         "unprotected world text reproduces glyph contamination of world depth");
    }
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    vf_test_force_depth_write(1);
    vf_test_suppress_depth_write(1);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    DWORD suppressed = TRUE;
    DWORD forced = FALSE;
    DWORD restored = TRUE;
    device->GetRenderState(D3DRS_ZWRITEENABLE, &suppressed);
    vf_test_suppress_depth_write(0);
    device->GetRenderState(D3DRS_ZWRITEENABLE, &forced);
    vf_test_force_depth_write(0);
    device->GetRenderState(D3DRS_ZWRITEENABLE, &restored);
    Check(suppressed == FALSE && forced == TRUE && restored == FALSE,
          "text suppression nests with liquid depth writes and restores the client's last request");
    state->Apply();
    state->Release();
}

struct FogFixtureHeader
{
    char magic[4];
    uint32_t version;
    uint32_t lightCount;
    uint32_t paramsCount;
    uint32_t keyCount;
    uint32_t layerCount;
    uint32_t zoneCount;
    uint32_t pointCount;
};

constexpr size_t kFogFixtureLightBytes = 60;
constexpr size_t kFogFixtureParamsBytes = 12;
constexpr size_t kFogFixtureKeyBytes = 12;
constexpr size_t kFogFixtureFirstKeyOffset = 4;
constexpr size_t kFogFixtureKeyCountOffset = 8;
constexpr size_t kFogFixtureFirstLayerOffset = 4;
constexpr size_t kFogFixtureLayerCountOffset = 2;

template <typename T>
void SetFogFixtureValue(std::vector<unsigned char>& bytes, size_t offset, T value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void CheckRejectedFogFixture(const std::wstring& path, const std::vector<unsigned char>& bytes, const char* label)
{
    std::FILE* file = _wfopen(path.c_str(), L"wb");
    bool written = false;
    if (file)
    {
        written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
        written = std::fclose(file) == 0 && written;
    }
    bool loaded = true;
    bool threw = false;
    FogData data;
    if (written)
    {
        try
        {
            loaded = data.Load(NarrowPath(path));
        }
        catch (...)
        {
            threw = true;
        }
    }
    Check(written && !threw && !loaded && !data.Loaded(), label);
}

void CheckFogDataBounds(const std::wstring& outDir, const std::string& dataPath)
{
    std::FILE* source = std::fopen(dataPath.c_str(), "rb");
    std::vector<unsigned char> original;
    if (source)
    {
        if (std::fseek(source, 0, SEEK_END) == 0)
        {
            const long size = std::ftell(source);
            if (size >= static_cast<long>(sizeof(FogFixtureHeader)) && std::fseek(source, 0, SEEK_SET) == 0)
            {
                original.resize(static_cast<size_t>(size));
                if (std::fread(original.data(), 1, original.size(), source) != original.size())
                    original.clear();
            }
        }
        std::fclose(source);
    }
    Check(!original.empty(), "Classic data is available for malformed-record regression checks");
    if (original.empty())
        return;
    FogFixtureHeader header;
    std::memcpy(&header, original.data(), sizeof(header));
    const size_t paramsOffset = sizeof(header) + header.lightCount * kFogFixtureLightBytes;
    const size_t keysOffset = paramsOffset + header.paramsCount * kFogFixtureParamsBytes;
    if (header.paramsCount == 0 || header.keyCount == 0 || keysOffset + kFogFixtureKeyBytes > original.size())
    {
        Check(false, "Classic fixture has parameter and layer ranges to validate");
        return;
    }
    const std::wstring fixture = FullPath(outDir + L"\\invalid-fogdata.bin");
    auto bytes = original;
    header.lightCount = UINT32_MAX;
    std::memcpy(bytes.data(), &header, sizeof(header));
    CheckRejectedFogFixture(fixture, bytes, "fogdata rejects impossible counts before allocating or throwing");

    bytes = original;
    SetFogFixtureValue<uint32_t>(bytes, paramsOffset + kFogFixtureFirstKeyOffset, UINT32_MAX);
    SetFogFixtureValue<uint32_t>(bytes, paramsOffset + kFogFixtureKeyCountOffset, 2);
    CheckRejectedFogFixture(fixture, bytes, "fogdata rejects overflowing time-key ranges");

    bytes = original;
    SetFogFixtureValue<uint32_t>(bytes, keysOffset + kFogFixtureFirstLayerOffset, UINT32_MAX);
    SetFogFixtureValue<uint16_t>(bytes, keysOffset + kFogFixtureLayerCountOffset, 2);
    CheckRejectedFogFixture(fixture, bytes, "fogdata rejects overflowing layer ranges");

    bytes = original;
    bytes.pop_back();
    CheckRejectedFogFixture(fixture, bytes, "fogdata rejects a truncated record array");
    DeleteFileW(fixture.c_str());
}
