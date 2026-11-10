# Rebax

**Rebax is an open-source game development environment built to make creating games more accessible, flexible, and connected.**

The project brings the essential parts of game development into one place: a visual editor, scene and node workflows, asset and project management, and tools that help turn a project into a playable build.

Rebax is being developed with a long-term goal: to grow into a capable, extensible engine where developers can work visually or closer to the code, without being locked into a single way of building their games.

## Vision

Game development involves many different tasks and tools. Rebax aims to bring these workflows together in a clear and approachable environment, while keeping the underlying project and development process open to developers.

The goal is not simply to provide an editor, but to build an engine that can evolve with its users and support projects of different sizes and styles.

## Core Principles

- **Accessible workflow** — make common development tasks understandable and straightforward.
- **Flexible development** — support visual workflows alongside code-oriented development.
- **Modular architecture** — keep engine systems and editor components organized so they can grow independently.
- **Extensibility** — make it possible to expand the engine with new node types, tools, resources, and platform capabilities.
- **Open development** — keep the project transparent and welcome feedback, testing, and contributions.

## What Rebax Brings Together

- **Visual editor** for working with projects, scenes, viewports, and object properties.
- **Scene and node system** for organizing game objects and their behavior.
- **2D and 3D workflows** as foundations for building different kinds of experiences.
- **Code-oriented tools** for developers who want more direct control over their projects.
- **Project and asset management** to keep development resources organized.
- **Build and export workflow** designed to connect the editor project with the environments it targets.

These areas form the foundation of the project and will continue to develop over time.

## Project Status

Rebax is an actively evolving project. Its editor, engine systems, development workflows, and platform support are being built and improved incrementally.

Some parts are more mature than others, and available functionality may change as the architecture develops. Builds and features should therefore be considered work in progress unless they are explicitly marked as stable.

Feedback, testing across different environments, and clear bug reports are especially valuable at this stage.

## Project Structure

The repository is organized around a few main areas:

- `src/` — engine, editor, project, and application source code.
- `sdks/` — development interfaces and runtime components.
- `resources/` — resources used by the application.
- `tools/` and `rebax_build_tool/` — supporting development and build utilities.
- `toolchains/` — platform toolchains used by the build process.
- `third_party/` — external libraries and dependencies.
- `examples/` and `docs/` — example projects and documentation as they grow.
- `tests/` — project tests.

The structure may evolve as Rebax grows; these directories describe the main responsibilities rather than a fixed public API.

## Getting Started

The source code and build instructions are maintained in this repository. Start by cloning the project and reviewing the build configuration:

```sh
git clone https://github.com/PS2HomeDeveloper/Rebax.git
cd Rebax
```

A C compiler, GNU Make, SDL2 development files, and the download utilities required by the build process are needed. Additional dependencies may be required depending on the host environment and build target.

## Contributing

Contributions and constructive feedback are welcome. Before making substantial changes, review [CONTRIBUTING.md](CONTRIBUTING.md) and the existing project structure.

When reporting an issue, include the steps to reproduce it, the environment in which it occurred, and relevant error output or logs. This makes problems easier to understand and resolve.

## Third-Party Software

Rebax uses external libraries and tools. Their license information is provided with the relevant components and in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License

Rebax is released under the [MIT License](LICENSE).
