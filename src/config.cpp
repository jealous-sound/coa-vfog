#include "config.h"

#include "log.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
constexpr const char* kSection = "CoAVolFog";
constexpr unsigned kMaxFunctionKey = 24;

struct IntSetting
{
    const char* key;
    int Config::*value;
    int lo;
    int hi;
};

struct FloatSetting
{
    const char* key;
    float Config::*value;
    float lo;
    float hi;
};

struct BoolSetting
{
    const char* key;
    bool Config::*value;
};

const IntSetting kIntSettings[] = {
    {"Quality", &Config::quality, 1, 3},     {"StockFog", &Config::stockFog, 0, 1},
    {"DataMode", &Config::dataMode, 0, 1},   {"ColorSpace", &Config::colorSpace, 0, 1},
    {"DebugView", &Config::debugView, 0, 3}, {"LogLevel", &Config::logLevel, 0, 2},
};

const FloatSetting kFloatSettings[] = {
    {"Density", &Config::density, 0.0f, 10.0f},
    {"Haze", &Config::haze, 0.0f, 10.0f},
    {"GroundFog", &Config::groundFog, 0.0f, 10.0f},
    {"FarFog", &Config::farFog, 0.0f, 10.0f},
    {"SunScatter", &Config::sunScatter, 0.0f, 10.0f},
    {"Ambient", &Config::ambient, 0.0f, 10.0f},
    {"Exposure", &Config::exposure, 0.0f, 10.0f},
    {"ClassicExposure", &Config::classicExposure, 0.0f, 10.0f},
    {"LocalLightIntensity", &Config::localLightIntensity, 0.0f, 8.0f},
    {"InteriorDensity", &Config::interiorDensity, 0.0f, 1.0f},
    {"NoiseAmount", &Config::noiseAmount, 0.0f, 1.0f},
    {"NoiseScale", &Config::noiseScale, 0.001f, 1.0f},
    {"NoiseWindSpeed", &Config::noiseWindSpeed, 0.0f, 10.0f},
    {"GodRays", &Config::godRays, 0.0f, 4.0f},
    {"FarClipMax", &Config::farClipMax, 0.0f, kEngineFarClipMax},
    {"MaxDistance", &Config::maxDistance, 200.0f, 5000.0f},
    {"Temporal", &Config::temporal, 0.0f, 0.97f},
};

const BoolSetting kBoolSettings[] = {
    {"LightShafts", &Config::lightShafts}, {"GlowCompensation", &Config::glowCompensation},
    {"WorldShadows", &Config::worldShadows},
    {"LocalLights", &Config::localLights}, {"InteriorAware", &Config::interiorAware},
    {"Underwater", &Config::underwater},   {"LiquidDepth", &Config::liquidDepth},
    {"SunMarker", &Config::sunMarker},
};

struct NamedKey
{
    const char* name;
    unsigned virtualKey;
};

const NamedKey kNamedKeys[] = {
    {"Insert", VK_INSERT}, {"Delete", VK_DELETE}, {"Home", VK_HOME},   {"End", VK_END},
    {"PageUp", VK_PRIOR},  {"PageDown", VK_NEXT}, {"Pause", VK_PAUSE}, {"ScrollLock", VK_SCROLL},
};

const NamedKey kReportedOnlyKeys[] = {
    {"Media Previous", VK_MEDIA_PREV_TRACK}, {"Media Next", VK_MEDIA_NEXT_TRACK},
    {"Media Play/Pause", VK_MEDIA_PLAY_PAUSE}, {"Media Stop", VK_MEDIA_STOP},
    {"Volume Mute", VK_VOLUME_MUTE},         {"Volume Down", VK_VOLUME_DOWN},
    {"Volume Up", VK_VOLUME_UP},             {"Cancel", VK_CANCEL},
};

unsigned long long FileStamp(const std::string& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data))
        return 0;
    return (static_cast<unsigned long long>(data.ftLastWriteTime.dwHighDateTime) << 32) |
           data.ftLastWriteTime.dwLowDateTime;
}

float ClampedSetting(float v, float lo, float hi)
{
    return v >= lo ? std::min(v, hi) : lo;
}

float ReadFloat(const std::string& path, const char* key, float fallback, float lo, float hi)
{
    char buf[64];
    GetPrivateProfileStringA(kSection, key, "", buf, sizeof(buf), path.c_str());
    if (!buf[0])
        return fallback;
    char* end = nullptr;
    float v = std::strtof(buf, &end);
    if (end == buf || v != v)
        return fallback;
    return std::clamp(v, lo, hi);
}

int ReadInt(const std::string& path, const char* key, int fallback, int lo, int hi)
{
    int v = static_cast<int>(GetPrivateProfileIntA(kSection, key, fallback, path.c_str()));
    return std::clamp(v, lo, hi);
}

Hotkey ReadHotkey(const std::string& path, const char* key, const Hotkey& fallback)
{
    char buf[64];
    GetPrivateProfileStringA(kSection, key, "", buf, sizeof(buf), path.c_str());
    Hotkey parsed = fallback;
    if (buf[0] && !ParseHotkey(buf, parsed))
        VF_LOG_ERROR("%s=%s is not a key; using %s", key, buf, HotkeyName(fallback).c_str());
    return parsed;
}

std::string SettingText(float value)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f", value);
    std::string text = buf;
    text.erase(text.find_last_not_of('0') + 1);
    if (text.back() == '.')
        text.pop_back();
    return text;
}

bool WriteSetting(const std::string& path, const char* key, const std::string& text)
{
    return WritePrivateProfileStringA(kSection, key, text.c_str(), path.c_str()) != FALSE;
}

void ClampLiveSettings(Config& c)
{
    for (const IntSetting& s : kIntSettings)
        c.*s.value = std::clamp(c.*s.value, s.lo, s.hi);
    for (const FloatSetting& s : kFloatSettings)
        c.*s.value = ClampedSetting(c.*s.value, s.lo, s.hi);
    if (c.farClipMax < kEngineFarClipMin)
        c.farClipMax = kFarClipMaxKeepsClientCap;
}

std::string Trimmed(const std::string& text)
{
    const size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos)
        return std::string();
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

bool SameText(const std::string& text, const char* name)
{
    return _stricmp(text.c_str(), name) == 0;
}

bool IsLetterOrDigitKey(unsigned virtualKey)
{
    return (virtualKey >= '0' && virtualKey <= '9') || (virtualKey >= 'A' && virtualKey <= 'Z');
}

bool ParseFunctionKey(const std::string& name, unsigned& virtualKey)
{
    if (name.size() < 2 || std::toupper(static_cast<unsigned char>(name[0])) != 'F' ||
        !std::all_of(name.begin() + 1, name.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
        return false;
    const unsigned number = static_cast<unsigned>(std::atoi(name.c_str() + 1));
    if (number < 1 || number > kMaxFunctionKey)
        return false;
    virtualKey = VK_F1 + number - 1;
    return true;
}

bool ParseKeyName(const std::string& name, unsigned& virtualKey)
{
    if (name.size() == 1)
    {
        const unsigned key = static_cast<unsigned>(std::toupper(static_cast<unsigned char>(name[0])));
        if (!IsLetterOrDigitKey(key))
            return false;
        virtualKey = key;
        return true;
    }
    if (ParseFunctionKey(name, virtualKey))
        return true;
    for (const NamedKey& k : kNamedKeys)
        if (SameText(name, k.name))
        {
            virtualKey = k.virtualKey;
            return true;
        }
    return false;
}

std::string KeyName(unsigned virtualKey)
{
    if (virtualKey >= VK_F1 && virtualKey < VK_F1 + kMaxFunctionKey)
        return "F" + std::to_string(virtualKey - VK_F1 + 1);
    for (const NamedKey& k : kNamedKeys)
        if (k.virtualKey == virtualKey)
            return k.name;
    if (IsLetterOrDigitKey(virtualKey))
        return std::string(1, static_cast<char>(virtualKey));
    for (const NamedKey& k : kReportedOnlyKeys)
        if (k.virtualKey == virtualKey)
            return k.name;
    return "key " + std::to_string(virtualKey);
}
}

bool ParseHotkey(const char* text, Hotkey& out)
{
    Hotkey key = {0, false, false, false};
    std::string rest = text ? text : "";
    for (size_t plus = rest.find('+'); plus != std::string::npos; plus = rest.find('+'))
    {
        const std::string modifier = Trimmed(rest.substr(0, plus));
        if (SameText(modifier, "Ctrl") || SameText(modifier, "Control"))
            key.ctrl = true;
        else if (SameText(modifier, "Shift"))
            key.shift = true;
        else if (SameText(modifier, "Alt"))
            key.alt = true;
        else
            return false;
        rest = rest.substr(plus + 1);
    }
    if (!ParseKeyName(Trimmed(rest), key.virtualKey))
        return false;
    out = key;
    return true;
}

std::string HotkeyName(const Hotkey& key)
{
    std::string name;
    if (key.ctrl)
        name += "Ctrl+";
    if (key.shift)
        name += "Shift+";
    if (key.alt)
        name += "Alt+";
    return name + KeyName(key.virtualKey);
}

bool SameLiveSettings(const Config& a, const Config& b)
{
    for (const IntSetting& s : kIntSettings)
        if (a.*s.value != b.*s.value)
            return false;
    for (const FloatSetting& s : kFloatSettings)
        if (a.*s.value != b.*s.value)
            return false;
    for (const BoolSetting& s : kBoolSettings)
        if (a.*s.value != b.*s.value)
            return false;
    return true;
}

void ConfigStore::Load(const std::string& path)
{
    m_path = path;
    m_stamp = FileStamp(path);
    Read();
}

bool ConfigStore::ReloadIfChanged()
{
    unsigned long long stamp = FileStamp(m_path);
    if (stamp == m_stamp)
        return false;
    m_stamp = stamp;
    ReadKeepingStartupSwitches();
    VF_LOG_INFO("config reloaded: density=%.2f haze=%.2f ground=%.2f far=%.2f stockfog=%d sun=%.2f rays=%.2f "
                "glow=%d farclipmax=%.0f quality=%d debug=%d",
                m_config.density, m_config.haze, m_config.groundFog, m_config.farFog, m_config.stockFog,
                m_config.sunScatter, m_config.godRays, m_config.glowCompensation ? 1 : 0, m_config.farClipMax,
                m_config.quality, m_config.debugView);
    return true;
}

void ConfigStore::Apply(const Config& edited)
{
    Config c = edited;
    c.enable = m_config.enable;
    c.hooks = m_config.hooks;
    c.overlay = m_config.overlay;
    c.overlayKey = m_config.overlayKey;
    ClampLiveSettings(c);
    m_config = c;
    LogSetLevel(c.logLevel);
}

bool ConfigStore::Save()
{
    const Config onDisk = ReadFile();
    Config merged = m_config;
    bool written = true;
    for (const IntSetting& s : kIntSettings)
    {
        if (m_config.*s.value == m_saved.*s.value)
            merged.*s.value = onDisk.*s.value;
        else if (m_config.*s.value != onDisk.*s.value)
            written = WriteSetting(m_path, s.key, std::to_string(m_config.*s.value)) && written;
    }
    for (const FloatSetting& s : kFloatSettings)
    {
        if (m_config.*s.value == m_saved.*s.value)
        {
            merged.*s.value = onDisk.*s.value;
            continue;
        }
        const std::string text = SettingText(m_config.*s.value);
        merged.*s.value = std::strtof(text.c_str(), nullptr);
        if (merged.*s.value != onDisk.*s.value)
            written = WriteSetting(m_path, s.key, text) && written;
    }
    for (const BoolSetting& s : kBoolSettings)
    {
        if (m_config.*s.value == m_saved.*s.value)
            merged.*s.value = onDisk.*s.value;
        else if (m_config.*s.value != onDisk.*s.value)
            written = WriteSetting(m_path, s.key, m_config.*s.value ? "1" : "0") && written;
    }
    m_stamp = FileStamp(m_path);
    if (!written)
    {
        VF_LOG_ERROR("settings could not be written to %s (error %lu)", m_path.c_str(), GetLastError());
        return false;
    }
    m_config = merged;
    m_saved = merged;
    LogSetLevel(merged.logLevel);
    VF_LOG_INFO("settings saved to %s", m_path.c_str());
    return true;
}

void ConfigStore::Revert()
{
    m_stamp = FileStamp(m_path);
    ReadKeepingStartupSwitches();
}

void ConfigStore::ReadKeepingStartupSwitches()
{
    bool enable = m_config.enable;
    bool hooks = m_config.hooks;
    Read();
    m_config.enable = enable;
    m_config.hooks = hooks;
}

void ConfigStore::Read()
{
    const Config c = ReadFile();
    m_config = c;
    m_saved = c;
    LogSetLevel(c.logLevel);
}

Config ConfigStore::ReadFile() const
{
    Config c;
    const std::string& p = m_path;
    c.enable = ReadInt(p, "Enable", 1, 0, 1) != 0;
    c.hooks = ReadInt(p, "EngineHooks", 1, 0, 1) != 0;
    c.overlay = ReadInt(p, "Overlay", 1, 0, 1) != 0;
    c.overlayKey = ReadHotkey(p, "OverlayKey", c.overlayKey);
    for (const IntSetting& s : kIntSettings)
        c.*s.value = ReadInt(p, s.key, c.*s.value, s.lo, s.hi);
    for (const FloatSetting& s : kFloatSettings)
        c.*s.value = ReadFloat(p, s.key, c.*s.value, s.lo, s.hi);
    for (const BoolSetting& s : kBoolSettings)
        c.*s.value = ReadInt(p, s.key, c.*s.value ? 1 : 0, 0, 1) != 0;
    ClampLiveSettings(c);
    return c;
}

ConfigStore& GlobalConfig()
{
    static ConfigStore store;
    return store;
}
