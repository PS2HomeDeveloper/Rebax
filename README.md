# Rebax Engine

Rebax Engine is a cross-platform engine for creating applications and games. It is built for native, low-level development: a project is exported as a real program for the target platform, with no bundled interpreter or virtual machine.

Website and downloads: **https://rebax.pages.dev/**

## Status

Rebax is in early development. The table shows what has been tested so far.

| Platform | Status |
| --- | --- |
| Linux | Tested |
| Android | Tested on a device |
| PlayStation 2 | Tested on real hardware |
| Windows | Not yet tested |
| macOS | Not yet tested |
| iOS | Not yet tested |
| PlayStation 1 | Not yet tested |

Reports from people who try the untested platforms are very welcome.

## Download

Prebuilt packages for every release are available on the [website](https://rebax.pages.dev/) and on the [Releases](https://github.com/PS2HomeDeveloper/Rebax/releases) page.

## Building from source

You need Git and `make`. A C compiler is optional: when none is installed, the build downloads one automatically.

```
git clone https://github.com/PS2HomeDeveloper/Rebax.git
cd Rebax
make
```

`make` builds Rebax for the machine you are using. To build for another platform, give the platform and the architecture:

```
make <platform> <architecture>
```

| Platform | Architectures |
| --- | --- |
| `windows` | `x86_64`, `x86`, `arm64`, `arm` |
| `linux` | `x86_64`, `x86`, `arm64`, `arm` |
| `macos` | `x86_64`, `arm64` |
| `ios` | `arm64`, `x86_64` |
| `android` | `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64` |

For Android, `make android all` builds one APK that contains all four architectures, and `make android <architecture>` builds a single one. Results are written to `build/output/`.

The older form `make linux-x86_64` still works.

### Android signing

Android packages are signed with the file `keystore/rebax_signing.pem`, which holds an unencrypted RSA private key followed by its certificate, both in PEM format. If the file does not exist, the build creates one. To create it without building, run `make keygen`. Keep the file and use the same one for every release, because Android only updates an app that is signed with the same key. Never commit it: the `keystore` folder is ignored by Git.

## Repositories

| Repository | Purpose |
| --- | --- |
| [Rebax](https://github.com/PS2HomeDeveloper/Rebax) | The engine and editor (this repository) |
| [Rebax SDKs](https://github.com/PS2HomeDeveloper/Rebax-SDKs) | SDK packages for each target |
| [Rebax Toolchains](https://github.com/PS2HomeDeveloper/Rebax-Toolchains) | Compiler packages |
| [Rebax Website](https://github.com/PS2HomeDeveloper/Rebax-Website) | The website |

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) first, and the [Code of Conduct](CODE_OF_CONDUCT.md). To report a security problem, see [SECURITY.md](SECURITY.md).

## License

- The engine, the editor and the build tool are licensed under the [Mozilla Public License 2.0](LICENSE).
- The runtime SDK in [`sdks/rebax`](sdks/rebax), which is compiled into the programs you export, is licensed under the [MIT License](sdks/rebax/LICENSE). Programs and games you make with Rebax belong to you.
- Third-party software is listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- "Rebax", "Rebax Engine" and the Rebax logo are trademarks of PS2HomeDeveloper. See the [trademark policy](TRADEMARKS.md).
