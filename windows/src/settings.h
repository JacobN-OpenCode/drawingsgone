// DrawingsGone for Windows — persisted state (HKCU\Software\Navaratne\DrawingsGone).
#pragma once

#include "common.h"

enum class CoverStyle : int {
    LiveArea = 0,    // real-time capture of a screen rectangle (overlays included)
    LiveWindow = 1,  // real-time capture of one window (overlays never included)
    Solid = 2        // opaque cover, no capture at all
};

struct Settings {
    CoverStyle style = CoverStyle::LiveArea;
    bool outline = true;
    bool hideOverlays = false;
    bool enabled = false;
    bool hasRegion = false;
    RECT region = {0, 0, 0, 0};
};

namespace settings {

Settings load();
void save(const Settings& s);

bool launchAtLogin();
void setLaunchAtLogin(bool on);

// Overlay windows we hid must come back if the app is killed while they are
// hidden. They are recorded here and restored by a fresh instance.
void rememberHidden(HWND hwnd);
void forgetHidden(HWND hwnd);
std::vector<HWND> pendingHidden();
void clearPendingHidden();

}  // namespace settings
