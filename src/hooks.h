#pragma once

bool InstallEngineHooks();

void InstallFarClipHooks();

struct FogFrameStatus
{
    bool drawn;
    const char* reason;
};

FogFrameStatus LastFogFrameStatus();
