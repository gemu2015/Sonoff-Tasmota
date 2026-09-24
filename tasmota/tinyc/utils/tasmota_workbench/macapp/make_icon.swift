// Renders the 1024 px app icon for Tasmota Workbench into the given PNG path.
// Usage: swift make_icon.swift out.png   (headless, no window server needed)

import AppKit

let out = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "icon_1024.png"
let size = 1024
guard let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size,
                                 bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                                 colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0) else { exit(1) }
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)

// macOS icon grid: 824 px rounded square centred on the 1024 canvas
let body = NSRect(x: 100, y: 100, width: 824, height: 824)
let path = NSBezierPath(roundedRect: body, xRadius: 185, yRadius: 185)
NSGradient(starting: NSColor(calibratedRed: 0.09, green: 0.33, blue: 0.42, alpha: 1),
           ending: NSColor(calibratedRed: 0.03, green: 0.14, blue: 0.20, alpha: 1))!.draw(in: path, angle: -90)

// radio waves = the fleet, the chip = the device
let cfg = NSImage.SymbolConfiguration(pointSize: 430, weight: .semibold)
if let sym = NSImage(systemSymbolName: "antenna.radiowaves.left.and.right", accessibilityDescription: nil)?
    .withSymbolConfiguration(cfg) {
    let tinted = NSImage(size: sym.size, flipped: false) { r in
        sym.draw(in: r)
        NSColor(calibratedRed: 0.55, green: 0.93, blue: 0.85, alpha: 1).set()
        r.fill(using: .sourceAtop)
        return true
    }
    let w = sym.size.width, h = sym.size.height
    tinted.draw(in: NSRect(x: 512 - w / 2, y: 540 - h / 2, width: w, height: h))
}
let label = "WORKBENCH" as NSString
let attrs: [NSAttributedString.Key: Any] = [
    .font: NSFont.systemFont(ofSize: 92, weight: .heavy),
    .foregroundColor: NSColor(calibratedWhite: 1, alpha: 0.92),
    .kern: 6
]
let ls = label.size(withAttributes: attrs)
label.draw(at: NSPoint(x: 512 - ls.width / 2, y: 190), withAttributes: attrs)

NSGraphicsContext.restoreGraphicsState()
guard let png = rep.representation(using: .png, properties: [:]) else { exit(1) }
try! png.write(to: URL(fileURLWithPath: out))
