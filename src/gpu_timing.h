#pragma once

#include <d3d9.h>

#include <cstddef>
#include <functional>
#include <vector>

struct FogGpuTime
{
    float medianMs = 0.0f;
    unsigned frames = 0;
    unsigned skipped = 0;
};

using QueryCreator = std::function<HRESULT(IDirect3DDevice9*, D3DQUERYTYPE, IDirect3DQuery9**)>;

HRESULT CreateDeviceQuery(IDirect3DDevice9* dev, D3DQUERYTYPE type, IDirect3DQuery9** query);

class FogGpuTimer
{
public:
    static constexpr unsigned kFramesBetweenCreationAttempts = 600;

    explicit FogGpuTimer(QueryCreator createQuery = CreateDeviceQuery);
    ~FogGpuTimer();

    void Release();
    bool Prepare(IDirect3DDevice9* dev);
    void Begin(IDirect3DDevice9* dev);
    void End();
    FogGpuTime TakeInterval();
    bool Unsupported() const { return m_unsupported; }

private:
    struct QuerySet
    {
        IDirect3DQuery9* disjoint = nullptr;
        IDirect3DQuery9* frequency = nullptr;
        IDirect3DQuery9* start = nullptr;
        IDirect3DQuery9* end = nullptr;
        bool pending = false;
    };

    static constexpr int kQuerySets = 32;
    static constexpr size_t kMaxIntervalSamples = 16384;

    void ReleaseQueries();
    HRESULT CreateQueries(IDirect3DDevice9* dev);
    void CollectFinished();
    bool Collect(QuerySet& set);
    void AddSample(float milliseconds);

    QueryCreator m_createQuery;
    QuerySet m_sets[kQuerySets];
    int m_next = 0;
    int m_open = -1;
    bool m_created = false;
    bool m_unsupported = false;
    unsigned m_framesUntilCreationAttempt = 0;
    std::vector<float> m_samples;
    size_t m_nextSample = 0;
    unsigned m_skipped = 0;
};

void DescribeFogGpuTime(FogGpuTimer& timer, IDirect3DDevice9* dev, char* text, size_t size);
