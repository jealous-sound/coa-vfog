#pragma once

#include "config.h"
#include "hooks.h"

#include <string>

class SettingsPanel
{
public:
    void Draw(ConfigStore& store, const FogFrameStatus& status, const std::string& hotkeyName, bool& open);

private:
    void DrawSaveRow(ConfigStore& store);

    bool m_saveFailed = false;
};
