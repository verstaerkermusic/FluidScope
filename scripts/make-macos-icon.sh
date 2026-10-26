#!/bin/bash
set -euo pipefail
project_root="$(cd "$(dirname "$0")/.." && pwd)"
source_png="$project_root/assets/FluidScope.png"
icon_tmp="$(mktemp -d "${TMPDIR:-/tmp}/fluidscope-icon.XXXXXX")"
trap 'rm -rf "$icon_tmp"' EXIT
iconset="$icon_tmp/FluidScope.iconset"
mkdir -p "$iconset"
# Native ImageIO resizing keeps the PNG alpha, including antialiased edges.
for size in 16 32 128 256 512; do
    /usr/bin/sips -s format png -z "$size" "$size" "$source_png" --out "$iconset/icon_${size}x${size}.png" >/dev/null
    retina_size=$((size * 2))
    /usr/bin/sips -s format png -z "$retina_size" "$retina_size" "$source_png" --out "$iconset/icon_${size}x${size}@2x.png" >/dev/null
done
/usr/bin/iconutil -c icns "$iconset" -o "$project_root/assets/FluidScope.icns"
# Compile the modern macOS icon stack; retain the ICNS above for older systems.
/usr/bin/sips -s format png -z 1024 1024 "$source_png" --out "$project_root/assets/FluidScope.icon/Assets/FluidScope.png" >/dev/null
mkdir -p "$icon_tmp/compiled"
/usr/bin/xcrun actool --compile "$icon_tmp/compiled" --platform macosx --minimum-deployment-target 12.0 --app-icon FluidScope --output-partial-info-plist "$icon_tmp/icon-info.plist" "$project_root/assets/FluidScope.icon" >/dev/null
cp "$icon_tmp/compiled/Assets.car" "$project_root/assets/Assets.car"
echo "Created native ICNS fallback and modern Assets.car from the original PNG."
