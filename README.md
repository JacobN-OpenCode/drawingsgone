# DrawingsGone

A live view of your own screen with annotation and drawing layers filtered out.
Nothing is frozen, and your screen share keeps running.

Downloads live at **https://drawingsgone.navaratne.uk** — `docs/` in this repo is
that site.

| | |
|---|---|
| Windows | `docs/downloads/DrawingsGone-1.0-windows-x64.exe` · Win10 2004+, 64-bit |
| macOS | `docs/downloads/DrawingsGone-1.0-macos.zip` · macOS 13+ |

## What it is for

You are presenting. Someone else — a huddle, a tutor, a collaborator — draws all
over your screen. Your screen share is fine; the problem is that *you* can no
longer read your own content underneath the scribbles.

DrawingsGone mirrors your screen (or one rectangle, or one window) in a window
that sits above everything, keeps updating in real time, and is marked
impossible to capture, so it never appears to anybody watching. Toggle it off and
you see the real thing again.

## Layout

```
macos/    Swift menu-bar app (ScreenCaptureKit, Swift 5, macOS 13+)
windows/  Win32 tray app (C++17, MSVC, no runtime to install)
docs/     the download site, plus both built binaries
```

## Building

### macOS

```sh
cd macos
./build.sh                  # -> build/DrawingsGone.app, ad-hoc signed
cp -R build/DrawingsGone.app ~/Applications/
```

Needs the Swift toolchain (Xcode Command Line Tools are enough).

### Windows

```powershell
cmake -S windows -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# -> build\Release\DrawingsGone.exe
```

Needs Visual Studio 2022 (MSVC) and CMake. `.github/workflows/build-windows.yml`
does exactly this on every push to `windows/` and uploads the `.exe` as an
artifact, which is also how impatient people avoid installing MSVC.

Cross-compiling from Linux/macOS with mingw-w64 also works, because the app is
plain Win32 + GDI with no MSVC-specific code:

```sh
x86_64-w64-mingw32-windres windows/res/app.rc -o build/app.res.o
x86_64-w64-mingw32-g++ -std=c++17 -O2 -mwindows -static \
  -DUNICODE -D_UNICODE -Iwindows/src windows/src/*.cpp build/app.res.o \
  -o DrawingsGone.exe -luser32 -lgdi32 -lshell32 -ladvapi32 -lmsimg32
```

## How the filtering actually works

**macOS** captures the display with ScreenCaptureKit and passes the drawing
overlays to `SCContentFilter(display:excludingWindows:)`. Overlays are picked out
by layer height and area (layer ≥ 500 covering more than 30% of the screen), plus
anything full-screen owned by a conferencing app. The mirror window is
`sharingType = .none`.

**Windows** has no way for one app to strip another app's full-screen overlay out
of a whole-desktop capture, so the annotation-free mode is *per window*: the
chosen window is captured on its own with `PrintWindow(hwnd, dc,
PW_RENDERFULLCONTENT)`, which by definition cannot include windows stacked on
top of it. The mirror window is `SetWindowDisplayAffinity(hwnd,
WDA_EXCLUDEFROMCAPTURE)`. Screen-area mode is a straight `BitBlt` of the desktop,
so it shows whatever is really on the screen, drawings included. An experimental
option also hides detected full-screen topmost overlay windows for as long as the
filter is on, and restores them on exit (and on the next launch, if the process
was killed first).

## Licence

© 2026 Jacob Navaratne. Ask if you want to use it for something.
