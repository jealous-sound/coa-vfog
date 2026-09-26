#pragma once

void CheckLightDisappearanceHistory(Harness& harness)
{
    Config saved;
    vf_test_get_config(&saved);
    Config config = saved;
    config.quality = 1;
    config.noiseAmount = 0;
    config.worldShadows = false;
    config.lightShafts = false;
    config.localLights = true;
    config.temporal = 0.85f;
    config.maxDistance = 1000;
    config.dataMode = 0;
    vf_test_set_config(&config);
    const D3DVIEWPORT9 world = {0, 0, 128, 96, 0, 1};
    const Vec3 eye = Add(kGameLikeWorldOffset, {0, 0, 9});
    const Vec3 at = Add(kGameLikeWorldOffset, {100, 2, 4});
    float view[16];
    float projection[16];
    CameraRelativeLookAt(eye, at, view);
    EngineProjection(128.0f / 96.0f, projection);
    FrameInputs input = MakeInputs(view, projection, eye, at, world);
    for (uint32_t i = 0; i < kMaxLocalPointLights; ++i)
    {
        LocalPointLight light;
        light.position[0] = eye.x + 30 + i;
        light.position[1] = eye.y + static_cast<float>(i) - 4;
        light.position[2] = eye.z;
        light.color[0] = 0.2f;
        light.color[1] = 0.1f;
        light.color[2] = 0.05f;
        light.attenuation[0] = 1;
        light.attenuation[2] = 0.005f;
        engine::SelectLocalPointLight(input.localLights, light, input.camPos);
    }
    const uint32_t counts[] = {kMaxLocalPointLights, 0, 0};
    const bool adaptive[] = {true, true, false};
    const char* labels[] = {
        "renderer enables adaptive history for eight point lights without noise or world shadows",
        "renderer retains adaptive history when the final eight point lights disappear",
        "renderer restores ordinary history after the disappearance frame without discarding all history"};
    for (int frame = 0; frame < 3; ++frame)
    {
        input.localLights.pointLightCount = counts[frame];
        harness.BeginFrame();
        harness.DrawScene(eye, view, projection, world);
        const char* skip = nullptr;
        const bool rendered = vf_test_render(&input, &skip) != 0;
        const bool active = vf_test_adaptive_lighting_history() != 0;
        harness.dev->EndScene();
        harness.dev->Present(nullptr, nullptr, nullptr, nullptr);
        Check(rendered && active == adaptive[frame], labels[frame]);
    }
    vf_test_set_config(&saved);
}
