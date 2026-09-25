# Task 01 — Build and Tests

Date: 2026-09-20

## Changes

- Root `CMakeLists.txt`: `option(SAKURA_BUILD_EDITOR)`, `include(CTest)`.
- `Root/CMakeLists.txt`: optional Editor/SakuraEditor/editor-linked tests; CTest registration with labels; intentional fail probe.
- `Root/Engine/CMakeLists.txt` and `Root/Editor/CMakeLists.txt`: replaced `file(GLOB_RECURSE)` with explicit `ENGINE_SRC` / `EDITOR_SRC` lists.
- `SakuraReflectionPythonTest` now links **Engine only** (no Qt).
- Added `CMakePresets.json` (Windows MSVC Debug/Release + engine-only) and `CMakeUserPresets.json.example`.
- `.gitignore`: `CMakeUserPresets.json`.

## New / documented options

| Variable | Meaning |
|---|---|
| `SAKURA_BUILD_EDITOR` | ON builds Editor, SakuraEditor, SakuraTest, SakuraAssetTest |
| `SAKURA_QT_ROOT` / `Qt6_DIR` / `CMAKE_PREFIX_PATH` | Qt discovery (existing) |
| `SAKURA_PYTHON_ROOT` / `SAKURA_ENABLE_PYTHON` | Python embed (existing) |

## CTest names and labels

| Test | Labels |
|---|---|
| `Engine.Reflection.Native` | headless, engine |
| `Engine.Reflection.Python` | headless, engine, python |
| `Engine.Assets.WithEditorImport` | headless, engine, editor |
| `Engine.Gpu.MainSmoke` | gpu, editor |
| `Engine.Probe.MustFail` | probe (intentionally fails; excluded via `-LE probe`) |

## Commands used

```bat
cmake -S . -B build -DSAKURA_BUILD_EDITOR=ON
cmake --build build --config Debug --target SakuraReflectionTest
cmake --build build --config Debug --target SakuraReflectionPythonTest
cmake --build build --config Debug --target SakuraAssetTest
cmake --build build --config Debug --target SakuraTest
cmake --build build --config Debug --target SakuraEditor
ctest --test-dir build -C Debug --show-only
ctest --test-dir build -C Debug -LE probe --output-on-failure
ctest --test-dir build -C Debug -L probe --output-on-failure
cmake --build build --config Release --target SakuraReflectionTest
ctest --test-dir build -C Release -R "Engine.Reflection.Native|Engine.Probe.MustFail"
```

Preset form (new binary dir):

```bat
cmake --preset windows-msvc-debug -DSAKURA_QT_ROOT=<qt> -DSAKURA_PYTHON_ROOT=<python>
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

## Results (this machine)

- Debug `-LE probe`: **4/4 passed** (Native, Python, Assets, Gpu.MainSmoke).
- Debug `-L probe`: **Engine.Probe.MustFail Failed**, ctest exit **8** (non-zero as required).
- Release: `SakuraReflectionTest` builds; Diligent/SDL DLLs deploy via `sakura_copy_engine_runtime_dlls`.
- Release `-L probe`: **MustFail** → non-zero ctest exit **8** (required acceptance).
- Release `Engine.Reflection.Native` still needs `python310.dll` beside the exe (Engine links embed); after manual copy the process exited `0xC0000409` on this machine — recorded as **existing failure / not fixed in Task 01**, separate from the probe check.

## Remaining risks

- Full clean configure via new preset binaryDir `build/windows-msvc-debug` not re-run end-to-end here (would re-fetch deps); existing `build/` reconfigured instead.
- `SAKURA_BUILD_EDITOR=OFF` configure not fully rebuilt in this session (option wired).
- Release headless tests may fail without Python runtime next to the exe; Debug folder already had deps from prior editor builds.
- Gpu.MainSmoke still only proves ~30 frames without visual asserts (known limit until task 09/11).
- Asset tests still require Editor module until task 07 boundary split.
