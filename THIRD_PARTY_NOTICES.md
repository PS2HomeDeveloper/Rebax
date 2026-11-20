# Third-Party Notices

Rebax Engine includes and uses the third-party software listed below. Each component remains under its own license. The full license text of each component is available from the project linked in the table or, where noted, in this repository.

## Included in this repository

| Component | Version | License | Location | Project |
| --- | --- | --- | --- | --- |
| Mbed TLS | 3.6.7 | Apache License 2.0 | `third_party/mbedtls` | https://github.com/Mbed-TLS/mbedtls |
| stb_image | 2.30 | Public domain or MIT | `third_party/stb_image.h` | https://github.com/nothings/stb |
| stb_truetype | 1.26 | Public domain or MIT | `third_party/stb_truetype.h` | https://github.com/nothings/stb |
| XZ Embedded | master | BSD Zero Clause (0BSD) | `third_party/xz_embedded.c`, `third_party/xz_embedded.h` | https://tukaani.org/xz/embedded.html |
| LZMA SDK (LZMA2 encoder) | 7-Zip | Public domain | `rebax_build_tool/src/lzma2_encoder.c` | https://www.7-zip.org/sdk.html |
| Mozilla CA certificate bundle | | Mozilla Public License 2.0 | `resources/certs/cacert.pem` | https://curl.se/docs/caextract.html |
| Space Grotesk font | | SIL Open Font License 1.1 | `resources/fonts` (license in `resources/fonts/OFL.txt`) | https://github.com/floriankarsten/space-grotesk |

Mbed TLS is available under the Apache License 2.0 or the GNU General Public License 2.0 or later. Rebax uses it under the Apache License 2.0. Its license is in `third_party/mbedtls/LICENSE`.

## Downloaded and used by Rebax

The following software is not stored in this repository. It is downloaded as part of the SDK and toolchain packages from the [Rebax SDKs](https://github.com/PS2HomeDeveloper/Rebax-SDKs) and [Rebax Toolchains](https://github.com/PS2HomeDeveloper/Rebax-Toolchains) repositories, which list it in their own notices.

| Component | License |
| --- | --- |
| SDL2 | zlib License |
| LLVM / Clang / LLD | Apache License 2.0 with LLVM Exceptions |
| musl libc | MIT License |
| mingw-w64 | Zope Public License 2.1 and public domain |
| GCC and GNU Binutils (PlayStation 2 and PlayStation 1) | GNU GPL 3.0 or later, with the GCC Runtime Library Exception |
| ps2sdk, gsKit, ps2link | Academic Free License 2.0 |
| PSn00bSDK | Mozilla Public License 2.0 |
| Android NDK | Apache License 2.0 |

## Your exported programs

The programs you export with Rebax include the runtime SDK from `sdk`, which is under the MIT License, and the SDK and compiler runtime libraries of the platform you export to. Depending on the platform, the license terms of those libraries may require a notice in your distribution. Check the notices of the [Rebax SDKs](https://github.com/PS2HomeDeveloper/Rebax-SDKs) repository for the platform you target.

## Reporting a mistake

If a component is missing or a license is described incorrectly, please open an issue.
