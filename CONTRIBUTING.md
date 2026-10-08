# Contributing to Rebax

Thank you for contributing to Rebax. Rebax is a 2D/3D editor and engine for creating applications that run on the PlayStation 2. Contributions must preserve the project's architectural direction: shared systems, predictable ownership, minimal duplication, and native PS2 output.

This document is normative for new code and for refactoring existing code.

## 1. Core Engineering Rules

### 1.1 Prefer one shared implementation

Before adding a new function, widget, dialog, browser, loader, serializer, or exporter path, search the existing source tree for an equivalent implementation.

If the behavior is shared by two or more features, implement it in one reusable module and expose a small public API through a header. Callers should provide configuration and callbacks instead of copying the implementation.

Examples:

- File and folder selection belongs to the shared **File Manager** library.
- Common dialog frame, title bar, close button, and backdrop belong to shared UI code.
- Shared colors, spacing, and control styling belong to the theme/UI modules.
- Shared path and filesystem operations belong to path/filesystem utilities.

Do not create a second implementation merely because a caller needs a different mode. Add an explicit mode, root, policy, or callback when the behavior is genuinely the same system.

### 1.2 Do not duplicate rendering logic

A feature with several modes must not copy the complete draw/update implementation for every mode. Keep the common layout, input handling, resource lifetime, and drawing in one implementation. Use small conditional branches only for the parts that are genuinely different.

For example, the File Manager may support:

- picking a file;
- picking a folder;
- saving a file name;
- browsing project Assets or the device filesystem.

These are modes of one File Manager, not separate file browser implementations.

### 1.3 Keep public libraries generic

A reusable library must not contain assumptions about one caller or one dialog. The caller selects the operation through the public API and receives the result through a callback or a documented result object.

Public headers must document:

- the purpose of the module;
- supported modes and roots;
- ownership and lifetime of pointers and returned strings;
- error and cancellation behavior;
- update/draw/shutdown requirements;
- whether the API is synchronous or asynchronous.

Use the actual public module name in filenames, symbols, comments, and documentation. For example, the file-selection library is named:

```text
src/file_manager.c
src/file_manager.h
```

Do not reintroduce the old `asset_browser` name for new code.

### 1.4 Make the smallest correct change

When fixing a bug, modify the smallest set of files that directly implements the fix. Do not reformat unrelated files, migrate unrelated modules, or include historical changes in a new patch.

Every delivery must contain only files added or modified for the current request.

## 2. Ownership, Lifetime, and Dynamic Data

### 2.1 Make ownership explicit

Every allocated resource must have a clear owner and release path. This includes:

- heap memory;
- textures and font images;
- directory-entry strings;
- file handles and pipes;
- child processes;
- generated files and temporary directories;
- callbacks and user-data pointers.

Document whether a pointer is borrowed, owned, or valid only until the next API call. Never return a pointer to memory that is about to be freed or overwritten.

### 2.2 Protect data used during rebuilding

Do not keep a pointer into an array or buffer while calling a function that clears, reallocates, or rebuilds that array or buffer.

Copy stable values, such as paths or identifiers, into local storage before rebuilding the underlying collection. This rule prevents navigation bugs such as using an old directory entry after the File Manager has reloaded its contents.

### 2.3 Avoid unnecessary fixed global state

Existing UI modules use a single-instance state model where appropriate, but new code must not add global state casually. Prefer an explicit context structure when a module may eventually support multiple instances, multiple projects, or background operations.

If a fixed limit is necessary, define it near the data structure, validate it, and document the behavior when the limit is reached. Do not silently corrupt memory or create an invalid partial result.

### 2.4 Keep dynamic operations non-blocking in the editor

The editor UI is single-threaded. Long operations must not block the event loop. Use a small state machine and advance it from the module's update function.

This applies to operations such as:

- toolchain extraction;
- filesystem scans;
- image conversion;
- export preparation;
- compiler and linker processes.

Use incremental polling or another documented non-blocking mechanism. The draw function must only render current state; it must not start an expensive operation or perform hidden blocking I/O.

## 3. Module and UI Conventions

### 3.1 Separate responsibilities

Keep these responsibilities separate:

- **Model/data:** project, scene, node, filesystem, and export data.
- **Controller/update:** input, state transitions, validation, and callbacks.
- **Rendering:** drawing the current state only.
- **Persistence:** reading and writing project/scene formats.
- **Export:** converting project data into a native buildable output.

A dialog should not duplicate filesystem logic, scene serialization, or export-generation logic.

### 3.2 Reuse shared UI primitives

Use the existing theme, dialog, layout, button, text-field, selection, icon, and path utilities. Do not introduce local copies of colors, button rendering, modal backdrops, text-field behavior, or path normalization unless the shared API cannot express the required behavior.

If a shared primitive is missing, improve the shared primitive once and migrate callers to it.

### 3.3 Update and draw ordering

Follow the existing frame lifecycle:

1. poll window events;
2. update the active state and input;
3. advance asynchronous operations;
4. draw the editor;
5. draw modal overlays above the editor;
6. release resources during shutdown.

Do not perform input mutation inside draw functions. Do not update the same state from multiple independent callers.

### 3.4 Error handling

Errors must be visible and actionable. Return failure, invoke the documented failure callback, or add a clear exporter log line. Never report success when a file, directory, compiler command, conversion, or link step failed.

Paths supplied by users must be validated before use. Avoid shell injection: commands must be constructed from validated values, escaped where required, or passed through a safer process API when available.


### 3.5 Node Icon Design and Category Colors

Node icons must follow a consistent visual system across the editor. Each node belongs to a defined category, and every node must follow the global color assigned to its category.

The icon itself may use additional colors, shapes, highlights, indicators, and other visual details, but these additions must not replace or conflict with the node category color.

#### 3.5.1 Element Color

"Element" is the common visual base shared by all node categories.

The "Element" color must always be:

```text
#FFFFFF
```

Individual node icons may use the category color together with white for internal details, highlights, indicators, or other visual elements.

#### 3.5.2 Node Category Colors

Each node category must have one documented global color. Every node belonging to that category must use its category color as its primary category identity.

Current category colors:

| Category | Global Color |
|---|---|
| 2D | `#29B6F6` |
| 3D | `#808080` |

For example:

- A `Sprite2D` node belongs to the `2D` category and therefore uses `#29B6F6` as its global category color.
- A 3D node belongs to the `3D` category and therefore uses `#808080` as its global category color.

The individual icon design may differ between nodes in the same category. The category color must remain consistent so that users can identify the node category visually.

#### 3.5.3 Adding a New Node Category

When a new node category is introduced, its global color must be added to the Node Category Colors section above.

The contributor adding the category must:

- define one global color for the new category;
- document the category and its color in this section;
- use that color as the primary category identity for all nodes belonging to the category;
- keep the color consistent across all node icons in that category;
- update this section whenever the editor introduces a new node category or changes an existing category color.

A new node category must not introduce an undocumented category color.

Individual node icons may have unique designs, but the category color is controlled by the category and must not be independently changed by individual nodes.

## 4. Project and Build Rules

The top-level `Makefile` discovers C sources recursively under `src`. New source files placed under `src` are therefore part of the build automatically; do not add unnecessary manual source lists.

Generated files, embedded resources, and build outputs must remain distinguishable from hand-written source files. Do not edit generated files by hand. Update the generator or the source asset instead.

A contribution should build with the project's normal command:

```sh
make clean
make -j2
```

Before submitting a change:

- build from a clean state;
- check compiler diagnostics;
- run a smoke test when the application can start in the available environment;
- test cancellation and failure paths for asynchronous operations;
- verify that no old duplicate module or stale include remains after a rename.


## 5. Editor Preparation: Properties and Code Editor

The Inspector, node metadata, and embedded code editor are shared editor systems. New nodes and editor features must extend these systems instead of adding a private copy for one node or one dialog.

### 5.1 Property metadata is the source of truth

A node declares its editor-visible properties once through its `node_property_t` metadata. The Inspector builds its rows from that metadata automatically. Do not duplicate a node's property names, types, defaults, or ordering in a second panel-specific table.

The property definition must stay aligned with the native PS2 node interface:

- the editor-side property order;
- the native property order;
- the property type;
- the native `property_offsets` entry;
- the generated scene data order.

When a property is added, update the node implementation and verify both editor display and native export. Properties that remain at their defaults must not cause unused runtime systems or duplicate libraries to be exported.

### 5.2 Static versus runtime-controlled properties

The exporter must distinguish between values that are fixed in the exported scene and values that can change during execution.

- A property that remains constant may be emitted as typed native data or folded into generated native code.
- A property that can be changed by C/C++ code must remain available through the native node instance and its runtime update/draw path.
- The exporter must preserve behavior that can change at runtime, even when the initial scene value is a default.
- The exporter must not add a general-purpose runtime interpreter merely to support editor metadata.

For example, a `Sprite2D` rotation of `0` can be emitted as initial data, but the native sprite implementation must still preserve the rotation field and apply it when code changes the value at runtime. This is required for equivalent native output.

### 5.3 Inspector interaction and scrolling

The Inspector uses the shared `text_field` implementation for editable values and the shared `ui_scrollbar` implementation for vertical scrolling. New property types must add conversion and validation to the shared path rather than creating a new text-input implementation inside a node panel.

The shared scrollbar width is **8 pixels**. Its thumb height remains proportional to the content length and viewport height. Mouse-wheel scrolling, dragging, focus changes, and keyboard input must continue to work for every panel that uses it.

### 5.4 Embedded code editor

The embedded editor is named `code_editor`, not `script_editor`, because Rebax edits native C and C++ source files as well as headers. Its implementation is intentionally separated from File Manager and Inspector:

```text
src/code_editor.h       # Small public lifecycle API
src/code_editor.c       # Code view, tabs, editing, save/open actions
```

The Script tab in the central workspace opens this editor. The editor must use File Manager for file selection and saving rather than reimplementing directory traversal.

The initial editor supports:

- C, C++, C header, and C++ header files;
- new file creation;
- opening an existing file;
- saving and Save As;
- tabs/status information;
- multiline text editing;
- clipboard operations;
- vertical scrolling;
- a dark code-focused background and gutter.

Future code-editor subsystems such as diagnostics, symbol indexing, syntax highlighting, debugging, and build integration must be added as focused modules. Do not turn `code_editor.c` into a second file manager, compiler, or exporter.


## 5. Exporter Architecture: Native PS2 Output

The Rebax exporter must target native PS2 development.

Exported projects must produce a native PS2 application equivalent in structure and final behavior to a project developed directly using C/C++ source files, header files, and the PS2 GCC/PS2Dev toolchain.

The intended flow is:

```text
Rebax Project
      |
      v
   Exporter
      |
      v
Native PS2 Project
      |
      v
PS2 GCC / PS2Dev Toolchain
      |
      v
ELF
```

### 5.1 No Rebax-specific runtime interpreter

The exported PS2 application must not require the PS2 runtime to understand or interpret Rebax-specific project files, editor metadata, `.rscene` files, or an editor-only virtual machine in order to execute.

Rebax project data must be resolved on the host during export whenever possible. The exporter should convert scenes, properties, resources, and required assets into native PS2 source code, generated data, or native runtime resources before the PS2 build begins.

The final ELF must be produced by the standard PS2 toolchain, not by a special Rebax execution format.

### 5.2 Equivalent Native Output, not byte-for-byte identity

The goal is **Equivalent Native Output**. The generated ELF does not need to be byte-for-byte identical to an ELF produced by a developer manually writing equivalent C/C++ code.

Differences are expected because of:

- section ordering;
- symbol names;
- compiler and linker optimization choices;
- source-generation details;
- object-file ordering;
- memory layout decisions.

The important guarantees are that the exported project is a normal native PS2 project, is built through the same class of PS2 GCC/PS2Dev toolchain, and has equivalent intended runtime behavior and platform requirements.

### 5.3 Exporter responsibilities

The exporter is responsible for:

1. validating the project and export settings;
2. resolving the scenes and resources required by the project;
3. resolving only the node types and engine modules actually used;
4. converting project properties into typed native representations;
5. converting or copying required assets into the exported project;
6. generating readable C/C++ source, headers, data files, and the build inputs;
7. generating any required linker or platform configuration;
8. invoking the PS2 toolchain with explicit, reproducible settings;
9. reporting compiler, linker, and packaging failures accurately;
10. placing the final ELF and any requested output artifacts in the selected destination.

Generated projects should be inspectable and buildable outside the editor whenever the required PS2 toolchain and dependencies are available.

### 5.4 What must not be exported unnecessarily

Do not copy editor-only systems into the PS2 output merely because they exist in the Rebax source tree. Do not export:

- editor windows and panels;
- editor-only metadata;
- unused node implementations;
- unused assets;
- host-only filesystem code;
- project browsing code;
- a parser for Rebax project files when the data can be generated ahead of time.

A small, explicit native runtime may be included when it is required by the generated application, but it must be ordinary PS2-side C/C++ code and must not act as a general Rebax project interpreter.

### 5.5 Determinism and reproducibility

For the same project, settings, toolchain, and source inputs, export should produce the same generated project structure and equivalent build inputs. Avoid embedding machine-specific absolute paths in generated source unless they are clearly documented.

Generated output must use stable ordering for:

- scenes;
- nodes;
- properties;
- assets;
- generated source files;
- compiler and linker inputs.

Deduplicate shared node types, assets, and generated resources. If an image or resource is used by multiple scenes, convert or copy it once and reference the shared result.

### 5.6 Debug and Release

Debug and Release exports must use the same native project architecture. They may differ in compiler flags, symbols, optimization, logging, and post-link stripping, but neither mode may depend on a different proprietary runtime format.

Release-only processing, such as stripping symbols, must happen after a successful native link and must never hide a failed build.

### 5.7 Native Node Export Contract

The first export milestone is **Native Node Export**. Every node that exists in the editor must have one corresponding PS2-native implementation under `embedded/nodes/`. The editor preview and the exported PS2 implementation must use the same property names, order, types, defaults, and semantics.

The native node file is the source of truth for its property metadata. Its `node_property_t` array, `property_offsets` array, `instance_size`, and `node_interface_t` must remain aligned. Property counts must be derived from the property array rather than repeated as unrelated numeric constants.

For each scene export, the exporter must:

1. select every node type actually used by the scene;
2. copy each selected native node implementation once;
3. copy only the dependencies required by those implementations;
4. convert scene property values into typed native data;
5. instantiate the native node with its real `instance_size`;
6. apply every exported property through its matching `property_offsets` entry;
7. call the native `init`, `update`, `draw`, and `destroy` lifecycle functions;
8. omit unused node implementations, formats, assets, and libraries.

The exported application must therefore execute the same meaningful node behavior as the editor preview. A property must not merely appear in the Inspector or be stored in `scene_data.c` while being ignored by the PS2 implementation.

#### 5.7.1 Coordinate convention

Rebax intentionally uses a beginner-friendly 2D coordinate convention:

- `(0, 0)` is the center of the PS2 output viewport;
- positive X moves right;
- positive Y moves down;
- a Sprite2D position describes the center of the sprite, not its top-left corner;
- a sprite with Position X = `0` and Position Y = `0` is centered on the screen;
- the editor viewport and the native PS2 renderer must preserve this convention.

This is an intentional Rebax design decision. Do not change it to a top-left-origin convention without a deliberate format/API migration.

#### 5.7.2 Transform properties

Transform properties are runtime data, not editor-only labels:

- `Element2D`: Position X/Y, Rotation, Scale X/Y;
- `Element3D`: Position X/Y/Z, Rotation X/Y/Z, Scale X/Y/Z;
- `Sprite2D`: Position X/Y, Rotation, Scale X/Y, Image Path, Raw Width, Raw Height, and Raw Format.

When a transform is not yet visually rendered by a particular node, its native implementation must still store and expose the property correctly so later runtime behavior and the public SDK can use it without changing the scene format.

Sprite2D rotation is stored in radians and must be applied around the sprite center in both the editor preview and the native PS2 renderer. Raw dimensions and pixel format are used only for headerless RAW assets and must not affect ordinary PNG, JPEG, BMP, TGA, TIFF, TIM, or TIM2 assets.

#### 5.7.3 Official PS2 build references

Native output must follow the official PS2SDK/gsKit conventions. Rebax does not run `make` or include the PS2SDK rule files; `ps2_build.c` reproduces the commands of these official fragments directly (same compilers, flags and order):

```text
$(PS2SDK)/samples/Makefile.pref
$(PS2SDK)/samples/Makefile.eeglobal
$(PS2SDK)/samples/Makefile.eeglobal_cpp
$(PS2SDK)/samples/Makefile.iopglobal
$(PS2SDK)/samples/Makefile.ioprp
```

Rebax must not copy the complete PS2SDK or gsKit source tree into every exported project. `ps2_build.c` compiles each source and links only the libraries required by the selected native nodes. C-only exports use the C rules; an export containing C++ sources uses the official C++ rules and the C++ link driver. EE ELF/ERL/library, IOP IRX/library and IOPRP targets are supported by the same module; when the official fragments change, update `ps2_build.c` to match.

### 5.8 C/C++ Project Source Export

The second export milestone copies the project's native source tree into the generated PS2 project without converting it into a Rebax-specific format.

The generated layout is:

```text
Native PS2 Project/
└── src/
    ├── scene_loader_main.c     # generated weak fallback entry point
    ├── scene_runtime.c/.h      # small native scene bridge
    ├── scene_data.c             # generated typed scene data
    ├── node implementations    # only node types used by the scenes
    └── project/                 # copied project-owned C/C++ files and headers
        └── <original project-relative paths>
```

The exporter copies these project extensions while preserving their relative paths:

- C: `.c`;
- C++: `.cc`, `.cpp`, `.cxx`;
- C headers: `.h`;
- C++ headers: `.hh`, `.hpp`, `.hxx`.

The exporter does not flatten or rename project files. This keeps local `#include "header.h"` relationships intact and makes the generated project inspectable by a native developer. Build/cache directories such as `.git`, `.svn`, `build`, and `Temp` are not copied as source inputs.

The build discovers source files recursively and compiles each object exactly once. If the project contains no C++ source, it follows the official `Makefile.eeglobal` rules and links with the C driver. If at least one `.cc`, `.cpp`, or `.cxx` file is present, it follows the official `Makefile.eeglobal_cpp` rules and links with the C++ driver. C and C++ files may therefore coexist in one exported project.

Project source files may declare additional native dependencies using the same file-local markers as native node implementations:

```c
/* @PS2_EXPORT_INCLUDES: -I$(PS2SDK)/ee/include */
/* @PS2_EXPORT_LIBS: -lpacket */
```

The exporter scans copied project sources and headers, deduplicates those flags, and passes them to the compiler and linker. It must not guess libraries from filenames or copy the PS2SDK into the project.

The generated scene bootstrap is a weak fallback `main`. A project-owned strong `main` in C or C++ overrides it, allowing a native developer to own the application entry point. If no project `main` exists, Rebax's generated scene loop remains the executable entry point.

Project source export is a build integration milestone, not the Native Rebax SDK. Project code must not depend on private exporter headers or `.rscene` parsing. Public runtime interfaces will be introduced by the Native Rebax SDK milestone.

### 5.9 Native Rebax SDK

The Native Rebax SDK is the public runtime API available to project-owned C/C++ files in an exported PS2 project. Its public header is `rebax_sdk.h`; project code must include that header rather than including `export_internal.h`, `scene_runtime.h`, or private node implementation structures.

The SDK provides:

1. stable node type identifiers and type-name lookup;
2. creation by type or by the editor-visible name;
3. property discovery by index or name;
4. typed float, integer, and string setters/getters;
5. the native `init`, `update`, `draw`, `destroy`, and `free` lifecycle;
6. consistent error codes for missing properties, wrong types, unsupported nodes, invalid arguments, and allocation failures.

The current SDK catalog is:

| Node | Public properties |
|---|---|
| `Element` | none |
| `Element2D` | Position X, Position Y, Rotation, Scale X, Scale Y |
| `Element3D` | Position X, Position Y, Position Z, Rotation X, Rotation Y, Rotation Z, Scale X, Scale Y, Scale Z |
| `Sprite2D` | Position X, Position Y, Rotation, Scale X, Scale Y, Image Path, Raw Width, Raw Height, Raw Format |

The SDK uses the exact property names, types, defaults, and coordinate semantics already defined by the native node interfaces. Position `(0, 0)` remains the center of the output viewport. Sprite2D rotation remains in radians and image paths are copied into SDK-owned string storage.

Example native project code:

```c
#include "rebax_sdk.h"

void game_setup(void) {
    rebax_node_t *sprite = NULL;
    if (rebax_node_create(REBAX_NODE_SPRITE_2D, &sprite) != REBAX_OK) return;

    rebax_node_set_float(sprite, "Position X", 0.0f);
    rebax_node_set_float(sprite, "Position Y", 0.0f);
    rebax_node_set_float(sprite, "Scale X", 1.0f);
    rebax_node_set_float(sprite, "Scale Y", 1.0f);
    rebax_node_set_string(sprite, "Image Path", "Assets/player.png");

    rebax_node_update(sprite, 1.0f / 60.0f);
    rebax_node_draw(sprite);
    rebax_node_free(sprite);
}
```

The generated SDK implementation contains one shared native interface entry per current node type. It does not generate one implementation per instance and does not interpret `.rscene` files on the PS2. Adding a new node requires adding its native implementation and metadata; the exporter must then add that node to the SDK catalog and keep the public header's type list and documentation synchronized.

SDK-owned strings must not be freed by project code. Call `rebax_node_free` exactly once for each created node; it invokes the node's native destroy callback and releases SDK-owned property storage.


## 6. Exporter Module Structure

The exporter is intentionally split into focused internal modules. Keep the public API in `src/ps2_exporter.h` stable while extending the implementation through the internal modules below. The editor should depend only on the public header and must not include `export_internal.h`.

```text
src/
├── ps2_exporter.h       # Stable public API used by the editor
├── ps2_exporter.c       # Export orchestration and state machine
├── export_internal.h    # Private contract shared only by exporter modules
├── export_process.c     # Non-blocking toolchain process and log queue
├── export_scene.c       # .rscene discovery, parsing, and scene C data generation
├── export_nodes.c       # Used-node selection, source copying, and PS2 dependencies
├── export_project.c     # Project C/C++ source/header collection and marker scanning
├── export_assets.c      # Project asset conversion and deduplicated embedding
├── export_codegen.c     # Native PS2 runtime and entry-point templates
└── ps2_build.c          # Direct PS2 toolchain driver (compile, link, strip, IRX, IOPRP)
```

### 6.1 Responsibilities and boundaries

- `ps2_exporter.c` coordinates stages; it must not become a second implementation of scene parsing, asset conversion, node selection, or process polling.
- `export_internal.h` is private. It defines the internal contract and shared export state; it is not a feature API.
- `export_process.c` owns non-blocking process I/O, cancellation, exit status, and the exporter log queue. It must not know scene or node semantics.
- `export_scene.c` is the only module that parses `.rscene` files. Other stages consume the parsed results and must not reparse the same files independently.
- `export_nodes.c` selects each used node type once, copies its native PS2 source once, and collects only the dependencies declared by the copied source.
- `export_project.c` copies project-owned C/C++ sources and headers under `src/project/` while preserving relative paths; it reuses the shared export-marker scanner and does not parse scenes.
- `export_assets.c` converts each logical asset once and emits native data. It must not embed the same asset once per scene or node instance.
- `export_codegen.c` emits project-specific native C files and the small native runtime bridge. It must not emit a private Rebax project interpreter.
- `export_codegen.c` also emits the single public `rebax_sdk.c` implementation from the complete current node catalog; the public declaration is `embedded/nodes/rebax_sdk.h` and is copied into each native export.
- `ps2_build.c` plans and runs the PS2 compiler and linker commands without `make`; it must include each source, include flag, and library flag only once and must stay free of scene or node semantics.

### 6.2 Stable public API

The following functions are the public boundary and must remain source-compatible unless a deliberate API migration is approved:

```c
int ps2_export_start(const char *exe_name, int is_release, const char *output_dir);
void ps2_export_update(void);
ps2_export_status_t ps2_export_get_status(void);
const char *ps2_export_poll_next_line(void);
void ps2_export_cancel(void);
```

Internal files may be renamed or reorganized behind this boundary. `export_dialog.c` must continue to use the public API rather than reaching into exporter internals.

### 6.3 One analysis pass

An export must follow this order:

```text
Start
  ↓
Create and clear export workspace
  ↓
Analyze project scenes once
  ↓
Collect unique node types and asset references
  ↓
Copy unique native node sources and dependencies
  ↓
Convert unique assets
  ↓
Generate native C/C++ and headers
  ↓
Plan deterministic compile and link commands
  ↓
Run PS2 GCC/PS2Dev through ps2_build and the non-blocking process module
  ↓
Copy the final ELF
```

Do not add a second scan of `.rscene` files merely to discover nodes, images, or properties. Extend the scene analysis result instead. This avoids inconsistent results and unnecessary work.

### 6.4 Deduplication contract

The generated project must deduplicate at every relevant level:

1. **Node types:** `Sprite2D` used by many instances is copied and compiled once.
2. **Node implementation:** instances reference the same `node_interface_t` implementation; no per-instance C source is generated.
3. **Assets:** the same canonical asset path is embedded once.
4. **Headers and shared modules:** shared runtime files are emitted once.
5. **Compiler flags:** repeated `-I` and `-l` declarations are removed before the build commands are planned.
6. **Build inputs:** the generated object list contains each source exactly once.

Multiple scene instances are represented as data in generated files such as `scene_data.c`; they must not produce duplicated copies of the implementation.

### 6.5 `embedded/nodes` is native runtime source

`embedded/nodes/` contains hand-written, PS2-side C/C++ runtime sources. It is not a per-project generated directory and it must not contain one generated implementation per scene instance.

The exporter reads this source repository, selects the node implementations needed by the analyzed project, and copies each selected implementation once into the temporary native project. The PS2 compiler then builds those ordinary sources.

When adding a node:

- keep its PS2 runtime implementation in `embedded/nodes/`;
- add the editor-side registration separately where needed;
- document its `@NODE` declaration;
- declare its required PS2 include and library flags in the source comments;
- avoid including loaders or libraries that the node does not use;
- verify that two instances of the node still produce one copied source file.

### 6.6 Generated native project layout

The temporary export output must remain inspectable and follow this shape:

```text
Temp/export/
└── src/
    ├── scene_loader_main.c
    ├── scene_runtime.c
    ├── scene_runtime.h
    ├── scene_data.c
    ├── embedded_images.c       # only when assets are used
    ├── embedded_images.h       # only when assets are used
    ├── node_interface.h
    ├── engine_context.h        # only when required
    ├── engine_context_impl.c   # only when required
    └── <unique used node sources>
```

The generated project must be buildable with the ordinary PS2 GCC toolchain and must not require `.rscene` files, the editor, or a Rebax-specific interpreter on the PS2.

### 6.7 Safe exporter refactoring

When moving code between exporter modules:

1. preserve the public declarations in `ps2_exporter.h`;
2. move one responsibility at a time;
3. keep shared data behind `export_internal.h`;
4. compile after each responsibility is moved;
5. compare generated file names and build inputs before and after the move;
6. test cancellation, failed builds, missing assets, and empty/invalid scenes;
7. do not mix unrelated editor changes into the exporter refactor.

A module split is successful only when behavior is preserved and each module has one clear reason to change.

## 7. Exporter Output and Native Build Guide

The exporter produces a normal native PS2 project. The build compiles the generated and selected native sources with the PS2SDK flags, adds only discovered include/library flags, and produces `<name>.elf`. Debug and Release use the same project structure; Release may strip symbols after a successful link.

A developer diagnosing an export should inspect, in order:

1. the exporter log for scene and node discovery;
2. the generated `src/scene_data.c` for typed scene data;
3. the compile and link commands recorded in the export log;
4. the PS2 compiler/linker output;
5. the final ELF path.

The generated files are build artifacts. Fix the exporter or the native node source rather than editing generated files manually.

## 8. Generated Code Quality

Generated C/C++ should be readable enough to diagnose a failed export. Use stable names, typed values, comments for generated sections, and clear ownership of generated resources.

Generated code must:

- compile with the selected PS2 toolchain;
- avoid undefined behavior where possible;
- respect PS2 memory and alignment requirements;
- avoid unbounded allocation during runtime initialization;
- check asset and resource availability;
- avoid relying on host-only APIs;
- keep platform-specific code isolated behind documented native modules.

If a generated representation is not sufficient for a feature, improve the exporter or the native runtime module deliberately. Do not silently fall back to interpreting the original Rebax data on PS2.

## 9. Review Checklist

Before merging a contribution, verify:

- [ ] An existing shared module was reused where applicable.
- [ ] No second implementation of an existing behavior was introduced.
- [ ] Public APIs and ownership rules are documented in headers.
- [ ] Pointers are not used after their backing collection is rebuilt.
- [ ] Dynamic or slow work is non-blocking in the editor.
- [ ] Generated files are produced by generators, not hand-edited.
- [ ] The project builds cleanly from a clean state.
- [ ] The exporter generates a native PS2 project and native build inputs.
- [ ] The exported ELF does not require a Rebax-specific project interpreter.
- [ ] Only required node types, assets, and runtime modules are exported.
- [ ] Debug and Release use the same native architecture.
- [ ] Export failures are reported and do not produce false success.
- [ ] The submitted patch contains only files relevant to the current change.

## 10. Scope of This Document

This document defines the direction and rules for future work. It does not by itself claim that every exporter feature described above is already complete. When implementing a missing capability, update the exporter incrementally while preserving the Native PS2 output contract described in Sections 5 and 6.
