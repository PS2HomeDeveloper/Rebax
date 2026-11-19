# Changelog

All notable changes to Rebax Engine are recorded here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- Running a PlayStation 2 project: simulation, emulator, or a real console over the network with ps2link.
- Built-in file manager with storage devices, folder creation, rename and delete.
- One universal Android APK containing every Android architecture, built with `make android all`.
- Release workflow that publishes packages for every supported platform.

### Changed
- The engine, editor and build tool are now licensed under the Mozilla Public License 2.0. The runtime SDK in `sdks/rebax` stays under the MIT License.
- Android packages are signed with a single signing file, `keystore/rebax_signing.pem`.
- Platform and architecture can be written as separate words: `make linux x86_64`.

### Removed
- Leftover source files from earlier layouts.
