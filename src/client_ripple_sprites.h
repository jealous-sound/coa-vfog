#pragma once

#include <cstddef>
#include <cstdint>

constexpr size_t kMaxClientGuardBytes = 16;
constexpr double kClientSpriteGraceSeconds = 1.0;

struct ClientCodeGuard
{
    const char* name;
    uintptr_t address;
    size_t size;
    unsigned char bytes[kMaxClientGuardBytes];
};

struct ClientCodeView
{
    uintptr_t base = 0;
    const unsigned char* bytes = nullptr;
    size_t size = 0;
};

namespace engine
{
constexpr uintptr_t kWaterRipplesCommandValue = 0x00ADF7F0;

const ClientCodeGuard* ClientRippleGateGuards(size_t& count);
bool ClientRippleGateCodeMatches(const ClientCodeView& code);
}

class ClientRippleSprites
{
public:
    bool Bind(volatile int32_t* gate, const ClientCodeView& code);
    void Update(bool allowed, bool shaded, double seconds);
    void Restore();
    bool Bound() const { return m_gate != nullptr; }
    bool Holding() const { return m_holding; }
    int32_t Remembered() const { return m_remembered; }

private:
    void Hold();
    void Detach(const char* reason);
    void LogTransition(const char* text, int32_t value);

    volatile int32_t* m_gate = nullptr;
    bool m_holding = false;
    int32_t m_remembered = 0;
    double m_lastShaded = 0.0;
    uint32_t m_transitions = 0;
};

ClientRippleSprites& GlobalClientRippleSprites();
