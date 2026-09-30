#include "settings_panel.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace
{
constexpr float kMultiplierSliderMax = 4.0f;
constexpr float kPanelWidthInLines = 30.0f;
constexpr float kPanelHeightInLines = 42.0f;
constexpr float kMinPanelWidthInLines = 20.0f;
constexpr float kMinPanelHeightInLines = 16.0f;
constexpr float kPanelMarginInLines = 2.0f;
constexpr float kLabelWidthInLines = 11.0f;
constexpr float kLabelShareOfWidth = 0.45f;
constexpr float kFirstFooterLines = 4.0f;
constexpr float kNoTextWrap = -1.0f;
constexpr size_t kStatusTextSize = 512;
const ImVec4 kDrawnColour = {0.45f, 0.85f, 0.45f, 1.0f};
const ImVec4 kSkippedColour = {0.95f, 0.75f, 0.35f, 1.0f};
const ImVec4 kErrorColour = {1.0f, 0.4f, 0.4f, 1.0f};

const char* const kQualityNames[] = {"Low: quarter resolution, 16 steps", "Medium: half resolution, 24 steps",
                                     "High: half resolution, 32 steps"};
const char* const kDebugViewNames[] = {"Off", "Fog radiance", "Transmittance", "Linear depth"};
const char* const kLogLevelNames[] = {"Errors", "Info", "Debug"};
const char* const kWaterQualityNames[] = {"Low: 128 waves, sky reflections only",
                                          "Medium: 256 waves, scene reflections",
                                          "High: 256 waves, finer scene reflections"};
const char* const kWaterDebugViewNames[] = {"Off",        "Normals",      "Foam",   "Transmittance",
                                           "Reflection", "Liquid class", "Ripples"};

template <int N>
bool Choice(const char* label, int& value, int first, const char* const (&names)[N], const char* help)
{
    int index = value - first;
    const bool changed = ImGui::Combo(label, &index, names, N);
    ImGui::SetItemTooltip("%s", help);
    if (changed)
        value = index + first;
    return changed;
}

class Section
{
public:
    explicit Section(const char* title, ImGuiTreeNodeFlags flags = 0)
        : m_title(title), m_open(ImGui::CollapsingHeader(title, flags))
    {
        if (m_open)
            ImGui::PushID(m_title);
    }
    ~Section()
    {
        if (m_open)
            ImGui::PopID();
    }
    Section(const Section&) = delete;
    Section& operator=(const Section&) = delete;
    explicit operator bool() const { return m_open; }

private:
    const char* m_title;
    bool m_open;
};

bool Slider(const char* label, float& value, float lo, float hi, const char* format, const char* help,
            ImGuiSliderFlags flags = ImGuiSliderFlags_None)
{
    const bool changed = ImGui::SliderFloat(label, &value, lo, hi, format, flags);
    ImGui::SetItemTooltip("%s", help);
    return changed;
}

bool Multiplier(const char* label, float& value, const char* help)
{
    return Slider(label, value, 0.0f, kMultiplierSliderMax, "%.2f", help);
}

bool Toggle(const char* label, bool& value, const char* help)
{
    const bool changed = ImGui::Checkbox(label, &value);
    ImGui::SetItemTooltip("%s", help);
    return changed;
}

bool Toggle(const char* label, int& value, const char* help)
{
    bool on = value != 0;
    const bool changed = Toggle(label, on, help);
    if (changed)
        value = on ? 1 : 0;
    return changed;
}

void DrawStatus(const FogFrameStatus& status)
{
    if (status.drawn)
    {
        ImGui::TextColored(kDrawnColour, "Fog: Drawing");
        if (status.reason && *status.reason)
            ImGui::TextWrapped("%s", status.reason);
    }
    else
        ImGui::TextColored(kSkippedColour, "Fog: Not drawing (%s)", status.reason);
}

void StatusLine(const ImVec4& colour, const char* format, ...)
{
    char text[kStatusTextSize];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    ImGui::PushTextWrapPos(kNoTextWrap);
    ImGui::TextColored(colour, "%s", text);
    ImGui::PopTextWrapPos();
    ImGui::SetItemTooltip("%s", text);
}

void DrawWaterStatus(const WaterFrameStatus& status)
{
    const bool named = status.reason && *status.reason;
    if (status.drawn && named)
        StatusLine(kDrawnColour, "Water: Drawing (%s)", status.reason);
    else if (status.drawn)
        StatusLine(kDrawnColour, "Water: Drawing");
    else
        StatusLine(kSkippedColour, "Water: Not drawing (%s)", named ? status.reason : "no reason given");
}

void DrawMultisamplingStatus(const MultisamplingStatus& status)
{
    if (status.method && *status.method)
        StatusLine(kDrawnColour, "Antialiasing: multisampling %dx kept (depth copied by %s)", status.samples,
                   status.method);
    else
        StatusLine(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "Antialiasing: multisampling off (%s)",
                   status.off);
}

bool DrawQuality(Config& c)
{
    const Section section("Quality", ImGuiTreeNodeFlags_DefaultOpen);
    if (!section)
        return false;
    bool changed = Choice("Quality", c.quality, 1, kQualityNames, "Resolution and ray-march steps of the fog pass");
    changed |= Slider("Temporal filter", c.temporal, 0.0f, 0.97f, "%.2f",
                      "History weight of the temporal filter, 0 = off");
    return changed;
}

bool DrawDensity(Config& c)
{
    const Section section("Density", ImGuiTreeNodeFlags_DefaultOpen);
    if (!section)
        return false;
    bool changed = Multiplier("Density", c.density,
                              "Fog thickness affects visibility and scattered light. Use Sun scatter for the halo.");
    changed |= Multiplier("Haze", c.haze, "Distance haze where no Classic data exists");
    changed |= Multiplier("Ground fog", c.groundFog, "Low ground mist where no Classic data exists");
    changed |= Multiplier("Distance fog", c.farFog, "The distance fog that replaces the stock fog");
    changed |= Toggle("Replace the stock fog", c.stockFog,
                      "On: the volumetric distance fog replaces the client's linear fog. Off: keep the stock fog");
    changed |= Toggle("Classic fog layers", c.dataMode,
                      "Use the Classic client's fog layers where its lights cover the map. Off: derived layers "
                      "everywhere");
    changed |= Slider("Fog range", c.maxDistance, 200.0f, 5000.0f, "%.0f yd",
                      "How far sky rays are integrated and the scale of the Classic distance curves");
    changed |= Slider("Density variation", c.noiseAmount, 0.0f, 1.0f, "%.2f",
                      "Spatial variation in haze and ground fog; 0 keeps the authored density smooth");
    changed |= Slider("Variation scale", c.noiseScale, 0.001f, 1.0f, "%.3f",
                      "Larger values make smaller mist patches", ImGuiSliderFlags_Logarithmic);
    changed |= Slider("Mist drift", c.noiseWindSpeed, 0.0f, 10.0f, "%.2f yd/s",
                      "Speed of drifting mist; 0 keeps it stationary");
    changed |= Toggle("Classic fog noise", c.classicNoise,
                      "Drifting fog banks where the Classic layers carry the modern client's noise, mostly in storms; "
                      "it thins those layers to about half their density on average. Off: the density variation "
                      "above applies to them instead");
    return changed;
}

bool DrawLight(Config& c)
{
    const Section section("Light", ImGuiTreeNodeFlags_DefaultOpen);
    if (!section)
        return false;
    bool changed = Multiplier("Sun scatter", c.sunScatter, "In-scattered sun or moon light: the halo around it");
    changed |= Multiplier("Ambient", c.ambient, "Ambient fog brightness");
    changed |= Multiplier("Exposure", c.exposure, "Brightness of the layers used where no Classic data exists");
    changed |= Multiplier("Classic exposure", c.classicExposure, "Brightness of the Classic layers, 1 = default");
    changed |= Toggle("Energy-normalised Classic phase", c.classicPhase,
                      "Scatter the sun and moon into the Classic layers with an energy-normalised phase, as their "
                      "authored intensities suggest the modern client does: dimmer horizon bands, a larger and "
                      "brighter halo around the sun. Off: the phase peaks at 1 toward the light");
    changed |= Toggle("Linear light", c.colorSpace,
                      "Scatter and blend in linear light like the modern client, with a soft highlight roll-off. "
                      "Off: gamma");
    changed |= Toggle("Local lights", c.localLights, "Scatter nearby point lights from the world into the fog");
    changed |= Slider("Local light intensity", c.localLightIntensity, 0.0f, 8.0f, "%.2f",
                      "Brightness of nearby point lights in the fog");
    changed |= Slider("Local light phase", c.localLightPhase, -0.9f, 0.9f, "%.2f",
                      "How every point light scatters in the fog: 0 evenly in all directions; toward 0.9 mostly in "
                      "the fog between you and the light; below 0 the fog behind the light, steeply brighter toward "
                      "-0.9. The modern client uses one such value for all its lights; 0.3 is a calibration");
    changed |= Slider("God rays", c.godRays, 0.0f, 4.0f, "%.2f",
                      "Radial rays from the bright sky around the sun, 0 = off");
    changed |= Toggle("Glow compensation", c.glowCompensation,
                      "Pre-compensate the fog for the client's full-screen glow, which otherwise bleaches bright fog "
                      "around the sun to white");
    return changed;
}

bool DrawGlowAndGrading(Config& c)
{
    const Section section("Glow and colour grading");
    if (!section)
        return false;
    bool changed = Toggle("Forever glow", c.foreverGlow,
                          "Use the modern client's glow amount where Classic lights cover the camera: 0 on most "
                          "continent lights, so the full-screen glow mostly disappears there. It replaces the client's "
                          "own amount, Ascension's ambientGlow included, and fades back to it at the edge of Classic "
                          "coverage. Off: the client's glow");
    changed |= Slider("Colour grading", c.colorGrading, 0.0f, 1.0f, "%.2f",
                      "Strength of the modern client's colour curve for the lights around the camera: brighter "
                      "midtones, and the brightest highlights clipped to white. Names, the interface, the ghost view "
                      "and the view under water are not graded. 0 = off");
    return changed;
}

bool DrawViewDistance(Config& c)
{
    bool lifted = c.farClipMax > kFarClipMaxKeepsClientCap;
    bool changed = Toggle("Lift the continent view distance", lifted,
                          "Ascension caps the continents at 791 yd; the engine allows 1583. Costs about four times "
                          "the loaded terrain. Turning it on when it was off at start-up needs a client restart");
    if (changed)
        c.farClipMax = lifted ? kEngineFarClipMax : kFarClipMaxKeepsClientCap;
    if (lifted)
        changed |= Slider("View distance", c.farClipMax, std::ceil(kEngineFarClipMin), kEngineFarClipMax, "%.0f yd",
                          "Continent view distance, within the client's own farclip setting",
                          ImGuiSliderFlags_AlwaysClamp);
    ImGui::TextDisabled("Applies at the next farclip change, map load or zone change.");
    return changed;
}

bool DrawWorld(Config& c)
{
    const Section section("World");
    if (!section)
        return false;
    bool changed = Toggle("Fog under water", c.underwater, "Keep the effect while the camera is under water");
    changed |= Toggle("Fog effects at their own distance", c.transparentFog,
                      "Particles, spell effects and other see-through models are fogged by their own distance, as "
                      "the stock client does, with a linear fog fitted to the volumetric fog within 100 yd; the fog "
                      "is drawn before them, after the water. Beyond 100 yd the line is extended, so distant effects "
                      "and fading models can be fogged more or less than the scene around them, and past its end "
                      "distant glows vanish. Off: the fog is drawn once after the whole world and they take the fog "
                      "of the scene behind them");
    changed |= Toggle("Water writes depth", c.liquidDepth,
                      "Let water surfaces write depth so water is fogged by its own distance; always on while Modern "
                      "water is drawn");
    changed |= Toggle("Interior-aware fog", c.interiorAware,
                      "Fade outdoor fog and direct sunlight as the camera enters a building or cave");
    changed |= Slider("Interior density", c.interiorDensity, 0.0f, 1.0f, "%.2f",
                      "Outdoor fog density retained indoors; the area's native distance fog still applies");
    changed |= DrawViewDistance(c);
    return changed;
}

bool DrawAntialiasing(Config& c)
{
    const Section section("Antialiasing");
    if (!section)
        return false;
    return Toggle("Keep the game's multisampling", c.multisampling,
                  "Keep the game's Multisampling video option (smooth edges) when the graphics driver can copy its "
                  "depth for the fog and water: NVIDIA through NVAPI, AMD and Intel through RESZ. Off: multisampling "
                  "stays off. The game lists its Multisampling choices once per start, so turning this on takes "
                  "effect after restarting the game; turning it off applies the next time the game resets its "
                  "display, for example after changing Multisampling or the resolution in its Video options");
}

bool DrawWater(Config& c)
{
    const Section section("Water");
    if (!section)
        return false;
    bool changed = Toggle("Modern water", c.water,
                          "Shade lakes, rivers, the sea and indoor pools like the modern client; water surfaces write "
                          "depth while it is drawn. Off: the client's own water");
    changed |= Choice("Water quality", c.waterQuality, 1, kWaterQualityNames,
                      "Size of the wave simulation and detail of the reflections");
    changed |= Slider("Waves", c.waterWaves, 0.0f, 2.0f, "%.2f", "Wave height, 0 = flat water");
    changed |= Slider("Wind", c.waterWind, 0.5f, 10.0f, "%.2f",
                      "Wind that drives the waves; stronger wind makes longer, rougher waves");
    changed |= Slider("Foam", c.waterFoam, 0.0f, 2.0f, "%.2f", "Foam on wave crests and along shores, 0 = none");
    changed |= Slider("Reflections", c.waterReflections, 0.0f, 2.0f, "%.2f",
                      "Reflections of the sky and the scene, 0 = none");
    changed |= Slider("Sun highlight", c.waterSpecular, 0.0f, 4.0f, "%.2f",
                      "Highlight of the sun or moon on the water, 0 = none");
    changed |= Slider("Clarity", c.waterClarity, 0.25f, 4.0f, "%.2f",
                      "How far you see into the water; larger is clearer", ImGuiSliderFlags_Logarithmic);
    changed |= Slider("Zone colours", c.waterZoneColors, 0.0f, 1.0f, "%.2f",
                      "How much the zone's own water colours from the client's lights tint the water, 0 = the modern "
                      "colours only");
    changed |= Slider("Ripples", c.waterRipples, 0.0f, 2.0f, "%.2f",
                      "Wakes of players, creatures, pets and mounts in the water: a thin V behind a unit that runs or "
                      "swims, one ring as it starts or stops, a calm surface while it stands. 0.5 = default, 0 = none");
    changed |= Toggle("Client splashes", c.waterClientSplashes,
                      "Keep the client's own flat splash and wake sprites while the ripples run; off hides them for "
                      "every unit, also those beyond the ripples' reach");
    changed |= Choice("Water view", c.waterDebugView, 0, kWaterDebugViewNames,
                      "Show one input of the water shading instead of the scene");
    return changed;
}

bool DrawDebug(Config& c)
{
    const Section section("Debug");
    if (!section)
        return false;
    bool changed = Choice("View", c.debugView, 0, kDebugViewNames, "Show one input of the fog instead of the scene");
    changed |= Toggle("Sun marker", c.sunMarker, "Red marker where the sun direction projects on screen");
    changed |= Choice("Log level", c.logLevel, 0, kLogLevelNames, "Detail written to CoAVolFog.log");
    return changed;
}
}

void SettingsPanel::PlaceWindow()
{
    const float line = ImGui::GetFontSize();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float margin = kPanelMarginInLines * line;
    const ImVec2 smallest(kMinPanelWidthInLines * line, kMinPanelHeightInLines * line);
    const ImVec2 largest(std::max(display.x - margin, smallest.x), std::max(display.y - margin, smallest.y));
    const ImVec2 firstSize(kPanelWidthInLines * line,
                           std::max(smallest.y, std::min(kPanelHeightInLines * line, display.y - 2.0f * margin)));
    const PanelPlacement& p = m_placement;
    ImGui::SetNextWindowPos(p.known ? ImVec2(p.position[0], p.position[1]) : ImVec2(margin, margin),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(p.known ? ImVec2(p.size[0], p.size[1]) : firstSize, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(smallest, largest);
}

void SettingsPanel::RememberPlacement()
{
    const ImVec2 position = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    m_placement = {true, {position.x, position.y}, {size.x, size.y}};
}

bool SettingsPanel::DrawSettings(Config& edited, const FogFrameStatus& fogStatus)
{
    const float footer =
        m_footerHeight > 0.0f ? m_footerHeight : kFirstFooterLines * ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("Settings", ImVec2(0.0f, -footer), ImGuiChildFlags_None, ImGuiWindowFlags_NoNavInputs);
    DrawStatus(fogStatus);
    const float labels =
        std::min(kLabelWidthInLines * ImGui::GetFontSize(), kLabelShareOfWidth * ImGui::GetContentRegionAvail().x);
    ImGui::PushItemWidth(-labels);
    bool changed = DrawQuality(edited);
    changed |= DrawDensity(edited);
    changed |= DrawLight(edited);
    changed |= DrawGlowAndGrading(edited);
    changed |= DrawWorld(edited);
    changed |= DrawAntialiasing(edited);
    changed |= DrawWater(edited);
    changed |= DrawDebug(edited);
    ImGui::PopItemWidth();
    ImGui::EndChild();
    return changed;
}

void SettingsPanel::DrawFooter(ConfigStore& store, const WaterFrameStatus& waterStatus,
                               const MultisamplingStatus& multisampling)
{
    const float top = ImGui::GetCursorPosY();
    ImGui::Separator();
    DrawWaterStatus(waterStatus);
    DrawMultisamplingStatus(multisampling);
    DrawSaveRow(store);
    m_footerHeight = ImGui::GetCursorPosY() - top;
}

void SettingsPanel::Draw(ConfigStore& store, const FogFrameStatus& fogStatus, const WaterFrameStatus& waterStatus,
                         const MultisamplingStatus& multisampling, bool& open)
{
    PlaceWindow();
    const bool expanded =
        ImGui::Begin("CoAVolFog", &open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavInputs);
    if (!expanded)
    {
        ImGui::End();
        return;
    }
    RememberPlacement();
    ImGui::PushTextWrapPos(0.0f);
    Config edited = store.Get();
    if (DrawSettings(edited, fogStatus))
        store.Apply(edited);
    if (!ImGui::IsAnyItemActive())
        store.LogSettledEdits();
    DrawFooter(store, waterStatus, multisampling);
    ImGui::PopTextWrapPos();
    ImGui::End();
}

void SettingsPanel::DrawSaveRow(ConfigStore& store)
{
    const bool unsaved = store.HasUnsavedChanges();
    ImGui::BeginDisabled(!unsaved);
    if (ImGui::Button("Save"))
        m_saveFailed = !store.Save();
    ImGui::SetItemTooltip("Write the changed settings to CoAVolFog.ini");
    ImGui::SameLine();
    if (ImGui::Button("Revert"))
    {
        store.Revert();
        m_saveFailed = false;
    }
    ImGui::SetItemTooltip("Go back to the settings in CoAVolFog.ini");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (m_saveFailed)
        ImGui::TextColored(kErrorColour, "Could not write CoAVolFog.ini");
    else
        ImGui::TextDisabled("%s", unsaved ? "Unsaved changes" : "Matches CoAVolFog.ini");
}
