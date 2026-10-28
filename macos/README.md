# DrawingsGone

A menu-bar app that shows your screen live, with annotation and drawing overlay
windows filtered out. Nothing is frozen, and your screen share keeps running.

Installed at `~/Applications/DrawingsGone.app` (menu-bar only, no Dock icon).

## Use

- Menu-bar icon: pencil = idle, crossed pencil = annotations hidden.
- **Hide Annotations** (`⌃⌥⌘D`) toggles the live filtered view.
- **Whole Screen** (`⌃⌥⌘F`) filters the display under the mouse.
- **Select Area…** (`⌃⌥⌘A`) filters just one rectangle. Esc cancels.
- **Peek for 5 Seconds** (`⌃⌥⌘P`) drops the filter temporarily.
- **Mode**
  - *Live (strip annotations)* is the default. It captures the screen in real
    time and excludes high-layer full-screen overlay windows, which is how
    drawing tools such as Slick (`dev.slick.byoe.handoff`, layer 1000) and
    conferencing annotations render.
  - *Solid cover* is a permission-free fallback that hides the area completely.
- **Show Outline** draws a 1px border around the filtered area.
- **Launch at Login** uses macOS login items.

The mirror window is marked `sharingType = .none`, so it is excluded from
screen shares and recordings. The menu drops the mirror automatically while it
is open.

## Permissions

The live filter needs **Screen Recording** (System Settings → Privacy &
Security → Screen Recording). If macOS asks, enable DrawingsGone and relaunch.
`~/Library/Logs/DrawingsGone.log` records excluded windows when diagnosing a
drawing layer that is not being caught.

## Build

```sh
cd ~/DrawingsGone
./build.sh                 # -> build/DrawingsGone.app (ad-hoc signed)
cp -R build/DrawingsGone.app ~/Applications/
```

Icon: `swift Tools/make-icon.swift Resources/icon-1024.png`, then regenerate
`Resources/AppIcon.icns` with `sips`/`iconutil` before building.
