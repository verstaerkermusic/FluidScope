# Building FluidScope 0.4.5

## Requirements

- macOS with Xcode or Command Line Tools and a macOS SDK.
- CMake 3.22 or newer and a C++17 compiler.
- The supported release target is Apple Silicon, macOS 12+.

JUCE 7.0.10 is included in `vendor/JUCE`. No submodule checkout, account, commercial JUCE licence or dependency download is needed for this GPL build. JUCE and bundled third-party notices remain in that directory. A local macOS window-snapshot change is documented in `vendor/JUCE/FLUIDSCOPE-MODIFICATIONS.md` and `patches/`.

## Build

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release --parallel 6
ctest --test-dir build --output-on-failure
```

The integration check creates a JUCE GUI and needs a normal macOS graphical login session. The tests cover trigger alignment, overlapping capture, XY delay/sine/follower, FFT calibration/history, visual gain and zoom state, audio passthrough and band audition.

Products:

- `build/FluidScope_artefacts/Release/VST3/FluidScope.vst3`
- `build/FluidScope_artefacts/Release/Standalone/FluidScope.app`

Building does not install the plugin automatically. Copy the VST3 bundle into your user's VST3 folder and restart the host. The build does not produce an AU.

An alternative JUCE tree can be supplied with `-DJUCE_PATH=/path/to/JUCE`, but the bundled snapshot is the release's reference dependency. The versioned source archive includes it so the release can be rebuilt without retrieving JUCE from another server.

## Native display checks

The integration-check executable also provides optional six-second live-rendering runs, from the build folder:

```sh
./FluidScopeCheck_artefacts/Release/FluidScopeCheck --benchmark 120 40 xy3d full
./FluidScopeCheck_artefacts/Release/FluidScopeCheck --benchmark 120 500 xy
./FluidScopeCheck_artefacts/Release/FluidScopeCheck --benchmark 120 40 quad full
```

These open visible test windows and generate synthetic audio inside the check process. Full-screen checks exercise ordering above a simulated floating host window, Escape/F11 return and reopening. GPU readback images are saved to the current folder. Timing reports are short local checks, not guaranteed host performance.
