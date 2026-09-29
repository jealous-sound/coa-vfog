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

GpuTimer::GpuTimer(const char* subject, QueryCreator createQuery)
    : m_subject(subject), m_createQuery(std::move(createQuery))
{
}

GpuTimer::~GpuTimer()
{
    ReleaseQueries();
}

void GpuTimer::ReleaseQueries()
{
    for (QuerySet& set : m_sets)
    {
        ReleaseQuery(set.disjoint);
        ReleaseQuery(set.frequency);
        ReleaseQuery(set.start);
        ReleaseQuery(set.pause);
        ReleaseQuery(set.resume);
        ReleaseQuery(set.end);
        set.pending = false;
        set.paused = false;
        set.split = false;
    }
    m_next = 0;
    m_open = -1;
    m_created = false;
}

void GpuTimer::Release()
{
    ReleaseQueries();
    m_framesUntilCreationAttempt = 0;
}

HRESULT GpuTimer::CreateQueries(IDirect3DDevice9* dev)
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
            {D3DQUERYTYPE_TIMESTAMP, &set.pause},
            {D3DQUERYTYPE_TIMESTAMP, &set.resume},
            {D3DQUERYTYPE_TIMESTAMP, &set.end},
        };
        for (const auto& request : requests)
            if (SUCCEEDED(result))
                result = m_createQuery(dev, request.type, request.query);
    }
    return result;
}

bool GpuTimer::Prepare(IDirect3DDevice9* dev)
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
        VF_LOG_INFO("%s gpu timing unavailable (timestamp query creation HRESULT 0x%08lX); the summary omits it",
                    m_subject, static_cast<unsigned long>(result));
        return false;
    }
    m_framesUntilCreationAttempt = kFramesBetweenCreationAttempts;
    VF_LOG_INFO("%s gpu timing queries not created (HRESULT 0x%08lX); retrying in %u frames or after the next Reset",
                m_subject, static_cast<unsigned long>(result), kFramesBetweenCreationAttempts);
    return false;
}

void GpuTimer::AddSample(float milliseconds)
{
    if (m_samples.size() < kMaxIntervalSamples)
    {
        m_samples.push_back(milliseconds);
        return;
    }
    m_samples[m_nextSample] = milliseconds;
    m_nextSample = (m_nextSample + 1) % kMaxIntervalSamples;
}

bool GpuTimer::Collect(QuerySet& set)
{
    BOOL disjoint = TRUE;
    UINT64 frequency = 0;
    UINT64 start = 0;
    UINT64 pause = 0;
    UINT64 resume = 0;
    UINT64 end = 0;
    const HRESULT results[] = {ReadWithoutFlush(set.end, end),
                               set.split ? ReadWithoutFlush(set.resume, resume) : S_OK,
                               set.split ? ReadWithoutFlush(set.pause, pause) : S_OK,
                               ReadWithoutFlush(set.start, start),
                               ReadWithoutFlush(set.frequency, frequency),
                               ReadWithoutFlush(set.disjoint, disjoint)};
    if (std::any_of(std::begin(results), std::end(results), [](HRESULT r) { return r == S_FALSE; }))
        return false;
    set.pending = false;
    const bool read = std::all_of(std::begin(results), std::end(results), [](HRESULT r) { return r == S_OK; });
    const bool ordered = set.split ? start <= pause && pause <= resume && resume <= end : start <= end;
    if (!read || disjoint || !frequency || !ordered)
        ++m_skipped;
    else
        AddSample(static_cast<float>(1000.0 * static_cast<double>(end - start - (resume - pause)) /
                                     static_cast<double>(frequency)));
    return true;
}

void GpuTimer::CollectFinished()
{
    for (int i = 0; i < kQuerySets; ++i)
    {
        QuerySet& set = m_sets[(m_next + i) % kQuerySets];
        if (set.pending && !Collect(set))
            return;
    }
}

void GpuTimer::Begin(IDirect3DDevice9* dev)
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
    set.paused = false;
    set.split = false;
    m_open = m_next;
    m_next = (m_next + 1) % kQuerySets;
}

void GpuTimer::Pause()
{
    if (m_open < 0)
        return;
    QuerySet& set = m_sets[m_open];
    if (set.paused || set.split)
        return;
    if (FAILED(set.pause->Issue(D3DISSUE_END)))
    {
        Cancel();
        return;
    }
    set.paused = true;
}

void GpuTimer::Resume()
{
    if (m_open < 0)
        return;
    QuerySet& set = m_sets[m_open];
    if (!set.paused)
        return;
    if (FAILED(set.resume->Issue(D3DISSUE_END)))
    {
        Cancel();
        return;
    }
    set.paused = false;
    set.split = true;
}

void GpuTimer::End()
{
    if (m_open < 0)
        return;
    Resume();
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

void GpuTimer::Cancel()
{
    if (m_open < 0)
        return;
    QuerySet& set = m_sets[m_open];
    m_open = -1;
    set.disjoint->Issue(D3DISSUE_END);
    set.pending = false;
    ++m_skipped;
}

GpuTime GpuTimer::TakeInterval()
{
    GpuTime interval;
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

void DescribeGpuTime(GpuTimer& timer, IDirect3DDevice9* dev, char* text, size_t size)
{
    timer.Prepare(dev);
    const GpuTime interval = timer.TakeInterval();
    if (timer.Unsupported())
        return;
    if (interval.frames > 0)
        std::snprintf(text, size, "%s gpu %.2f ms (median of %u frames, %u skipped)", timer.Subject(),
                      interval.medianMs, interval.frames, interval.skipped);
    else
        std::snprintf(text, size, "%s gpu no samples (%u skipped)", timer.Subject(), interval.skipped);
}
