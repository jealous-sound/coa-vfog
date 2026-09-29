#pragma once

#include "log.h"

struct WaterLogLine
{
    bool write;
    LogLevel level;
};

class WaterStatusLog
{
public:
    static constexpr unsigned kMaxSkipsLogged = 50;

    WaterLogLine Idle(const char* state);
    WaterLogLine Skip(const char* reason);
    void Drawn();
    unsigned SkipsLogged() const { return m_skipsLogged; }

private:
    static constexpr int kMaxIdleStates = 8;

    bool IdleStateSeen(const char* state);

    const char* m_lastIdle = "";
    const char* m_lastSkip = "";
    unsigned m_skipsLogged = 0;
    const char* m_seenIdleStates[kMaxIdleStates] = {};
    int m_seenIdleStateCount = 0;
};
