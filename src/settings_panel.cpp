#include "settings_panel.h"

#include "imgui.h"

#include <cfloat>
#include <cmath>

namespace
{
constexpr float kMultiplierSliderMax = 4.0f;
constexpr float kPanelWidthInLines = 30.0f;
constexpr float kPanelMarginInLines = 2.0f;
const ImVec4 kDrawnColour = {0.45f, 0.85f, 0.45f, 1.0f};
const ImVec4 kSkippedColour = {0.95f, 0.75f, 0.35f, 1.0f};
const ImVec4 kErrorColour = {1.0f, 0.4f, 0.4f, 1.0f};

const char* const kQualityNames[] = {"Low: quarter resolution, 16 steps", "Medium: half resolution, 24 steps",
                                     "High: half resolution, 32 steps"};
const char* const kDebugViewNames[] = {"Off", "Fog radiance", "Transmittance", "Linear depth"};
const char* const kLogLevelNames[] = {"Errors", "Info", "Debug"};

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

void DrawStatus(const FogFrameStatus& status, const std::string& hotkeyName)
{
    if (status.drawn)
        ImGui::TextColored(kDrawnColour, "Fog: drawing");
    else
        ImGui::TextColored(kSkippedColour, "Fog: not drawn (%s)", status.reason);
    ImGui::TextDisabled("%s shows and hides this window. Ctrl+click a slider to type a value.", hotkeyName.c_str());
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
    bool changed = Multiplier("Density", c.density, "Global density multiplier");
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
    return changed;
}

bool DrawLight(Config& c)
{
    const Section section("Light", ImGuiTreeNodeFlags_DefaultOpen);
    if (!section)
        return false;
    bool changed = Multiplier("Sun scatter", c.sunScatter, "In-scattered sun or moon light: the halo and the shafts");
    changed |= Multiplier("Ambient", c.ambient, "Ambient fog brightness");
    changed |= Multiplier("Exposure", c.exposure, "Brightness of the layers used where no Classic data exists");
    changed |= Multiplier("Classic exposure", c.classicExposure, "Brightness of the Classic layers, 1 = as authored");
    changed |= Toggle("Linear light", c.colorSpace,
                      "Scatter and blend in linear light like the modern client, with a soft highlight roll-off. "
                      "Off: gamma");
    changed |= Toggle("Light shafts", c.lightShafts,
                      "Shadowed in-scattering: light shafts through trees, buildings and terrain");
    changed |= Slider("God rays", c.godRays, 0.0f, 4.0f, "%.2f",
                      "Radial rays from the bright sky around the sun, 0 = off");
    changed |= Toggle("Glow compensation", c.glowCompensation,
                      "Pre-compensate the fog for the client's full-screen glow, which otherwise bleaches bright fog "
                      "around the sun to white");
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
    changed |= Toggle("Water writes depth", c.liquidDepth,
                      "Let water surfaces write depth so water is fogged by its own distance");
    changed |= DrawViewDistance(c);
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

void SettingsPanel::Draw(ConfigStore& store, const FogFrameStatus& status, const std::string& hotkeyName, bool& open)
{
    const float line = ImGui::GetFontSize();
    const float width = kPanelWidthInLines * line;
    ImGui::SetNextWindowPos(ImVec2(kPanelMarginInLines * line, kPanelMarginInLines * line), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, FLT_MAX));
    if (!ImGui::Begin("CoAVolFog", &open,
                      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                          ImGuiWindowFlags_NoNavInputs))
    {
        ImGui::End();
        return;
    }
    DrawStatus(status, hotkeyName);
    Config edited = store.Get();
    bool changed = DrawQuality(edited);
    changed |= DrawDensity(edited);
    changed |= DrawLight(edited);
    changed |= DrawWorld(edited);
    changed |= DrawDebug(edited);
    if (changed)
        store.Apply(edited);
    ImGui::Separator();
    DrawSaveRow(store);
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
