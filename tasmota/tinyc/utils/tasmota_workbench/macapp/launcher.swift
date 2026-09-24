// Tasmota Workbench -- native macOS launcher for the standalone app.
//
// The app bundle carries its own Python runtime (Contents/Resources/python,
// universal x86_64 + arm64) and the server script. This launcher starts the
// server as a CHILD process and stays alive as the app the user sees.
//
// Why a native launcher instead of the old shell script:
//  * macOS "Local Network" privacy attributes network access to the
//    RESPONSIBLE app. A child started from here inherits this bundle, whose
//    Info.plist carries NSLocalNetworkUsageDescription, so macOS asks ONCE and
//    remembers it. The old script ran whatever python3 was on PATH (Homebrew),
//    which never gets that permission: LAN scans and the multicast "Shares"
//    monitor saw nothing, silently.
//  * It needs no Python on the machine, and it is universal (Intel + Apple
//    silicon).
//
// Quitting the app (menu, Cmd-Q, Dock) terminates the server; the server's own
// Quit button ends the child, and the app follows.

import AppKit

let serverURL = URL(string: "http://127.0.0.1:8124/")!

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var server: Process?
    private var quitting = false
    private var logHandle: FileHandle?

    func applicationDidFinishLaunching(_ notification: Notification) {
        buildMenu()
        startServer()
    }

    // Clicking the Dock icon again opens the page again.
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        openPage(nil)
        return false
    }

    func applicationWillTerminate(_ notification: Notification) {
        quitting = true
        guard let p = server, p.isRunning else { return }
        p.terminate()                                   // SIGTERM
        let deadline = Date().addingTimeInterval(2.0)
        while p.isRunning && Date() < deadline { usleep(50_000) }
        if p.isRunning { kill(p.processIdentifier, SIGKILL) }
    }

    private func startServer() {
        guard let res = Bundle.main.resourceURL else { return fail("Resources folder missing.") }
        let python = res.appendingPathComponent("python/bin/python3")
        let script = res.appendingPathComponent("tasmota_workbench_server.py")
        guard FileManager.default.isExecutableFile(atPath: python.path) else {
            return fail("The bundled Python runtime is missing:\n\(python.path)")
        }

        let p = Process()
        p.executableURL = python
        p.arguments = ["-u", script.path]
        p.currentDirectoryURL = res
        var env = ProcessInfo.processInfo.environment
        // Keep the bundled interpreter hermetic: nothing from the user's own
        // Python setup may leak in (PYTHONPATH, user site-packages).
        for k in ["PYTHONPATH", "PYTHONHOME", "PYTHONSTARTUP", "VIRTUAL_ENV"] { env.removeValue(forKey: k) }
        env["PYTHONNOUSERSITE"] = "1"
        env["PYTHONDONTWRITEBYTECODE"] = "1"     // the bundle stays unmodified (signature)
        env["TASMOTA_WORKBENCH_BUNDLED"] = "1"
        p.environment = env

        // stdout/stderr to ~/Library/Logs/TasmotaWorkbench.log (fresh each run)
        let logURL = FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Logs/TasmotaWorkbench.log")
        FileManager.default.createFile(atPath: logURL.path, contents: nil)
        if let h = try? FileHandle(forWritingTo: logURL) {
            logHandle = h
            p.standardOutput = h
            p.standardError = h
        }

        p.terminationHandler = { [weak self] proc in
            DispatchQueue.main.async {
                guard let self = self else { return }
                if !self.quitting && proc.terminationStatus != 0 {
                    self.alert("Tasmota Workbench stopped (exit \(proc.terminationStatus)).",
                               "Details: ~/Library/Logs/TasmotaWorkbench.log")
                }
                self.quitting = true
                NSApp.terminate(nil)
            }
        }

        do {
            try p.run()
            server = p
        } catch {
            fail("Could not start the server: \(error.localizedDescription)")
        }
    }

    @objc private func openPage(_ sender: Any?) {
        NSWorkspace.shared.open(serverURL)
    }

    @objc private func openLog(_ sender: Any?) {
        NSWorkspace.shared.open(FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Logs/TasmotaWorkbench.log"))
    }

    private func buildMenu() {
        let main = NSMenu()
        let appItem = NSMenuItem()
        main.addItem(appItem)
        let m = NSMenu()
        m.addItem(withTitle: "Open Tasmota Workbench in Browser", action: #selector(openPage(_:)), keyEquivalent: "o")
        m.addItem(withTitle: "Show Log", action: #selector(openLog(_:)), keyEquivalent: "l")
        m.addItem(.separator())
        m.addItem(withTitle: "Quit Tasmota Workbench", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appItem.submenu = m
        NSApp.mainMenu = main
    }

    private func alert(_ text: String, _ info: String) {
        let a = NSAlert()
        a.messageText = text
        a.informativeText = info
        a.alertStyle = .warning
        NSApp.activate(ignoringOtherApps: true)
        a.runModal()
    }

    private func fail(_ text: String) {
        alert("Tasmota Workbench cannot start.", text)
        quitting = true
        NSApp.terminate(nil)
    }
}

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.run()
