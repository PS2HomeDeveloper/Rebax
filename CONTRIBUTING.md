# Contributing to Rebax

Thank you for helping with Rebax. This guide is short on purpose.

## Before you start

- Read the [Code of Conduct](CODE_OF_CONDUCT.md).
- For a bug, search the existing issues first, then open a new one using the bug report form.
- For a new feature or a large change, start a discussion or open an issue before writing code, so the direction can be agreed first.

## Building

See "Building from source" in the [README](README.md). Run `make` and make sure the build finishes without errors or new warnings.

## Making a change

1. Fork the repository and create a branch from `main`.
2. Keep each pull request focused on one change.
3. Follow the style of the surrounding code: C11, four spaces for indentation, and the same naming as the files around it.
4. Every new source file starts with the standard Rebax header used by the other files.
5. Do not add third-party code unless it is needed. If you do, make sure its license allows it, and list it in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
6. Test the change on the platform you can reach, and say in the pull request what you tested.
7. Open the pull request and fill in the template.

## Layout

| Folder | Contents |
| --- | --- |
| `src/` | The engine and editor |
| `sdks/rebax/` | The runtime SDK that is compiled into exported programs (MIT) |
| `rebax_build_tool/` | The build tool used by the Makefile |
| `third_party/` | Third-party source used by the engine |
| `resources/` | Fonts, images and certificates |

## Licensing of contributions

The engine is licensed under the Mozilla Public License 2.0 and the runtime SDK under the MIT License. By submitting a contribution you agree that it is licensed under the license of the folder you changed, and that you have the right to submit it.

## Questions

Ask in the [discussions](https://github.com/PS2HomeDeveloper/Rebax/discussions).
