import AppKit

final class SelectionWindow: NSWindow {
    override var canBecomeKey: Bool { true }
    override var canBecomeMain: Bool { true }
}

final class SelectionView: NSView {
    var onFinish: ((NSRect?) -> Void)?
    private var startPoint: NSPoint?
    private var currentRect: NSRect?

    override var acceptsFirstResponder: Bool { true }

    override func resetCursorRects() {
        addCursorRect(bounds, cursor: .crosshair)
    }

    override func draw(_ dirtyRect: NSRect) {
        let shade = NSBezierPath(rect: bounds)
        if let rect = currentRect { shade.append(NSBezierPath(rect: rect)) }
        shade.windingRule = .evenOdd
        NSColor.black.withAlphaComponent(0.38).setFill()
        shade.fill()

        let hint = "Drag to select the area to keep covered    Esc to cancel"
        let attrs: [NSAttributedString.Key: Any] = [
            .font: NSFont.systemFont(ofSize: 15, weight: .medium),
            .foregroundColor: NSColor.white.withAlphaComponent(0.92)
        ]
        let textSize = (hint as NSString).size(withAttributes: attrs)
        let origin = NSPoint(x: bounds.midX - textSize.width / 2, y: bounds.maxY - 74)
        let pill = NSRect(x: origin.x - 18, y: origin.y - 11,
                          width: textSize.width + 36, height: textSize.height + 22)
        NSColor.black.withAlphaComponent(0.55).setFill()
        NSBezierPath(roundedRect: pill, xRadius: 11, yRadius: 11).fill()
        (hint as NSString).draw(at: origin, withAttributes: attrs)

        if let rect = currentRect, rect.width > 1, rect.height > 1 {
            NSColor(calibratedRed: 0.925, green: 0.216, blue: 0.314, alpha: 1).setStroke()
            let outline = NSBezierPath(rect: rect)
            outline.lineWidth = 2
            outline.stroke()

            let label = "\(Int(rect.width)) × \(Int(rect.height))"
            let labelAttrs: [NSAttributedString.Key: Any] = [
                .font: NSFont.monospacedDigitSystemFont(ofSize: 12, weight: .semibold),
                .foregroundColor: NSColor.white
            ]
            let labelSize = (label as NSString).size(withAttributes: labelAttrs)
            let badge = NSRect(x: rect.minX + 8, y: rect.minY + 8,
                               width: labelSize.width + 16, height: labelSize.height + 8)
            NSColor(calibratedRed: 0.925, green: 0.216, blue: 0.314, alpha: 1).setFill()
            NSBezierPath(roundedRect: badge, xRadius: 6, yRadius: 6).fill()
            (label as NSString).draw(at: NSPoint(x: badge.minX + 8, y: badge.minY + 4),
                                     withAttributes: labelAttrs)
        }
    }

    override func mouseDown(with event: NSEvent) {
        startPoint = convert(event.locationInWindow, from: nil)
        currentRect = nil
        needsDisplay = true
    }

    override func mouseDragged(with event: NSEvent) {
        guard let start = startPoint else { return }
        let point = convert(event.locationInWindow, from: nil)
        currentRect = NSRect(x: min(start.x, point.x), y: min(start.y, point.y),
                             width: abs(point.x - start.x), height: abs(point.y - start.y))
        needsDisplay = true
    }

    override func mouseUp(with event: NSEvent) {
        guard let rect = currentRect, rect.width >= 24, rect.height >= 24, let window else {
            onFinish?(nil)
            return
        }
        let inWindow = convert(rect, to: nil)
        onFinish?(window.convertToScreen(inWindow))
    }

    override func keyDown(with event: NSEvent) {
        if event.keyCode == 53 { onFinish?(nil) } else { super.keyDown(with: event) }
    }

    override func cancelOperation(_ sender: Any?) {
        onFinish?(nil)
    }
}

final class RegionSelector {
    private var windows: [NSWindow] = []
    private var completion: ((NSRect?) -> Void)?
    private var keyMonitor: Any?

    func begin(completion: @escaping (NSRect?) -> Void) {
        guard windows.isEmpty else { return }
        self.completion = completion
        NSApp.activate(ignoringOtherApps: true)

        for screen in NSScreen.screens {
            let window = SelectionWindow(contentRect: screen.frame,
                                         styleMask: .borderless,
                                         backing: .buffered,
                                         defer: false)
            window.level = .screenSaver
            window.backgroundColor = .clear
            window.isOpaque = false
            window.hasShadow = false
            window.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .stationary]
            window.animationBehavior = .none

            let view = SelectionView(frame: NSRect(origin: .zero, size: screen.frame.size))
            view.onFinish = { [weak self] rect in self?.finish(rect) }
            window.contentView = view
            window.makeKeyAndOrderFront(nil)
            window.makeFirstResponder(view)
            windows.append(window)
        }

        let mouse = NSEvent.mouseLocation
        windows.first { $0.frame.contains(mouse) }?.makeKey()

        keyMonitor = NSEvent.addLocalMonitorForEvents(matching: .keyDown) { [weak self] event in
            if event.keyCode == 53 {
                self?.finish(nil)
                return nil
            }
            return event
        }
    }

    private func finish(_ rect: NSRect?) {
        guard let completion else { return }
        self.completion = nil
        if let keyMonitor { NSEvent.removeMonitor(keyMonitor) }
        keyMonitor = nil
        let closing = windows
        windows = []
        for window in closing { window.orderOut(nil) }
        DispatchQueue.main.async { completion(rect) }
    }
}
