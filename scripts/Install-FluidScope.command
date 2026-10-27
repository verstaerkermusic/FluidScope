#!/bin/bash
# Keep this installer beside FluidScope.vst3 and FluidScope.component.
# Offers a current-user or system-wide installation of FluidScope plugins.
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd -P)"
sources=(); targets=(); stages=(); backups=(); installed=(); backed_up=()
backup_root=''
committed=0
system_wide=0
run() {
    if [ "$system_wide" -eq 1 ]; then /usr/bin/sudo "$@"; else "$@"; fi
}

finish() {
    result=$?
    trap - EXIT INT TERM
    set +e
    if [ "$result" -ne 0 ] && [ "$committed" -eq 0 ]; then
        for ((i=${#targets[@]}-1; i>=0; --i)); do
            if [ "${installed[$i]:-0}" -eq 1 ]; then
                printf 'Rolling back: %s\n' "${targets[$i]}" >&2
                run /bin/mv "${targets[$i]}" "${stages[$i]}/failed-install" || true
            fi
            if [ "${backed_up[$i]:-0}" -eq 1 ]; then
                if [ -e "${targets[$i]}" ] || [ -L "${targets[$i]}" ]; then
                    printf 'Restore manually from: %s\n' "${backups[$i]}" >&2
                else
                    run /bin/mv "${backups[$i]}" "${targets[$i]}" || printf 'Restore manually from: %s\n' "${backups[$i]}" >&2
                fi
            fi
        done
    fi
    for stage in ${stages[@]+"${stages[@]}"}; do
        # Only remove the temporary directories created by this invocation.
        if [ -n "$stage" ]; then run /bin/rm -rf -- "$stage"; fi
    done
    if [ -t 0 ]; then read -r -p 'Press Enter to close … ' || true; fi
    exit "$result"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

if [ "$(/usr/bin/id -u)" -eq 0 ]; then
    printf 'Run this installer as your normal user, without sudo. Choose System-wide when prompted.\n' >&2
    exit 1
fi
printf 'FluidScope installer — VST3 + Audio Unit\n\nPlease quit your DAW before installing.\n'
printf '\n1) User installation (recommended): ~/Library/Audio/Plug-Ins/\n'
printf '2) System-wide installation: /Library/Audio/Plug-Ins/ (administrator password)\n'
if ! read -r -p 'Choose installation location [1]: ' choice; then
    printf '\nInstallation cancelled.\n' >&2
    exit 1
fi
case "$choice" in
    ''|1) plugin_root="$HOME/Library/Audio/Plug-Ins"; backup_parent="$HOME/Library/Application Support/FluidScope/Backups" ;;
    2) system_wide=1; plugin_root='/Library/Audio/Plug-Ins'; backup_parent='/Library/Application Support/FluidScope/Backups' ;;
    *) printf 'Invalid choice. Nothing was installed.\n' >&2; exit 1 ;;
esac

# Check every source bundle before asking for administrator access or writing.
for format in VST3 Components; do
    if [ "$format" = VST3 ]; then name=FluidScope.vst3; else name=FluidScope.component; fi
    source_bundle="$script_dir/$name"
    if [ ! -d "$source_bundle" ]; then continue; fi
    printf '\nChecking %s…\n' "$name"
    identifier="$(/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "$source_bundle/Contents/Info.plist")"
    if [ "$identifier" != audio.tino.fluidscope ]; then
        printf 'Unexpected bundle identifier. Installation cancelled.\n' >&2
        exit 1
    fi
    /usr/bin/codesign --verify --deep --strict --all-architectures "$source_bundle"
    index=${#targets[@]}
    sources[$index]="$source_bundle"; targets[$index]="$plugin_root/$format/$name"
    backups[$index]=''; installed[$index]=0; backed_up[$index]=0
done

if [ "${#targets[@]}" -eq 0 ]; then
    printf '\nNo FluidScope plugins found beside this installer. Extract the complete download first.\n' >&2
    exit 1
fi

if [ "$system_wide" -eq 1 ]; then /usr/bin/sudo -v; fi
for ((i=0; i<${#targets[@]}; ++i)); do
    parent="$(dirname "${targets[$i]}")"; name="$(basename "${targets[$i]}")"
    run /bin/mkdir -p "$parent"
    stage="$(run /usr/bin/mktemp -d "$parent/.FluidScope-install.XXXXXX")"; stages[$i]="$stage"
    run /usr/bin/ditto "${sources[$i]}" "$stage/$name"
    run /usr/bin/codesign --verify --deep --strict --all-architectures "$stage/$name"
    run /usr/bin/xattr -dr com.apple.quarantine "$stage/$name"
done

# Stage and verify all available formats before replacing any installed plugin.
for ((i=0; i<${#targets[@]}; ++i)); do
    target="${targets[$i]}"; name="$(basename "$target")"
    if [ -e "$target" ] || [ -L "$target" ]; then
        if [ -z "$backup_root" ]; then
            run /bin/mkdir -p "$backup_parent"
            backup_root="$(run /usr/bin/mktemp -d "$backup_parent/$(/bin/date +%Y%m%d-%H%M%S)-XXXXXX")"
        fi
        backups[$i]="$backup_root/$name"
        run /bin/mv "$target" "${backups[$i]}"
        backed_up[$i]=1
    fi
    run /bin/mv "${stages[$i]}/$name" "$target"
    installed[$i]=1
    run /usr/bin/codesign --verify --deep --strict --all-architectures "$target"
    printf 'Installed: %s\n' "$target"
done
committed=1
# Refresh only the current user's AU registration metadata, not the DAW or
# audio engine. macOS relaunches this registration service on demand.
for target in "${targets[@]}"; do
    if [[ "$target" == *.component ]]; then
        run /usr/bin/touch "$target/Contents/Info.plist" "$target" "$(dirname "$target")"
        /usr/bin/killall -9 -u "$(/usr/bin/id -un)" AudioComponentRegistrar >/dev/null 2>&1 || true
    fi
done
printf '\nInstalled successfully. FluidScope quarantine attributes have been removed.\n'
if [ -n "$backup_root" ]; then printf 'Previous versions: %s\n' "$backup_root"; fi
printf 'Restart your DAW and rescan plugins if needed. Manufacturer: verstaerker.\n'
printf 'The standalone app is unchanged; copy FluidScope.app wherever you prefer.\n'
