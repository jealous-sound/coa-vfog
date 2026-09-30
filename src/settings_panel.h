#pragma once

#include "config.h"
#include "hooks.h"
#include "msaa_depth.h"

#include <string>

struct PanelPlacement
{
    bool known = false;
    float position[2] = {};
    float size[2] = {};
};

class SettingsPanel
{
public:
    void Draw(ConfigStore& store, const FogFrameStatus& fogStatus, const WaterFrameStatus& waterStatus,
              const MultisamplingStatus& multisampling, bool& open);
    const PanelPlacement& Placement() const { return m_placement; }

private:
    void PlaceWindow();
    void RememberPlacement();
    bool DrawSettings(Config& edited, const FogFrameStatus& fogStatus);
    void DrawFooter(ConfigStore& store, const WaterFrameStatus& waterStatus, const MultisamplingStatus& multisampling);
    void DrawSaveRow(ConfigStore& store);

    bool m_saveFailed = false;
    PanelPlacement m_placement;
    float m_footerHeight = 0.0f;
};
