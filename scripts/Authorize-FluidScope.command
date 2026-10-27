#!/bin/bash
# Quit your DAW, double-click this file, then restart your DAW.
# Removes only quarantine from installed FluidScope VST3 and AU copies.
set -euo pipefail
finish() {
    result=$?
    if [ "$result" -ne 0 ]; then printf '\nAuthorization failed. Please check the message above.\n'; fi
    if [ -t 0 ]; then read -r -p 'Press Enter to close … ' || true; fi
    exit "$result"
}
trap finish EXIT
found=0
for plugin in \
    "$HOME/Library/Audio/Plug-Ins/VST3/FluidScope.vst3" \
    "$HOME/Library/Audio/Plug-Ins/Components/FluidScope.component" \
    '/Library/Audio/Plug-Ins/VST3/FluidScope.vst3' \
    '/Library/Audio/Plug-Ins/Components/FluidScope.component'; do
    if [ ! -d "$plugin" ]; then continue; fi
    found=1
    printf 'Verifying signature: %s\n' "$plugin"
    identifier="$(/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "$plugin/Contents/Info.plist")"
    if [ "$identifier" != audio.tino.fluidscope ]; then printf 'Unexpected bundle identifier. Stopping.\n' >&2; exit 1; fi
    /usr/bin/codesign --verify --deep --strict --all-architectures "$plugin"
    if [ -w "$plugin" ]; then
        /usr/bin/xattr -dr com.apple.quarantine "$plugin"
    else
        printf 'The system-wide installation requires your administrator password.\n'
        /usr/bin/sudo /usr/bin/xattr -dr com.apple.quarantine "$plugin"
    fi
done
if [ "$found" -eq 0 ]; then printf 'FluidScope was not found. Please install the plugins first.\n' >&2; exit 1; fi
printf '\nFluidScope: quarantine attributes removed. Please restart your DAW.\n'
