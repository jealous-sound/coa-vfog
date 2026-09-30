#pragma once

#include <cstddef>

namespace forever_look_checks
{
constexpr uintptr_t kGlowEffectVtableValue = 0x00A941C8;
constexpr uintptr_t kGlowFirstPassVtableValue = 0x00A94274;
constexpr uintptr_t kGlowGauss4PassVtableValue = 0x00A9425C;
constexpr uintptr_t kGlowCompositePassVtableValue = 0x00A94294;
constexpr uintptr_t kDeathEffectVtableValue = 0x00A418D8;
constexpr uintptr_t kWrongVtableValue = 0x00A94000;
constexpr uint32_t kPassListCapacity = 4;
constexpr uint32_t kGlowPasses = 3;
constexpr uint32_t kTooFewPasses = 2;
constexpr uintptr_t kGlowPassVtableValues[kGlowPasses] = {kGlowFirstPassVtableValue, kGlowGauss4PassVtableValue,
                                                          kGlowCompositePassVtableValue};
constexpr int kCompositePass = 2;
constexpr int kClearViewList = 0;
constexpr int kUnderwaterList = 1;
constexpr int kPassLists = 2;
constexpr int kColourAlpha = 3;
constexpr uint8_t kBlurByte = 40;
constexpr uint8_t kOtherPassAlpha = 200;
constexpr uint8_t kClientGlowByte = 102;
constexpr uint8_t kWrappedClientGlowByte = 24;
constexpr uint8_t kForeignGlowByte = 77;
constexpr uint8_t kUntouchedGlowByte = 90;
constexpr uint8_t kHalfWayFromClientToZero = 51;
constexpr uint8_t kSaturatedGlowByte = 255;
constexpr float kGlowByteScale = 255.0f;
constexpr float kFullCoverage = 1.0f;
constexpr float kThreeQuarterCoverage = 0.75f;
constexpr float kHalfCoverage = 0.5f;
constexpr float kForeverContinentGlow = 0.0f;
constexpr float kAboveOneGlow = 1.3f;
constexpr int kMaxGuardRanges = 64;
constexpr size_t kCallDisplacementOffset = 1;
constexpr size_t kCallDisplacementSize = 4;
constexpr uintptr_t kCallRel32Bytes = 5;
constexpr int kCodes = 256;
constexpr int kLastCode = kCodes - 1;
constexpr int kBlueCodeStride = 37;
constexpr UINT kStripeSampleRow = 300;
constexpr UINT kStandInGlowTop = 620;
constexpr UINT kStandInGlowBottom = 680;
constexpr UINT kStandInGlowSampleRow = 650;
constexpr int kStandInGlowCode = 128;
constexpr float kUnormConversionTolerance = 0.6f;
constexpr float kDeliveredGlowTolerance = 1e-6f;
constexpr float kClampedDayNightGlow = 1.0f;
constexpr float kPrimedGlowCompensation = 0.5f;
constexpr float kHalfStrength = 0.5f;
constexpr float kSyntheticCurveGamma = 0.8f;
constexpr D3DVIEWPORT9 kGradedSubRect = {100, 50, 800, 400, 0.0f, 0.94f};
constexpr const char* kNoGradedLightReason = "no Classic light around the camera carries a grading curve";

struct FakeGlowPass
{
    uintptr_t vtable;
    unsigned char state[0x2C];
    uint8_t colour[4];
};

struct FakePassList
{
    uint32_t capacity;
    uint32_t count;
    uintptr_t data;
};

struct FakeGlowEffect
{
    uintptr_t vtable;
    uintptr_t enableCVar;
    FakePassList clearView;
    uint32_t unknown14;
    uint32_t unknown18;
    FakePassList underwater;
    uint32_t unknown28;
    uint32_t underwaterFlag;
};

static_assert(offsetof(FakeGlowPass, colour) == 0x30, "the composite pass colour is at +0x30");
static_assert(offsetof(FakePassList, count) == 4 && offsetof(FakePassList, data) == 8, "pass list layout");
static_assert(offsetof(FakeGlowEffect, clearView) == 0x08 && offsetof(FakeGlowEffect, underwater) == 0x1C,
              "the glow effect's pass lists are at +0x08 and +0x1C");

class FakeGlowGraph
{
public:
    explicit FakeGlowGraph(uint8_t clientByte)
    {
        for (int list = 0; list < kPassLists; ++list)
            for (uint32_t pass = 0; pass < kGlowPasses; ++pass)
            {
                FakeGlowPass& p = m_passes[list][pass];
                const bool composite = pass == kCompositePass;
                p.vtable = kGlowPassVtableValues[pass];
                p.colour[0] = p.colour[1] = p.colour[2] = kBlurByte;
                p.colour[kColourAlpha] = composite ? clientByte : kOtherPassAlpha;
                m_passPointers[list][pass] = reinterpret_cast<uintptr_t>(&p);
            }
        m_glow.vtable = kGlowEffectVtableValue;
        m_glow.clearView = {kPassListCapacity, kGlowPasses, reinterpret_cast<uintptr_t>(m_passPointers[0])};
        m_glow.underwater = {kPassListCapacity, kGlowPasses, reinterpret_cast<uintptr_t>(m_passPointers[1])};
        m_death.vtable = kDeathEffectVtableValue;
    }
    FakeGlowGraph(const FakeGlowGraph&) = delete;
    FakeGlowGraph& operator=(const FakeGlowGraph&) = delete;

    uint8_t GlowByte(int list) const { return m_passes[list][kCompositePass].colour[kColourAlpha]; }
    void SetGlowByte(int list, uint8_t value) { m_passes[list][kCompositePass].colour[kColourAlpha] = value; }
    bool BothLists(uint8_t value) const
    {
        return GlowByte(kClearViewList) == value && GlowByte(kUnderwaterList) == value;
    }

    bool OtherColoursUntouched() const
    {
        for (int list = 0; list < kPassLists; ++list)
            for (uint32_t pass = 0; pass < kGlowPasses; ++pass)
            {
                const uint8_t* colour = m_passes[list][pass].colour;
                if (colour[0] != kBlurByte || colour[1] != kBlurByte || colour[2] != kBlurByte)
                    return false;
                if (pass != kCompositePass && colour[kColourAlpha] != kOtherPassAlpha)
                    return false;
            }
        return true;
    }

    engine::ScreenEffects GlowCurrent() const
    {
        engine::ScreenEffects effects;
        effects.current = reinterpret_cast<uintptr_t>(&m_glow);
        effects.glow = effects.current;
        effects.death = reinterpret_cast<uintptr_t>(&m_death);
        return effects;
    }

    engine::ScreenEffects DeathCurrent() const
    {
        engine::ScreenEffects effects = GlowCurrent();
        effects.current = effects.death;
        return effects;
    }

    FakeGlowEffect& Glow() { return m_glow; }
    FakeGlowPass& Composite(int list) { return m_passes[list][kCompositePass]; }
    void DropComposite(int list) { m_passPointers[list][kCompositePass] = 0; }

private:
    FakeGlowPass m_passes[kPassLists][kGlowPasses] = {};
    uintptr_t m_passPointers[kPassLists][kGlowPasses] = {};
    FakeGlowEffect m_glow = {};
    FakeGlowEffect m_death = {};
};

void IdentityCurve(float* curve)
{
    for (int i = 0; i < kGradingCurveEntries; ++i)
        curve[i] = static_cast<float>(i) / (kGradingCurveEntries - 1);
}

void SyntheticCurve(float* curve)
{
    for (int i = 0; i < kGradingCurveEntries; ++i)
        curve[i] = std::pow(static_cast<float>(i) / (kGradingCurveEntries - 1), kSyntheticCurveGamma);
}

ForeverLook LookAt(float coverage, float foreverGlow, const float* curve)
{
    ForeverLook look;
    look.valid = true;
    look.coverage = coverage;
    look.hasGlow = true;
    look.glow = foreverGlow;
    look.hasGradingCurve = true;
    std::memcpy(look.gradingCurve, curve, sizeof(look.gradingCurve));
    return look;
}

ForeverLookFrame GlowFrame(const FakeGlowGraph& graph, float coverage, float foreverGlow)
{
    float identity[kGradingCurveEntries];
    IdentityCurve(identity);
    ForeverLookFrame frame;
    frame.effects = graph.GlowCurrent();
    frame.look = LookAt(coverage, foreverGlow, identity);
    return frame;
}

void UseNoForeverLook()
{
    const ForeverLookFrame none = {};
    vf_test_use_forever_look_frame(&none);
}

void UseForeverGlow(bool on)
{
    Config cfg = {};
    cfg.foreverGlow = on ? 1 : 0;
    vf_test_set_config(&cfg);
}

struct GlowRun
{
    uint8_t duringFrame[kPassLists] = {};
    uint8_t afterFrame[kPassLists] = {};
    bool delivered = false;
    float deliveredAmount = -1.0f;

    bool WroteBoth(uint8_t value) const
    {
        return duringFrame[kClearViewList] == value && duringFrame[kUnderwaterList] == value;
    }
    bool RestoredBoth(uint8_t value) const
    {
        return afterFrame[kClearViewList] == value && afterFrame[kUnderwaterList] == value;
    }
};

GlowRun RunGlowFrame(FakeGlowGraph& graph, const ForeverLookFrame& frame, bool foreverGlow)
{
    UseForeverGlow(foreverGlow);
    vf_test_use_forever_look_frame(&frame);
    vf_test_forever_look_world_done();
    GlowRun run;
    for (int list = 0; list < kPassLists; ++list)
        run.duringFrame[list] = graph.GlowByte(list);
    run.delivered = vf_test_delivered_glow(&run.deliveredAmount) != 0;
    vf_test_hook_frame_end();
    for (int list = 0; list < kPassLists; ++list)
        run.afterFrame[list] = graph.GlowByte(list);
    return run;
}

void PrintGlowRun(const char* what, const GlowRun& run)
{
    std::printf("     %s: glow bytes %u/%u during the frame, %u/%u after it; delivered %s %.4f\n", what,
                run.duringFrame[0], run.duringFrame[1], run.afterFrame[0], run.afterFrame[1],
                run.delivered ? "yes" : "no", run.deliveredAmount);
}

void CheckForeverGlowWritesAndRestores()
{
    FakeGlowGraph graph(kClientGlowByte);
    const GlowRun run = RunGlowFrame(graph, GlowFrame(graph, kFullCoverage, kForeverContinentGlow), true);
    PrintGlowRun("ForeverGlow=1, Forever glow 0, full coverage", run);
    Check(run.WroteBoth(0) && graph.OtherColoursUntouched(),
          "ForeverGlow=1 under full Classic coverage writes the Forever glow as the alpha of the glow composite's "
          "colour in both pass lists and leaves every other colour byte alone");
    Check(run.RestoredBoth(kClientGlowByte), "the frame end gives both pass lists the client's glow byte back");
    Check(run.delivered && run.deliveredAmount == 0.0f,
          "GlowCompensation receives the byte the glow composite gets (0), not the client's DayNight glow");
}

void CheckForeignGlowByteSurvives()
{
    FakeGlowGraph graph(kClientGlowByte);
    UseForeverGlow(true);
    const ForeverLookFrame frame = GlowFrame(graph, kFullCoverage, kForeverContinentGlow);
    vf_test_use_forever_look_frame(&frame);
    vf_test_forever_look_world_done();
    graph.SetGlowByte(kUnderwaterList, kForeignGlowByte);
    vf_test_hook_frame_end();
    std::printf("     after a foreign write to the underwater list: glow bytes %u/%u\n",
                graph.GlowByte(kClearViewList), graph.GlowByte(kUnderwaterList));
    Check(graph.GlowByte(kClearViewList) == kClientGlowByte && graph.GlowByte(kUnderwaterList) == kForeignGlowByte,
          "the frame end restores only bytes that still hold the DLL's value; a byte written by someone else survives");
}

struct BrokenGraph
{
    const char* what;
    void (*breakGraph)(FakeGlowGraph& graph, ForeverLookFrame& frame);
};

const BrokenGraph kBrokenGraphs[] = {
    {"the death effect is current",
     [](FakeGlowGraph& graph, ForeverLookFrame& frame) { frame.effects = graph.DeathCurrent(); }},
    {"no screen effect is current", [](FakeGlowGraph&, ForeverLookFrame& frame) { frame.effects.current = 0; }},
    {"the glow effect's vtable differs",
     [](FakeGlowGraph& graph, ForeverLookFrame&) { graph.Glow().vtable = kWrongVtableValue; }},
    {"the underwater composite's vtable differs",
     [](FakeGlowGraph& graph, ForeverLookFrame&) { graph.Composite(kUnderwaterList).vtable = kWrongVtableValue; }},
    {"the clear-view list holds two passes",
     [](FakeGlowGraph& graph, ForeverLookFrame&) { graph.Glow().clearView.count = kTooFewPasses; }},
    {"the underwater list has no pass array",
     [](FakeGlowGraph& graph, ForeverLookFrame&) { graph.Glow().underwater.data = 0; }},
    {"the underwater list holds a null composite",
     [](FakeGlowGraph& graph, ForeverLookFrame&) { graph.DropComposite(kUnderwaterList); }},
};

void CheckNoWriteWithoutTheGlowComposite()
{
    bool untouched = true;
    for (const BrokenGraph& broken : kBrokenGraphs)
    {
        FakeGlowGraph graph(kClientGlowByte);
        ForeverLookFrame frame = GlowFrame(graph, kFullCoverage, kForeverContinentGlow);
        broken.breakGraph(graph, frame);
        const GlowRun run = RunGlowFrame(graph, frame, true);
        const bool left = run.WroteBoth(kClientGlowByte) && run.RestoredBoth(kClientGlowByte) && !run.delivered;
        if (!left)
            PrintGlowRun(broken.what, run);
        untouched = untouched && left;
    }
    Check(untouched,
          "no glow byte is written while the death effect or no effect is current, or when the glow effect's "
          "vtable, a composite pass vtable, a pass count, a pass array or a composite entry does not match the "
          "client's");
}

struct CoverageCase
{
    float coverage;
    float foreverGlow;
    bool valid;
    bool hasGlow;
    uint8_t expected;
};

void CheckGlowFadesWithClassicCoverage()
{
    const CoverageCase cases[] = {
        {kFullCoverage, kForeverContinentGlow, true, true, 0},
        {kThreeQuarterCoverage, kForeverContinentGlow, true, true, kHalfWayFromClientToZero},
        {kHalfCoverage, kForeverContinentGlow, true, true, kClientGlowByte},
        {kFullCoverage, kForeverContinentGlow, false, true, kClientGlowByte},
        {kFullCoverage, kForeverContinentGlow, true, false, kClientGlowByte},
        {kFullCoverage, kAboveOneGlow, true, true, kSaturatedGlowByte},
    };
    bool faded = true;
    for (const CoverageCase& c : cases)
    {
        FakeGlowGraph graph(kClientGlowByte);
        ForeverLookFrame frame = GlowFrame(graph, c.coverage, c.foreverGlow);
        frame.look.valid = c.valid;
        frame.look.hasGlow = c.hasGlow;
        const GlowRun run = RunGlowFrame(graph, frame, true);
        std::printf("     coverage %.2f, Forever glow %.2f%s%s: glow byte %u (expected %u), compensation %.4f\n",
                    c.coverage, c.foreverGlow, c.valid ? "" : ", no Classic inputs",
                    c.hasGlow ? "" : ", no Classic glow", run.duringFrame[0], c.expected, run.deliveredAmount);
        faded = faded && run.WroteBoth(c.expected) && run.RestoredBoth(kClientGlowByte) && run.delivered &&
                std::fabs(run.deliveredAmount - c.expected / kGlowByteScale) < kDeliveredGlowTolerance;
    }
    Check(faded,
          "the glow byte blends from the client's to Forever's as Classic coverage rises from 0.5 to 1, keeps the "
          "client's without Classic inputs or glow, and clamps a Forever glow above 1 to 255 instead of wrapping");
}

void CheckCompensationTakesTheDeliveredByte()
{
    FakeGlowGraph graph(kWrappedClientGlowByte);
    const GlowRun run = RunGlowFrame(graph, GlowFrame(graph, kFullCoverage, kForeverContinentGlow), false);
    PrintGlowRun("ForeverGlow=0 with a client glow of 1.1 (byte 24)", run);
    Check(run.WroteBoth(kWrappedClientGlowByte) && run.RestoredBoth(kWrappedClientGlowByte) && run.delivered &&
              std::fabs(run.deliveredAmount - kWrappedClientGlowByte / kGlowByteScale) < kDeliveredGlowTolerance,
          "with ForeverGlow=0 the glow byte is left alone and GlowCompensation takes the delivered byte / 255 (24 "
          "when a client glow of 1.1 wraps), not the clamped DayNight glow 1.0");
}

bool Delivered()
{
    float amount = -1.0f;
    return vf_test_delivered_glow(&amount) != 0;
}

void CheckFrameEndWithoutWorldDone()
{
    FakeGlowGraph graph(kUntouchedGlowByte);
    UseForeverGlow(true);
    const ForeverLookFrame frame = GlowFrame(graph, kFullCoverage, kForeverContinentGlow);
    vf_test_use_forever_look_frame(&frame);
    vf_test_hook_frame_end();
    const bool untouchedWithoutWorldDone = graph.BothLists(kUntouchedGlowByte);
    const bool deliveredWithoutWorldDone = Delivered();
    vf_test_forever_look_world_done();
    const bool fedAtWorldDone = graph.BothLists(0);
    vf_test_hook_frame_end();
    vf_test_hook_frame_end();
    std::printf("     frame end alone: glow bytes %s; after world done, frame end and another frame end: %u/%u\n",
                untouchedWithoutWorldDone ? "untouched" : "written", graph.GlowByte(kClearViewList),
                graph.GlowByte(kUnderwaterList));
    Check(untouchedWithoutWorldDone && !deliveredWithoutWorldDone && fedAtWorldDone &&
              graph.BothLists(kUntouchedGlowByte) && !Delivered(),
          "a frame end without a world done writes no glow byte into a graph the DLL could feed and reports no "
          "delivered glow, and a second frame end after a fed frame changes nothing");
}

struct PatchedRange
{
    uintptr_t address;
    size_t size;
};

bool Overlaps(const engine::CodeRange& guard, const PatchedRange& patched)
{
    return guard.address < patched.address + patched.size && patched.address < guard.address + guard.size;
}

void CheckGuardsCompareOnlyUnpatchedBytes()
{
    engine::CodeRange guards[kMaxGuardRanges] = {};
    const int count = vf_test_forever_look_guards(guards, kMaxGuardRanges);
    const uintptr_t callSites[] = {engine::kWorldRenderSite,    engine::kOpaqueM2PassSite,  engine::kLiquidSurfaceSite,
                                   engine::kWorldTextDrawSite,  engine::kScreenEffectsSite, engine::kWaterPassSite,
                                   engine::kFarClipCVarSetSite, engine::kFarClipMapLoadSite, engine::kM2BatchFogSite,
                                   engine::kGlarePassSite};
    const uintptr_t pointerSlots[] = {engine::kGetProcAddressSlot, engine::kWaterMaterialRenderSlot,
                                      engine::kWaterNoSpecMaterialRenderSlot};
    std::vector<PatchedRange> patched;
    for (uintptr_t site : callSites)
        patched.push_back({site + kCallDisplacementOffset, kCallDisplacementSize});
    for (uintptr_t slot : pointerSlots)
        patched.push_back({slot, sizeof(uintptr_t)});
    bool disjoint = count > 0 && count <= kMaxGuardRanges;
    bool tailGuarded = false;
    for (int i = 0; i < count && i < kMaxGuardRanges; ++i)
    {
        for (const PatchedRange& p : patched)
            if (Overlaps(guards[i], p))
            {
                std::printf("     guard 0x%08X+%u overlaps the patched bytes at 0x%08X\n",
                            static_cast<unsigned>(guards[i].address), static_cast<unsigned>(guards[i].size),
                            static_cast<unsigned>(p.address));
                disjoint = false;
            }
        tailGuarded = tailGuarded || guards[i].address == engine::kScreenEffectsSite + kCallRel32Bytes;
    }
    std::printf("     %d Forever look guard ranges\n", count);
    Check(disjoint && tailGuarded,
          "the Forever glow and grading guards compare only bytes that no hook patches, starting right after the "
          "patched FFX end call, so they still match once the engine hooks are installed");
}

bool ListsOnce(const std::vector<std::string>& lines, const char* line)
{
    return std::count(lines.begin(), lines.end(), std::string(line)) == 1;
}

void CheckLookSettings(const std::wstring& outDir, const std::wstring& shippedIni)
{
    ConfigStore shipped;
    shipped.Load(NarrowPath(shippedIni));
    const std::string text = ReadText(shippedIni);
    const std::vector<std::string> lines = water_settings_checks::IniLines(text);
    Check(shipped.Get().foreverGlow == 0 && shipped.Get().colorGrading == 0.0f && ListsOnce(lines, "ForeverGlow=0") &&
              ListsOnce(lines, "ColorGrading=0") &&
              text.find("; 1 = the modern client's glow amount") != std::string::npos &&
              text.find("; Strength of the modern client's colour curve") != std::string::npos,
          "the shipped INI keeps ForeverGlow and ColorGrading off and documents both");

    const std::wstring lookIni = FullPath(outDir + L"\\look.ini");
    CopyFileW(shippedIni.c_str(), lookIni.c_str(), FALSE);
    WritePrivateProfileStringW(L"CoAVolFog", L"ForeverGlow", L"3", lookIni.c_str());
    WritePrivateProfileStringW(L"CoAVolFog", L"ColorGrading", L"2.5", lookIni.c_str());
    ConfigStore store;
    store.Load(NarrowPath(lookIni));
    Check(store.Get().foreverGlow == 1 && store.Get().colorGrading == 1.0f,
          "ForeverGlow and ColorGrading are clamped to 0..1 when read");

    Config edited = store.Get();
    edited.foreverGlow = 0;
    edited.colorGrading = 0.35f;
    const std::string changes = SettingChanges(store.Get(), edited);
    store.Apply(edited);
    const bool saved = store.Save();
    ConfigStore reloaded;
    reloaded.Load(NarrowPath(lookIni));
    const std::vector<std::string> savedLines = water_settings_checks::IniLines(ReadText(lookIni));
    std::printf("     the window's change logs \"%s\"\n", changes.c_str());
    Check(saved && changes == "ForeverGlow 1 -> 0, ColorGrading 1 -> 0.35" && reloaded.Get().foreverGlow == 0 &&
              reloaded.Get().colorGrading == 0.35f && ListsOnce(savedLines, "ForeverGlow=0") &&
              ListsOnce(savedLines, "ColorGrading=0.35") && SameFogSettings(shipped.Get(), reloaded.Get()),
          "ForeverGlow and ColorGrading changes are logged, saved and reloaded, and leave the fog history alone");
}

void CheckForeverGlow()
{
    CheckForeverGlowWritesAndRestores();
    CheckForeignGlowByteSurvives();
    CheckNoWriteWithoutTheGlowComposite();
    CheckGlowFadesWithClassicCoverage();
    CheckCompensationTakesTheDeliveredByte();
    CheckFrameEndWithoutWorldDone();
    CheckGuardsCompareOnlyUnpatchedBytes();
    UseNoForeverLook();
    Config defaults = {};
    vf_test_set_config(&defaults);
}

int StripeCode(int code, int channel)
{
    if (channel == 0)
        return code;
    if (channel == 1)
        return kLastCode - code;
    return (code * kBlueCodeStride) & kLastCode;
}

D3DCOLOR StripeColour(int code)
{
    return D3DCOLOR_XRGB(StripeCode(code, 0), StripeCode(code, 1), StripeCode(code, 2));
}

UINT StripeWidth(const D3DVIEWPORT9& area)
{
    return area.Width / kCodes;
}

void ClearRect(IDirect3DDevice9* dev, LONG x0, LONG y0, LONG x1, LONG y1, D3DCOLOR colour)
{
    const D3DRECT rect = {x0, y0, x1, y1};
    dev->Clear(1, &rect, D3DCLEAR_TARGET, colour, 1.0f, 0);
}

void FillStripes(IDirect3DDevice9* dev, const D3DVIEWPORT9& area)
{
    const UINT width = StripeWidth(area);
    for (int code = 0; code < kCodes; ++code)
    {
        const LONG x = static_cast<LONG>(area.X + code * width);
        ClearRect(dev, x, static_cast<LONG>(area.Y), x + static_cast<LONG>(width),
                  static_cast<LONG>(area.Y + area.Height), StripeColour(code));
    }
}

void PrepareClearableTarget(IDirect3DDevice9* dev)
{
    IDirect3DSurface9* target = nullptr;
    D3DSURFACE_DESC desc = {};
    dev->GetRenderTarget(0, &target);
    target->GetDesc(&desc);
    target->Release();
    const D3DVIEWPORT9 full = {0, 0, desc.Width, desc.Height, 0.0f, 1.0f};
    dev->SetViewport(&full);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
}

float ReferenceGraded(int code, const float* curve, float strength)
{
    const float x = code / kGlowByteScale;
    const float position = x * (kGradingCurveEntries - 1);
    const int lower = static_cast<int>(std::floor(position));
    const int upper = std::min(lower + 1, kGradingCurveEntries - 1);
    const float graded = curve[lower] + (curve[upper] - curve[lower]) * (position - lower);
    return kGlowByteScale * (x + (graded - x) * strength);
}

struct CurveMatch
{
    int compared = 0;
    int nearest = 0;
    float largestError = 0.0f;

    void Add(int got, float expected)
    {
        ++compared;
        nearest += got == static_cast<int>(std::lround(expected)) ? 1 : 0;
        largestError = std::max(largestError, std::fabs(static_cast<float>(got) - expected));
    }
    bool WithinConversion() const { return compared > 0 && largestError <= kUnormConversionTolerance; }
};

int ChannelOf(const unsigned char* bgra, int channel)
{
    return bgra[2 - channel];
}

CurveMatch CompareStripes(const Image& image, const D3DVIEWPORT9& area, UINT row, const float* curve, float strength)
{
    CurveMatch match;
    const UINT width = StripeWidth(area);
    for (int code = 0; code < kCodes; ++code)
    {
        const unsigned char* pixel = image.At(area.X + code * width + width / 2, row);
        for (int channel = 0; channel < 3; ++channel)
            match.Add(ChannelOf(pixel, channel), ReferenceGraded(StripeCode(code, channel), curve, strength));
    }
    return match;
}

CurveMatch CompareStandInGlow(const Image& image, const D3DVIEWPORT9& area, const float* curve, float strength)
{
    CurveMatch match;
    const unsigned char* pixel = image.At(area.X + area.Width / 2, kStandInGlowSampleRow);
    for (int channel = 0; channel < 3; ++channel)
        match.Add(ChannelOf(pixel, channel), ReferenceGraded(kStandInGlowCode, curve, strength));
    return match;
}

void PrintCurveMatch(const char* what, const CurveMatch& m)
{
    std::printf("     %s: %d of %d channel codes round to the nearest, largest error %.3f LSB (D3D allows %.1f)\n",
                what, m.nearest, m.compared, m.largestError, kUnormConversionTolerance);
}

struct GradedFrame
{
    Image image;
    bool statesKept = false;
    GradingStats before;
    GradingStats after;
    ForeverLookStatus status;

    unsigned Draws() const { return after.grades - before.grades; }
};

struct GradedFrameRequest
{
    D3DVIEWPORT9 filled;
    D3DVIEWPORT9 world;
    ForeverLookFrame frame;
    float colorGrading = 1.0f;
    bool worldDone = true;
    bool throughTheWorldDoneHook = false;
    bool standInGlow = false;
};

ForeverLookFrame GradingFrame(float coverage, const float* curve)
{
    ForeverLookFrame frame;
    frame.look = LookAt(coverage, kForeverContinentGlow, curve);
    return frame;
}

GradedFrameRequest Request(const D3DVIEWPORT9& world, float colorGrading, const float* curve)
{
    GradedFrameRequest request;
    request.filled = world;
    request.world = world;
    request.frame = GradingFrame(kFullCoverage, curve);
    request.colorGrading = colorGrading;
    return request;
}

struct HookedCamera
{
    Vec3 eye;
    Vec3 at;
    float view[16];
    float projection[16];
};

HookedCamera CameraOver(const D3DVIEWPORT9& world)
{
    HookedCamera camera = {Add({0, 0, 9}, kGameLikeWorldOffset), Add({100, 2, 4}, kGameLikeWorldOffset)};
    CameraRelativeLookAt(camera.eye, camera.at, camera.view);
    EngineProjection(static_cast<float>(world.Width) / world.Height, camera.projection);
    return camera;
}

FrameInputs HookedInputs(const HookedCamera& camera, const D3DVIEWPORT9& world)
{
    return MakeInputs(camera.view, camera.projection, camera.eye, camera.at, world);
}

void CaptureWorldViewport(IDirect3DDevice9* dev, const D3DVIEWPORT9& world)
{
    const FrameInputs inputs = HookedInputs(CameraOver(world), world);
    vf_test_use_world_hook_client(&inputs, 1);
    D3DVIEWPORT9 current = {};
    dev->GetViewport(&current);
    dev->SetViewport(&world);
    vf_test_hook_opaque_done();
    dev->SetViewport(&current);
}

void RunWorldDone(bool throughTheHook)
{
    if (throughTheHook)
        vf_test_hook_world_done();
    else
        vf_test_forever_look_world_done();
}

GradedFrame RenderGradedFrame(Harness& h, const GradedFrameRequest& request)
{
    Config cfg = {};
    cfg.colorGrading = request.colorGrading;
    vf_test_set_config(&cfg);
    vf_test_use_forever_look_frame(&request.frame);
    GradedFrame result;
    h.BeginFrame();
    PrepareClearableTarget(h.dev);
    FillStripes(h.dev, request.filled);
    CaptureWorldViewport(h.dev, request.world);
    vf_test_grading_stats(&result.before);
    if (request.worldDone)
        RunWorldDone(request.throughTheWorldDoneHook);
    if (request.standInGlow)
        ClearRect(h.dev, static_cast<LONG>(request.filled.X), kStandInGlowTop,
                  static_cast<LONG>(request.filled.X + request.filled.Width), kStandInGlowBottom,
                  D3DCOLOR_XRGB(kStandInGlowCode, kStandInGlowCode, kStandInGlowCode));
    h.SetEngineState(request.filled);
    Sentinel s0;
    ReadSentinel(h.dev, s0);
    vf_test_hook_frame_end();
    Sentinel s1;
    ReadSentinel(h.dev, s1);
    result.statesKept = SameSentinel(s0, s1);
    if (!result.statesKept)
        ReportSentinelDifferences(s0, s1);
    ReleaseSentinel(s0);
    ReleaseSentinel(s1);
    result.image = Capture(h.dev);
    h.dev->EndScene();
    vf_test_grading_stats(&result.after);
    vf_test_forever_look_status(&result.status);
    return result;
}

bool StripesUngraded(const Image& image, const D3DVIEWPORT9& area, UINT row)
{
    float identity[kGradingCurveEntries];
    IdentityCurve(identity);
    return CompareStripes(image, area, row, identity, 0.0f).WithinConversion();
}

void CheckIdentityAndShippedCurves(Harness& h, const D3DVIEWPORT9& world, const float* shipped)
{
    float identity[kGradingCurveEntries];
    IdentityCurve(identity);
    const GradedFrame identityFrame = RenderGradedFrame(h, Request(world, 1.0f, identity));
    const CurveMatch identityMatch = CompareStripes(identityFrame.image, world, kStripeSampleRow, identity, 1.0f);
    PrintCurveMatch("identity curve", identityMatch);
    Check(identityFrame.Draws() == 1 && identityMatch.nearest == identityMatch.compared,
          "grading with the identity curve returns every 8-bit code of R, G and B unchanged");

    GradedFrameRequest shippedRequest = Request(world, 1.0f, shipped);
    shippedRequest.standInGlow = true;
    const GradedFrame full = RenderGradedFrame(h, shippedRequest);
    const CurveMatch fullMatch = CompareStripes(full.image, world, kStripeSampleRow, shipped, 1.0f);
    PrintCurveMatch("Stormwind noon curve (8286666) at strength 1", fullMatch);
    Check(full.Draws() == 1 && fullMatch.WithinConversion(),
          "the Stormwind noon curve is applied per channel with the LUT's linear interpolation, within D3D's float "
          "to 8-bit conversion tolerance");
    const CurveMatch standIn = CompareStandInGlow(full.image, world, shipped, 1.0f);
    Check(standIn.WithinConversion(),
          "grading reads the frame at the frame end: what the glow drew after the world is done is graded");
    Check(full.statesKept,
          "grading restores render, sampler, texture, shader, pixel constant c0/c1, stream, viewport, scissor, "
          "target and depth state");

    shippedRequest.colorGrading = kHalfStrength;
    const GradedFrame half = RenderGradedFrame(h, shippedRequest);
    const CurveMatch halfMatch = CompareStripes(half.image, world, kStripeSampleRow, shipped, kHalfStrength);
    PrintCurveMatch("Stormwind noon curve at strength 0.5", halfMatch);
    GradedFrameRequest fadedRequest = Request(world, 1.0f, shipped);
    fadedRequest.frame.look.coverage = kThreeQuarterCoverage;
    const GradedFrame faded = RenderGradedFrame(h, fadedRequest);
    const CurveMatch fadedMatch = CompareStripes(faded.image, world, kStripeSampleRow, shipped, kHalfStrength);
    Check(halfMatch.WithinConversion() && fadedMatch.WithinConversion(),
          "ColorGrading=0.5 blends halfway to the curve, as ColorGrading=1 does at 0.75 Classic coverage");
}

void CheckUngradedFrames(Harness& h, const D3DVIEWPORT9& world, const float* shipped)
{
    const GradedFrame off = RenderGradedFrame(h, Request(world, 0.0f, shipped));
    Check(off.Draws() == 0 && StripesUngraded(off.image, world, kStripeSampleRow) && !off.after.holdsSceneCopy,
          "ColorGrading=0 leaves the frame bit-exact, draws nothing and releases the grading copy");

    GradedFrameRequest noWorld = Request(world, 1.0f, shipped);
    noWorld.worldDone = false;
    const GradedFrame withoutWorld = RenderGradedFrame(h, noWorld);
    Check(withoutWorld.Draws() == 0 && StripesUngraded(withoutWorld.image, world, kStripeSampleRow),
          "a frame end without a world done this frame grades nothing");

    int deathEffect = 0;
    GradedFrameRequest ghost = Request(world, 1.0f, shipped);
    ghost.frame.effects.death = reinterpret_cast<uintptr_t>(&deathEffect);
    ghost.frame.effects.current = ghost.frame.effects.death;
    const GradedFrame ghostFrame = RenderGradedFrame(h, ghost);
    GradedFrameRequest diving = Request(world, 1.0f, shipped);
    diving.frame.cameraInLiquid = true;
    const GradedFrame divingFrame = RenderGradedFrame(h, diving);
    std::printf("     grading states: ghost \"%s\", under water \"%s\"\n", ghostFrame.status.grading,
                divingFrame.status.grading);
    Check(ghostFrame.Draws() == 0 && StripesUngraded(ghostFrame.image, world, kStripeSampleRow) &&
              std::strcmp(ghostFrame.status.grading, "ghost effect") == 0,
          "the ghost (death) effect's desaturated view is not graded");
    Check(divingFrame.Draws() == 0 && StripesUngraded(divingFrame.image, world, kStripeSampleRow) &&
              std::strcmp(divingFrame.status.grading, "camera under water") == 0,
          "the view from under water is not graded");

    GradedFrameRequest edge = Request(world, 1.0f, shipped);
    edge.frame.look.coverage = kHalfCoverage;
    const GradedFrame edgeFrame = RenderGradedFrame(h, edge);
    Check(edgeFrame.Draws() == 0 && StripesUngraded(edgeFrame.image, world, kStripeSampleRow),
          "at half Classic coverage the grading has faded out and draws nothing");

    float identity[kGradingCurveEntries];
    IdentityCurve(identity);
    GradedFrameRequest ungradedLights = Request(world, 1.0f, identity);
    ungradedLights.frame.look.hasGradingCurve = false;
    const GradedFrame ungradedFrame = RenderGradedFrame(h, ungradedLights);
    std::printf("     full coverage by lights without a grading key: \"%s\"\n", ungradedFrame.status.grading);
    Check(ungradedFrame.Draws() == 0 && StripesUngraded(ungradedFrame.image, world, kStripeSampleRow) &&
              std::strcmp(ungradedFrame.status.grading, kNoGradedLightReason) == 0,
          "under full coverage by Classic lights without a grading key the identity curve is not drawn");
}

bool OutsideRect(UINT x, UINT y, const D3DVIEWPORT9& rect)
{
    return x < rect.X || y < rect.Y || x >= rect.X + rect.Width || y >= rect.Y + rect.Height;
}

void CheckGradedSubRect(Harness& h, const D3DVIEWPORT9& world, const float* shipped)
{
    GradedFrameRequest request = Request(world, 1.0f, shipped);
    request.world = kGradedSubRect;
    const GradedFrame graded = RenderGradedFrame(h, request);
    float identity[kGradingCurveEntries];
    IdentityCurve(identity);
    const UINT width = StripeWidth(world);
    CurveMatch inside;
    bool outsideKept = true;
    for (UINT y = world.Y; y < world.Y + world.Height; y += 7)
        for (int code = 0; code < kCodes; ++code)
        {
            const UINT x = world.X + code * width + width / 2;
            const unsigned char* pixel = graded.image.At(x, y);
            const bool outside = OutsideRect(x, y, kGradedSubRect);
            for (int channel = 0; channel < 3; ++channel)
            {
                const int input = StripeCode(code, channel);
                if (outside)
                    outsideKept = outsideKept && ChannelOf(pixel, channel) == input;
                else
                    inside.Add(ChannelOf(pixel, channel), ReferenceGraded(input, shipped, 1.0f));
            }
        }
    PrintCurveMatch("inside a 800x400 world viewport at 100,50", inside);
    Check(graded.Draws() == 1 && outsideKept && inside.WithinConversion(),
          "grading covers the saved world viewport only; pixels outside it stay bit-exact");
}

void CheckCurveUploadedOnChange(Harness& h, const D3DVIEWPORT9& world, const float* shipped)
{
    float synthetic[kGradingCurveEntries];
    SyntheticCurve(synthetic);
    const GradedFrame first = RenderGradedFrame(h, Request(world, 1.0f, shipped));
    const GradedFrame same = RenderGradedFrame(h, Request(world, 1.0f, shipped));
    const GradedFrame changed = RenderGradedFrame(h, Request(world, 1.0f, synthetic));
    const GradedFrame back = RenderGradedFrame(h, Request(world, 1.0f, shipped));
    std::printf("     curve uploads: %u, then %u with the same curve, %u with another, %u back to the first\n",
                first.after.curveUploads, same.after.curveUploads, changed.after.curveUploads,
                back.after.curveUploads);
    Check(same.after.curveUploads == first.after.curveUploads &&
              changed.after.curveUploads == first.after.curveUploads + 1 &&
              back.after.curveUploads == first.after.curveUploads + 2,
          "the curve texture is locked only when the blended curve changes");
}

float g_shippedCurve[kGradingCurveEntries] = {};

bool ResolveShippedCurve(const FogData& data)
{
    AuthoredFog noon = {};
    const bool resolved =
        data.Resolve(kEasternKingdoms, authored_fog::kStormwindLightCentre, kNoon, kClearWeather, noon);
    std::memcpy(g_shippedCurve, noon.gradingCurve, sizeof(g_shippedCurve));
    return resolved && Near(g_shippedCurve[authored_fog::kCurveMidInput], authored_fog::kCurve8286666Mid);
}

void CheckGradingAfterAFogException(Harness& h, const D3DVIEWPORT9& world)
{
    GradedFrameRequest request = Request(world, 1.0f, g_shippedCurve);
    request.throughTheWorldDoneHook = true;
    vf_test_simulate_fog_hook_failure(1);
    const GradedFrame first = RenderGradedFrame(h, request);
    const GradedFrame second = RenderGradedFrame(h, request);
    vf_test_simulate_fog_hook_failure(0);
    const CurveMatch match = CompareStripes(second.image, world, kStripeSampleRow, g_shippedCurve, 1.0f);
    PrintCurveMatch("the second hooked frame after a fog exception", match);
    std::printf("     grading after the fog stopped: %u and %u draws, state \"%s\"\n", first.Draws(), second.Draws(),
                second.status.grading);
    Check(first.Draws() == 1 && second.Draws() == 1 && match.WithinConversion(),
          "after a fog exception the opaque hook still captures the world viewport every frame, so the grading "
          "still draws through the world-done and frame-end hooks");
}

void CheckColourGrading(Harness& h, const D3DVIEWPORT9& world, const FogData& data)
{
    Check(ResolveShippedCurve(data), "the Stormwind noon grading curve (8286666) resolves for the grading checks");
    CheckIdentityAndShippedCurves(h, world, g_shippedCurve);
    CheckUngradedFrames(h, world, g_shippedCurve);
    CheckGradedSubRect(h, world, g_shippedCurve);
    CheckCurveUploadedOnChange(h, world, g_shippedCurve);
    CheckGradingAfterAFogException(h, world);
    Config defaults = {};
    vf_test_set_config(&defaults);
}

struct GlowHandOff
{
    const char* what;
    uint8_t clientByte;
    bool foreverGlow;
    bool glowEffectRuns;
    uint8_t compensatedByte;
};

const GlowHandOff kGlowHandOffs[] = {
    {"ForeverGlow=1 under full coverage (client byte 102, Forever glow 0)", kClientGlowByte, true, true, 0},
    {"ForeverGlow=0 with a client glow of 1.1 (byte 24)", kWrappedClientGlowByte, false, true,
     kWrappedClientGlowByte},
    {"ForeverGlow=0 with the ffx CVar off", kWrappedClientGlowByte, false, false, 0},
};

void PrimeDrawnGlowCompensation(Harness& h, const HookedCamera& camera, const D3DVIEWPORT9& world)
{
    FrameInputs primed = HookedInputs(camera, world);
    primed.clientGlowAmount = kPrimedGlowCompensation;
    const char* skip = "";
    h.BeginFrame();
    h.DrawScene(camera.eye, camera.view, camera.projection, world);
    vf_test_render(&primed, &skip);
    h.dev->EndScene();
}

float FogGlowThroughTheHooks(Harness& h, const Config& fog, const D3DVIEWPORT9& world, const GlowHandOff& handOff)
{
    const HookedCamera camera = CameraOver(world);
    Config cfg = fog;
    cfg.foreverGlow = handOff.foreverGlow ? 1 : 0;
    vf_test_set_config(&cfg);
    PrimeDrawnGlowCompensation(h, camera, world);
    FakeGlowGraph graph(handOff.clientByte);
    const ForeverLookFrame frame = GlowFrame(graph, kFullCoverage, kForeverContinentGlow);
    vf_test_use_forever_look_frame(&frame);
    FrameInputs inputs = HookedInputs(camera, world);
    inputs.clientGlowAmount = handOff.glowEffectRuns ? kClampedDayNightGlow : 0.0f;
    vf_test_use_world_hook_client(&inputs, handOff.glowEffectRuns ? 1 : 0);
    h.BeginFrame();
    h.DrawScene(camera.eye, camera.view, camera.projection, world);
    vf_test_hook_opaque_done();
    h.SetEngineState(world);
    vf_test_hook_world_done();
    const float drawn = vf_test_drawn_glow_compensation();
    vf_test_hook_frame_end();
    h.dev->EndScene();
    return drawn;
}

void CheckFogCompensatesTheDeliveredGlow(Harness& h, const Config& fog, const D3DVIEWPORT9& world)
{
    bool compensated = true;
    for (const GlowHandOff& handOff : kGlowHandOffs)
    {
        const float drawn = FogGlowThroughTheHooks(h, fog, world, handOff);
        const float expected = handOff.compensatedByte / kGlowByteScale;
        std::printf("     %s: the fog composite's glow (c98.w) %.4f, expected %.4f\n", handOff.what, drawn, expected);
        compensated = compensated && std::fabs(drawn - expected) < kDeliveredGlowTolerance;
    }
    UseNoForeverLook();
    vf_test_set_config(&fog);
    Check(compensated,
          "through the world-done hook the fog is compensated for the glow byte the composite receives (0 when "
          "ForeverGlow feeds Forever's 0, 24/255 when a client glow of 1.1 wraps, 0 with the ffx CVar off), never "
          "for the clamped DayNight glow 1.0");
}

void CheckColourGradingAfterReset(Harness& h, const D3DVIEWPORT9& world, unsigned uploadsBeforeReset)
{
    const GradedFrame graded = RenderGradedFrame(h, Request(world, 1.0f, g_shippedCurve));
    const CurveMatch match = CompareStripes(graded.image, world, kStripeSampleRow, g_shippedCurve, 1.0f);
    PrintCurveMatch("after Reset to 1024x600", match);
    Check(graded.Draws() == 1 && match.WithinConversion() && graded.before.holdsCurve &&
              graded.after.curveUploads == uploadsBeforeReset,
          "grading draws after Reset with the managed curve texture it kept, without uploading it again");
    Config defaults = {};
    vf_test_set_config(&defaults);
}

unsigned CurveUploads()
{
    GradingStats stats;
    vf_test_grading_stats(&stats);
    return stats.curveUploads;
}

Image GradedSyntheticStripes(Harness& h, const D3DVIEWPORT9& world)
{
    float synthetic[kGradingCurveEntries];
    SyntheticCurve(synthetic);
    const GradedFrame graded = RenderGradedFrame(h, Request(world, 1.0f, synthetic));
    Config defaults = {};
    vf_test_set_config(&defaults);
    return graded.Draws() == 1 && graded.statesKept ? graded.image : Image();
}

CurveMatch CompareSyntheticStripes(const Image& image, const D3DVIEWPORT9& world)
{
    float synthetic[kGradingCurveEntries];
    SyntheticCurve(synthetic);
    return image.w ? CompareStripes(image, world, kStripeSampleRow, synthetic, 1.0f) : CurveMatch();
}
}
