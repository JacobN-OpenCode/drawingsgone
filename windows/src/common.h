// DrawingsGone for Windows — shared declarations.
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <shellapi.h>
#include <windowsx.h>

#include <cstdint>
#include <string>
#include <vector>

// Windows 10 2004+. mingw headers are old, so define them here.
#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif
#ifndef WDA_MONITOR
#define WDA_MONITOR 0x00000001
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

// Messages between the overlay controller and its windows.
#define WM_DG_TRAY (WM_APP + 1)
#define WM_DG_TRAYMENU (WM_APP + 2)

enum TrayCommand {
    CMD_TOGGLE = 1,
    CMD_PEEK,
    CMD_WHOLE,
    CMD_SELECT,
    CMD_WINDOW,
    CMD_STYLE_AREA,
    CMD_STYLE_WINDOW,
    CMD_STYLE_SOLID,
    CMD_OUTLINE,
    CMD_HIDE_OVERLAYS,
    CMD_LAUNCH_AT_LOGIN,
    CMD_ABOUT,
    CMD_QUIT,
    CMD_PICK_WINDOW_FIRST = 1000  // + index into the enumerated list
};

void dgLog(const std::wstring& message);
std::wstring exePath();
