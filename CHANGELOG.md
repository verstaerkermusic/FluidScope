# Changelog

## 0.4.12 — 2026-10-28

- Add a centre grip for sweeping the selected 2D FFT band, plus left/right edge handles for resizing. Keep octave width on logarithmic axes and Hz width on linear axes, including at frequency bounds. Works in the editor, Quad and fullscreen; ordinary dragging preserves the manual Listen band state. Alt/Option plus the centre grip temporarily auditions the band until the key or mouse is released, without writing or saving the manual Listen parameter.

## 0.4.11 — 2026-10-28

- Fix stepped input meters: publish Peak/RMS/True Peak every 5 ms and animate at display refresh with elapsed-time peak release and smooth RMS markers. Keep all LUFS windows, gating and 100-ms update timing unchanged.
- Add a macOS Audio Unit effect alongside VST3 and standalone, targeting Apple Silicon and Intel.
- Include an English double-click installer with a choice of user or system-wide VST3/AU installation, scoped quarantine removal, previous-version backups and rollback on failure. Refresh the current user's AU registration after installation.
- Rename the waveform display from Stereo to Oscilloscope, keeping its saved view index unchanged.
- Add an optional input-meter sidebar in all views and fullscreen, independent of Clean mode: L/R sample peaks and peak holds, 400-ms RMS, four-times-oversampled True Peak with a retained maximum, and momentary/short-term/gated integrated LUFS.
- Add a saved True Peak warning threshold (default -1 dBTP), configurable LUFS colours (green at -14, deep red at -8), manual measurement reset and optional reset on playback start.
- Keep input metering independent of visual gain, the internal sine and band audition; run its analysis on a separate worker with bounded memory.
- Use English release information and helper-script messages.
- Include `Authorize-FluidScope.command` in the macOS download: a double-click helper that verifies installed FluidScope VST3/AU signatures and removes only their quarantine attributes, without changing global Gatekeeper settings.

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
