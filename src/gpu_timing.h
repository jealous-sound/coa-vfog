#pragma once

#include <d3d9.h>

#include <cstddef>
#include <functional>
#include <vector>

struct GpuTime
{
    float medianMs = 0.0f;
    unsigned frames = 0;
    unsigned skipped = 0;
};

using QueryCreator = std::function<HRESULT(IDirect3DDevice9*, D3DQUERYTYPE, IDirect3DQuery9**)>;

HRESULT CreateDeviceQuery(IDirect3DDevice9* dev, D3DQUERYTYPE type, IDirect3DQuery9** query);

class GpuTimer
{
public:
    static constexpr unsigned kFramesBetweenCreationAttempts = 600;

    explicit GpuTimer(const char* subject, QueryCreator createQuery = CreateDeviceQuery);
    ~GpuTimer();

    void Release();
    bool Prepare(IDirect3DDevice9* dev);
    void Begin(IDirect3DDevice9* dev);
    void Pause();
    void Resume();
    void End();
    void Cancel();
    GpuTime TakeInterval();
    bool Unsupported() const { return m_unsupported; }
    const char* Subject() const { return m_subject; }

private:
    struct QuerySet
    {
        IDirect3DQuery9* disjoint = nullptr;
        IDirect3DQuery9* frequency = nullptr;
        IDirect3DQuery9* start = nullptr;
        IDirect3DQuery9* pause = nullptr;
        IDirect3DQuery9* resume = nullptr;
        IDirect3DQuery9* end = nullptr;
        bool pending = false;
        bool paused = false;
        bool split = false;
    };

    static constexpr int kQuerySets = 32;
    static constexpr size_t kMaxIntervalSamples = 16384;

    void ReleaseQueries();
    HRESULT CreateQueries(IDirect3DDevice9* dev);
    void CollectFinished();
    bool Collect(QuerySet& set);
    void AddSample(float milliseconds);

    const char* m_subject;
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

void DescribeGpuTime(GpuTimer& timer, IDirect3DDevice9* dev, char* text, size_t size);
