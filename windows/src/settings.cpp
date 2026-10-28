// DrawingsGone for Windows — persisted state.
#include "settings.h"

#include <cstring>

namespace {

const wchar_t* kAppKey = L"Software\\Navaratne\\DrawingsGone";
const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* kRunValue = L"DrawingsGone";
const wchar_t* kHiddenValue = L"HiddenOverlays";

DWORD readDword(HKEY key, const wchar_t* name, DWORD fallback) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) ==
            ERROR_SUCCESS &&
        type == REG_DWORD) {
        return value;
    }
    return fallback;
}

void writeDword(HKEY key, const wchar_t* name, DWORD value) {
    RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

}  // namespace

namespace settings {

Settings load() {
    Settings s;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAppKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return s;
    }

    int style = static_cast<int>(readDword(key, L"Style", 0));
    if (style < 0 || style > 2) {
        style = 0;
    }
    s.style = static_cast<CoverStyle>(style);
    s.outline = readDword(key, L"Outline", 1) != 0;
    s.hideOverlays = readDword(key, L"HideOverlays", 0) != 0;
    s.enabled = readDword(key, L"Enabled", 0) != 0;

    int left = static_cast<int>(readDword(key, L"RegionLeft", 0x7fffffff));
    int top = static_cast<int>(readDword(key, L"RegionTop", 0));
    int right = static_cast<int>(readDword(key, L"RegionRight", 0));
    int bottom = static_cast<int>(readDword(key, L"RegionBottom", 0));
    s.hasRegion = readDword(key, L"HasRegion", 0) != 0 && right - left >= 24 && bottom - top >= 24;
    if (s.hasRegion) {
        s.region = {left, top, right, bottom};
    }

    RegCloseKey(key);
    return s;
}

void save(const Settings& s) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kAppKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS) {
        return;
    }
    writeDword(key, L"Style", static_cast<DWORD>(s.style));
    writeDword(key, L"Outline", s.outline ? 1 : 0);
    writeDword(key, L"HideOverlays", s.hideOverlays ? 1 : 0);
    writeDword(key, L"Enabled", s.enabled ? 1 : 0);
    writeDword(key, L"HasRegion", s.hasRegion ? 1 : 0);
    if (s.hasRegion) {
        writeDword(key, L"RegionLeft", static_cast<DWORD>(s.region.left));
        writeDword(key, L"RegionTop", static_cast<DWORD>(s.region.top));
        writeDword(key, L"RegionRight", static_cast<DWORD>(s.region.right));
        writeDword(key, L"RegionBottom", static_cast<DWORD>(s.region.bottom));
    }
    RegCloseKey(key);
}

bool launchAtLogin() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t buffer[1024] = {};
    DWORD size = sizeof(buffer) - sizeof(wchar_t);
    DWORD type = 0;
    bool found = RegQueryValueExW(key, kRunValue, nullptr, &type,
                                 reinterpret_cast<LPBYTE>(buffer), &size) == ERROR_SUCCESS;
    RegCloseKey(key);
    return found && type == REG_SZ;
}

void setLaunchAtLogin(bool on) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS) {
        return;
    }
    if (on) {
        std::wstring command = L"\"" + exePath() + L"\"";
        RegSetValueExW(key, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                       static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kRunValue);
    }
    RegCloseKey(key);
}

namespace {

std::vector<HWND> readHidden() {
    std::vector<HWND> result;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAppKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return result;
    }
    DWORD size = 0;
    DWORD type = 0;
    if (RegQueryValueExW(key, kHiddenValue, nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
        type == REG_BINARY && size >= sizeof(HWND) && size % sizeof(HWND) == 0) {
        std::vector<BYTE> raw(size);
        if (RegQueryValueExW(key, kHiddenValue, nullptr, &type, raw.data(), &size) ==
            ERROR_SUCCESS) {
            size_t count = size / sizeof(HWND);
            result.resize(count);
            std::memcpy(result.data(), raw.data(), count * sizeof(HWND));
        }
    }
    RegCloseKey(key);
    return result;
}

void writeHidden(const std::vector<HWND>& handles) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kAppKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS) {
        return;
    }
    if (handles.empty()) {
        RegDeleteValueW(key, kHiddenValue);
    } else {
        RegSetValueExW(key, kHiddenValue, 0, REG_BINARY,
                       reinterpret_cast<const BYTE*>(handles.data()),
                       static_cast<DWORD>(handles.size() * sizeof(HWND)));
    }
    RegCloseKey(key);
}

}  // namespace

void rememberHidden(HWND hwnd) {
    std::vector<HWND> handles = readHidden();
    for (HWND existing : handles) {
        if (existing == hwnd) {
            return;
        }
    }
    handles.push_back(hwnd);
    writeHidden(handles);
}

void forgetHidden(HWND hwnd) {
    std::vector<HWND> handles = readHidden();
    std::vector<HWND> kept;
    for (HWND existing : handles) {
        if (existing != hwnd) {
            kept.push_back(existing);
        }
    }
    writeHidden(kept);
}

std::vector<HWND> pendingHidden() { return readHidden(); }

void clearPendingHidden() { writeHidden({}); }

}  // namespace settings
