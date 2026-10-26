# Changelog

## 0.4.9

- Fixed the standalone app icon's unwanted light frame on current macOS by bundling a modern icon stack with an explicit dark background matching the original artwork.
- Kept the original Lissajous Ribbon PNG unchanged and added Icon Composer source plus a reproducible native icon-generation script.
- Included a complete native ICNS fallback for older macOS versions.
- Refresh the app bundle modification time after building so Finder does not keep the previous icon cache.
- Added a macOS-rendered icon check, including verification after release-archive extraction; no manually applied Finder icon is required.

## 0.4.8

- Added the selected Lissajous Ribbon macOS app icon, with icon sizes generated from a transparent PNG.
- Prepared Intel x86_64 and Universal arm64+x86_64 binaries; Intel regression checks run under Rosetta, with native Intel hardware testing still pending.

## 0.4.7

- Fixed the normal editor's Clean toggle hit area: appearance labels no longer cover it.
- Added actual hit-target and toggle-click regressions for all views at minimum/default editor widths.
- Added a saved 3D FFT Wireframe toggle, available in the editor, Quad and fullscreen; can be combined with Clean mode.

## 0.4.6

- Full horizontal and vertical orbit rotation in 3D FFT and 3D XY, with continuous wrap and double-click reset.
- Legacy camera parameter ranges and saved perspectives retained; new orbit offsets default to zero.
- Camera-dependent FFT mesh traversal for views from either side.
- Saved Clean mode removes plot grids, scales, axes, markers and hints across all views, Quad and fullscreen; controls remain accessible.
- Escape from a focused spectrum panel can bubble to the fullscreen presentation.

## 0.4.5

- Mouse-wheel/trackpad zoom in 3D FFT and 3D XY; separate saved zoom values and double-click reset.
- Reduced redundant overlapping persistence deposits in long 2D XY windows.
- Bounded subpixel polyline reduction for 2D XY display paths.
- Shared-point projection and antialiased segment expansion for 3D XY, retaining all raw trajectory samples.

## Earlier development

Stereo scope with stable triggers and persistence; visual appearance controls; XY delay/internal sine and envelope following; log/linear FFT, smoothing/weighting and visual gain; band audition; 3D history and rear fade; Quad, standalone and fullscreen with floating-host window ordering.
