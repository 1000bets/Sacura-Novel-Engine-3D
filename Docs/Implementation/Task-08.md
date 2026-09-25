# Task 08 — Scene document IO and startupScene

Date: 2026-09-20

## Changes

### Runtime `SceneSerializer` (`sakura.scene` v1)
- Format: `format`, `formatVersion`, `name`, `objects[]` with stable string `id`, `parent`, `bActive`/`bVisual`, `transform`, reflected `components[]` (`type` / `typeVersion` / `properties` via `ReflectionJson`).
- Load stages: validate → create instances without `OnCreate` → fill properties → resolve parents (cycle / missing ref fail) → `OnCreate` → `SetLoaded(true)`.
- Atomic write: temp file + content verify + rename (`WriteTextFileAtomically`).
- Failure cleanup via `DestroyPartialScene` (destroy GOs then Scene).
- `GameObject::AddExistingComponent(Comp, bInvokeCreate)` for deferred lifecycle.
- `Scene` destructor nulls child `m_Parent` before clearing children lists.

### EditorCore `SceneDocument`
- Path, dirty flag, `Open` / `Save` / `SaveAs` / `Close` / `MarkDirty`.
- Failed `Open` keeps the previous document.
- Replacing the bound scene also destroys a prior `Engine::GetActiveScene()` that was not document-owned (avoids leak when opening after `ProjectSession` startup load).
- Scene save does not rewrite `.project`.

### Project integration
- `ProjectGenerator` writes a real empty `startupScene` file.
- `ProjectSession` loads `startupScene` after content scan; missing/corrupt → `Degraded`, no silent empty scene.
- `ProjectSession::CloseProject` destroys the active scene; `Engine::UnloadProjectContent` only clears the non-owning pointer.
- Fixture: `Samples/SampleProject/Content/Scenes/Main.scene`.

## Tests
`SakuraSceneSerializerTest` — Failures: 0; no MemorySubsystem leaks.

## Commands
```bat
cmake --build build --config Debug --target SakuraSceneSerializerTest
ctest --test-dir build -C Debug -R Engine.Scene.Serializer --output-on-failure
```

## Remaining risks
- Non-reflected components (e.g. raw `MeshRendererComponent` GPU handles) cannot be serialized yet; need AssetRef fields (task 11).
- Ownership of `Engine::ActiveScene` is split between Session close and `SceneDocument`; editors must `Close` the document before replacing session ownership carelessly.
- No schema migrations beyond `formatVersion == 1` reject.
