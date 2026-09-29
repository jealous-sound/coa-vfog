#pragma once

#include <d3d9.h>

enum class DepthCopyMethod
{
    None,
    Nvapi,
    Resz,
};

constexpr int kDepthCopyMethodFromDriver = -1;

const char* DepthCopyMethodName(DepthCopyMethod method);

struct DepthCopyProbe
{
    DepthCopyMethod method = DepthCopyMethod::None;
    const char* unavailable = "";
};

DepthCopyProbe ProbeDepthCopy(IDirect3D9* d3d, UINT adapter, D3DDEVTYPE type);
void ForceDepthCopyMethod(int method);
bool SameSampleCount(const D3DSURFACE_DESC& target, const D3DSURFACE_DESC& depth);

class DepthCopy
{
public:
    DepthCopy() = default;
    DepthCopy(const DepthCopy&) = delete;
    DepthCopy& operator=(const DepthCopy&) = delete;
    ~DepthCopy();

    bool Attach(DepthCopyMethod method, IDirect3DSurface9* source, IDirect3DTexture9* destination);
    void Detach();
    bool Copy(IDirect3DDevice9* dev) const;
    DepthCopyMethod Method() const { return m_method; }

private:
    DepthCopyMethod m_method = DepthCopyMethod::None;
    IDirect3DSurface9* m_source = nullptr;
    IDirect3DTexture9* m_destination = nullptr;
};

struct DepthCopyTest
{
    bool passed = false;
    float expected[2] = {};
    float read[2] = {};
    float copyMilliseconds = -1.0f;
};

DepthCopyTest TestDepthCopy(IDirect3DDevice9* dev, const DepthCopy& copy, IDirect3DSurface9* source,
                            IDirect3DTexture9* destination);

struct DepthTexel
{
    UINT x;
    UINT y;
};

bool ReadDepthTexels(IDirect3DDevice9* dev, IDirect3DTexture9* depth, const DepthTexel* texels, int count,
                     float* values);

struct SceneDepth
{
    IDirect3DTexture9* texture = nullptr;
    IDirect3DSurface9* bound = nullptr;
    const DepthCopy* copy = nullptr;

    bool Multisampled() const { return copy != nullptr; }
    bool Refresh(IDirect3DDevice9* dev) const { return !copy || copy->Copy(dev); }
};

struct MultisamplingStatus
{
    int samples = 0;
    const char* method = "";
    const char* off = "no fog device";
    float copyMilliseconds = -1.0f;
};
