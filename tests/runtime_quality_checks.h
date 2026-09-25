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
        }
        if (state)
            state->Release();
        Check(ok, desired ? "liquid depth override restores enabled writes applied by a state block" :
                            "liquid depth override restores disabled writes applied by a state block");
    }
    device->SetRenderState(D3DRS_ZWRITEENABLE, initial);
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
