# Task 07 — Editor boundaries, session, real diagnostics

Date: 2026-09-20

## Changes

### Module split
- Logical layers: EngineRuntime (`Engine`), EditorCore (`EditorCore`), QtEditorFramework (`Editor`).
- `EditorCore` = AssetTools + ufbx (`Sakura::EditorCoreThirdParty`), no Qt.
- `Editor` = Qt shell (`Sakura::EditorQtThirdParty`) linked on top of EditorCore.
- `SakuraAssetTest` links Engine + EditorCore (no `sakura_deploy_qt_runtime`).

### Project / session
- `ProjectDescriptor::TryLoadFromFile` — typed JSON fields, no throw on `{"name":42}`; does not mutate `OutDescriptor` on failure; `startupScene` must stay inside project root.
- `ProjectGenerator::CreateProject` — `ProjectName` is a single path segment; rejects `../` escapes.
- `Engine::LoadProjectContent` returns `AssetDiagnostic`.
- `ProjectSession` exposes `ProjectSessionHealth` (`Closed` / `Ready` / `Degraded` / `Failed`), `GetLastError`, `GetIssueCount`, `GetLastContentDiagnostic`.
- `AssetRegistry::ScanContent` fails when mount iteration fails or any `.meta` ingest error was recorded (diagnostics kept).

### Qt host
- `EditorMainWindow` QTimer (~16 ms) calls `Engine::Tick` (maintenance / `PumpCompletions`) without `Engine::Run`.
- Status bar shows real `IssueCount` (+ degraded/failed).
- Open / New while a project is open → `QProcess::startDetached` with `--project` (one project per process).

### AGENTS.md
- Host model: Qt owns editor loop; SDL owns standalone; no second SDL window for editor viewport; Diligent stays in Rendering/RHI.

## New / updated API
- `ProjectSessionHealth`, `ProjectSession::GetHealth/GetLastError/GetIssueCount/GetLastContentDiagnostic`
- `AssetDiagnostic Engine::LoadProjectContent(...)`
- CMake: `EditorCore`, `Sakura::EditorCoreThirdParty`, `Sakura::EditorQtThirdParty`

## Tests
- `SakuraProjectSessionTest` — Failures: 0 (descriptor type/path rejection, generator escape, session Failed/Ready, IssueCount, Tick).
- `SakuraAssetTest` (EditorCore, no Qt) — Failures: 0.

## Commands
```bat
cmake --build build --config Debug --target EditorCore
cmake --build build --config Debug --target SakuraProjectSessionTest
cmake --build build --config Debug --target SakuraAssetTest
ctest --test-dir build -C Debug -R "Engine.Project.Session|Engine.Assets" --output-on-failure
```

Actual:
```
SakuraProjectSessionTest → Failures: 0
SakuraAssetTest → Failures: 0
EditorCore.lib / Editor.lib / SakuraEditor.exe built (Debug)
```

## Remaining risks
- Configure of `Root/Editor` still `find_package(Qt6)` whenever `SAKURA_BUILD_EDITOR=ON` (EditorCore itself does not link Qt; engine-only preset skips Editor).
- Multi-process Open does not auto-close/save the current document (save is still UI stub).
- Process-isolation of Python modules is by OS process only — no in-process sandbox.
