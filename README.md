# OpenMidgard

An open source recreation of the Ragnarok Online client, focused on the classic pre-Renewal, 2008-era experience.

OpenMidgard combines classic gameplay and packet behavior with modern rendering backends and a Qt 6 / QML interface. The goal is to preserve how the game plays while improving how the client looks, builds, and runs on modern systems.

## Progress showcase

[![Watch the latest OpenMidgard progress showcase](https://img.youtube.com/vi/WLjiZiYqHPY/hqdefault.jpg)](https://www.youtube.com/watch?v=WLjiZiYqHPY)

[Watch the latest showcase on YouTube](https://www.youtube.com/watch?v=WLjiZiYqHPY) · [Previous showcase](https://www.youtube.com/watch?v=ApS1wgUmgzE)

## Current status

The client is in active development. Current work includes:

- **Classic gameplay:** character selection, world navigation, combat, NPC conversations and shops, inventory, equipment, and storage.
- **Modern Qt interface:** a shared window theme, redesigned Basic Info and Status windows, and an equipment window with full item names and a compact mode.
- **Skill tree:** job skill trees with prerequisite highlighting, pending point assignments, Apply/Reset controls, and selectable shortcut skill levels.
- **Shortcut bars:** up to three rows of nine slots, configurable keybindings, and item/skill drag and drop.
- **Combat presentation:** skill effects, projectiles, damage/healing numbers, status effects, and companion animations, with ongoing work against the original client behavior.
- **Rendering and platform work:** Vulkan and Windows Direct3D backends, plus Linux and macOS build paths.

Feature coverage and compatibility are still evolving; this is not yet a complete replacement for the original client. Windows with MinGW and Qt is the main day-to-day development workflow. Linux and macOS integration is less exercised.

For detailed skill/effect implementation status and remaining work, see the [skill status tracker](docs/skill/STATUS.md).

## Protocol target

The default client-originating map-server packet target is **`packet_ver 23` (`2008-09-10aSakexe`)**. Receive-side parsing also needs to match the packets actually sent by the server; this does not imply compatibility with every server configuration.

See [Packet Version Alignment](PACKET_VERSION_ALIGNMENT.md) for the profile, scope, and runtime overrides.

## Requirements

- CMake 3.21 or newer for the checked-in presets (manual configurations require at least 3.20).
- A C++17 compiler and Git.
- A compiler-compatible Qt 6 desktop kit with Core, Gui, Qml, and Quick for the Qt interface.
- Vulkan development files for Linux, and Vulkan with MoltenVK for macOS. On Windows, set `VULKAN_SDK` to enable compilation of the Vulkan backend.
- Ragnarok Online client data and a compatible server for in-game testing. Supply your own legally obtained game assets; building the source does not supply a runtime data installation.

## Build on Windows

### MinGW + Qt 6

The checked-in development preset uses these installation paths:

- Qt: `C:/Qt/6.11.0/mingw_64`
- MinGW: `C:/Qt/Tools/mingw1310_64`

From the repository root:

```powershell
cmake --preset mingw-qt-x64 -DRO_ENABLE_DEV_DEPLOY=OFF
cmake --build --preset build-mingw-qt-x64 --parallel 6
```

The executable is written to `build-mingw-preset/open-midgard.exe`. Matching Qt libraries, plugins, QML modules, and notices are deployed into the build output automatically.

For a debug build:

```powershell
cmake --preset mingw-qt-x64-debug -DRO_ENABLE_DEV_DEPLOY=OFF
cmake --build --preset build-mingw-qt-x64-debug --parallel 6
```

The debug output is in `build-mingw-preset-debug/`.

If your Qt or compiler installation differs, use a local `CMakeUserPresets.json` or a manual configuration with your own paths. The development presets set the compiler, make program, and `PATH` as well as `RO_QT_ROOT`; changing only the Qt root does not change the compiler installation. The `mingw-qt-x64-ci` preset is also available for environments that already select their toolchain.

**Local deployment:** the MinGW development presets enable `RO_ENABLE_DEV_DEPLOY` by default, which copies the executable and Qt runtime to the developer-specific `D:/Spel/OldRO` directory. The commands above turn this off. Enable it only when using that local runtime layout.

### Visual Studio 2022

The repository also provides Win32 and x64 MSVC presets:

```powershell
cmake --preset vs2022-x64 -DRO_ENABLE_DEV_DEPLOY=OFF
cmake --build --preset build-release-x64 --config Release --parallel
```

For Win32, use `vs2022-win32` and `build-release`. The corresponding output directories are `build64/Release/` and `build/Release/`.

The Visual Studio presets do not enable the Qt interface by default. To include it, configure with `-DRO_ENABLE_QT6_UI=ON -DRO_QT_UI_DEFAULT_ENABLED=ON` and set `RO_QT_ROOT` to a matching MSVC Qt kit. Native Direct3D 11 and Direct3D 12 backends are available only in Windows MSVC builds.

## Build on Linux

Install CMake, Ninja, a C++17 compiler, Qt 6 development packages including QML/Quick, and Vulkan development packages. Ensure the Qt QML runtime modules are installed too; some distributions package `QtQml.WorkerScript` separately.

```bash
cmake --preset linux-qt-vulkan
cmake --build --preset build-linux-qt-vulkan --parallel
```

The executable is written to `build-linux-qt-vulkan/open-midgard`. Set `CMAKE_PREFIX_PATH` during configuration if Qt is installed outside the toolchain's search paths.

Non-Windows builds enable the Qt interface and disable native Direct3D backends and local Windows deployment automatically.

## Build on macOS

There is no checked-in macOS preset. Use Xcode Command Line Tools, CMake, Ninja, Qt 6, and a Vulkan SDK with MoltenVK available to CMake:

```bash
cmake -S . -B build-macos \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRO_ENABLE_DEV_DEPLOY=OFF \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/macos

cmake --build build-macos --parallel
```

The executable is written to `build-macos/open-midgard`. This path may require additional platform integration work.

## Run the client

Provide a compatible runtime data directory containing the required GRF files or extracted `data/` files, together with the server connection configuration such as `data/clientinfo.xml`.

On Windows, the client resolves relative asset paths from the executable's directory. Place the built executable and its deployed runtime dependencies in your client data installation, keeping `qt.conf` and the deployed Qt directories together.

On Linux/macOS, you can select the runtime data root explicitly:

```bash
OPEN_MIDGARD_DATA_DIR=/path/to/runtime ./build-linux-qt-vulkan/open-midgard
```

For macOS, substitute `./build-macos/open-midgard`. `OPEN_MIDGARD_DATA_DIR` also selects the settings directory; it does not redirect Windows asset loading away from the executable directory.

## Development and bug reports

Useful starting points:

- [Qt GUI modernization plan](docs/QT_GUI_MODERNIZATION_PLAN.md)
- [Skill implementation plan](docs/SKILL_SYSTEM_1TO1_IMPLEMENTATION_PLAN.md) and [current status](docs/skill/STATUS.md)
- [Qt build and deployment notes](docs/qt_build_and_compliance.md)
- [Standalone Qt UI preview tool](tools/qtui_preview/README.md)

When reporting a bug, include the build/platform, renderer backend, steps to reproduce, and expected versus actual behavior. For gameplay issues, include the character's job, relevant skill/equipment, and server packet configuration when known. Windows builds write `debug_hp_<pid>.log`; if a crash produces files in `crash-dumps/`, include those alongside the matching log. Review logs and dumps for private information before sharing them.

## Repository layout

- `src/` — client source, including gameplay, networking, rendering, and Qt UI.
- `cmake/` — build configuration and deployment helpers.
- `docs/` — implementation plans, validation notes, and progress tracking.
- `tools/` — regression checks, audits, replay tooling, and UI previews.
- `third_party/` — third-party notices and bundled support files.

## License

See [LICENSE](LICENSE) for the GNU General Public License v3 text. Third-party components have their own notices; Qt build outputs include the notice bundle under `licenses/qt/`.
