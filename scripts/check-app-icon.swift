import AppKit
import Foundation

// Read the same macOS icon service used by Finder, not just the raw PNG/ICNS.
// No manually assigned Finder icon is needed by a correct release bundle.
let args = CommandLine.arguments
if args.count != 3 {
    fputs("Usage: swift scripts/check-app-icon.swift /path/FluidScope.app /path/preview.png\n", stderr)
    exit(2)
}
let bundlePath = URL(fileURLWithPath: args[1]).standardizedFileURL.path
let icon = NSWorkspace.shared.icon(forFile: bundlePath)
let side = 256
let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: side, pixelsHigh: side,
    bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
    colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
NSColor.clear.setFill()
NSRect(x: 0, y: 0, width: side, height: side).fill(using: .copy)
icon.draw(in: NSRect(x: 0, y: 0, width: side, height: side), from: .zero,
    operation: .sourceOver, fraction: 1)
NSGraphicsContext.restoreGraphicsState()
try rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: args[2]))
var paleBorderPixels = 0
var artworkPixels = 0
for y in 0..<side {
    for x in 0..<side {
        guard let c = rep.colorAt(x: x, y: y)?.usingColorSpace(.deviceRGB), c.alphaComponent > 0.5 else { continue }
        let hi = max(c.redComponent, c.greenComponent, c.blueComponent)
        let lo = min(c.redComponent, c.greenComponent, c.blueComponent)
        let outerBand = x < 62 || x > 193 || y < 62 || y > 193
        if outerBand && lo > 0.55 && hi - lo < 0.08 { paleBorderPixels += 1 }
        if c.greenComponent > c.redComponent + 0.12 && c.blueComponent > c.redComponent + 0.05 { artworkPixels += 1 }
    }
}
print("macOS icon: \(paleBorderPixels) pale border pixels; \(artworkPixels) mint artwork pixels")
if paleBorderPixels > 256 || artworkPixels < 500 {
    fputs("Icon check failed: unexpected light frame or missing artwork.\n", stderr)
    exit(1)
}
