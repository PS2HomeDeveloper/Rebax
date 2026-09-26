# Rebax Engine

Rebax Engine builds for one export target: **PlayStation 2 (PS2)**. The engine embeds the resources required for exporting; there is no optional template or external-resource mode.

## Project layout

- `src/` — editor, runtime setup, and PS2 exporter sources.
- `embedded/resources/` — fonts and other runtime assets. Icon source images live under `images/icons/icons_src/`; the build generates and embeds the icon atlas.
- `embedded/nodes/` — PS2 node implementations and shared headers. The build archives this directory as `embedded/nodes.tar.xz` without changing the source tree.
- `embedded/toolchains/` — platform-matched `make` and `ps2dev.tar.xz`. Each file is embedded directly as an individual resource. If required files are absent, Makefile's existing release-download flow fetches the matching package here.
- `build/` — temporary host build tools, objects, and output.
- `build_tools/` — source for the self-contained build helper.

## Build

Run `make` from the repository root. A host C compiler, `objcopy`, Make, SDL2 development files, and `curl` or `wget` (only when toolchain files are missing) are required. The resulting engine executable contains the resources and each toolchain file separately. On first launch it creates `Rebax/Engine/toolchains`, `Rebax/Engine/resources/nodes`, `Rebax/Temp/export`, and `Rebax/Settings`, writes `make` and `ps2dev.tar.xz` directly into the toolchains directory, then extracts and removes the PS2Dev archive and extracts the node archive.

During export, Rebax uses `Rebax/Engine/toolchains/ps2dev`, `Rebax/Engine/toolchains/make`, and `Rebax/Engine/resources/nodes`. The working export directory is `Rebax/Temp/export`.

## License

See [`LICENSE`](LICENSE). No project license grant was included with the supplied source archive, so this file does not assign a new license.
