import AppKit

enum CoverStyle: Int, CaseIterable {
    case live, solid

    var title: String {
        switch self {
        case .live: return "Live (strip annotations)"
        case .solid: return "Solid cover"
        }
    }
}

final class OverlayController {
    private let defaults = UserDefaults.standard
    private var solidWindow: NSWindow?
    private var liveMirrors: [LiveMirror] = []
    private var peekWorkItem: DispatchWorkItem?

    var onStateChange: (() -> Void)?

    private(set) var isEnabled = false
    private(set) var region: NSRect?
    var style: CoverStyle {
        didSet { defaults.set(style.rawValue, forKey: "style") }
    }
    var showsOutline: Bool {
        didSet { defaults.set(showsOutline, forKey: "outline") }
    }

    init() {
        let stored = defaults.object(forKey: "style") as? Int
        if let stored, let style = CoverStyle(rawValue: stored) {
            self.style = style
        } else {
            style = .live
        }
        showsOutline = defaults.object(forKey: "outline") as? Bool ?? true
        if let saved = defaults.string(forKey: "region") {
            let rect = NSRectFromString(saved)
            if rect.width >= 24, rect.height >= 24 { region = rect }
        }
    }

    func restore() {
        guard defaults.bool(forKey: "enabled"), region != nil else { return }
        setEnabled(true)
    }

    func setRegion(_ rect: NSRect) {
        region = rect
        defaults.set(NSStringFromRect(rect), forKey: "region")
        if isEnabled {
            hideAll()
            present()
        }
    }

    func setStyle(_ newStyle: CoverStyle) {
        guard style != newStyle else { return }
        let wasEnabled = isEnabled
        if wasEnabled { hideAll() }
        style = newStyle
        if wasEnabled { present() }
        onStateChange?()
    }

    func setEnabled(_ enabled: Bool) {
        peekWorkItem?.cancel()
        peekWorkItem = nil

        guard enabled, hasVisibleRegion() else {
            isEnabled = false
            defaults.set(false, forKey: "enabled")
            hideAll()
            onStateChange?()
            return
        }

        isEnabled = true
        defaults.set(true, forKey: "enabled")
        present()
        onStateChange?()
    }

    func peek(seconds: TimeInterval = 5) {
        guard isEnabled else { return }
        peekWorkItem?.cancel()
        hideAll()
        let work = DispatchWorkItem { [weak self] in
            guard let self, self.isEnabled else { return }
            self.peekWorkItem = nil
            self.present()
        }
        peekWorkItem = work
        DispatchQueue.main.asyncAfter(deadline: .now() + seconds, execute: work)
    }

    func screensChanged() {
        guard isEnabled else { return }
        hideAll()
        present()
    }

    /// The mirror sits above the menu bar, so drop it while a menu is open.
    func setMenuOpen(_ open: Bool) {
        guard isEnabled else { return }
        if open { hideAll() } else { present() }
    }

    // MARK: - Presentation

    private func present() {
        guard let region else { return }
        switch style {
        case .live:
            for screen in NSScreen.screens {
                let rect = region.intersection(screen.frame)
                guard !rect.isNull, rect.width >= 16, rect.height >= 16 else { continue }
                let mirror = LiveMirror()
                liveMirrors.append(mirror)
                mirror.show(region: rect, outline: showsOutline)
            }
        case .solid:
            if let rect = clippedRegion() { showSolid(rect) }
        }
    }

    private func hideAll() {
        for mirror in liveMirrors { mirror.hide() }
        liveMirrors.removeAll()
        solidWindow?.orderOut(nil)
        solidWindow = nil
    }

    private func hasVisibleRegion() -> Bool {
        guard let region else { return false }
        return NSScreen.screens.contains { screen in
            let rect = region.intersection(screen.frame)
            return !rect.isNull && rect.width >= 16 && rect.height >= 16
        }
    }

    private func clippedRegion() -> NSRect? {
        guard let region else { return nil }
        let center = NSPoint(x: region.midX, y: region.midY)
        let screen = NSScreen.screens.first { $0.frame.contains(center) } ?? NSScreen.main
        guard let screen else { return nil }
        var clipped = region.intersection(screen.frame)
        if clipped.isNull || clipped.isEmpty {
            clipped = region.intersection(NSScreen.screens.first?.frame ?? .zero)
        }
        guard !clipped.isNull, clipped.width >= 16, clipped.height >= 16 else { return nil }
        return clipped
    }

    private func showSolid(_ rect: NSRect) {
        if solidWindow == nil {
            let window = NSWindow(contentRect: .zero,
                                  styleMask: .borderless,
                                  backing: .buffered,
                                  defer: false)
            window.level = NSWindow.Level(rawValue: Int(CGShieldingWindowLevel()))
            window.collectionBehavior = [.canJoinAllSpaces, .stationary,
                                         .fullScreenAuxiliary, .ignoresCycle]
            window.ignoresMouseEvents = true
            window.hasShadow = false
            window.isOpaque = true
            window.backgroundColor = .windowBackgroundColor
            window.isReleasedWhenClosed = false
            window.isMovable = false
            window.animationBehavior = .none
            solidWindow = window
        }
        guard let window = solidWindow else { return }

        let container = NSView(frame: NSRect(origin: .zero, size: rect.size))
        container.autoresizingMask = [.width, .height]
        if showsOutline {
            let border = NSView(frame: container.bounds)
            border.autoresizingMask = [.width, .height]
            border.wantsLayer = true
            border.layer?.borderWidth = 1
            border.layer?.borderColor = NSColor.labelColor.withAlphaComponent(0.16).cgColor
            container.addSubview(border)
        }
        window.contentView = container
        window.setFrame(rect, display: true)
        window.orderFrontRegardless()
    }
}
