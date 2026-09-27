Rebax

Rebax is a 2D and 3D game engine designed for creating games and applications that run on the PlayStation 2.

The project aims to bring together tools from the PS2 Developer Community and transform traditionally command-line-based workflows into an easy-to-use graphical application.

Rebax is designed to simplify the development process by providing a graphical interface and hiding unnecessary low-level details, allowing developers to focus on creating their games and applications rather than manually managing complex toolchain commands.

Export Formats

Rebax supports exporting projects in the following formats:

- ISO
- Raw ELF

Build Requirements

Before building Rebax, make sure the following requirements are available on your system.

1. Internet Connection

An active internet connection is required to download the appropriate toolchain package during the build process.

2. GCC or Clang

A C compiler such as GCC or Clang is required to compile the project's C source files and produce the Rebax executable.

3. Make

GNU Make is required to run the project's build system and compile the project.

4. SDL2

The SDL2 library is required for Rebax's graphical interface and windowing functionality, including handling the application window, input, rendering-related functionality, and other platform-level features.

5. wget or curl

Either wget or curl is required to download the prebuilt toolchain package corresponding to the target architecture.

Installing the Requirements

Linux

Termux

Windows

macOS

Building

Usage

License