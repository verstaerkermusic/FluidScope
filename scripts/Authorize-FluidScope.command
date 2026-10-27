#!/bin/bash
# Quit your DAW, double-click this file, then restart your DAW.
# Removes only the quarantine attribute from the installed FluidScope VST3.
set -euo pipefail

finish() {
    result=$?
    if [ "$result" -ne 0 ]; then
        printf '\nAuthorization failed. Please check the message above.\n'
    fi
    if [ -t 0 ]; then
        read -r -p 'Press Enter to close … ' || true
    fi
    exit "$result"
}
trap finish EXIT

plugin="$HOME/Library/Audio/Plug-Ins/VST3/FluidScope.vst3"
if [ ! -d "$plugin" ]; then
    plugin='/Library/Audio/Plug-Ins/VST3/FluidScope.vst3'
fi
if [ ! -d "$plugin" ]; then
    printf 'FluidScope.vst3 was not found. Please install the plugin first.\n' >&2
    exit 1
fi

printf 'Verifying signature: %s\n' "$plugin"
/usr/bin/codesign --verify --deep --strict --all-architectures "$plugin"

if [ -w "$plugin" ]; then
    /usr/bin/xattr -dr com.apple.quarantine "$plugin"
else
    printf 'The system-wide installation requires your administrator password.\n'
    /usr/bin/sudo /usr/bin/xattr -dr com.apple.quarantine "$plugin"
fi

printf '\nFluidScope: quarantine attribute removed. Please restart your DAW.\n'
