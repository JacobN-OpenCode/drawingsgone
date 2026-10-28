// DrawingsGone for Windows — tray app.
//
// Mirrors a screen area (or a single window) live, with the mirror window marked
// WDA_EXCLUDEFROMCAPTURE so it never appears in a screen share, and offers a
// solid cover as a permission-free fallback.
#include "common.h"
#include "overlay.h"
#include "selector.h"
#include "settings.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

const wchar_t* kMainClass = L"DG_Main";
const UINT kTrayId = 1;
const int kIconId = 101;

const UINT kHotToggle = 1;
const UINT kHotPeek = 2;
const UINT kHotArea = 3;
const UINT kHotWhole = 4;
const UINT kHotWindow = 5;

Overlay gOverlay;
HWND gMain = nullptr;
HICON gIconIdle = nullptr;
HICON gIconOff = nullptr;
std::wstring gLogPath;
std::vector<WindowInfo> gMenuWindows;

std::wstring localAppData() {
    wchar_t buffer[MAX_PATH * 2] = {};
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH * 2);
    if (length == 0 || length >= MAX_PATH * 2) {
        return L".";
    }
    return buffer;
}

bool tipShown() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Navaratne\\DrawingsGone", 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return false;
    }
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    bool shown = RegQueryValueExW(key, L"TipShown", nullptr, &type,
                                  reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS &&
                 type == REG_DWORD && value != 0;
    RegCloseKey(key);
    return shown;
}

void markTipShown() {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Navaratne\\DrawingsGone", 0, nullptr, 0,
                        KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }
    DWORD one = 1;
    RegSetValueExW(key, L"TipShown", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&one),
                   sizeof(one));
    RegCloseKey(key);
}

void updateTrayIcon() {
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = gMain;
    data.uID = kTrayId;
    data.uFlags = NIF_ICON | NIF_TIP;
    data.hIcon = gOverlay.enabled() ? gIconOff : gIconIdle;
    wcscpy_s(data.szTip, gOverlay.enabled() ? L"DrawingsGone \u00b7 filtered"
                                            : L"DrawingsGone \u00b7 idle");
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

std::wstring areaLabel() {
    if (!gOverlay.hasRegion()) {
        return L"Area: whole screen";
    }
    RECT r = gOverlay.region();
    wchar_t buffer[96];
    wsprintfW(buffer, L"Area: %d \u00d7 %d", static_cast<int>(r.right - r.left),
              static_cast<int>(r.bottom - r.top));
    return buffer;
}

void appendMenu(HMENU menu, UINT flags, UINT_PTR id, const std::wstring& text) {
    AppendMenuW(menu, flags, id, text.c_str());
}

UINT check(bool on) { return on ? MF_CHECKED : MF_UNCHECKED; }

void addWindowItems(HMENU submenu) {
    gMenuWindows = listMirrorWindows();
    if (gMenuWindows.empty()) {
        appendMenu(submenu, MF_STRING | MF_DISABLED, 0, L"No windows to mirror");
        return;
    }
    size_t count = gMenuWindows.size() > 20 ? 20 : gMenuWindows.size();
    for (size_t i = 0; i < count; ++i) {
        std::wstring title = gMenuWindows[i].title;
        if (title.size() > 60) {
            title = title.substr(0, 57) + L"\u2026";
        }
        appendMenu(submenu, MF_STRING, CMD_PICK_WINDOW_FIRST + i, title);
    }
}

void showMenu() {
    gOverlay.menuOpen(true);

    HMENU menu = CreatePopupMenu();
    appendMenu(menu, MF_STRING | MF_DISABLED, 0,
               gOverlay.enabled() ? L"DrawingsGone \u00b7 filtered" : L"DrawingsGone \u00b7 idle");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    appendMenu(menu, MF_STRING | check(gOverlay.enabled()), CMD_TOGGLE, L"Hide Annotations\tCtrl+Alt+Shift+D");
    appendMenu(menu, MF_STRING | (gOverlay.enabled() ? MF_ENABLED : MF_GRAYED), CMD_PEEK,
               L"Peek for 5 Seconds\tCtrl+Alt+Shift+P");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    appendMenu(menu, MF_STRING, CMD_WHOLE, L"Whole Screen\tCtrl+Alt+Shift+F");
    appendMenu(menu, MF_STRING, CMD_SELECT, L"Select Area\u2026\tCtrl+Alt+Shift+A");

    HMENU windows = CreatePopupMenu();
    addWindowItems(windows);
    AppendMenuW(menu, MF_POPUP | MF_STRING,
                reinterpret_cast<UINT_PTR>(windows), L"Mirror a Window\u2026");
    appendMenu(menu, MF_STRING | MF_DISABLED, 0, areaLabel());
    if (gOverlay.style() == CoverStyle::LiveWindow && !gOverlay.targetTitle().empty()) {
        appendMenu(menu, MF_STRING | MF_DISABLED, 0, L"Window: " + gOverlay.targetTitle());
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    HMENU modes = CreatePopupMenu();
    appendMenu(modes,
               MF_STRING | check(gOverlay.style() == CoverStyle::LiveArea),
               CMD_STYLE_AREA, L"Live \u00b7 screen area");
    appendMenu(modes,
               MF_STRING | check(gOverlay.style() == CoverStyle::LiveWindow),
               CMD_STYLE_WINDOW, L"Live \u00b7 one window (no overlays)");
    appendMenu(modes, MF_STRING | check(gOverlay.style() == CoverStyle::Solid), CMD_STYLE_SOLID,
               L"Solid cover");
    AppendMenuW(menu, MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>(modes), L"Mode");

    appendMenu(menu, MF_STRING | check(gOverlay.outline()), CMD_OUTLINE, L"Show Outline");
    appendMenu(menu, MF_STRING | check(gOverlay.hideOverlays()), CMD_HIDE_OVERLAYS,
               L"Also Hide Overlay Windows (experimental)");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    appendMenu(menu, MF_STRING | check(settings::launchAtLogin()), CMD_LAUNCH_AT_LOGIN,
               L"Launch at Login");
    appendMenu(menu, MF_STRING, CMD_ABOUT, L"About DrawingsGone");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    appendMenu(menu, MF_STRING, CMD_QUIT, L"Quit DrawingsGone");

    POINT cursor;
    GetCursorPos(&cursor);
    SetForegroundWindow(gMain);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, cursor.x, cursor.y, 0, gMain, nullptr);
    PostMessageW(gMain, WM_NULL, 0, 0);
    DestroyMenu(menu);

    gOverlay.menuOpen(false);
}

void showAbout() {
    const wchar_t* text =
        L"DrawingsGone 1.0 for Windows\r\n\r\n"
        L"A live mirror of your screen (or of one window) so you can keep reading "
        L"your own content while someone draws over your screen share. Nothing is "
        L"frozen and the share keeps running.\r\n\r\n"
        L"The mirror window is excluded from screen captures (WDA_EXCLUDEFROMCAPTURE), "
        L"so it never shows up in anyone else's share.\r\n\r\n"
        L"Mode \u00b7 screen area: a real-time copy of a rectangle. Windows cannot remove "
        L"another app's overlay from a whole-screen capture, so drawings drawn over "
        L"the screen will still appear here.\r\n\r\n"
        L"Mode \u00b7 one window: a real-time copy of a single window. Because that window "
        L"is captured on its own, overlays drawn on top of it never appear. This is "
        L"the annotation-free mode.\r\n\r\n"
        L"Mode \u00b7 solid cover: hides the area completely, no capture needed.\r\n\r\n"
        L"Tray icon: pencil = idle, crossed pencil = filtered.\r\n\r\n"
        L"\u00a9 2026 Jacob Navaratne";
    MessageBoxW(nullptr, text, L"About DrawingsGone", MB_OK | MB_ICONINFORMATION);
}

void startRegionSelection() {
    bool wasEnabled = gOverlay.enabled();
    if (wasEnabled) {
        gOverlay.setEnabled(false);
    }
    beginRegionSelection(GetModuleHandleW(nullptr), [wasEnabled](bool chosen, RECT rect) {
        if (chosen) {
            gOverlay.setRegion(rect);
            gOverlay.setEnabled(true);
        } else if (wasEnabled) {
            gOverlay.setEnabled(true);
        }
    });
}

void enableWithTip() {
    gOverlay.toggle();
    if (gOverlay.enabled() && gOverlay.style() == CoverStyle::LiveArea && !tipShown()) {
        markTipShown();
        MessageBoxW(nullptr,
                    L"Windows will not let an app remove someone else's drawing layer from a "
                    L"whole-screen view, so drawings made over the screen still appear here.\n\n"
                    L"For a view with no overlays at all, choose Mirror a Window from this menu "
                    L"and pick the window you are sharing: that window is captured on its own, "
                    L"underneath any annotations.",
                    L"DrawingsGone", MB_OK | MB_ICONINFORMATION);
    }
}

void handleCommand(UINT id) {
    switch (id) {
        case CMD_TOGGLE:
            enableWithTip();
            break;
        case CMD_PEEK:
            gOverlay.peek();
            break;
        case CMD_WHOLE:
            gOverlay.wholeScreen();
            break;
        case CMD_SELECT:
            startRegionSelection();
            break;
        case CMD_STYLE_AREA:
            gOverlay.setStyle(CoverStyle::LiveArea);
            break;
        case CMD_STYLE_WINDOW:
            gOverlay.setStyle(CoverStyle::LiveWindow);
            if (!gOverlay.targetTitle().empty()) {
                gOverlay.setEnabled(true);
            }
            break;
        case CMD_STYLE_SOLID:
            gOverlay.setStyle(CoverStyle::Solid);
            break;
        case CMD_OUTLINE:
            gOverlay.setOutline(!gOverlay.outline());
            break;
        case CMD_HIDE_OVERLAYS:
            gOverlay.setHideOverlays(!gOverlay.hideOverlays());
            break;
        case CMD_LAUNCH_AT_LOGIN:
            settings::setLaunchAtLogin(!settings::launchAtLogin());
            break;
        case CMD_ABOUT:
            showAbout();
            break;
        case CMD_QUIT:
            DestroyWindow(gMain);
            break;
        default:
            if (id >= CMD_PICK_WINDOW_FIRST) {
                size_t index = id - CMD_PICK_WINDOW_FIRST;
                if (index < gMenuWindows.size()) {
                    gOverlay.setStyle(CoverStyle::LiveWindow);
                    gOverlay.setTarget(gMenuWindows[index].hwnd, gMenuWindows[index].title);
                    gOverlay.setEnabled(true);
                }
            }
            break;
    }
    updateTrayIcon();
}

void registerHotKeys() {
    const UINT mods = MOD_CONTROL | MOD_ALT | MOD_SHIFT;
    RegisterHotKey(nullptr, kHotToggle, mods, 'D');
    RegisterHotKey(nullptr, kHotPeek, mods, 'P');
    RegisterHotKey(nullptr, kHotArea, mods, 'A');
    RegisterHotKey(nullptr, kHotWhole, mods, 'F');
    RegisterHotKey(nullptr, kHotWindow, mods, 'W');
}

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_DG_TRAY:
            if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU || lparam == WM_LBUTTONUP) {
                showMenu();
            }
            return 0;
        case WM_DG_TRAYMENU:
            showMenu();
            return 0;
        case WM_HOTKEY:
            switch (wparam) {
                case kHotToggle:
                    enableWithTip();
                    updateTrayIcon();
                    return 0;
                case kHotPeek:
                    gOverlay.peek();
                    return 0;
                case kHotArea:
                    startRegionSelection();
                    return 0;
                case kHotWhole:
                    gOverlay.wholeScreen();
                    updateTrayIcon();
                    return 0;
                case kHotWindow:
                    showMenu();
                    return 0;
                default:
                    break;
            }
            return 0;
        case WM_COMMAND:
            handleCommand(LOWORD(wparam));
            return 0;
        case WM_TIMER:
            // Peek timer: put the filter back.
            KillTimer(hwnd, 1);
            gOverlay.setEnabled(true);
            updateTrayIcon();
            return 0;
        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
            gOverlay.displayChanged();
            return 0;
        case WM_ENDSESSION:
            gOverlay.shutdown();
            return 0;
        case WM_DESTROY:
            gOverlay.shutdown();
            UnregisterHotKey(nullptr, kHotToggle);
            UnregisterHotKey(nullptr, kHotPeek);
            UnregisterHotKey(nullptr, kHotArea);
            UnregisterHotKey(nullptr, kHotWhole);
            UnregisterHotKey(nullptr, kHotWindow);
            {
                NOTIFYICONDATAW data = {};
                data.cbSize = sizeof(data);
                data.hWnd = hwnd;
                data.uID = kTrayId;
                Shell_NotifyIconW(NIM_DELETE, &data);
            }
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

void dgLog(const std::wstring& message) {
    if (gLogPath.empty()) {
        return;
    }
    SYSTEMTIME now;
    GetLocalTime(&now);
    wchar_t line[1024];
    wsprintfW(line, L"[%04d-%02d-%02d %02d:%02d:%02d] %s\r\n", now.wYear, now.wMonth, now.wDay,
              now.wHour, now.wMinute, now.wSecond, message.c_str());
    FILE* file = nullptr;
    if (_wfopen_s(&file, gLogPath.c_str(), L"a, ccs=UTF-8") == 0 && file) {
        fputws(line, file);
        fclose(file);
    }
}

std::wstring exePath() {
    wchar_t buffer[MAX_PATH * 2] = {};
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH * 2);
    return std::wstring(buffer, length);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    // Per-monitor DPI awareness also comes from the embedded manifest; this
    // covers the case of an older manifest-less launch.
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetContext = BOOL(WINAPI*)(void*);
        auto setContext =
            reinterpret_cast<SetContext>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setContext) {
            setContext(reinterpret_cast<void*>(-4));  // PER_MONITOR_AWARE_V2
        }
    }

    HANDLE single = CreateMutexW(nullptr, TRUE, L"Local\\DrawingsGone.Instance");
    if (single && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"DrawingsGone is already running. Look for the pencil in the "
                             L"notification area.",
                    L"DrawingsGone", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    std::wstring logDir = localAppData() + L"\\DrawingsGone";
    CreateDirectoryW(logDir.c_str(), nullptr);
    gLogPath = logDir + L"\\drawingsgone.log";

    WNDCLASSEXW cls = {};
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = MainProc;
    cls.hInstance = instance;
    cls.lpszClassName = kMainClass;
    RegisterClassExW(&cls);

    gMain = CreateWindowExW(0, kMainClass, L"DrawingsGone", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                            instance, nullptr);
    if (!gMain) {
        return 1;
    }

    gIconIdle = LoadIconW(instance, MAKEINTRESOURCEW(kIconId));
    if (!gIconIdle) {
        gIconIdle = LoadIconW(nullptr, IDI_APPLICATION);
    }
    gIconOff = gIconIdle;

    gOverlay.init(instance, gMain);
    Overlay::restoreHiddenOverlays();

    NOTIFYICONDATAW tray = {};
    tray.cbSize = sizeof(tray);
    tray.hWnd = gMain;
    tray.uID = kTrayId;
    tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    tray.uCallbackMessage = WM_DG_TRAY;
    tray.hIcon = gIconIdle;
    wcscpy_s(tray.szTip, L"DrawingsGone \u00b7 idle");
    Shell_NotifyIconW(NIM_ADD, &tray);
    dgLog(L"started");

    registerHotKeys();
    gOverlay.restoreSession();
    updateTrayIcon();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    gOverlay.shutdown();
    if (single) {
        ReleaseMutex(single);
        CloseHandle(single);
    }
    return 0;
}
