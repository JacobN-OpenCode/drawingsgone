// DrawingsGone for Windows — the mirror / cover windows.
#pragma once

#include "common.h"
#include "settings.h"

struct WindowInfo {
    HWND hwnd = nullptr;
    std::wstring title;
};

// Top-level windows that could usefully be mirrored, in z-order.
std::vector<WindowInfo> listMirrorWindows();

class Overlay {
public:
    void init(HINSTANCE instance, HWND notify);
    void shutdown();

    CoverStyle style() const { return style_; }
    bool enabled() const { return enabled_; }
    bool outline() const { return outline_; }  // 1px border around the filtered area
    bool hideOverlays() const { return hideOverlays_; }
    bool hasRegion() const { return hasRegion_; }
    RECT region() const { return region_; }
    std::wstring targetTitle() const { return targetTitle_; }
    bool hasTarget() const { return target_ && IsWindow(target_) && !IsIconic(target_); }

    void setStyle(CoverStyle style);
    void setOutline(bool on);
    void setHideOverlays(bool on);
    void setRegion(const RECT& r);
    void setTarget(HWND window, const std::wstring& title);
    void setEnabled(bool enabled);
    void toggle();
    void peek();
    void wholeScreen();
    void displayChanged();
    void menuOpen(bool open);

    void restoreSession();
    static void restoreHiddenOverlays();

private:
    void present();
    void hideAll();
    bool regionOnSomeMonitor() const;
    void persist() const;
    void hideDetectedOverlays();
    void restoreDetectedOverlays();

    HINSTANCE instance_ = nullptr;
    HWND notify_ = nullptr;

    CoverStyle style_ = CoverStyle::LiveArea;
    bool enabled_ = false;
    bool outline_ = true;
    bool hideOverlays_ = false;
    bool hasRegion_ = false;
    RECT region_ = {0, 0, 0, 0};

    HWND target_ = nullptr;
    std::wstring targetTitle_;

    std::vector<HWND> mirrors_;
    HWND solid_ = nullptr;
    std::vector<HWND> hiddenOverlays_;
    UINT_PTR peekTimer_ = 0;
};
