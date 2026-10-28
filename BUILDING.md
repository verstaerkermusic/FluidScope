# Building FluidScope 0.4.12

## Requirements

- macOS with Xcode or Command Line Tools and a macOS SDK.
- CMake 3.22 or newer and a C++17 compiler.
- Release targets are Apple Silicon (arm64) and Intel (x86_64), macOS 12+. The 3D views require OpenGL 3.2+. Intel cross-build regression tests were run under Rosetta; native Intel hardware testing remains pending.

JUCE 7.0.10 is included in `vendor/JUCE`. No submodule checkout, account, commercial JUCE licence or dependency download is needed for this GPL build. JUCE and bundled third-party notices remain in that directory. A local macOS window-snapshot change is documented in `vendor/JUCE/FLUIDSCOPE-MODIFICATIONS.md` and `patches/`.

## Build

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release --parallel 6
ctest --test-dir build --output-on-failure
```

The integration check creates a JUCE GUI and needs a normal macOS graphical login session. The tests cover trigger alignment, overlapping capture, XY delay/sine/follower, FFT calibration/history, visual gain and zoom state, audio passthrough, band audition, calibrated input meters, integrated gating, meter reset and sidebar hit targets.

Products:

- `build/FluidScope_artefacts/Release/VST3/FluidScope.vst3`
- `build/FluidScope_artefacts/Release/AU/FluidScope.component`
- `build/FluidScope_artefacts/Release/Standalone/FluidScope.app`

Building does not install the plugin automatically. Copy the VST3 bundle into your user's VST3 folder and restart the host. Copy the AU component into `~/Library/Audio/Plug-Ins/Components/` for AU hosts.

An alternative JUCE tree can be supplied with `-DJUCE_PATH=/path/to/JUCE`, but the bundled snapshot is the release's reference dependency. The versioned source archive includes it so the release can be rebuilt without retrieving JUCE from another server.

## Release packaging checklist

- Write release notes, changelog additions, bundled installation instructions and helper-script messages in English. Use `docs/README.en.md` and `docs/QUICKSTART.en.md` for the binary package's primary documentation.
- Include executable `scripts/Install-FluidScope.command` at the top level. It asks for user or system-wide installation, verifies and stages VST3/AU, backs up previous versions, removes quarantine only on these plugins and rolls back failed replacements.
- Include `scripts/Authorize-FluidScope.command` at the top level of the macOS download, alongside `FluidScope.app`, `FluidScope.vst3` and `FluidScope.component`, with its executable permission preserved.
- Include the installation instructions and the changelog. Carry the `Unreleased` notes into the next version's release notes.
- After extracting the final ZIP, verify that `Authorize-FluidScope.command` is executable and passes `bash -n`. Do not run it on the release bundles: it targets the installed plugin.
- Validate the AU with `auval -v aufx FlSc Tino -strict` on Apple Silicon and via `arch -x86_64` under Rosetta. Confirm the reported component version matches the build. Refresh stale per-user AU registration metadata when necessary.

## Native display checks

The integration-check executable also provides optional six-second live-rendering runs, from the build folder:

```sh
./FluidScopeCheck_artefacts/Release/FluidScopeCheck --benchmark 120 40 xy3d full
./FluidScopeCheck_artefacts/Release/FluidScopeCheck --benchmark 120 500 xy
./FluidScopeCheck_artefacts/Release/FluidScopeCheck --benchmark 120 40 meters
./FluidScopeCheck_artefacts/Release/FluidScopeCheck --benchmark 120 40 quad full
```

These open visible test windows and generate synthetic audio inside the check process. Full-screen checks exercise ordering above a simulated floating host window, Escape/F11 return and reopening. GPU readback images are saved to the current folder. Timing reports are short local checks, not guaranteed host performance.

## Intel and Universal binaries

For an Intel-only build, replace `-DCMAKE_OSX_ARCHITECTURES=arm64` with `-DCMAKE_OSX_ARCHITECTURES=x86_64` and use a separate build folder. On Apple Silicon, running Intel helper tools/tests may require Rosetta. A Universal app/plugin can contain the arm64 and x86_64 executables from otherwise identical builds. The app icon uses a modern catalog (assets/Assets.car) plus a native ICNS fallback (assets/FluidScope.icns), both derived from the unchanged assets/FluidScope.png. Regenerate with scripts/make-macos-icon.sh using Xcode 26+; the original PNG and Icon Composer source are included. Ordinary builds use the checked-in resources.

To check the actual macOS-rendered icon (Finder/AppKit), rather than just the source image:

```sh
swift scripts/check-app-icon.swift build/FluidScope_artefacts/Release/Standalone/FluidScope.app /tmp/FluidScope-icon.png
```

This requires access to macOS icon services in a graphical login session. The check rejects the pale frame around the former legacy icon and verifies the artwork is present. Release packaging repeats this check after ZIP extraction without a manually assigned Finder icon.
