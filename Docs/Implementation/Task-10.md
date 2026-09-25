# Task 10 — Hierarchy, Inspector, Undo

Date: 2026-09-20

## Changes

### EditorCore commands
- `EditorCommand` / `EditorCommandStack` — execute / undo / redo / BeginGroup+EndGroup; binds `SceneDocument` and marks dirty on successful edits.
- Factories in `EditorCommands.h`: `MakeRenameObjectCommand`, `MakeSetTransformCommand`, `MakeCreateObjectCommand`, `MakeDeleteObjectCommand`, `MakeReparentObjectCommand`, `MakeAddComponentCommand`, `MakeRemoveComponentCommand`, `MakeSetPropertyCommand`, `MakeResetPropertyCommand`.
- Delete / create-undo / remove-component store **serializable JSON** via `SceneSerializer` subtree/component helpers (not raw deleted pointers).
- Reparent uses `GameObject::SetParent` (cycle → fail, no stack push).

### SceneSerializer helpers
- `SerializeSubtreeToJson` / `DeserializeSubtreeFromJson`
- `SerializeComponentToJson` / `DeserializeComponentFromJson`

### SceneDocument
- `AdoptScene` — bind an already-loaded `Engine::ActiveScene` (startup from `ProjectSession`) without reloading.

### Reflection
- `LightComponent` → `ENGINE_CLASS` (`engine.LightComponent`) with editable/serializable fields; `light_type` as int64. Anchored in `ReflectionAnchor.cpp`.

### Qt Editor shell
- Hierarchy shows real `SceneDocument` roots/children; selection via `ObjectHandle` in item data.
- Inspector: name + transform (command-backed) + component combo + `ReflectionInspector` routed through set/reset commands.
- File → Save Scene (real `SceneDocument::Save`); dirty `*` in title; close prompts on unsaved changes.
- Edit: Undo / Redo / Create / Delete; Create menu + hierarchy context: Camera / Light components.

## Tests
`SakuraEditorCommandTest` (`Editor.Commands.Undo`) — Failures: 0  
Also green: `Engine.Reflection.Native`, `Engine.Scene.Serializer`.

## Commands
```bat
cmake --build build --config Debug --target SakuraEditorCommandTest Editor SakuraEditor
ctest --test-dir build -C Debug -R "Editor.Commands.Undo|Engine.Scene.Serializer" --output-on-failure
```

## Acceptance notes
- Create → add Camera/Light → set property → undo/redo → save → reopen: covered by headless command test + Save/Open path.
- Viewport reflects scene extraction each tick (camera/light changes affect extract when present); no GPU framebuffer assert in this slice.
- Hierarchy drag-drop is enabled; cycle still rejected by `SetParent` if wired through reparent command (UI drop → Qt InternalMove may move items visually without command — remaining risk).

## Remaining risks
- Hierarchy `InternalMove` can desync tree from scene if drop is not converted to `MakeReparentObjectCommand` (command API ready; drop handler not fully hooked).
- Transform edits create one undo entry per spinbox `editingFinished` (no drag group yet).
- Dual Present / GPU readback still from Task 09.
