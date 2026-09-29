#include "status_log.h"

#include <cstring>

namespace
{
bool SameText(const char* a, const char* b)
{
    return std::strcmp(a ? a : "", b ? b : "") == 0;
}
}

bool StatusLog::IdleStateSeen(const char* state)
{
    for (int i = 0; i < m_seenIdleStateCount; ++i)
        if (SameText(m_seenIdleStates[i], state))
            return true;
    if (m_seenIdleStateCount < kMaxIdleStates)
        m_seenIdleStates[m_seenIdleStateCount++] = state;
    return false;
}

StatusLogLine StatusLog::Idle(const char* state)
{
    m_lastSkip = "";
    if (SameText(state, m_lastIdle))
        return {false, LogLevel::Debug};
    m_lastIdle = state;
    return {true, IdleStateSeen(state) ? LogLevel::Debug : LogLevel::Info};
}

StatusLogLine StatusLog::Skip(const char* reason)
{
    m_lastIdle = "";
    if (SameText(reason, m_lastSkip))
        return {false, LogLevel::Info};
    m_lastSkip = reason;
    if (m_skipsLogged >= kMaxSkipsLogged)
        return {false, LogLevel::Info};
    ++m_skipsLogged;
    return {true, LogLevel::Info};
}

void StatusLog::Drawn()
{
    m_lastIdle = "";
    m_lastSkip = "";
}
