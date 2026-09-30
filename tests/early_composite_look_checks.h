#pragma once

namespace early_composite_look
{
constexpr int kSampleStep = 7;
constexpr int kMinFogChange = 8;

using forever_look_checks::FakeGlowGraph;
using forever_look_checks::GlowHandOff;
using transparent_fog_checks::View;

struct LookedFrame
{
    Image scene;
    Image image;
    float compensationAtLiquidEnd = 0.0f;
    float compensationAtWorldDone = 0.0f;
    unsigned grades = 0;
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
    vf_test_hook_liquid_end();
    frame.compensationAtLiquidEnd = vf_test_drawn_glow_compensation();
    vf_test_hook_world_done();
    frame.compensationAtWorldDone = vf_test_drawn_glow_compensation();
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

struct GradedPath
{
    int fogChange = 0;
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
    for (bool transparentFog : {false, true})
    {
        const GradedPath path = GradeFinishedFrame(h, v, transparentFog, curve);
        char what[96];
        std::snprintf(what, sizeof(what), "TransparentFog=%d, fog and god rays change the scene by up to %d/255",
                      transparentFog ? 1 : 0, path.fogChange);
        forever_look_checks::PrintCurveMatch(what, path.match);
        graded = graded && path.fogChange >= kMinFogChange && path.ungradedDraws == 0 && path.gradedDraws == 1 &&
                 path.match.WithinConversion();
    }
    forever_look_checks::UseNoForeverLook();
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
