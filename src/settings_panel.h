#pragma once

#include "config.h"
#include "hooks.h"
#include "msaa_depth.h"

#include <string>

class SettingsPanel
{
public:
    void Draw(ConfigStore& store, const FogFrameStatus& fogStatus, const WaterFrameStatus& waterStatus,
              const MultisamplingStatus& multisampling, bool& open);

private:
    void DrawSaveRow(ConfigStore& store);

    bool m_saveFailed = false;
};
