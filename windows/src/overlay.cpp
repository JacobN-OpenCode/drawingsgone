// DrawingsGone for Windows — mirror and cover windows.
//
// Two different things are on screen here:
//   * mirror windows: a real-time copy of the screen (or of one window), marked
//     with WDA_EXCLUDEFROMCAPTURE so they never show up in anyone else's screen
//     share, recording or our own capture loop.
//   * the solid cover: a plain opaque window that is *meant* to be captured.
#include "overlay.h"

#include <algorithm>

namespace {

const wchar_t* kMirrorClass = L"DG_Mirror";
const wchar_t* kSolidClass = L"DG_Solid";
const int kAreaFrameMs = 33;   // ~30 fps
const int kWindowFrameMs = 50; // PrintWindow is not free; ~20 fps

struct Mirror {
    HWND hwnd = nullptr;
    RECT target = {0, 0, 0, 0};  // where it sits on the virtual screen
    int width = 0;
    int height = 0;
    HDC memory = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous = nullptr;
    bool windowMode = false;
    HWND source = nullptr;
    bool outline = true;

    ~Mirror() { release(); }

    void release() {
        if (memory && previous) {
            SelectObject(memory, previous);
        }
        previous = nullptr;
        if (bitmap) {
            DeleteObject(bitmap);
            bitmap = nullptr;
        }
        if (memory) {
            DeleteDC(memory);
            memory = nullptr;
        }
        width = 0;
        height = 0;
    }

    void ensure(int w, int h) {
        if (w <= 0 || h <= 0) {
            return;
        }
        if (w == width && h == height && memory) {
            return;
        }
        release();
        HDC screen = GetDC(nullptr);
        memory = CreateCompatibleDC(screen);
        bitmap = CreateCompatibleBitmap(screen, w, h);
        ReleaseDC(nullptr, screen);
        if (!memory || !bitmap) {
            release();
            return;
        }
        previous = SelectObject(memory, bitmap);
        width = w;
        height = h;
    }

    // Fills the mirror bitmap with the live picture.
    bool capture() {
        if (windowMode) {
            if (!source || !IsWindow(source)) {
                return false;
            }
            RECT rect = {0, 0, 0, 0};
            if (!GetWindowRect(source, &rect)) {
                return false;
            }
            int w = rect.right - rect.left;
            int h = rect.bottom - rect.top;
            if (w < 8 || h < 8) {
                return false;
            }
            ensure(w, h);
            if (!memory) {
                return false;
            }
            if (target.left != rect.left || target.top != rect.top) {
                target = {rect.left, rect.top, rect.right, rect.bottom};
                SetWindowPos(hwnd, HWND_TOPMOST, target.left, target.top, w, h,
                             SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            }
            if (!PrintWindow(source, memory, PW_RENDERFULLCONTENT)) {
                HDC windowDc = GetWindowDC(source);
                if (windowDc) {
                    BitBlt(memory, 0, 0, w, h, windowDc, 0, 0, SRCCOPY | CAPTUREBLT);
                    ReleaseDC(source, windowDc);
                }
            }
            return true;
        }

        int w = target.right - target.left;
        int h = target.bottom - target.top;
        if (w < 8 || h < 8) {
            return false;
        }
        ensure(w, h);
        if (!memory) {
            return false;
        }
        HDC screen = GetDC(nullptr);
        if (!screen) {
            return false;
        }
        BitBlt(memory, 0, 0, w, h, screen, target.left, target.top, SRCCOPY | CAPTUREBLT);
        ReleaseDC(nullptr, screen);
        return true;
    }

    void blit(HDC destination) {
        if (!memory) {
            return;
        }
        RECT client = {0, 0, 0, 0};
        GetClientRect(hwnd, &client);
        if (client.right <= 0 || client.bottom <= 0) {
            return;
        }
        SetStretchBltMode(destination, HALFTONE);
        SetBrushOrgEx(destination, 0, 0, nullptr);
        StretchBlt(destination, 0, 0, client.right, client.bottom, memory, 0, 0, width, height,
                   SRCCOPY);
        if (outline) {
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(64, 68, 76));
            HGDIOBJ oldPen = SelectObject(destination, pen);
            HGDIOBJ oldBrush = SelectObject(destination, GetStockObject(NULL_BRUSH));
            Rectangle(destination, 0, 0, client.right, client.bottom);
            SelectObject(destination, oldBrush);
            SelectObject(destination, oldPen);
            DeleteObject(pen);
        }
    }
};

LRESULT CALLBACK MirrorProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Mirror* mirror = reinterpret_cast<Mirror*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (mirror) {
                mirror->blit(dc);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_TIMER: {
            if (mirror && mirror->capture()) {
                HDC dc = GetDC(hwnd);
                if (dc) {
                    mirror->blit(dc);
                    ReleaseDC(hwnd, dc);
                }
            }
            return 0;
        }
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK SolidProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT client;
            GetClientRect(hwnd, &client);
            HBRUSH background = CreateSolidBrush(RGB(16, 17, 20));
            FillRect(dc, &client, background);
            DeleteObject(background);
            if (GetPropW(hwnd, L"outline")) {
                HBRUSH border = CreateSolidBrush(RGB(52, 56, 63));
                FrameRect(dc, &client, border);
                DeleteObject(border);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

MONITORINFO monitorFor(const RECT& rect) {
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    POINT centre = {(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
    HMONITOR monitor = MonitorFromPoint(centre, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfoW(monitor, &info);
    return info;
}

RECT intersect(const RECT& a, const RECT& b) {
    RECT r;
    r.left = std::max(a.left, b.left);
    r.top = std::max(a.top, b.top);
    r.right = std::min(a.right, b.right);
    r.bottom = std::min(a.bottom, b.bottom);
    if (r.right < r.left) {
        r.right = r.left;
    }
    if (r.bottom < r.top) {
        r.bottom = r.top;
    }
    return r;
}

RECT rectOf(HWND hwnd) {
    RECT r = {0, 0, 0, 0};
    GetWindowRect(hwnd, &r);
    return r;
}

bool coversMostOfItsMonitor(const RECT& rect) {
    MONITORINFO monitor = monitorFor(rect);
    double screenArea = static_cast<double>(monitor.rcMonitor.right - monitor.rcMonitor.left) *
                        static_cast<double>(monitor.rcMonitor.bottom - monitor.rcMonitor.top);
    if (screenArea <= 0) {
        return false;
    }
    RECT clipped = intersect(rect, monitor.rcMonitor);
    double area = static_cast<double>(clipped.right - clipped.left) *
                  static_cast<double>(clipped.bottom - clipped.top);
    return area / screenArea > 0.45;
}

bool isOwnedByUs(DWORD pid) { return pid == GetCurrentProcessId(); }

BOOL CALLBACK collectOverlays(HWND hwnd, LPARAM lparam) {
    struct Context {
        std::vector<HWND>* out;
    };
    auto* context = reinterpret_cast<Context*>(lparam);

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (isOwnedByUs(pid) || !IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        return TRUE;
    }

    wchar_t className[256] = {};
    GetClassNameW(hwnd, className, 256);
    const std::wstring cls = className;
    if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd" ||
        cls == L"Shell_SecondaryTrayWnd" || cls == L"SysShadow" || cls == L"DV2ControlHost" ||
        cls.find(L"Windows.UI.Core") == 0) {
        return TRUE;
    }

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    bool layered = (exStyle & WS_EX_LAYERED) != 0;
    bool transparent = (exStyle & WS_EX_TRANSPARENT) != 0;
    bool topmost = (exStyle & WS_EX_TOPMOST) != 0;
    if (!topmost || !(layered || transparent) || (exStyle & WS_EX_TOOLWINDOW) == 0) {
        return TRUE;
    }

    RECT rect = rectOf(hwnd);
    if (rect.right - rect.left < 200 || rect.bottom - rect.top < 200) {
        return TRUE;
    }
    if (!coversMostOfItsMonitor(rect)) {
        return TRUE;
    }

    context->out->push_back(hwnd);
    return TRUE;
}

std::vector<WindowInfo> gWindowList;

BOOL CALLBACK collectWindows(HWND hwnd, LPARAM lparam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (isOwnedByUs(pid) || !IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        return TRUE;
    }
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_TOOLWINDOW) != 0) {
        return TRUE;
    }
    int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) {
        return TRUE;
    }
    std::wstring title(length + 1, L'\0');
    GetWindowTextW(hwnd, title.data(), length + 1);
    title.resize(static_cast<size_t>(length));
    RECT rect = rectOf(hwnd);
    if (rect.right - rect.left < 120 || rect.bottom - rect.top < 90) {
        return TRUE;
    }
    gWindowList.push_back({hwnd, title});
    return TRUE;
}

}  // namespace

std::vector<WindowInfo> listMirrorWindows() {
    gWindowList.clear();
    EnumWindows(collectWindows, 0);
    if (gWindowList.size() > 40) {
        gWindowList.resize(40);
    }
    return gWindowList;
}

void Overlay::init(HINSTANCE instance, HWND notify) {
    instance_ = instance;
    notify_ = notify;

    WNDCLASSEXW mirrorClass = {};
    mirrorClass.cbSize = sizeof(mirrorClass);
    mirrorClass.style = CS_DBLCLKS;
    mirrorClass.lpfnWndProc = MirrorProc;
    mirrorClass.hInstance = instance;
    mirrorClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    mirrorClass.lpszClassName = kMirrorClass;
    RegisterClassExW(&mirrorClass);

    WNDCLASSEXW solidClass = {};
    solidClass.cbSize = sizeof(solidClass);
    solidClass.style = CS_DBLCLKS;
    solidClass.lpfnWndProc = SolidProc;
    solidClass.hInstance = instance;
    solidClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    solidClass.lpszClassName = kSolidClass;
    RegisterClassExW(&solidClass);

    Settings stored = settings::load();
    style_ = stored.style;
    outline_ = stored.outline;
    hideOverlays_ = stored.hideOverlays;
    hasRegion_ = stored.hasRegion;
    region_ = stored.region;
}

void Overlay::shutdown() {
    peekTimer_ = 0;
    hideAll();
    restoreDetectedOverlays();
}

void Overlay::persist() const {
    Settings s;
    s.style = style_;
    s.outline = outline_;
    s.hideOverlays = hideOverlays_;
    s.enabled = enabled_;
    s.hasRegion = hasRegion_;
    s.region = region_;
    settings::save(s);
}

void Overlay::setStyle(CoverStyle style) {
    if (style_ == style) {
        return;
    }
    bool wasEnabled = enabled_;
    if (wasEnabled) {
        hideAll();
    }
    style_ = style;
    if (style_ != CoverStyle::LiveWindow) {
        target_ = nullptr;
        targetTitle_.clear();
    }
    persist();
    if (wasEnabled) {
        present();
    }
}

void Overlay::setOutline(bool on) {
    if (outline_ == on) {
        return;
    }
    outline_ = on;
    persist();
    if (enabled_) {
        hideAll();
        present();
    }
}

void Overlay::setHideOverlays(bool on) {
    if (hideOverlays_ == on) {
        return;
    }
    hideOverlays_ = on;
    persist();
    if (enabled_) {
        hideAll();
        present();
    }
}

void Overlay::setRegion(const RECT& region) {
    if (region.right - region.left < 24 || region.bottom - region.top < 24) {
        return;
    }
    region_ = region;
    hasRegion_ = true;
    persist();
    if (enabled_) {
        hideAll();
        present();
    }
}

void Overlay::setTarget(HWND window, const std::wstring& title) {
    target_ = window;
    targetTitle_ = title;
    if (!window) {
        return;
    }
    RECT rect = rectOf(window);
    if (rect.right - rect.left >= 24 && rect.bottom - rect.top >= 24) {
        setRegion(rect);
    }
}

void Overlay::setEnabled(bool enabled) {
    peekTimer_ = 0;
    if (!enabled) {
        enabled_ = false;
        persist();
        hideAll();
        return;
    }
    if (!regionOnSomeMonitor()) {
        enabled_ = false;
        persist();
        hideAll();
        return;
    }
    enabled_ = true;
    persist();
    hideAll();
    present();
}

void Overlay::toggle() {
    if (!enabled_ && !hasRegion_) {
        wholeScreen();
        return;
    }
    setEnabled(!enabled_);
}

void Overlay::peek() {
    if (!enabled_) {
        return;
    }
    hideAll();
    peekTimer_ = SetTimer(notify_, 1, 5000, nullptr);
}

void Overlay::wholeScreen() {
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    setRegion({x, y, x + w, y + h});
    setEnabled(true);
}

void Overlay::displayChanged() {
    if (!enabled_) {
        return;
    }
    hideAll();
    present();
}

void Overlay::menuOpen(bool open) {
    if (!enabled_) {
        return;
    }
    if (open) {
        hideAll();
    } else {
        present();
    }
}

void Overlay::restoreSession() {
    Settings stored = settings::load();
    if (!stored.enabled || !hasRegion_) {
        return;
    }
    setEnabled(true);
}

bool Overlay::regionOnSomeMonitor() const {
    if (!hasRegion_) {
        return false;
    }
    struct Context {
        const RECT* region;
        bool found;
    };
    Context context{&region_, false};
    EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR monitor, HDC, LPRECT, LPARAM lparam) -> BOOL {
            auto* ctx = reinterpret_cast<Context*>(lparam);
            MONITORINFO info = {};
            info.cbSize = sizeof(info);
            if (!GetMonitorInfoW(monitor, &info)) {
                return TRUE;
            }
            RECT clipped = intersect(*ctx->region, info.rcMonitor);
            if (clipped.right - clipped.left >= 16 && clipped.bottom - clipped.top >= 16) {
                ctx->found = true;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&context));
    return context.found;
}

void Overlay::present() {
    if (!hasRegion_) {
        return;
    }

    if (hideOverlays_ && style_ != CoverStyle::Solid) {
        hideDetectedOverlays();
    }

    if (style_ == CoverStyle::Solid) {
        MONITORINFO monitor = monitorFor(region_);
        RECT rect = intersect(region_, monitor.rcMonitor);
        if (rect.right - rect.left < 16 || rect.bottom - rect.top < 16) {
            return;
        }
        solid_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT |
                                     WS_EX_NOACTIVATE,
                                 kSolidClass, L"DrawingsGone cover", WS_POPUP, rect.left,
                                 rect.top, rect.right - rect.left, rect.bottom - rect.top,
                                 nullptr, nullptr, instance_, nullptr);
        if (solid_) {
            SetPropW(solid_, L"outline", reinterpret_cast<HANDLE>(outline_ ? 1 : 0));
            SetWindowPos(solid_, HWND_TOPMOST, rect.left, rect.top, rect.right - rect.left,
                         rect.bottom - rect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
        return;
    }

    if (style_ == CoverStyle::LiveWindow) {
        if (!target_ || !IsWindow(target_)) {
            dgLog(L"window mirror: target is gone");
            return;
        }
        RECT rect = rectOf(target_);
        if (rect.right - rect.left < 16 || rect.bottom - rect.top < 16) {
            return;
        }
        auto* mirror = new Mirror();
        mirror->windowMode = true;
        mirror->source = target_;
        mirror->target = rect;
        mirror->outline = outline_;
        mirror->hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT |
                                           WS_EX_NOACTIVATE | WS_EX_LAYERED,
                                       kMirrorClass, L"DrawingsGone mirror", WS_POPUP, rect.left,
                                       rect.top, rect.right - rect.left, rect.bottom - rect.top,
                                       nullptr, nullptr, instance_, nullptr);
        if (!mirror->hwnd) {
            delete mirror;
            return;
        }
        SetLayeredWindowAttributes(mirror->hwnd, 0, 255, LWA_ALPHA);
        SetWindowLongPtrW(mirror->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(mirror));
        SetWindowDisplayAffinity(mirror->hwnd, WDA_EXCLUDEFROMCAPTURE);
        SetWindowPos(mirror->hwnd, HWND_TOPMOST, rect.left, rect.top, rect.right - rect.left,
                     rect.bottom - rect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        SetTimer(mirror->hwnd, 1, kWindowFrameMs, nullptr);
        mirrors_.push_back(mirror->hwnd);
        mirror->capture();
        HDC dc = GetDC(mirror->hwnd);
        if (dc) {
            mirror->blit(dc);
            ReleaseDC(mirror->hwnd, dc);
        }
        dgLog(L"window mirror on one window");
        return;
    }

    // LiveArea: one mirror per monitor the region touches.
    EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR monitor, HDC, LPRECT, LPARAM lparam) -> BOOL {
            auto* self = reinterpret_cast<Overlay*>(lparam);
            MONITORINFO info = {};
            info.cbSize = sizeof(info);
            if (!GetMonitorInfoW(monitor, &info)) {
                return TRUE;
            }
            RECT rect = intersect(self->region_, info.rcMonitor);
            if (rect.right - rect.left < 16 || rect.bottom - rect.top < 16) {
                return TRUE;
            }
            auto* mirror = new Mirror();
            mirror->windowMode = false;
            mirror->target = rect;
            mirror->outline = self->outline_;
            mirror->hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT |
                                               WS_EX_NOACTIVATE | WS_EX_LAYERED,
                                           kMirrorClass, L"DrawingsGone mirror", WS_POPUP,
                                           rect.left, rect.top, rect.right - rect.left,
                                           rect.bottom - rect.top, nullptr, nullptr,
                                           self->instance_, nullptr);
            if (!mirror->hwnd) {
                delete mirror;
                return TRUE;
            }
            SetLayeredWindowAttributes(mirror->hwnd, 0, 255, LWA_ALPHA);
            SetWindowLongPtrW(mirror->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(mirror));
            SetWindowDisplayAffinity(mirror->hwnd, WDA_EXCLUDEFROMCAPTURE);
            SetWindowPos(mirror->hwnd, HWND_TOPMOST, rect.left, rect.top, rect.right - rect.left,
                         rect.bottom - rect.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            SetTimer(mirror->hwnd, 1, kAreaFrameMs, nullptr);
            self->mirrors_.push_back(mirror->hwnd);
            mirror->capture();
            HDC dc = GetDC(mirror->hwnd);
            if (dc) {
                mirror->blit(dc);
                ReleaseDC(mirror->hwnd, dc);
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(this));
    dgLog(L"area mirror presented");
}

void Overlay::hideAll() {
    for (HWND handle : mirrors_) {
        if (!IsWindow(handle)) {
            continue;
        }
        KillTimer(handle, 1);
        auto* mirror = reinterpret_cast<Mirror*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
        SetWindowLongPtrW(handle, GWLP_USERDATA, 0);
        DestroyWindow(handle);
        delete mirror;
    }
    mirrors_.clear();

    if (solid_ && IsWindow(solid_)) {
        DestroyWindow(solid_);
    }
    solid_ = nullptr;

    if (peekTimer_) {
        KillTimer(notify_, 1);
        peekTimer_ = 0;
    }

    restoreDetectedOverlays();
}

void Overlay::hideDetectedOverlays() {
    std::vector<HWND> found;
    struct Context {
        std::vector<HWND>* out;
    } context{&found};
    EnumWindows(collectOverlays, reinterpret_cast<LPARAM>(&context));
    for (HWND window : found) {
        if (std::find(hiddenOverlays_.begin(), hiddenOverlays_.end(), window) !=
            hiddenOverlays_.end()) {
            continue;
        }
        settings::rememberHidden(window);
        ShowWindow(window, SW_HIDE);
        hiddenOverlays_.push_back(window);
        dgLog(L"hid overlay window");
    }
}

void Overlay::restoreDetectedOverlays() {
    for (HWND window : hiddenOverlays_) {
        if (IsWindow(window)) {
            ShowWindow(window, SW_SHOWNOACTIVATE);
        }
        settings::forgetHidden(window);
    }
    hiddenOverlays_.clear();
}

// A previous run may have been killed while overlay windows were hidden.
void Overlay::restoreHiddenOverlays() {
    std::vector<HWND> pending = settings::pendingHidden();
    for (HWND window : pending) {
        if (IsWindow(window)) {
            ShowWindow(window, SW_SHOWNOACTIVATE);
        }
    }
    if (!pending.empty()) {
        settings::clearPendingHidden();
        dgLog(L"restored overlays hidden by a previous run");
    }
}
