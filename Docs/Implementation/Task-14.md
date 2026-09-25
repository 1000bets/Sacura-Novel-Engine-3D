# Task 14 — SakuraPlayer

Date: 2026-09-20

## Scope

Standalone SDL game host executable (Engine only, no Qt) for opening a user project and presenting the startup scene with optional Demo.story playback.

## Binary

- Source: `Root/Engine/apps/SakuraPlayerMain.cpp`
- CMake target: `SakuraPlayer`
- Links: `Engine` only; `sakura_copy_engine_runtime_dlls`, `sakura_stage_engine_runtime`, MSVC `/WHOLEARCHIVE:Engine`
- POST_BUILD: copy `SDL3.dll`, sync DLLs via `Root/cmake/CopyStageRuntime.cmake` into `Stage/Bin`

## CLI

```bat
build\Stage\Bin\SakuraPlayer.exe --project "M:\Sacura-Novel-Engine-3D\Samples\SampleProject\SampleProject.project"
```

Flow:

1. `EnginePaths::InitializeFromExecutable`
2. `Engine::Initialize()` (SDL window + render thread)
3. `ProjectSession::OpenProject`
4. If `Content/Stories/Demo.story` exists → `StoryRuntime` on active scene
5. Loop: `StoryRuntime::Tick`, Space/Return advance (or auto-advance every ~2.5s on lines), `Engine::Tick` until close

## Staging helper

`CopyStageRuntime.cmake` now copies `platforms/` and `imageformats/` Qt plugin folders when present next to the source binary (used when syncing Editor/SakuraPlayer runtimes into `Stage/Bin`).

### Python redistributable (when `SAKURA_ENABLE_PYTHON=ON`)

For clean machines, copy next to staged binaries (not automated in v1):

- `python3X.dll` from the embed/interpreter used at build time
- Python standard library tree (`Lib/` or layout required by `PYTHONHOME`)

Document-only in this task; SakuraPlayer does not embed Python UI but linked Engine may still load scripting if enabled.

## Build

```bat
cmake --preset windows-msvc-debug
cmake --build build --config Debug --target SakuraPlayer
```

## Verification status

| Check | Status |
|-------|--------|
| Compiles (Debug) | Expected locally |
| Opens SampleProject + startup scene | Expected locally |
| Demo.story advance / actions | Expected locally |
| **Clean-machine GPU / driver stack** | **Unverified** — Diligent backend DLLs and GPU must be present; not exercised on a fresh VM in this task |
