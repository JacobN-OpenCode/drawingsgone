import AppKit
import AVFoundation
import CoreMedia
import ScreenCaptureKit

/// A live, click-through window showing a real-time capture of a screen region
/// with drawing/annotation overlay windows filtered out. The window itself is
/// marked `sharingType = .none` so it is not part of anyone else's screen share.
final class LiveMirror: NSObject, SCStreamOutput, SCStreamDelegate {
    private var window: NSWindow?
    private var mirrorView: MirrorView?
    private var stream: SCStream?
    private let sampleQueue = DispatchQueue(label: "uk.navaratne.drawingsgone.mirror",
                                           qos: .userInteractive)
    private var targetRect: NSRect = .zero
    private var currentExclusionKey = ""
    private var refreshTimer: Timer?
    private var generation = 0
    private var isShowing = false
    private var didLogFirstFrame = false

    private static let conferencing: Set<String> = [
        "com.tinyspeck.slackmacgap",
        "com.slack.Slack",
        "us.zoom.xos",
        "com.microsoft.teams2",
        "com.microsoft.teams",
        "com.cisco.webexmeetingsapp",
        "com.hnc.Discord"
    ]

    func show(region: NSRect, outline: Bool) {
        targetRect = region
        isShowing = true
        if window == nil { window = makeWindow() }
        guard let window else { return }

        let view = MirrorView(outline: outline)
        mirrorView = view
        window.contentView = view
        window.setFrame(region, display: true)
        window.orderFrontRegardless()

        start()
        startRefreshTimer()
        log("show region=\(Int(region.width))x\(Int(region.height)) at \(Int(region.minX)),\(Int(region.minY)) permission=\(CGPreflightScreenCaptureAccess())")
    }

    func hide() {
        isShowing = false
        refreshTimer?.invalidate()
        refreshTimer = nil
        generation += 1
        let old = stream
        stream = nil
        if let old { Task { try? await old.stopCapture() } }
        window?.orderOut(nil)
        window = nil
        mirrorView = nil
        log("hide")
    }

    func screensChanged() {
        guard isShowing else { return }
        start()
    }

    // MARK: - Window

    private func makeWindow() -> NSWindow {
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
        window.backgroundColor = .black
        window.isReleasedWhenClosed = false
        window.isMovable = false
        window.animationBehavior = .none
        // Keep this window out of screen shares/recordings.
        window.sharingType = .none
        return window
    }

    // MARK: - Capture

    private struct ScreenInfo {
        let screen: NSScreen
        let displayID: CGDirectDisplayID
        let scale: CGFloat
    }

    private func screenInfo() -> ScreenInfo? {
        let center = NSPoint(x: targetRect.midX, y: targetRect.midY)
        let screen = NSScreen.screens.first { $0.frame.contains(center) } ?? NSScreen.main
        guard let screen,
              let number = screen.deviceDescription[NSDeviceDescriptionKey("NSScreenNumber")] as? NSNumber
        else { return nil }
        return ScreenInfo(screen: screen,
                          displayID: CGDirectDisplayID(number.uint32Value),
                          scale: screen.backingScaleFactor)
    }

    private func start() {
        generation += 1
        let gen = generation
        Task { [weak self] in
            guard let self else { return }
            do {
                let content = try await SCShareableContent.excludingDesktopWindows(false,
                                                                                  onScreenWindowsOnly: true)
                guard self.generation == gen, self.isShowing else { return }
                guard let info = self.screenInfo(),
                      let display = content.displays.first(where: { $0.displayID == info.displayID })
                else { return }

                let excluded = Self.excludedWindows(content: content, screen: info.screen)
                self.currentExclusionKey = Self.key(for: excluded)
                self.logCandidates(content, screen: info.screen, excluded: excluded)

                let filter = SCContentFilter(display: display, excludingWindows: excluded)
                let config = SCStreamConfiguration()
                config.sourceRect = CGRect(x: self.targetRect.minX - info.screen.frame.minX,
                                           y: info.screen.frame.maxY - self.targetRect.maxY,
                                           width: self.targetRect.width,
                                           height: self.targetRect.height)
                config.width = Int((self.targetRect.width * info.scale).rounded())
                config.height = Int((self.targetRect.height * info.scale).rounded())
                config.minimumFrameInterval = CMTime(value: 1, timescale: 30)
                config.pixelFormat = kCVPixelFormatType_32BGRA
                config.showsCursor = true
                config.queueDepth = 6

                let newStream = SCStream(filter: filter, configuration: config, delegate: self)
                try newStream.addStreamOutput(self, type: .screen, sampleHandlerQueue: self.sampleQueue)
                try await newStream.startCapture()

                guard self.generation == gen, self.isShowing else {
                    try? await newStream.stopCapture()
                    return
                }
                let previous = self.stream
                self.stream = newStream
                if let previous { try? await previous.stopCapture() }
                self.log("stream started, excluding \(excluded.count) window(s)")
            } catch {
                self.log("stream start failed: \(error.localizedDescription)")
                DispatchQueue.main.async {
                    self.mirrorView?.setStatus("Screen capture failed. Check Screen Recording permission.")
                }
            }
        }
    }

    private func startRefreshTimer() {
        refreshTimer?.invalidate()
        refreshTimer = Timer.scheduledTimer(withTimeInterval: 1.5, repeats: true) { [weak self] _ in
            self?.refreshExclusions()
        }
        if let refreshTimer { RunLoop.main.add(refreshTimer, forMode: .common) }
    }

    /// Annotation overlays come and go, so re-check the exclusion list and
    /// restart the capture when one appears or disappears.
    private func refreshExclusions() {
        guard isShowing else { return }
        Task { [weak self] in
            guard let self, self.isShowing, let info = self.screenInfo() else { return }
            do {
                let content = try await SCShareableContent.excludingDesktopWindows(false,
                                                                                  onScreenWindowsOnly: true)
                let excluded = Self.excludedWindows(content: content, screen: info.screen)
                let key = Self.key(for: excluded)
                if key != self.currentExclusionKey {
                    self.currentExclusionKey = key
                    self.log("overlay set changed, restarting capture")
                    self.start()
                }
            } catch {
                self.log("exclusion refresh failed: \(error.localizedDescription)")
            }
        }
    }

    private static func key(for windows: [SCWindow]) -> String {
        windows.map(\.windowID).sorted().map(String.init).joined(separator: ",")
    }

    /// Full-screen/high-layer overlays are the drawing layers conferencing apps
    /// put over a shared screen. Also catches full-screen windows owned by the
    /// conferencing app itself. Our own mirror window is always excluded.
    private static func excludedWindows(content: SCShareableContent,
                                        screen: NSScreen) -> [SCWindow] {
        let screenArea = screen.frame.width * screen.frame.height
        let myBundle = Bundle.main.bundleIdentifier
        return content.windows.filter { window in
            guard window.isOnScreen else { return false }
            if window.owningApplication?.bundleIdentifier == myBundle { return true }
            guard screenArea > 0 else { return false }
            let area = window.frame.width * window.frame.height
            guard area.isFinite else { return false }
            let ratio = area / screenArea

            if window.windowLayer >= 500, ratio > 0.30 { return true }

            let bundle = (window.owningApplication?.bundleIdentifier ?? "").lowercased()
            let isDrawingApp = conferencing.contains(bundle)
                || bundle.contains("slack")
                || bundle.contains("slick")
                || bundle.contains("zoom")
                || bundle.contains("teams")
                || bundle.contains("webex")
                || bundle.contains("discord")
                || bundle.contains("screencaptureui")
            if isDrawingApp, window.windowLayer > 0, ratio > 0.30 {
                return true
            }
            return false
        }
    }

    // MARK: - SCStreamOutput

    func stream(_ stream: SCStream, didOutputSampleBuffer sampleBuffer: CMSampleBuffer,
                of type: SCStreamOutputType) {
        guard type == .screen, sampleBuffer.isValid else { return }
        guard let attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer,
                                                                       createIfNecessary: false) as? [[SCStreamFrameInfo: Any]],
              let raw = attachments.first?[.status] as? Int,
              let status = SCFrameStatus(rawValue: raw), status == .complete
        else { return }

        DispatchQueue.main.async { [weak self] in
            guard let self, let view = self.mirrorView else { return }
            let layer = view.displayLayer
            if #available(macOS 15.0, *) {
                let renderer = layer.sampleBufferRenderer
                if renderer.status == .failed { renderer.flush() }
                renderer.enqueue(sampleBuffer)
            } else {
                if layer.status == .failed { layer.flush() }
                layer.enqueue(sampleBuffer)
            }
            view.hideStatus()
            if !self.didLogFirstFrame {
                self.didLogFirstFrame = true
                self.log("first live frame received")
            }
        }
    }

    func stream(_ stream: SCStream, didStopWithError error: Error) {
        log("stream stopped: \(error.localizedDescription)")
    }

    // MARK: - Logging

    private func logCandidates(_ content: SCShareableContent, screen: NSScreen,
                               excluded: [SCWindow]) {
        let myBundle = Bundle.main.bundleIdentifier
        let interesting = content.windows.filter { window in
            guard window.isOnScreen else { return false }
            if window.owningApplication?.bundleIdentifier == myBundle { return false }
            return window.windowLayer > 0
                || Self.conferencing.contains(window.owningApplication?.bundleIdentifier ?? "")
        }
        let summary = interesting.map { window in
            let bundle = window.owningApplication?.bundleIdentifier ?? "?"
            return "\(bundle)/layer\(window.windowLayer)/\(Int(window.frame.width))x\(Int(window.frame.height))"
        }
        log("high-layer candidates: \(summary)")
        log("excluded: \(excluded.map { "\($0.owningApplication?.bundleIdentifier ?? "?")/layer\($0.windowLayer)" })")
    }

    private func log(_ message: String) {
        let line = "[\(Date())] \(message)\n"
        let url = URL(fileURLWithPath: NSHomeDirectory())
            .appendingPathComponent("Library/Logs/DrawingsGone.log")
        if let data = line.data(using: .utf8) {
            if FileManager.default.fileExists(atPath: url.path),
               let handle = try? FileHandle(forWritingTo: url) {
                handle.seekToEndOfFile()
                handle.write(data)
                try? handle.close()
            } else {
                try? data.write(to: url)
            }
        }
    }
}

final class MirrorView: NSView {
    let displayLayer = AVSampleBufferDisplayLayer()
    private let statusLabel = NSTextField(labelWithString: "Waiting for screen capture…")
    private var borderView: NSView?

    init(outline: Bool) {
        super.init(frame: .zero)
        wantsLayer = true
        layer = CALayer()
        layer?.backgroundColor = NSColor.black.cgColor
        displayLayer.videoGravity = .resize
        layer?.addSublayer(displayLayer)

        statusLabel.font = NSFont.systemFont(ofSize: 15, weight: .medium)
        statusLabel.textColor = NSColor.white.withAlphaComponent(0.65)
        statusLabel.alignment = .center
        statusLabel.translatesAutoresizingMaskIntoConstraints = false
        addSubview(statusLabel)
        NSLayoutConstraint.activate([
            statusLabel.centerXAnchor.constraint(equalTo: centerXAnchor),
            statusLabel.centerYAnchor.constraint(equalTo: centerYAnchor)
        ])

        if outline {
            let border = NSView(frame: .zero)
            border.autoresizingMask = [.width, .height]
            border.wantsLayer = true
            border.layer?.borderWidth = 1
            border.layer?.borderColor = NSColor.labelColor.withAlphaComponent(0.16).cgColor
            addSubview(border)
            borderView = border
        }
    }

    func hideStatus() {
        statusLabel.isHidden = true
    }

    func setStatus(_ text: String) {
        statusLabel.stringValue = text
        statusLabel.isHidden = false
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    override func layout() {
        super.layout()
        CATransaction.begin()
        CATransaction.setDisableActions(true)
        displayLayer.frame = bounds
        borderView?.frame = bounds
        CATransaction.commit()
    }
}
