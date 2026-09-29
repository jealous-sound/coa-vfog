#pragma once

#include "water_classify.h"

namespace water_settings_checks
{
struct LiquidRow
{
    uint32_t id;
    const char* name;
    uint32_t soundBank;
    uint32_t materialId;
    const char* texture0;
    WaterClass expected;
};

const LiquidRow kClientLiquidRows[] = {
    {1, "Water", 0, 1, "XTextures\\river\\lake_a.%d.blp", WaterClass::Lake},
    {2, "Ocean", 1, 1, "XTextures\\ocean\\ocean_h.%d.blp", WaterClass::Ocean},
    {5, "Slow Water", 0, 1, "XTextures\\river\\lake_a.%d.blp", WaterClass::Lake},
    {6, "Slow Ocean", 1, 1, "XTextures\\ocean\\ocean_h.%d.blp", WaterClass::Ocean},
    {9, "Fast Water", 0, 1, "XTextures\\river\\fast_a.%d.blp", WaterClass::River},
    {10, "Fast Ocean", 1, 1, "XTextures\\ocean\\ocean_h.%d.blp", WaterClass::Ocean},
    {13, "WMO Water", 0, 1, "XTextures\\river\\lake_a.%d.blp", WaterClass::Lake},
    {14, "WMO Ocean", 1, 1, "XTextures\\ocean\\ocean_h.%d.blp", WaterClass::Ocean},
    {17, "WMO Water - Interior", 0, 1, "XTextures\\river\\lake_a.%d.blp", WaterClass::Interior},
    {41, "Coilfang Raid - Water", 0, 1, "XTextures\\river\\lake_a.%d.blp", WaterClass::Lake},
    {81, "Lake Wintergrasp - Water", 0, 1, "XTextures\\river\\lake_a.%d.blp", WaterClass::Lake},
    {3, "Magma", 2, 2, "XTextures\\lava\\lava.%d.blp", WaterClass::None},
    {4, "Slime", 3, 2, "XTextures\\slime\\slime.%d.blp", WaterClass::None},
    {15, "Green Lava", 2, 2, "XTextures\\LavaGreen\\lavagreen.%d.blp", WaterClass::None},
    {181, "Orange Slime", 0, 1, "XTEXTURES\\LavaOrange\\LavaOrange.%d.blp", WaterClass::None},
    {100, "Basic Procedural Water", 1, 3, "XTextures\\procWater\\basicReflectionMap.blp", WaterClass::None},
    {2, "Ocean with an unknown texture", 1, 1, "XTextures\\custom\\sea.%d.blp", WaterClass::None},
    {1, "Water with an upper-case path", 0, 1, "XTEXTURES\\RIVER\\LAKE_A.%D.BLP", WaterClass::Lake},
    {1, "Water with a folder name inside a file name", 0, 1, "XTextures\\lake\\river_a.%d.blp", WaterClass::None},
    {1, "Water without a texture", 0, 1, nullptr, WaterClass::None},
};

const char* const kEditedWaterKeys[] = {"Water", "WaterQuality", "WaterWaves", "WaterFoam", "WaterDebugView"};

std::vector<std::string> IniLines(const std::string& text)
{
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size())
    {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
        start = end + 1;
    }
    while (!lines.empty() && lines.back().empty())
        lines.pop_back();
    return lines;
}

bool IsKeyLine(const std::string& line, const char* key)
{
    const std::string prefix = std::string(key) + "=";
    return line.compare(0, prefix.size(), prefix) == 0;
}

bool IsEditedWaterKeyLine(const std::string& line)
{
    for (const char* key : kEditedWaterKeys)
        if (IsKeyLine(line, key))
            return true;
    return false;
}

bool SameWaterSettings(const Config& a, const Config& b)
{
    return a.water == b.water && a.waterQuality == b.waterQuality && a.waterWaves == b.waterWaves &&
           a.waterWind == b.waterWind && a.waterFoam == b.waterFoam && a.waterReflections == b.waterReflections &&
           a.waterSpecular == b.waterSpecular && a.waterClarity == b.waterClarity &&
           a.waterZoneColors == b.waterZoneColors && a.waterDebugView == b.waterDebugView;
}

void CheckShippedWaterDefaults(const std::wstring& shippedIni)
{
    ConfigStore store;
    store.Load(NarrowPath(shippedIni));
    Check(SameWaterSettings(store.Get(), Config()), "the shipped INI's water keys load with their defaults");
    const std::vector<std::string> lines = IniLines(ReadText(shippedIni));
    const char* const shippedLines[] = {"Water=1",          "WaterQuality=2",     "WaterWaves=1.0",
                                        "WaterWind=2.0",    "WaterFoam=1.0",      "WaterReflections=1.0",
                                        "WaterSpecular=1.0", "WaterClarity=1.0", "WaterZoneColors=0.5",
                                        "WaterDebugView=0"};
    int found = 0;
    for (const char* shipped : shippedLines)
        found += std::count(lines.begin(), lines.end(), std::string(shipped)) == 1 ? 1 : 0;
    std::printf("     water keys written once in the shipped INI: %d of %d\n", found,
                static_cast<int>(sizeof(shippedLines) / sizeof(shippedLines[0])));
    Check(found == static_cast<int>(sizeof(shippedLines) / sizeof(shippedLines[0])),
          "the shipped INI lists every water key once with its default");
    const std::string text = ReadText(shippedIni);
    Check(text.find("; Modern water:") != std::string::npos &&
              text.find("surfaces write depth whatever LiquidDepth says") != std::string::npos,
          "the shipped INI documents the water keys and that water writes depth while Water=1");
}

void CheckWaterSettingsSave(const std::wstring& outDir, const std::wstring& shippedIni)
{
    const std::wstring savedIni = FullPath(outDir + L"\\water-saved.ini");
    Check(CopyFileW(shippedIni.c_str(), savedIni.c_str(), FALSE) != FALSE, "shipped CoAVolFog.ini copied for water");
    ConfigStore store;
    store.Load(NarrowPath(savedIni));
    Config edited = store.Get();
    edited.water = false;
    edited.waterQuality = 3;
    edited.waterWaves = 1.5f;
    edited.waterFoam = 0.25f;
    edited.waterDebugView = 5;
    store.Apply(edited);
    Check(store.HasUnsavedChanges() && store.Save() && !store.HasUnsavedChanges(),
          "water settings edited in the window save to the INI");

    ConfigStore reloaded;
    reloaded.Load(NarrowPath(savedIni));
    Check(SameWaterSettings(reloaded.Get(), edited) && SameLiveSettings(reloaded.Get(), store.Get()),
          "saved water settings reload unchanged");

    const std::vector<std::string> shipped = IniLines(ReadText(shippedIni));
    const std::vector<std::string> saved = IniLines(ReadText(savedIni));
    int rewritten = 0;
    bool onlyEditedKeys = shipped.size() == saved.size();
    for (size_t i = 0; onlyEditedKeys && i < shipped.size(); ++i)
    {
        if (shipped[i] == saved[i])
            continue;
        ++rewritten;
        onlyEditedKeys = IsEditedWaterKeyLine(shipped[i]) &&
                         shipped[i].substr(0, shipped[i].find('=')) == saved[i].substr(0, saved[i].find('='));
    }
    std::printf("     INI lines: shipped %d, saved %d, rewritten %d\n", static_cast<int>(shipped.size()),
                static_cast<int>(saved.size()), rewritten);
    const int editedKeyCount = static_cast<int>(sizeof(kEditedWaterKeys) / sizeof(kEditedWaterKeys[0]));
    Check(onlyEditedKeys && rewritten == editedKeyCount,
          "saving water settings rewrites only the edited water lines and keeps every comment");
    const auto has = [&saved](const char* line) {
        return std::find(saved.begin(), saved.end(), std::string(line)) != saved.end();
    };
    Check(has("Water=0") && has("WaterQuality=3") && has("WaterWaves=1.5") && has("WaterFoam=0.25") &&
              has("WaterDebugView=5") && has("WaterWind=2.0"),
          "the saved water lines hold the edited values and the untouched ones keep their text");
}

void CheckWaterSettingClamps(const std::wstring& outDir, const std::wstring& shippedIni)
{
    const std::wstring clampedIni = FullPath(outDir + L"\\water-clamped.ini");
    Check(CopyFileW(shippedIni.c_str(), clampedIni.c_str(), FALSE) != FALSE, "shipped CoAVolFog.ini copied for clamps");
    const wchar_t* const outOfRange[][2] = {
        {L"Water", L"7"},           {L"WaterQuality", L"9"},     {L"WaterWaves", L"-3"},
        {L"WaterWind", L"100"},     {L"WaterFoam", L"5"},        {L"WaterReflections", L"-1"},
        {L"WaterSpecular", L"9"},   {L"WaterClarity", L"0"},     {L"WaterZoneColors", L"2"},
        {L"WaterDebugView", L"42"},
    };
    for (const auto& key : outOfRange)
        WritePrivateProfileStringW(L"CoAVolFog", key[0], key[1], clampedIni.c_str());
    ConfigStore store;
    store.Load(NarrowPath(clampedIni));
    const Config& c = store.Get();
    std::printf("     clamped: quality %d waves %.2f wind %.2f foam %.2f reflections %.2f specular %.2f clarity %.2f "
                "zone %.2f view %d\n",
                c.waterQuality, c.waterWaves, c.waterWind, c.waterFoam, c.waterReflections, c.waterSpecular,
                c.waterClarity, c.waterZoneColors, c.waterDebugView);
    Check(c.water && c.waterQuality == 3 && c.waterWaves == 0.0f && c.waterWind == 10.0f && c.waterFoam == 2.0f &&
              c.waterReflections == 0.0f && c.waterSpecular == 4.0f && c.waterClarity == 0.25f &&
              c.waterZoneColors == 1.0f && c.waterDebugView == 5,
          "out-of-range water keys in the INI are clamped to their ranges");

    WritePrivateProfileStringW(L"CoAVolFog", L"WaterWaves", L"nan", clampedIni.c_str());
    WritePrivateProfileStringW(L"CoAVolFog", L"WaterFoam", L"lots", clampedIni.c_str());
    WritePrivateProfileStringW(L"CoAVolFog", L"WaterQuality", L"0", clampedIni.c_str());
    store.Revert();
    Check(store.Get().waterWaves == Config().waterWaves && store.Get().waterFoam == Config().waterFoam &&
              store.Get().waterQuality == 1,
          "unreadable water values fall back to their defaults and a low quality clamps to 1");

    Config edited = store.Get();
    edited.waterQuality = 0;
    edited.waterWind = 0.1f;
    edited.waterClarity = 100.0f;
    edited.waterZoneColors = -0.5f;
    edited.waterDebugView = -1;
    store.Apply(edited);
    const Config& applied = store.Get();
    Check(applied.waterQuality == 1 && applied.waterWind == 0.5f && applied.waterClarity == 4.0f &&
              applied.waterZoneColors == 0.0f && applied.waterDebugView == 0,
          "water edits from the window are clamped like the INI");
}

void CheckLiquidClassification()
{
    int wrong = 0;
    for (const LiquidRow& row : kClientLiquidRows)
    {
        const WaterClass got = ClassifyLiquid(row.id, row.soundBank, row.materialId, row.texture0);
        std::printf("     liquid type %u (%s, sound bank %u, material %u) -> %s\n", row.id, row.name, row.soundBank,
                    row.materialId, WaterClassLabel(got));
        if (got != row.expected)
        {
            ++wrong;
            std::printf("     expected %s\n", WaterClassLabel(row.expected));
        }
    }
    Check(wrong == 0, "the client's LiquidType rows classify as lake, river, ocean, interior or none");
    Check(ClassifyLiquid(17, 1, 1, "XTextures\\ocean\\ocean_h.%d.blp") == WaterClass::Interior &&
              ClassifyLiquid(9, 1, 1, "XTextures\\river\\fast_a.%d.blp") == WaterClass::Ocean,
          "interior wins over the ocean sound bank, which wins over fast water");
}

void CheckWaterOnlyEditsKeepFogHistory()
{
    const Config base;
    Config waterOnly = base;
    waterOnly.waterSpecular = 2.0f;
    waterOnly.waterWind = 4.0f;
    waterOnly.waterClarity = 2.0f;
    waterOnly.waterWaves = 0.5f;
    waterOnly.waterFoam = 1.5f;
    waterOnly.waterReflections = 0.5f;
    waterOnly.waterZoneColors = 0.25f;
    waterOnly.waterQuality = 3;
    waterOnly.waterDebugView = 4;
    Check(SameFogSettings(base, waterOnly) && !SameLiveSettings(base, waterOnly),
          "water-only edits leave the fog's settings (and its temporal history) alone but count as live changes");
    Config waterOff = base;
    waterOff.water = false;
    Config densityEdit = base;
    densityEdit.density = 2.0f;
    Check(!SameFogSettings(base, waterOff) && !SameFogSettings(base, densityEdit),
          "turning water off (which changes the depth the fog sees) and fog edits reset the fog's history");
}

void CheckSettingChangesListed()
{
    const Config before;
    Config after = before;
    after.waterQuality = 3;
    after.density = 2.5f;
    after.waterWind = 4.0f;
    after.waterFoam = 1.5f;
    after.water = false;
    const std::string changes = SettingChanges(before, after);
    std::printf("     changes: %s\n", changes.c_str());
    Check(changes == "WaterQuality 2 -> 3, Density 1 -> 2.5, WaterWind 2 -> 4, WaterFoam 1 -> 1.5, Water 1 -> 0" &&
              SettingChanges(before, before).empty(),
          "a settings change lists every changed fog and water key with its old and new value");
}

size_t CountOf(const std::string& text, const char* fragment)
{
    size_t count = 0;
    for (size_t at = text.find(fragment); at != std::string::npos; at = text.find(fragment, at + 1))
        ++count;
    return count;
}

void CheckSettingChangesLogged(const std::wstring& outDir, const std::wstring& shippedIni)
{
    const std::wstring logPath = FullPath(outDir + L"\\harness-settings.log");
    LogOpen(NarrowPath(logPath).c_str());
    const std::wstring iniPath = FullPath(outDir + L"\\settings-log.ini");
    Check(CopyFileW(shippedIni.c_str(), iniPath.c_str(), FALSE) != FALSE, "shipped CoAVolFog.ini copied for logging");
    ConfigStore store;
    store.Load(NarrowPath(iniPath));
    LogSetLevel(static_cast<int>(LogLevel::Info));
    const size_t start = ReadText(logPath).size();
    auto logged = [&logPath, start]() {
        const std::string text = ReadText(logPath);
        return text.size() > start ? text.substr(start) : std::string();
    };

    WritePrivateProfileStringW(L"CoAVolFog", L"Density", L"2.5", iniPath.c_str());
    WritePrivateProfileStringW(L"CoAVolFog", L"WaterFoam", L"1.5", iniPath.c_str());
    const bool reloaded = store.ReloadIfChanged();
    Config edited = store.Get();
    edited.waterWind = 3.0f;
    store.Apply(edited);
    edited.waterWind = 4.0f;
    store.Apply(edited);
    const bool quietWhileEditing = logged().find("WaterWind") == std::string::npos;
    store.LogSettledEdits();
    store.LogSettledEdits();
    store.Revert();
    const std::string text = logged();
    std::printf("%s", text.c_str());
    Check(reloaded && runtime_cost::HasLine(text, "settings from CoAVolFog.ini: Density 1 -> 2.5, WaterFoam 1 -> 1.5"),
          "an INI reload logs the changed keys with their old and new values");
    Check(quietWhileEditing && CountOf(text, "settings: WaterWind 2 -> 4") == 1 &&
              CountOf(text, "WaterWind 2 -> 3") == 0,
          "a window edit logs one line once it settles, from the value before the edit to the value after it");
    Check(runtime_cost::HasLine(text, "settings reverted to CoAVolFog.ini: WaterWind 4 -> 2"),
          "reverting logs the keys it changes");
}

size_t DllLogSize()
{
    return ReadText(runtime_cost::FogLogBesideTheFogDll()).size();
}

void CheckSliderDragLoggedOnce(size_t logStart)
{
    const std::string text = runtime_cost::LogWrittenSince(logStart);
    const size_t densityLines = CountOf(text, "settings: Density 1 -> ");
    std::printf("     settings lines after the Density drag: %zu, Density changes %zu\n", CountOf(text, "settings: "),
                densityLines);
    Check(densityLines == 1 && CountOf(text, "Density ") == 1,
          "dragging a slider in the settings window logs one settings line when the drag ends");
}

void CheckWaterSettings(const std::wstring& outDir, const std::wstring& shippedIni)
{
    CheckSettingChangesListed();
    CheckSettingChangesLogged(outDir, shippedIni);
    CheckWaterOnlyEditsKeepFogHistory();
    CheckShippedWaterDefaults(shippedIni);
    CheckWaterSettingsSave(outDir, shippedIni);
    CheckWaterSettingClamps(outDir, shippedIni);
    CheckLiquidClassification();
}
}
