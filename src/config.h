#pragma once

#include <string>

constexpr float kEngineFarClipMin = 183.333f;
constexpr float kEngineFarClipMax = 1583.33f;
constexpr float kFarClipMaxKeepsClientCap = 0.0f;

constexpr unsigned kVirtualKeyF7 = 0x76;

struct Hotkey
{
    unsigned virtualKey;
    bool ctrl;
    bool shift;
    bool alt;
};

constexpr Hotkey kDefaultOverlayHotkey = {kVirtualKeyF7, true, false, false};

bool ParseHotkey(const char* text, Hotkey& out);
std::string HotkeyName(const Hotkey& key);

struct Config
{
    bool enable = true;
    bool hooks = true;
    bool overlay = true;
    Hotkey overlayKey = kDefaultOverlayHotkey;
    int quality = 2;
    float density = 1.0f;
    float haze = 1.0f;
    float groundFog = 0.6f;
    float farFog = 1.0f;
    int stockFog = 1;
    int dataMode = 1;
    int colorSpace = 1;
    float sunScatter = 1.0f;
    float ambient = 1.0f;
    float exposure = 1.0f;
    float classicExposure = 1.0f;
    bool lightShafts = true;
    float godRays = 0.0f;
    bool glowCompensation = true;
    float farClipMax = kFarClipMaxKeepsClientCap;
    float maxDistance = 5000.0f;
    float temporal = 0.85f;
    bool underwater = false;
    bool liquidDepth = true;
    int debugView = 0;
    bool sunMarker = false;
    int logLevel = 1;
};

bool SameLiveSettings(const Config& a, const Config& b);

class ConfigStore
{
public:
    void Load(const std::string& path);
    bool ReloadIfChanged();
    void Override(const Config& config) { m_config = config; }
    void Apply(const Config& edited);
    bool Save();
    void Revert();
    bool HasUnsavedChanges() const { return !SameLiveSettings(m_config, m_saved); }
    const Config& Get() const { return m_config; }
    const std::string& Path() const { return m_path; }

private:
    void Read();
    void ReadKeepingStartupSwitches();

    std::string m_path;
    unsigned long long m_stamp = 0;
    Config m_config;
    Config m_saved;
};

ConfigStore& GlobalConfig();
