# Rebax

Rebax is a 2D and 3D game engine and editor for creating games and applications that run on the **PlayStation 2**.

The project aims to bring together tools from the **PS2 Developer Community** and transform traditionally command-line-based workflows into an easy-to-use graphical application.

Rebax is designed to simplify the development process by providing a graphical interface and hiding unnecessary low-level details, allowing developers to focus on creating their games and applications rather than manually managing complex toolchain commands.

## Export Formats

Rebax supports exporting projects in the following formats:

- **ISO**
- **Raw ELF**

## Build Requirements

The following requirements are needed to build Rebax.

### 1. Internet Connection

An active internet connection is required to download the appropriate toolchain package during the build process.

### 2. GCC or Clang

A C compiler such as **GCC** or **Clang** is required to compile the project's C source files and produce the Rebax executable.

### 3. Make

**GNU Make** is required to run the project's build system and compile Rebax.

### 4. SDL2

The **SDL2** library is required for Rebax's graphical interface, window management, input handling, and other platform-level functionality.

### 5. wget or curl

Either **wget** or **curl** is required to download the prebuilt toolchain package corresponding to the target architecture.

## Installing the Requirements

### Linux

### Windows

### macOS

### Android

## Building

## Usage

## Notes

### iOS

- The iOS package (`.ipa`) is **not signed**. It cannot be installed on a device as downloaded. It must be signed first, either with your own Apple ID or developer account, or with a sideloading tool such as AltStore or Sideloadly. Signing is up to whoever installs it.
- The iOS build is experimental. It has not been tested on a real device and is not guaranteed to work.
- **Exporting to PlayStation 2 is not supported on iOS.** iOS does not allow apps to run other programs, so the build tools (`make` and `ps2dev`) cannot run there. On iOS you can create and edit projects, then copy the project folder to a PC, Mac or Linux computer and export it from there. The app stores its files in its Documents folder, which should be available in the Files app.

## License

Rebax is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for details.