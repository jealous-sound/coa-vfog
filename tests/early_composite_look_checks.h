#pragma once

namespace early_composite_look
{
constexpr int kSampleStep = 7;
constexpr int kMinFogChange = 8;
constexpr int kMinLateRayChange = 8;

using forever_look_checks::FakeGlowGraph;
using forever_look_checks::GlowHandOff;
using transparent_fog_checks::View;

struct LookedFrame
{
    Image scene;
    Image afterLiquid;
    Image afterWorld;
    Image image;
    float compensationAtLiquidEnd = 0.0f;
    float compensationAtWorldDone = 0.0f;
    unsigned grades = 0;
    int glarePassesBeforeTheFog = 0;
    int glarePassesAtOwnCall = 0;
};

void UseClientWithTransparentFogHooks(const FrameInputs& in, bool glowScreenEffectRuns)
{
    vf_test_glare_pass_thunk(reinterpret_cast<uintptr_t>(&transparent_fog_checks::RecordGlarePass));
    vf_test_use_fog_hook_client(&in);
    vf_test_use_world_hook_client(&in, glowScreenEffectRuns ? 1 : 0);
}

LookedFrame RenderLookedFrame(Harness& h, const View& v)
{
    LookedFrame frame;
    GradingStats before;
    vf_test_grading_stats(&before);
    vf_test_hook_frame_begin();
    h.BeginFrame();
    h.dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    h.DrawScene(v.eye, v.view, v.proj, v.world);
    frame.scene = Capture(h.dev);
    h.dev->SetViewport(&v.world);
    vf_test_hook_opaque_done();
    const int glareBeforeLiquidEnd = transparent_fog_checks::g_glarePasses;
    vf_test_hook_liquid_end();
    frame.glarePassesBeforeTheFog = transparent_fog_checks::g_glarePasses - glareBeforeLiquidEnd;
    frame.compensationAtLiquidEnd = vf_test_drawn_glow_compensation();
    frame.afterLiquid = Capture(h.dev);
    const int glareBeforeOwnCall = transparent_fog_checks::g_glarePasses;
    transparent_fog_checks::CallGlarePassThunk();
    frame.glarePassesAtOwnCall = transparent_fog_checks::g_glarePasses - glareBeforeOwnCall;
    vf_test_hook_world_done();
    frame.compensationAtWorldDone = vf_test_drawn_glow_compensation();
    frame.afterWorld = Capture(h.dev);
    vf_test_hook_frame_end();
    frame.image = Capture(h.dev);
    h.dev->EndScene();
    h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    GradingStats after;
    vf_test_grading_stats(&after);
    frame.grades = after.grades - before.grades;
    return frame;
}

LookedFrame RenderSettledLookedFrame(Harness& h, const View& v)
{
    LookedFrame frame;
    for (int i = 0; i < transparent_fog_checks::kWarmUpFrames; ++i)
        frame = RenderLookedFrame(h, v);
    return frame;
}

void PrimeDrawnCompensation(Harness& h, const View& v)
{
    FrameInputs primed = v.in;
    primed.clientGlowAmount = forever_look_checks::kPrimedGlowCompensation;
    const char* skip = "";
    h.BeginFrame();
    h.DrawScene(v.eye, v.view, v.proj, v.world);
    vf_test_render(&primed, &skip);
    h.dev->EndScene();
}

float EarlyCompositeGlow(Harness& h, const GlowHandOff& handOff, float& atWorldDone)
{
    const View v;
    Config cfg = transparent_fog_checks::HookConfig(true);
    cfg.foreverGlow = handOff.foreverGlow ? 1 : 0;
    vf_test_set_config(&cfg);
    FakeGlowGraph graph(handOff.clientByte);
    const ForeverLookFrame look = forever_look_checks::GlowFrame(graph, forever_look_checks::kFullCoverage,
                                                                 forever_look_checks::kForeverContinentGlow);
    vf_test_use_forever_look_frame(&look);
    FrameInputs inputs = v.in;
    inputs.clientGlowAmount = handOff.glowEffectRuns ? forever_look_checks::kClampedDayNightGlow : 0.0f;
    UseClientWithTransparentFogHooks(inputs, handOff.glowEffectRuns);
    RenderSettledLookedFrame(h, v);
    PrimeDrawnCompensation(h, v);
    const LookedFrame measured = RenderLookedFrame(h, v);
    forever_look_checks::UseNoForeverLook();
    atWorldDone = measured.compensationAtWorldDone;
    return measured.compensationAtLiquidEnd;
}

void CheckEarlyCompositeCompensatesTheDeliveredGlow(Harness& h)
{
    bool compensated = true;
    for (const GlowHandOff& handOff : forever_look_checks::kGlowHandOffs)
    {
        float atWorldDone = 0.0f;
        const float early = EarlyCompositeGlow(h, handOff, atWorldDone);
        const float expected = handOff.compensatedByte / forever_look_checks::kGlowByteScale;
        std::printf("     TransparentFog=1, %s: the early composite's glow (c98.w) %.4f at the liquid end, %.4f after "
                    "the world, expected %.4f\n",
                    handOff.what, early, atWorldDone, expected);
        compensated = compensated &&
                      std::fabs(early - expected) < forever_look_checks::kDeliveredGlowTolerance &&
                      atWorldDone == early;
    }
    Check(compensated,
          "with TransparentFog=1 the Forever glow is fed before the early composite at the liquid end, so the fog "
          "drawn there is compensated for the glow byte the composite receives (0 under ForeverGlow, 24/255 for a "
          "wrapped 1.1, 0 with the ffx CVar off), never for the clamped DayNight glow 1.0");
}

struct FogPath
{
    int glarePassesBeforeTheFog = 0;
    int glarePassesAtOwnCall = 0;
    int changeAtLiquidEnd = 0;
    int changeAtWorldDone = 0;
};

FogPath FogPathOf(const LookedFrame& frame, const View& v)
{
    return {frame.glarePassesBeforeTheFog, frame.glarePassesAtOwnCall,
            transparent_fog_checks::LargestDifference(frame.scene, frame.afterLiquid, v.world),
            transparent_fog_checks::LargestDifference(frame.afterLiquid, frame.afterWorld, v.world)};
}

bool TookEarlyCompositeWithLateRays(const FogPath& path)
{
    return path.glarePassesBeforeTheFog == 1 && path.glarePassesAtOwnCall == 0 &&
           path.changeAtLiquidEnd >= kMinFogChange && path.changeAtWorldDone >= kMinLateRayChange;
}

bool TookSingleComposite(const FogPath& path)
{
    return path.glarePassesBeforeTheFog == 0 && path.glarePassesAtOwnCall == 1 && path.changeAtLiquidEnd == 0 &&
           path.changeAtWorldDone >= kMinFogChange;
}

bool TookItsPath(const FogPath& path, bool transparentFog)
{
    return transparentFog ? TookEarlyCompositeWithLateRays(path) : TookSingleComposite(path);
}

struct GradedPath
{
    int fogChange = 0;
    FogPath fogged;
    FogPath graded;
    unsigned ungradedDraws = 0;
    unsigned gradedDraws = 0;
    forever_look_checks::CurveMatch match;
};

GradedPath GradeFinishedFrame(Harness& h, const View& v, bool transparentFog, const float* curve)
{
    Config cfg = transparent_fog_checks::HookConfig(transparentFog);
    cfg.godRays = transparent_fog_checks::kRaysOn;
    UseClientWithTransparentFogHooks(v.in, true);
    vf_test_set_config(&cfg);
    const LookedFrame fogged = RenderSettledLookedFrame(h, v);
    cfg.colorGrading = 1.0f;
    vf_test_set_config(&cfg);
    const LookedFrame graded = RenderSettledLookedFrame(h, v);
    GradedPath path;
    path.fogChange = transparent_fog_checks::LargestDifference(fogged.scene, fogged.image, v.world);
    path.fogged = FogPathOf(fogged, v);
    path.graded = FogPathOf(graded, v);
    path.ungradedDraws = fogged.grades;
    path.gradedDraws = graded.grades;
    for (UINT y = v.world.Y; y < v.world.Y + v.world.Height; y += kSampleStep)
        for (UINT x = v.world.X; x < v.world.X + v.world.Width; x += kSampleStep)
            for (int channel = 0; channel < 3; ++channel)
                path.match.Add(forever_look_checks::ChannelOf(graded.image.At(x, y), channel),
                               forever_look_checks::ReferenceGraded(
                                   forever_look_checks::ChannelOf(fogged.image.At(x, y), channel), curve, 1.0f));
    return path;
}

void CheckGradingSeesTheFoggedFrame(Harness& h)
{
    const View v;
    const float* curve = forever_look_checks::g_shippedCurve;
    const ForeverLookFrame look = forever_look_checks::GradingFrame(forever_look_checks::kFullCoverage, curve);
    vf_test_use_forever_look_frame(&look);
    bool graded = true;
    bool tookTheirPaths = true;
    for (bool transparentFog : {false, true})
    {
        const GradedPath path = GradeFinishedFrame(h, v, transparentFog, curve);
        char what[96];
        std::snprintf(what, sizeof(what), "TransparentFog=%d, fog and god rays change the scene by up to %d/255",
                      transparentFog ? 1 : 0, path.fogChange);
        forever_look_checks::PrintCurveMatch(what, path.match);
        for (const FogPath* drawn : {&path.fogged, &path.graded})
            std::printf("     TransparentFog=%d, %s frame: glare %d before the fog and %d at its own call; the liquid "
                        "end changes the scene by up to %d/255, the end of the world by up to %d/255\n",
                        transparentFog ? 1 : 0, drawn == &path.fogged ? "ungraded" : "graded",
                        drawn->glarePassesBeforeTheFog, drawn->glarePassesAtOwnCall, drawn->changeAtLiquidEnd,
                        drawn->changeAtWorldDone);
        tookTheirPaths = tookTheirPaths && TookItsPath(path.fogged, transparentFog) &&
                         TookItsPath(path.graded, transparentFog);
        graded = graded && path.fogChange >= kMinFogChange && path.ungradedDraws == 0 && path.gradedDraws == 1 &&
                 path.match.WithinConversion();
    }
    forever_look_checks::UseNoForeverLook();
    Check(tookTheirPaths,
          "the graded frames take the path they are named for: with TransparentFog=1 the glare and the fog are drawn "
          "at the liquid end, the glare is skipped at its own call and the god rays are added at the end of the "
          "world; with TransparentFog=0 the liquid end draws nothing, the glare is drawn at its own call and the fog "
          "and god rays at the end of the world");
    Check(graded,
          "the colour grading at the frame end grades the finished fogged frame, god rays included, over the whole "
          "world viewport, its first column too, both after the single composite (TransparentFog=0) and after the "
          "early composite with the late god rays (TransparentFog=1)");
}

void CheckLookThroughTheEarlyComposite(Harness& h)
{
    Config saved;
    vf_test_get_config(&saved);
    CheckEarlyCompositeCompensatesTheDeliveredGlow(h);
    CheckGradingSeesTheFoggedFrame(h);
    vf_test_set_config(&saved);
}
}
