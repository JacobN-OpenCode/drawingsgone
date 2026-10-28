# DrawingsGone for Windows

A notification-area app that mirrors your screen live so you can keep reading it
while somebody else draws over your screen share. The mirror window is marked
`WDA_EXCLUDEFROMCAPTURE`, so it never appears in anybody's share, recording, or in
its own capture loop.

## Modes

| Mode | What it does |
|---|---|
| **Live · one window** | Captures a single window on its own with `PrintWindow`, so overlays drawn *on top of it* can never appear. This is the annotation-free mode. |
| **Live · screen area** | Straight `BitBlt` of a desktop rectangle at ~30 fps. Honest pixels: anything drawn over the screen is in it. |
| **Solid cover** | An opaque blanket over the area. No capture at all. |

Plus **Also Hide Overlay Windows (experimental)**: while the filter is on, any
full-screen topmost layered/transparent window is hidden with `ShowWindow(SW_HIDE)`
and restored when the filter comes off, on quit, and by the next launch if this
process is killed while they are hidden (the list is kept in the registry for
exactly that reason). Off by default.

## Use

- Tray icon: pencil = idle, crossed pencil = filtered.
- `Ctrl+Alt+Shift+D` hide / show annotations
- `Ctrl+Alt+Shift+P` peek for five seconds
- `Ctrl+Alt+Shift+A` select an area
- `Ctrl+Alt+Shift+F` whole screen
- `Ctrl+Alt+Shift+W` pick a window to mirror

State (mode, area, outline, launch-at-login, experimental flag) is stored in
`HKCU\Software\Navaratne\DrawingsGone`. A log is written to
`%LOCALAPPDATA%\DrawingsGone\drawingsgone.log`.

Requires Windows 10 2004 (build 19041) or newer for
`WDA_EXCLUDEFROMCAPTURE`. The build is unsigned, so SmartScreen needs
"More info → Run anyway" once.

## Build

```powershell
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

`build\Release\DrawingsGone.exe` is a single file with the MSVC runtime linked
statically — nothing to install. `.github/workflows/build-windows.yml` builds it
on a Windows runner for every push.

## Layout

```
src/common.h      shared declarations, COM-free Win32 only
src/main.cpp      tray icon, menu, hotkeys, single instance
src/overlay.cpp   mirror windows, solid cover, overlay hiding
src/selector.cpp  the drag-a-rectangle picker
src/settings.cpp  registry persistence
res/app.rc        icon + version info
res/app.manifest  per-monitor-v2 DPI, as-invoker
res/app.ico       app + tray icons (generated from the macOS 1024px master)
```
