#!/bin/bash
# DAW beenden, diese Datei doppelklicken, DAW neu starten.
# Entfernt ausschließlich den Quarantänevermerk von FluidScope.
set -euo pipefail

finish() {
    result=$?
    if [ "$result" -ne 0 ]; then
        printf '\nFreigabe fehlgeschlagen. Bitte die Meldung oben prüfen.\n'
    fi
    if [ -t 0 ]; then
        read -r -p 'Zum Schließen Enter drücken … ' || true
    fi
    exit "$result"
}
trap finish EXIT

plugin="$HOME/Library/Audio/Plug-Ins/VST3/FluidScope.vst3"
if [ ! -d "$plugin" ]; then
    plugin='/Library/Audio/Plug-Ins/VST3/FluidScope.vst3'
fi
if [ ! -d "$plugin" ]; then
    printf 'FluidScope.vst3 ist nicht installiert. Bitte zuerst das Plugin installieren.\n' >&2
    exit 1
fi

printf 'Prüfe Signatur: %s\n' "$plugin"
/usr/bin/codesign --verify --deep --strict --all-architectures "$plugin"

if [ -w "$plugin" ]; then
    /usr/bin/xattr -dr com.apple.quarantine "$plugin"
else
    printf 'Für die systemweite Installation wird dein Administratorpasswort benötigt.\n'
    /usr/bin/sudo /usr/bin/xattr -dr com.apple.quarantine "$plugin"
fi

printf '\nFluidScope: Quarantänevermerk entfernt. Bitte die DAW neu starten.\n'
