#include "forever_look.h"

#include "config.h"
#include "d3d9_wrap.h"
#include "log.h"
#include "status_log.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
constexpr float kGlowByteScale = 255.0f;
constexpr unsigned kDebugSummaryFrames = 600;
constexpr const char* kGradingOff = "off";
constexpr const char* kGhostEffect = "ghost effect";
constexpr const char* kCameraUnderWater = "camera under water";
constexpr const char* kNoClassicLight = "no Classic light covers the camera";
constexpr const char* kStoppedAfterException = "stopped after an exception, see CoAVolFog.log";

struct GlowWrite
{
    uintptr_t pass;
    uint8_t client;
    uint8_t fed;
};

struct GlowFrame
{
    bool delivered = false;
    uint8_t clientByte = 0;
    uint8_t deliveredByte = 0;
    float weight = 0.0f;
    int writeCount = 0;
    GlowWrite writes[engine::kGlowPassLists] = {};
};

enum class GradingState
{
    NoWorld,
    Off,
    Idle,
    Skipped,
    Ready,
};

struct GradingFrame
{
    GradingState state = GradingState::NoWorld;
    const char* reason = "";
    D3DVIEWPORT9 viewport = {};
    float curve[kGradingCurveEntries] = {};
    float strength = 0.0f;
};

struct ForeverLookClient
{
    FogDevice* (*device)();
    ForeverLookFrame (*frame)(const Config& cfg);
};

bool LookNeeded(const Config& cfg)
{
    return cfg.foreverGlow != 0 || cfg.colorGrading > 0.0f;
}

ForeverLook ResolveForeverLook()
{
    ForeverLook look;
    engine::ClassicLightInputs light;
    if (!GlobalFogData().Loaded() || !engine::ReadClassicLightInputs(light))
        return look;
    AuthoredFog fog = {};
    GlobalFogData().Resolve(light.mapId, light.camPos, light.dayFraction, light.lightParams, fog);
    look.valid = std::isfinite(fog.coverage) && std::isfinite(fog.glow);
    look.coverage = fog.coverage;
    look.hasGlow = fog.hasGlow;
    look.glow = fog.glow;
    std::memcpy(look.gradingCurve, fog.gradingCurve, sizeof(look.gradingCurve));
    return look;
}

ForeverLookFrame GameForeverLookFrame(const Config& cfg)
{
    ForeverLookFrame frame;
    frame.effects = engine::ReadScreenEffects();
    frame.cameraInLiquid = engine::CameraInLiquid();
    frame.hasWorldViewport = engine::OpaqueViewport(frame.worldViewport);
    if (LookNeeded(cfg))
        frame.look = ResolveForeverLook();
    return frame;
}

ForeverLookFrame g_testFrame;

ForeverLookFrame TestForeverLookFrame(const Config&)
{
    return g_testFrame;
}

FogDevice* NoFogDevice()
{
    return nullptr;
}

ForeverLookClient g_client = {&NoFogDevice, &GameForeverLookFrame};
bool g_glowAvailable = false;
bool g_gradingAvailable = false;
bool g_glowFailed = false;
bool g_gradingFailed = false;
GlowFrame g_glow;
GradingFrame g_grading;
FogDevice* g_gradedDevice = nullptr;
ForeverLookStatus g_status;
StatusLog g_gradingLog;
bool g_loggedGlowOverride = false;
unsigned g_frames = 0;

int ForeverLookFilter(unsigned code, const char* where)
{
    VF_LOG_ERROR("exception 0x%08X in %s; it is off for this session, the fog continues", code, where);
    return EXCEPTION_EXECUTE_HANDLER;
}

float GlowWeight(const ForeverLook& look, const Config& cfg)
{
    if (!cfg.foreverGlow || !look.valid || !look.hasGlow)
        return 0.0f;
    return ForeverLookWeight(look.coverage);
}

void RestoreGlow()
{
    for (int i = 0; i < g_glow.writeCount; ++i)
    {
        const GlowWrite& write = g_glow.writes[i];
        uint8_t current = 0;
        if (engine::ReadGlowByte(write.pass, current) && current == write.fed)
            engine::WriteGlowByte(write.pass, write.client);
    }
    g_glow = GlowFrame();
}

void FeedGlow(const ForeverLookFrame& frame, const Config& cfg)
{
    g_glow = GlowFrame();
    engine::GlowCompositePasses passes;
    if (!g_glowAvailable || g_glowFailed || !engine::FindGlowCompositePasses(frame.effects, passes))
        return;
    const float weight = GlowWeight(frame.look, cfg);
    for (int list = 0; list < engine::kGlowPassLists; ++list)
    {
        uint8_t client = 0;
        if (!engine::ReadGlowByte(passes.pass[list], client))
        {
            RestoreGlow();
            return;
        }
        const uint8_t fed = weight > 0.0f ? ForeverGlowByte(client, frame.look.glow, weight) : client;
        if (fed != client && engine::WriteGlowByte(passes.pass[list], fed))
            g_glow.writes[g_glow.writeCount++] = {passes.pass[list], client, fed};
        if (list == 0)
            g_glow.clientByte = client;
    }
    g_glow.weight = weight;
    g_glow.delivered = engine::ReadGlowByte(passes.pass[0], g_glow.deliveredByte);
}

void LogGlowOverride()
{
    const bool overriding = g_glow.writeCount > 0;
    if (overriding == g_loggedGlowOverride)
        return;
    g_loggedGlowOverride = overriding;
    if (overriding)
        VF_LOG_INFO("Forever glow: the glow composite gets %u where the client set %u (Classic weight %.2f)",
                    g_glow.deliveredByte, g_glow.clientByte, g_glow.weight);
    else
        VF_LOG_INFO("Forever glow: the client's own glow applies");
}

void RecordGlowStatus()
{
    g_status.glowDelivered = g_glow.delivered;
    g_status.clientGlowByte = g_glow.clientByte;
    g_status.deliveredGlowByte = g_glow.deliveredByte;
    g_status.weight = g_glow.weight;
}

GradingFrame NotGraded(GradingState state, const char* reason)
{
    GradingFrame grading;
    grading.state = state;
    grading.reason = reason;
    return grading;
}

GradingFrame PrepareGrading(const ForeverLookFrame& frame, const Config& cfg)
{
    if (cfg.colorGrading <= 0.0f)
        return NotGraded(GradingState::Off, kGradingOff);
    if (!g_gradingAvailable)
        return NotGraded(GradingState::Skipped, "unavailable in this client");
    if (g_gradingFailed)
        return NotGraded(GradingState::Skipped, kStoppedAfterException);
    if (frame.effects.death && frame.effects.current == frame.effects.death)
        return NotGraded(GradingState::Idle, kGhostEffect);
    if (frame.cameraInLiquid)
        return NotGraded(GradingState::Idle, kCameraUnderWater);
    if (!frame.hasWorldViewport)
        return NotGraded(GradingState::Skipped, "no world viewport");
    if (!frame.look.valid)
        return NotGraded(GradingState::Skipped, "no Classic light data or camera inputs");
    const float strength = cfg.colorGrading * ForeverLookWeight(frame.look.coverage);
    if (!(strength > 0.0f))
        return NotGraded(GradingState::Idle, kNoClassicLight);
    GradingFrame grading = NotGraded(GradingState::Ready, "");
    grading.viewport = frame.worldViewport;
    std::memcpy(grading.curve, frame.look.gradingCurve, sizeof(grading.curve));
    grading.strength = strength;
    return grading;
}

void RecordGrading(GradingState state, const char* reason, float strength)
{
    g_status.graded = state == GradingState::Ready;
    g_status.gradingStrength = g_status.graded ? strength : 0.0f;
    g_status.grading = reason;
    if (state == GradingState::Ready)
    {
        g_gradingLog.Drawn();
        return;
    }
    if (state == GradingState::Idle)
    {
        const StatusLogLine line = g_gradingLog.Idle(reason);
        if (line.write)
            LogWrite(line.level, "colour grading idle: %s%s", reason,
                     line.level == LogLevel::Info ? " (repeats are logged at LogLevel 2)" : "");
        return;
    }
    const StatusLogLine line = g_gradingLog.Skip(reason);
    if (line.write)
        LogWrite(line.level, "colour grading skipped: %s", reason);
}

void ReleaseGradingWhenOff()
{
    FogDevice* device = g_gradedDevice;
    g_gradedDevice = nullptr;
    if (device && device == g_client.device())
        ReleaseGrading(device);
}

void GradeWorldFrame()
{
    const GradingFrame grading = g_grading;
    g_grading = GradingFrame();
    if (GlobalConfig().Get().colorGrading <= 0.0f)
    {
        ReleaseGradingWhenOff();
        g_status.graded = false;
        g_status.grading = kGradingOff;
        return;
    }
    if (grading.state == GradingState::NoWorld)
        return;
    if (grading.state != GradingState::Ready)
    {
        RecordGrading(grading.state, grading.reason, 0.0f);
        return;
    }
    FogDevice* device = g_client.device();
    const char* skip = "";
    if (GradeWorld(device, grading.viewport, grading.curve, grading.strength, &skip))
    {
        g_gradedDevice = device;
        RecordGrading(GradingState::Ready, "", grading.strength);
    }
    else
        RecordGrading(GradingState::Skipped, skip && *skip ? skip : "the grading pass could not draw", 0.0f);
}

void LogDebugSummary()
{
    if (++g_frames % kDebugSummaryFrames != 0 || !LogEnabled(LogLevel::Debug))
        return;
    if (g_status.graded)
        VF_LOG_DEBUG("Forever look: glow byte %u (client %u, Classic weight %.2f%s), colour grading %.2f",
                     g_status.deliveredGlowByte, g_status.clientGlowByte, g_status.weight,
                     g_status.glowDelivered ? "" : ", glow composite not current", g_status.gradingStrength);
    else
        VF_LOG_DEBUG("Forever look: glow byte %u (client %u, Classic weight %.2f%s), colour grading %s",
                     g_status.deliveredGlowByte, g_status.clientGlowByte, g_status.weight,
                     g_status.glowDelivered ? "" : ", glow composite not current", g_status.grading);
}

bool ReadFrame(ForeverLookFrame& frame)
{
    __try
    {
        frame = g_client.frame(GlobalConfig().Get());
        return true;
    }
    __except (ForeverLookFilter(GetExceptionCode(), "the Forever look inputs"))
    {
        g_glowFailed = true;
        g_gradingFailed = true;
        return false;
    }
}

void FailGlow()
{
    g_glowFailed = true;
    __try
    {
        RestoreGlow();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_glow = GlowFrame();
    }
}
}

float ForeverLookWeight(float coverage)
{
    if (!(coverage > kMinimumClassicCoverage))
        return 0.0f;
    return std::min((coverage - kMinimumClassicCoverage) / (1.0f - kMinimumClassicCoverage), 1.0f);
}

uint8_t ForeverGlowByte(uint8_t clientByte, float foreverGlow, float weight)
{
    const float client = clientByte / kGlowByteScale;
    const float blended = client + (foreverGlow - client) * std::clamp(weight, 0.0f, 1.0f);
    if (!(blended > 0.0f))
        return 0;
    return static_cast<uint8_t>(std::floor(std::min(blended, 1.0f) * kGlowByteScale + 0.5f));
}

void EnableForeverLook(FogDevice* (*device)(), bool glowPassColour, bool gradingPlacement)
{
    g_client = {device, &GameForeverLookFrame};
    g_glowAvailable = glowPassColour;
    g_gradingAvailable = gradingPlacement;
    VF_LOG_INFO("Forever glow %s, colour grading %s", glowPassColour ? "available" : "unavailable",
                gradingPlacement ? "available" : "unavailable");
}

void UseTestForeverLookFrame(const ForeverLookFrame& frame)
{
    g_testFrame = frame;
    g_client = {&LatestFogDevice, &TestForeverLookFrame};
    g_glowAvailable = true;
    g_gradingAvailable = true;
    g_glowFailed = false;
    g_gradingFailed = false;
}

void ForeverLookAtWorldDone()
{
    ForeverLookFrame frame;
    if (!ReadFrame(frame))
    {
        FailGlow();
        g_grading = GradingFrame();
        return;
    }
    const Config& cfg = GlobalConfig().Get();
    __try
    {
        FeedGlow(frame, cfg);
        RecordGlowStatus();
        LogGlowOverride();
    }
    __except (ForeverLookFilter(GetExceptionCode(), "the Forever glow"))
    {
        FailGlow();
    }
    __try
    {
        g_grading = PrepareGrading(frame, cfg);
    }
    __except (ForeverLookFilter(GetExceptionCode(), "colour grading"))
    {
        g_gradingFailed = true;
        g_grading = GradingFrame();
    }
}

void ForeverLookAtFrameEnd()
{
    __try
    {
        RestoreGlow();
    }
    __except (ForeverLookFilter(GetExceptionCode(), "the Forever glow restore"))
    {
        g_glowFailed = true;
        g_glow = GlowFrame();
    }
    __try
    {
        GradeWorldFrame();
    }
    __except (ForeverLookFilter(GetExceptionCode(), "colour grading"))
    {
        g_gradingFailed = true;
        g_grading = GradingFrame();
    }
    LogDebugSummary();
}

bool DeliveredGlowThisFrame(float& amount)
{
    if (!g_glow.delivered)
        return false;
    amount = g_glow.deliveredByte / kGlowByteScale;
    return true;
}

ForeverLookStatus LastForeverLookStatus()
{
    return g_status;
}
