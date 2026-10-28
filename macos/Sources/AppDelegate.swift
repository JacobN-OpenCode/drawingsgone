import AppKit
import Carbon.HIToolbox
import ServiceManagement

final class AppDelegate: NSObject, NSApplicationDelegate, NSMenuDelegate {
    private var statusItem: NSStatusItem!
    private let overlay = OverlayController()
    private let selector = RegionSelector()
    private var hotKeys: HotKeyManager?

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)

        overlay.onStateChange = { [weak self] in self?.updateStatusButton() }

        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        if let button = statusItem.button {
            button.image = NSImage(systemSymbolName: "pencil", accessibilityDescription: "DrawingsGone")
            button.image?.isTemplate = true
            button.toolTip = "DrawingsGone"
        }
        let menu = NSMenu()
        menu.delegate = self
        statusItem.menu = menu

        let hotKeys = HotKeyManager()
        let modifiers = UInt32(cmdKey | optionKey | controlKey)
        hotKeys.bind(keyCode: UInt32(kVK_ANSI_D), modifiers: modifiers) { [weak self] in
            self?.toggleCover()
        }
        hotKeys.bind(keyCode: UInt32(kVK_ANSI_P), modifiers: modifiers) { [weak self] in
            self?.overlay.peek()
        }
        hotKeys.bind(keyCode: UInt32(kVK_ANSI_A), modifiers: modifiers) { [weak self] in
            self?.selectArea()
        }
        hotKeys.bind(keyCode: UInt32(kVK_ANSI_F), modifiers: modifiers) { [weak self] in
            self?.wholeScreen()
        }
        self.hotKeys = hotKeys

        NotificationCenter.default.addObserver(self,
                                               selector: #selector(screensChanged),
                                               name: NSApplication.didChangeScreenParametersNotification,
                                               object: nil)

        updateStatusButton()
        overlay.restore()

        if ProcessInfo.processInfo.environment["DRAWINGSGONE_DEMO"] == "1" {
            overlay.setRegion(NSRect(x: 240, y: 240, width: 520, height: 340))
            overlay.setEnabled(true)
        }
    }

    @objc private func screensChanged() {
        overlay.screensChanged()
    }

    func menuWillOpen(_ menu: NSMenu) {
        overlay.setMenuOpen(true)
        rebuildMenu()
    }

    func menuDidClose(_ menu: NSMenu) {
        overlay.setMenuOpen(false)
    }

    private func rebuildMenu() {
        guard let menu = statusItem.menu else { return }
        menu.removeAllItems()

        let header = NSMenuItem(title: overlay.isEnabled ? "DrawingsGone · annotations hidden" : "DrawingsGone · idle",
                                action: nil, keyEquivalent: "")
        header.isEnabled = false
        menu.addItem(header)
        menu.addItem(.separator())

        let cover = NSMenuItem(title: "Hide Annotations", action: #selector(toggleCover), keyEquivalent: "d")
        cover.keyEquivalentModifierMask = [.control, .option, .command]
        cover.target = self
        cover.state = overlay.isEnabled ? .on : .off
        menu.addItem(cover)

        let peek = NSMenuItem(title: "Peek for 5 Seconds", action: #selector(peek), keyEquivalent: "p")
        peek.keyEquivalentModifierMask = [.control, .option, .command]
        peek.target = self
        peek.isEnabled = overlay.isEnabled
        menu.addItem(peek)

        menu.addItem(.separator())

        let whole = NSMenuItem(title: "Whole Screen", action: #selector(wholeScreen), keyEquivalent: "f")
        whole.keyEquivalentModifierMask = [.control, .option, .command]
        whole.target = self
        menu.addItem(whole)

        let select = NSMenuItem(title: "Select Area…", action: #selector(selectArea), keyEquivalent: "a")
        select.keyEquivalentModifierMask = [.control, .option, .command]
        select.target = self
        menu.addItem(select)

        let areaText = overlay.region.map { "Area: \(Int($0.width)) × \(Int($0.height))" } ?? "Area: whole screen"
        let area = NSMenuItem(title: areaText, action: nil, keyEquivalent: "")
        area.isEnabled = false
        menu.addItem(area)

        menu.addItem(.separator())

        let styleItem = NSMenuItem(title: "Mode", action: nil, keyEquivalent: "")
        let styleMenu = NSMenu()
        for style in CoverStyle.allCases {
            let item = NSMenuItem(title: style.title, action: #selector(chooseStyle(_:)), keyEquivalent: "")
            item.target = self
            item.tag = style.rawValue
            item.state = overlay.style == style ? .on : .off
            styleMenu.addItem(item)
        }
        styleItem.submenu = styleMenu
        menu.addItem(styleItem)

        let outline = NSMenuItem(title: "Show Outline", action: #selector(toggleOutline), keyEquivalent: "")
        outline.target = self
        outline.state = overlay.showsOutline ? .on : .off
        menu.addItem(outline)

        menu.addItem(.separator())

        let login = NSMenuItem(title: "Launch at Login", action: #selector(toggleLaunchAtLogin), keyEquivalent: "")
        login.target = self
        login.state = launchAtLoginEnabled ? .on : .off
        menu.addItem(login)

        let about = NSMenuItem(title: "About DrawingsGone", action: #selector(showAbout), keyEquivalent: "")
        about.target = self
        menu.addItem(about)

        menu.addItem(.separator())

        let quit = NSMenuItem(title: "Quit DrawingsGone", action: #selector(quit), keyEquivalent: "q")
        quit.target = self
        quit.keyEquivalentModifierMask = [.command]
        menu.addItem(quit)
    }

    private func updateStatusButton() {
        guard let button = statusItem.button else { return }
        let symbol = overlay.isEnabled ? "pencil.slash" : "pencil"
        let image = NSImage(systemSymbolName: symbol, accessibilityDescription: "DrawingsGone")
        image?.isTemplate = true
        button.image = image ?? button.image
        button.toolTip = overlay.isEnabled ? "DrawingsGone · annotations hidden" : "DrawingsGone · idle"
    }

    // MARK: - Actions

    @objc private func toggleCover() {
        guard ensureLivePermission() else { return }
        if overlay.region == nil {
            wholeScreen()
        } else {
            overlay.setEnabled(!overlay.isEnabled)
        }
    }

    @objc private func wholeScreen() {
        guard ensureLivePermission() else { return }
        guard let first = NSScreen.screens.first else { return }
        var union = first.frame
        for screen in NSScreen.screens.dropFirst() {
            union = union.union(screen.frame)
        }
        overlay.setRegion(union)
        overlay.setEnabled(true)
    }

    @objc private func peek() {
        overlay.peek()
    }

    @objc private func selectArea() {
        guard ensureLivePermission() else { return }
        let wasEnabled = overlay.isEnabled
        if wasEnabled { overlay.setEnabled(false) }
        selector.begin { [weak self] rect in
            guard let self else { return }
            if let rect {
                self.overlay.setRegion(rect)
                self.overlay.setEnabled(true)
            } else if wasEnabled {
                self.overlay.setEnabled(true)
            }
        }
    }

    /// The live mirror needs Screen Recording. Returns false if the user was
    /// sent to System Settings; falls back to Solid if they decline.
    private func ensureLivePermission() -> Bool {
        guard overlay.style == .live, !SnapshotPermission.granted else { return true }
        SnapshotPermission.request()
        if SnapshotPermission.granted { return true }

        let alert = NSAlert()
        alert.messageText = "Screen Recording permission needed"
        alert.informativeText = """
        DrawingsGone shows a live view of your screen with annotation layers filtered out. \
        That needs Screen Recording permission.

        Allow DrawingsGone in System Settings → Privacy & Security → Screen Recording, \
        then relaunch it. Your screen share itself is unaffected.

        Or use the solid cover, which needs no permission.
        """
        alert.addButton(withTitle: "Open System Settings")
        alert.addButton(withTitle: "Use Solid Cover")
        NSApp.activate(ignoringOtherApps: true)
        if alert.runModal() == .alertFirstButtonReturn {
            NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture")!)
            return false
        }
        overlay.setStyle(.solid)
        return true
    }

    @objc private func chooseStyle(_ sender: NSMenuItem) {
        guard let style = CoverStyle(rawValue: sender.tag) else { return }
        if style == .live {
            guard ensureLivePermission() else { return }
            if overlay.style != .live { return }
        }
        overlay.setStyle(style)
    }

    @objc private func toggleOutline() {
        overlay.showsOutline.toggle()
        if overlay.isEnabled {
            overlay.setEnabled(false)
            overlay.setEnabled(true)
        }
    }

    private var launchAtLoginEnabled: Bool {
        if #available(macOS 13.0, *) {
            return SMAppService.mainApp.status == .enabled
        }
        return false
    }

    @objc private func toggleLaunchAtLogin() {
        guard #available(macOS 13.0, *) else { return }
        let service = SMAppService.mainApp
        do {
            if service.status == .enabled {
                try service.unregister()
            } else {
                try service.register()
            }
        } catch {
            let alert = NSAlert()
            alert.messageText = "Could not change login item"
            alert.informativeText = error.localizedDescription
            alert.addButton(withTitle: "OK")
            NSApp.activate(ignoringOtherApps: true)
            alert.runModal()
        }
    }

    @objc private func showAbout() {
        let credits = NSAttributedString(string: """
        A live view of your screen (whole screen or one area) with annotation and drawing \
        overlays filtered out. Nothing is frozen: the picture keeps updating, and your \
        screen share keeps running. The mirror window is marked not to be captured.

        Needs Screen Recording permission. A solid cover is available as a fallback.

        Menu bar: pencil = idle, crossed pencil = annotations hidden.
        """, attributes: [
            .font: NSFont.systemFont(ofSize: 11),
            .foregroundColor: NSColor.secondaryLabelColor
        ])
        NSApp.activate(ignoringOtherApps: true)
        NSApp.orderFrontStandardAboutPanel(options: [
            .applicationName: "DrawingsGone",
            .applicationVersion: "1.1",
            .version: "1.1",
            .credits: credits
        ])
    }

    @objc private func quit() {
        NSApp.terminate(nil)
    }
}

enum SnapshotPermission {
    static var granted: Bool { CGPreflightScreenCaptureAccess() }
    static func request() { CGRequestScreenCaptureAccess() }
}
