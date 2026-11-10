# Rebax Engine

**Rebax** is a graphical 2D/3D game development environment focused on creating and exporting games for the **PlayStation 2**. It brings scene editing, node-based workflows, project management and PS2 export tools into one application.

The project is written primarily in C and uses SDL2 for its desktop and mobile application layer.

## Features

- **Scene editing** — create and manage scenes through the editor workspace, scene tabs and scene tree.
- **Node system** — register engine nodes and their editor counterparts through generated registries.
- **2D and 3D workflows** — dedicated 2D and 3D viewports, with node types such as Element2D, Element3D and Sprite2D.
- **Asset and project management** — project creation, file browsing, resource handling and package management.
- **PS2 export** — build projects for PlayStation 2 and generate either an ISO image or a raw ELF executable.
- **Image support** — image-loading code for formats including BMP, JPEG, PNG, TGA, TIFF, RAW, TIM and TIM2.
- **Integrated build support** — bundled build tooling, SDK packages and generated editor resources.

> Rebax itself can be built for several host platforms. Game export is currently focused on PlayStation 2; these are separate kinds of platform support.

## Platform Support

### Rebax application

The Makefile contains build targets for:

| Platform | Architectures |
| --- | --- |
| Linux | x86, x86_64, ARM, ARM64 |
| Windows | x86, x86_64, ARM, ARM64 |
| Android | armeabi-v7a, arm64-v8a, x86, x86_64 |
| macOS | Intel x86_64, Apple Silicon arm64 |
| iOS | arm64, x86_64 simulator |

Build availability does not mean every target has been tested on physical hardware. In particular, macOS and iOS should be considered experimental until tested on the intended devices.

### Game export

- **PlayStation 2** — supported export target.
- **ISO** and **raw ELF** — available PS2 output formats.
- **iOS** — project editing only; exporting is not supported on iOS.

## Repository Structure

```text
Rebax_Engine/
├── src/
│   ├── core/           # Application entry point and shared state
│   ├── editor/         # Editor workspace, panels, dialogs, UI and viewports
│   ├── engine/         # Scene data and engine node registry
│   ├── exporters/      # Shared export pipeline and PS2 exporter
│   ├── filesystem/     # Project, asset and resource file operations
│   ├── network/        # HTTPS, downloads and file hashing
│   ├── packages/       # Package management
│   ├── platform/       # Platform-specific application support
│   └── project/        # Project creation and project catalog
├── sdks/
│   └── rebax/ps2/      # Rebax PS2 SDK headers and runtime node sources
├── resources/          # Fonts, icons and certificate bundle
├── rebax_build_tool/   # Native helper used by the build system
├── third_party/        # Third-party libraries and supporting code
├── build/              # Generated build files and output
├── build-cache/        # Reusable compiled objects
├── Makefile
├── LICENSE
└── THIRD_PARTY_NOTICES.md
```

## Build Requirements

A normal build requires:

- **GCC or Clang** — C compiler.
- **GNU Make** — build orchestration.
- **SDL2 development files** — application windowing, input and graphics support.
- **curl or wget** — downloading the required prebuilt toolchain and SDK packages.
- **Git** — recommended for cloning and updating the repository.

Some targets require their platform-specific SDK or packaging tools. The build system can bootstrap supported compiler toolchains and SDK packages from the Rebax release repositories; an internet connection is therefore needed when those packages are not already available.

## Getting the Source

```sh
git clone https://github.com/PS2HomeDeveloper/Rebax.git
cd Rebax
```

## Building

Build for the current host platform:

```sh
make
```

Select a target explicitly:

```sh
make linux-x86_64
make windows-x86_64
make android-arm64-v8a
make macos-arm64
```

Build an Android APK:

```sh
make android-arm64-v8a.APK
```

The Makefile also defines targets for the other architectures listed in the platform table. Build artifacts are placed under `build/output/`.

Useful maintenance targets:

```sh
make clean       # Remove generated build output
make clean-cache # Remove cached compiled objects
make clean-tool  # Remove the generated build helper
make clean-all   # Run all cleanup targets
```

## PS2 Development SDK

The Rebax PS2 SDK is located in `sdks/rebax/ps2/`.

- `include/core/` — engine context and public SDK interface.
- `include/nodes/` — node interface.
- `include/common/` — shared image-loader interfaces.
- `src/nodes/` — PS2 runtime implementations for the supplied nodes and image loaders.

The SDK is intended for projects exported through Rebax's PlayStation 2 workflow.

## Troubleshooting and Bug Reports

When reporting a build or export problem, include:

1. The exact command or action that failed.
2. The complete terminal output, especially the first compiler or linker error.
3. Your operating system, architecture and compiler version.
4. The relevant export log when a PS2 export fails.

Rebax writes an export log each time an export is started. The log is overwritten by the next export, so copy it before trying again.

| Platform | Export log location |
| --- | --- |
| Windows | `%LOCALAPPDATA%\\Rebax\\export_log.txt` |
| Linux | `~/.local/share/Rebax/export_log.txt` |
| macOS | `~/Library/Application Support/Rebax/export_log.txt` |
| Android | `Rebax_export_log.txt` in the main shared-storage folder |

Exporting is not available on iOS, so no iOS export log is produced.

Please report reproducible issues through the project's GitHub issue tracker. Bug reports, platform test results and contributions are welcome.

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting changes, and include a clear description of what was changed and how it was tested.

## Third-Party Software

Rebax includes third-party components. Their licenses and notices are documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), with the corresponding license files retained in the repository.

## License

Rebax is released under the **MIT License**. See [LICENSE](LICENSE) for the full license text.
