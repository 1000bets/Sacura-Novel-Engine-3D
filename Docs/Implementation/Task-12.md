# Task 12 — PlaySession (EditWorld / PlayWorld)

Date: 2026-09-20

## Changes

### Scene simulation
- `Scene::Tick(float)` — walks `GetAllObjects` order; for each active-in-hierarchy object, ticks enabled components; then `FlushPendingDestroys`.

### PlaySession (`Engine::Play`)
- States: `Stopped`, `Playing`, `Paused`.
- `StartPlay()` — serializes `Engine::GetActiveScene()` (edit document scene) to JSON, deserializes into a new **PlayWorld**, `SetActiveScene(PlayWorld)`; **EditWorld** pointer and `EditSceneJson` kept; edit scene not destroyed.
- `StopPlay()` — destroys PlayWorld only, restores `SetActiveScene(EditWorld)`; edit contents unchanged.
- `Pause` / `Resume` / `StepFrame` (one `Scene::Tick` while paused) / `Tick` (simulation only when `Playing`).
- `IsSimulating()` — `Playing` or `Paused`.

### Engine
- `Engine::Tick` — after asset resolve on active scene, if `Play.IsSimulating()` calls `Play.Tick`; edit mode (stopped) does not tick scene components.
- `Engine::Shutdown` — `Play.StopPlay()` before clearing active scene.

### Editor
- Play / Pause / Step toolbar wired to `PlaySession`.
- Hierarchy edits blocked during play (status message); hierarchy still shows **EditWorld** via `SceneDocument`.
- `SubmitEditorFrame` — Scene tab (index 0) extracts **EditWorld** while simulating; Game tab uses **ActiveScene** (PlayWorld).

## Tests
`SakuraPlaySessionTest` (`Engine.Play.Session`) — edit/play isolation, pause/step/resume, multiple play/stop cycles.

## Commands
```bat
cmake --build build --config Debug --target SakuraPlaySessionTest Engine Editor
ctest --test-dir build -C Debug -R "Engine.Play.Session" --output-on-failure
```

## Acceptance notes
- Play duplicates scene via serializer; script `OnCreate` runs on deserialize (existing SceneSerializer path).
- Story / Player modes out of scope for this task.
