#pragma once

#include "log.h"

struct StatusLogLine
{
    bool write;
    LogLevel level;
};

class StatusLog
{
public:
    static constexpr unsigned kMaxSkipsLogged = 50;

    StatusLogLine Idle(const char* state);
    StatusLogLine Skip(const char* reason);
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
