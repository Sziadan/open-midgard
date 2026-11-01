# Release workflow review

The release convention remains `v0.0.x` / `Version 0.0.x`, with Windows Qt x64 ZIP and Linux Qt/Vulkan tar.gz assets built by GitHub Actions on release publication. Manual workflow runs build artifacts without publishing them.

## Corrections

- Qt 6.8.3 now uses its supported MinGW 13.1 toolchain, selected explicitly for C, C++ and make. The former job installed MinGW 9 and picked the first compiler found recursively. Toolchain versions are corroborated by https://doc.qt.io/qt-6.8/windows.html and Qt's `tools_mingw1310` repository.
- Windows configuration explicitly sets `CMAKE_BUILD_TYPE=Release`; the CI preset previously left the single-configuration build type unset.
- Runner images are pinned to Windows 2022 and Ubuntu 24.04. Jobs have bounded timeouts and release/ref concurrency does not cancel publication in progress.
- Build jobs have read-only contents permission and checkout credentials are not persisted. Only the asset-attachment job can write releases. Release tags are passed through an environment variable rather than interpolated into shell source.
- Linux installs the QML runtime modules actually needed by QtQuick and QtQml, including WorkerScript. Development packages alone were insufficient.
- Packaging fails for missing mandatory Windows runtime files, missing Linux QML/platform modules, or unresolved Linux shared libraries. Both packages contain the project README and license alongside the Qt notices. Both Qt QML import environment variables are set by the Linux launcher.
- The attachment job downloads only the client artifacts, merges their unique filenames, and adds `SHA256SUMS` for the two archives. Existing archive names and layouts are retained.
- Both platforms run the production-body world-cleanup regression and production watchdog lifecycle checks before packaging. These checks need no proprietary reference assets.

## Source portability blockers found by a clean Linux build

- CMake's global Qt includes used an unset Windows kit root on Linux. They now come from the discovered Qt imported targets.
- The main-loop watchdog now uses a condition variable and joining C++ thread on non-Windows platforms. The Windows event/thread implementation and polling/heartbeat/stall timings are preserved.
- The compatibility layer now includes SDK-defined key codes, `LONGLONG`, the depth-comparison constants, and a correctly packed 14-byte BMP file header. Actor-bound sizing explicitly uses `LONG` rather than assuming it is C++ `long` on every platform.
- CP949 map labels use iconv on non-Windows platforms; Windows retains its existing code-page conversion. CMake resolves the platform's Iconv dependency.

## Validation

The workflow passes actionlint. Both the MinGW Qt client and the Linux Qt/Vulkan Release client build successfully. The actual workflow packaging steps produce both archives locally; required files, Linux dependency resolution, launcher permissions, archive integrity and the Windows executable's equality with the completed build are checked.

Production regressions pass on Windows and Linux for world cleanup, watchdog start/stop/restart, and CP949 map-label conversion. Existing ammunition, targeting, casting-pose, weather, staff and frozen-shell checks also pass on MinGW. Documentation hygiene and whitespace checks pass. Full skill/effect parity or in-game acceptance is not implied by release-build success.
