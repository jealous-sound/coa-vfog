#pragma once

namespace client_sprite_checks
{
using water_checks::BasinClient;
using water_checks::WaterCalls;
using water_checks::WaterFrame;
using water_checks::WaterView;

constexpr int32_t kClientDefault = 1;
constexpr int32_t kPlayerChoice = 2;
constexpr unsigned char kFillerByte = 0xCC;
constexpr double kSpriteStart = 200.0;
constexpr double kSpriteFrame = 1.0 / 60.0;
constexpr double kShortDive = 0.5;
constexpr double kLongDive = 1.5;
constexpr size_t kFlippedGuard = 2;
constexpr uint64_t kWaderGuid = 7;
constexpr float kWaderX = 65.0f;
constexpr float kWaderY = 0.0f;
constexpr uint32_t kWaderForwardFlag = 0x1;
constexpr uint32_t kWaderClockMs = 3600000;
constexpr int kNoRippleFault = 0;
constexpr int kUnsupportedRipples = 1;
constexpr int kFailingRippleMaps = 2;

volatile int32_t g_gate = kClientDefault;

struct SyntheticClientCode
{
    uintptr_t base = 0;
    std::vector<unsigned char> bytes;
};

SyntheticClientCode ClientCodeWithGuards()
{
    size_t count = 0;
    const ClientCodeGuard* guards = engine::ClientRippleGateGuards(count);
    uintptr_t low = UINTPTR_MAX;
    uintptr_t high = 0;
    for (size_t i = 0; i < count; ++i)
    {
        low = std::min(low, guards[i].address);
        high = std::max(high, guards[i].address + guards[i].size);
    }
    SyntheticClientCode code;
    code.base = low;
    code.bytes.assign(high - low, kFillerByte);
    for (size_t i = 0; i < count; ++i)
        std::memcpy(&code.bytes[guards[i].address - low], guards[i].bytes, guards[i].size);
    return code;
}

bool BindGate(const SyntheticClientCode& code)
{
    return vf_test_bind_client_ripple_gate(&g_gate, code.base, code.bytes.data(), code.bytes.size()) != 0;
}

struct SpriteFrames
{
    BasinClient& client;
    Config cfg;
    double seconds = kSpriteStart;

    uint32_t emitted = 0;

    int32_t After(const Config& settings, bool underWater = false, const WaterContactFrame& contacts = {})
    {
        cfg = settings;
        vf_test_set_config(&cfg);
        vf_test_set_water_seconds(seconds);
        WaterView view = client.View();
        view.in.inLiquid = underWater;
        view.water.contacts = contacts;
        client.UseView(view);
        WaterFrame hooked;
        hooked.calls = WaterCalls::Hooks;
        client.Render(hooked);
        seconds += kSpriteFrame;
        return g_gate;
    }

    int32_t Later(double gap, bool underWater)
    {
        seconds += gap;
        return After(cfg, underWater);
    }

    int32_t Wading(const Config& settings, int frames)
    {
        int32_t gate = g_gate;
        for (int i = 0; i < frames; ++i)
        {
            WaterContact wader =
                water_checks::BasinContact(kWaderGuid, kWaderX, kWaderY, water_checks::kRippleWadingDepth);
            wader.movementFlags = kWaderForwardFlag;
            wader.nextRippleMs = kWaderClockMs + ++emitted;
            gate = After(settings, false, water_checks::ContactsOf({wader}));
        }
        return gate;
    }
};

Config With(const Config& on, bool water, float ripples, bool clientSplashes)
{
    Config cfg = on;
    cfg.water = water;
    cfg.waterRipples = ripples;
    cfg.waterClientSplashes = clientSplashes;
    return cfg;
}

void CheckSettingsHoldAndRestoreTheGate(SpriteFrames& frames, const Config& on)
{
    const Config hide = With(on, true, 1.0f, false);
    const int32_t held = frames.After(hide);
    g_gate = kPlayerChoice;
    const int32_t heldAgain = frames.After(hide);
    const int32_t kept = frames.After(With(on, true, 1.0f, true));
    std::printf("     waterRipples %d: %d with shaded ripples, %d after the player set %d, %d with "
                "WaterClientSplashes 1\n",
                kClientDefault, held, heldAgain, kPlayerChoice, kept);
    Check(held == 0 && heldAgain == 0 && kept == kPlayerChoice,
          "while modern water with ripples is shaded the client's waterRipples value is held at 0, a value the "
          "player sets meanwhile is held too, and WaterClientSplashes 1 puts back the player's latest value");

    frames.After(hide);
    const int32_t noRipples = frames.After(With(on, true, 0.0f, false));
    frames.After(hide);
    const int32_t noWater = frames.After(With(on, false, 1.0f, false));
    std::printf("     restored with WaterRipples 0: %d, with Water 0: %d\n", noRipples, noWater);
    Check(noRipples == kPlayerChoice && noWater == kPlayerChoice,
          "WaterRipples 0 and Water 0 give the client back its waterRipples value at the end of the frame");
}

void CheckUnderwaterGrace(SpriteFrames& frames, const Config& on)
{
    const Config hide = With(on, true, 1.0f, false);
    frames.After(hide);
    const int32_t shortDive = frames.Later(kShortDive, true);
    frames.After(hide);
    frames.Later(kShortDive, true);
    const int32_t longDive = frames.Later(kLongDive - kShortDive, true);
    std::printf("     camera under water: %d after %.1f s, %d after %.1f s\n", shortDive, kShortDive, longDive,
                kLongDive);
    Check(shortDive == 0 && longDive == kPlayerChoice,
          "the value stays held while the camera is under water for less than a second and comes back after it");
}

void CheckRefusedCaptureKeepsTheSprites(SpriteFrames& frames, const Config& on)
{
    const Config hide = With(on, true, 1.0f, false);
    const int32_t held = frames.After(hide);
    vf_test_refuse_water_contacts(1);
    const int32_t refused = frames.After(hide);
    const int32_t stillRefused = frames.After(hide);
    vf_test_refuse_water_contacts(0);
    const int32_t heldAgain = frames.After(hide);
    std::printf("     unit walk refused: waterRipples %d while held, %d and %d refused, %d with the walk back\n",
                held, refused, stillRefused, heldAgain);
    Check(held == 0 && refused == kPlayerChoice && stillRefused == kPlayerChoice && heldAgain == 0,
          "when the client's units cannot be read no rings replace the client's sprites, so its waterRipples value is "
          "put back at the end of that frame and left alone until the walk is supported again");
}

void CheckUnavailableRipplesKeepTheSprites(SpriteFrames& frames, const Config& on)
{
    const Config hide = With(on, true, 1.0f, false);
    const Config released = With(on, true, 0.0f, false);
    frames.After(released);
    vf_test_inject_water_ripple_fault(kUnsupportedRipples);
    const int32_t unsupported = frames.After(hide);
    const int32_t unsupportedWading = frames.Wading(hide, 2);
    frames.After(released);
    vf_test_inject_water_ripple_fault(kNoRippleFault);
    const int32_t supported = frames.After(hide);
    std::printf("     ripple map unsupported: waterRipples %d without units, %d with a wader; %d once supported\n",
                unsupported, unsupportedWading, supported);
    Check(unsupported == kPlayerChoice && unsupportedWading == kPlayerChoice && supported == 0,
          "a device without a usable ripple map leaves the client's waterRipples value alone, before and while a "
          "unit wades");

    frames.After(released);
    vf_test_inject_water_ripple_fault(kFailingRippleMaps);
    const int32_t beforeRings = frames.Wading(hide, 1);
    const int32_t failing = frames.Wading(hide, 1);
    const int32_t stillFailing = frames.Wading(hide, 1);
    const int32_t failingDry = frames.After(hide);
    vf_test_inject_water_ripple_fault(kNoRippleFault);
    const int32_t retried = frames.Wading(hide, 1);
    std::printf("     ripple map creation failing: waterRipples %d before the first ring, %d and %d with rings, %d "
                "without units; %d once created\n",
                beforeRings, failing, stillFailing, failingDry, retried);
    Check(beforeRings == 0 && failing == kPlayerChoice && stillFailing == kPlayerChoice &&
              failingDry == kPlayerChoice && retried == 0,
          "a failed ripple map creation puts the client's waterRipples value back at the end of that frame and the "
          "hold resumes once the map is created");
}

void CheckGuardMismatchLeavesTheGate(SpriteFrames& frames, const Config& on, SyntheticClientCode code)
{
    size_t count = 0;
    const ClientCodeGuard* guards = engine::ClientRippleGateGuards(count);
    code.bytes[guards[kFlippedGuard].address - code.base] ^= 0xFF;
    g_gate = kClientDefault;
    const bool bound = BindGate(code);
    const int32_t untouched = frames.After(With(on, true, 1.0f, false));
    std::printf("     guard %s changed: bound %d, waterRipples %d after a shaded frame\n", guards[kFlippedGuard].name,
                bound, untouched);
    Check(!bound && untouched == kClientDefault,
          "a single changed byte in the client's ripple code leaves the waterRipples value alone");
}

void CheckClientSprites(Harness& h)
{
    Config base;
    vf_test_get_config(&base);
    const Config on = water_checks::RippleConfig(base, 1.0f);
    BasinClient client(h, water_checks::DefaultWaterView());
    SpriteFrames frames{client, on};
    const SyntheticClientCode code = ClientCodeWithGuards();
    g_gate = kClientDefault;
    Check(BindGate(code) && Config().waterClientSplashes == false,
          "the sprite gate binds to a client image whose ripple code matches, and WaterClientSplashes defaults to 0");
    CheckSettingsHoldAndRestoreTheGate(frames, on);
    CheckUnderwaterGrace(frames, on);
    CheckRefusedCaptureKeepsTheSprites(frames, on);
    CheckUnavailableRipplesKeepTheSprites(frames, on);
    CheckGuardMismatchLeavesTheGate(frames, on, code);
    vf_test_bind_client_ripple_gate(nullptr, 0, nullptr, 0);
    vf_test_set_water_seconds(water_checks::kRealTime);
    vf_test_set_config(&base);
}

void HoldGateForDeviceRelease()
{
    g_gate = kClientDefault;
    const SyntheticClientCode code = ClientCodeWithGuards();
    const bool bound = BindGate(code);
    vf_test_update_client_ripple_sprites(1, 1, kSpriteStart);
    Check(bound && g_gate == 0, "the sprite gate is held before the device is released");
}

void CheckDeviceReleaseRestoresTheGate()
{
    Check(g_gate == kClientDefault, "releasing the device gives the client back its waterRipples value");
    vf_test_bind_client_ripple_gate(nullptr, 0, nullptr, 0);
}
}
