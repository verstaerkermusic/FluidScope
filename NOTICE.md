# Notices and source availability

FluidScope 0.4.10 — verstaerker

FluidScope's new implementation is licensed under the GNU General Public License, version 3 or (at your option) any later version. See LICENSE.
SPDX-License-Identifier: GPL-3.0-or-later

## Upstream inspiration

FluidScope was developed from the Oscilloscope/XYScope concepts and repository structure of FigBug/slPlugins (SocaLabs), reference revision 43319db1e25e3898a420cd771c0f67e5b75d2d38. Original copyright 2020 Roland Rabien. The original BSD 3-Clause notice is reproduced in UPSTREAM-LICENSE.md. FluidScope supplies its own capture engine, combined interface and analysis views. No endorsement by upstream authors is implied.

## JUCE

JUCE 7.0.10, copyright Raw Material Software Limited, is used under GPLv3. Its source and original notices are included in vendor/JUCE. That directory also contains the third-party licences associated with JUCE's bundled dependencies. The snapshot is based on upstream commit a8ae6edda6d3be78a139ec5e429dc57ef047e82a, with the macOS snapshot modification documented in vendor/JUCE/FLUIDSCOPE-MODIFICATIONS.md and patches/.

## Corresponding source

The repository and the FluidScope-0.4.10-source.zip release asset include FluidScope sources, tests, CMake build definitions and the exact JUCE snapshot used by this release. No JUCE submodule checkout is required. Build instructions are in BUILDING.md. Source and binaries are provided together at https://github.com/verstaerkermusic/FluidScope/releases/tag/v0.4.10 once published.

Legacy bundle/four-character plugin identifiers are retained so existing DAW sessions continue to recognise the plugin; the displayed manufacturer is verstaerker.

## App artwork

The FluidScope app icon in assets/FluidScope.png is the selected Lissajous Ribbon design generated for this project and included under the project licence. The modern icon stack and native ICNS fallback are generated from this PNG by scripts/make-macos-icon.sh. Source layers and metadata are included in assets/FluidScope.icon; the checked-in Assets.car is the compiled resource for modern macOS.
