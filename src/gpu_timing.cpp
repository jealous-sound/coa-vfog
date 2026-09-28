#include "gpu_timing.h"

#include "log.h"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace
{
template <typename T>
void ReleaseQuery(T*& query)
{
    if (query)
    {
        query->Release();
        query = nullptr;
    }
}

template <typename T>
HRESULT ReadWithoutFlush(IDirect3DQuery9* query, T& value)
{
    return query->GetData(&value, sizeof(value), 0);
}

bool TimestampQueriesUnsupported(IDirect3DDevice9* dev, HRESULT creation)
{
    return (creation == D3DERR_NOTAVAILABLE || creation == D3DERR_INVALIDCALL) &&
           dev->TestCooperativeLevel() == D3D_OK;
}
}

HRESULT CreateDeviceQuery(IDirect3DDevice9* dev, D3DQUERYTYPE type, IDirect3DQuery9** query)
{
    return dev->CreateQuery(type, query);
}

FogGpuTimer::FogGpuTimer(QueryCreator createQuery) : m_createQuery(std::move(createQuery))
{
}

FogGpuTimer::~FogGpuTimer()
{
    ReleaseQueries();
}

void FogGpuTimer::ReleaseQueries()
{
    for (QuerySet& set : m_sets)
    {
        ReleaseQuery(set.disjoint);
        ReleaseQuery(set.frequency);
        ReleaseQuery(set.start);
        ReleaseQuery(set.end);
        set.pending = false;
    }
    m_next = 0;
    m_open = -1;
    m_created = false;
}

void FogGpuTimer::Release()
{
    ReleaseQueries();
    m_framesUntilCreationAttempt = 0;
}

HRESULT FogGpuTimer::CreateQueries(IDirect3DDevice9* dev)
{
    HRESULT result = D3D_OK;
    for (QuerySet& set : m_sets)
    {
        const struct
        {
            D3DQUERYTYPE type;
            IDirect3DQuery9** query;
        } requests[] = {
            {D3DQUERYTYPE_TIMESTAMPDISJOINT, &set.disjoint},
            {D3DQUERYTYPE_TIMESTAMPFREQ, &set.frequency},
            {D3DQUERYTYPE_TIMESTAMP, &set.start},
            {D3DQUERYTYPE_TIMESTAMP, &set.end},
        };
        for (const auto& request : requests)
            if (SUCCEEDED(result))
                result = m_createQuery(dev, request.type, request.query);
    }
    return result;
}

bool FogGpuTimer::Prepare(IDirect3DDevice9* dev)
{
    if (m_created)
        return true;
    if (m_unsupported || m_framesUntilCreationAttempt > 0)
        return false;
    const HRESULT result = CreateQueries(dev);
    if (SUCCEEDED(result))
    {
        m_created = true;
        return true;
    }
    ReleaseQueries();
    m_unsupported = TimestampQueriesUnsupported(dev, result);
    if (m_unsupported)
    {
        VF_LOG_INFO("fog gpu timing unavailable (timestamp query creation HRESULT 0x%08lX); the frame summary omits it",
                    static_cast<unsigned long>(result));
        return false;
    }
    m_framesUntilCreationAttempt = kFramesBetweenCreationAttempts;
    VF_LOG_INFO("fog gpu timing queries not created (HRESULT 0x%08lX); retrying in %u frames or after the next Reset",
                static_cast<unsigned long>(result), kFramesBetweenCreationAttempts);
    return false;
}

void FogGpuTimer::AddSample(float milliseconds)
{
    if (m_samples.size() < kMaxIntervalSamples)
    {
        m_samples.push_back(milliseconds);
        return;
    }
    m_samples[m_nextSample] = milliseconds;
    m_nextSample = (m_nextSample + 1) % kMaxIntervalSamples;
}

bool FogGpuTimer::Collect(QuerySet& set)
{
    BOOL disjoint = TRUE;
    UINT64 frequency = 0;
    UINT64 start = 0;
    UINT64 end = 0;
    const HRESULT results[] = {ReadWithoutFlush(set.end, end), ReadWithoutFlush(set.start, start),
                               ReadWithoutFlush(set.frequency, frequency), ReadWithoutFlush(set.disjoint, disjoint)};
    if (std::any_of(std::begin(results), std::end(results), [](HRESULT r) { return r == S_FALSE; }))
        return false;
    set.pending = false;
    const bool read = std::all_of(std::begin(results), std::end(results), [](HRESULT r) { return r == S_OK; });
    if (!read || disjoint || !frequency || end < start)
        ++m_skipped;
    else
        AddSample(static_cast<float>(1000.0 * static_cast<double>(end - start) / static_cast<double>(frequency)));
    return true;
}

void FogGpuTimer::CollectFinished()
{
    for (int i = 0; i < kQuerySets; ++i)
    {
        QuerySet& set = m_sets[(m_next + i) % kQuerySets];
        if (set.pending && !Collect(set))
            return;
    }
}

void FogGpuTimer::Begin(IDirect3DDevice9* dev)
{
    m_open = -1;
    if (m_framesUntilCreationAttempt > 0)
        --m_framesUntilCreationAttempt;
    if (!Prepare(dev))
    {
        if (!m_unsupported)
            ++m_skipped;
        return;
    }
    CollectFinished();
    QuerySet& set = m_sets[m_next];
    if (set.pending)
    {
        ++m_skipped;
        return;
    }
    if (FAILED(set.disjoint->Issue(D3DISSUE_BEGIN)) || FAILED(set.frequency->Issue(D3DISSUE_END)) ||
        FAILED(set.start->Issue(D3DISSUE_END)))
    {
        ++m_skipped;
        return;
    }
    m_open = m_next;
    m_next = (m_next + 1) % kQuerySets;
}

void FogGpuTimer::End()
{
    if (m_open < 0)
        return;
    QuerySet& set = m_sets[m_open];
    m_open = -1;
    if (FAILED(set.end->Issue(D3DISSUE_END)) || FAILED(set.disjoint->Issue(D3DISSUE_END)))
    {
        ++m_skipped;
        return;
    }
    set.pending = true;
}

FogGpuTime FogGpuTimer::TakeInterval()
{
    FogGpuTime interval;
    interval.frames = static_cast<unsigned>(m_samples.size());
    interval.skipped = m_skipped;
    if (!m_samples.empty())
    {
        const auto middle = m_samples.begin() + m_samples.size() / 2;
        std::nth_element(m_samples.begin(), middle, m_samples.end());
        interval.medianMs = *middle;
    }
    m_samples.clear();
    m_nextSample = 0;
    m_skipped = 0;
    return interval;
}

void DescribeFogGpuTime(FogGpuTimer& timer, IDirect3DDevice9* dev, char* text, size_t size)
{
    timer.Prepare(dev);
    const FogGpuTime interval = timer.TakeInterval();
    if (timer.Unsupported())
        return;
    if (interval.frames > 0)
        std::snprintf(text, size, ", fog gpu %.2f ms (median of %u frames, %u skipped)", interval.medianMs,
                      interval.frames, interval.skipped);
    else
        std::snprintf(text, size, ", fog gpu no samples (%u skipped)", interval.skipped);
}
