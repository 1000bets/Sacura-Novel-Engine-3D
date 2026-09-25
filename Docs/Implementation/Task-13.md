# Task 13 — Story Runtime

Date: 2026-09-20

## Scope

Engine-side visual novel story playback (no Qt): JSON documents, runtime state machine, scene actions, editor playtest UI hook, headless tests.

## Format `sakura.story` v1

- Root: `format`, `formatVersion`, `name`, `startNodeId`, `nodes[]`.
- Node kinds: `Line` (speaker, text, nextNodeId), `Choice` (prompt, choices[{label, nextNodeId}]), `Action` (actions[], nextNodeId), `End`.
- Action kinds: `Wait` (durationSeconds), `MoveTo` (targetObjectName, position, durationSeconds), `CameraCut` (targetObjectName, optional useTargetObjectTransform, durationSeconds, optional position/rotation).

Implementation: `StoryTypes.h`, `StoryDocumentIO`, atomic save via `SceneSerializer::WriteTextFileAtomically`.

## StoryRuntime

- `BindScene`, `LoadFromFile` / `LoadDocument`, `Reset`, `Stop` (CancelAll).
- Dialogue: `AdvanceDialogue`, `SelectChoice`, query APIs for speaker/line/choices.
- `Tick(float)` runs sequential blocking actions; states `Running` / `Completed` / `Failed` / `Cancelled`.
- `MoveTo` lerps local transform position on a named object.
- `CameraCut` blends primary camera owner transform (copy from named object when flagged).

## Sample

`Samples/SampleProject/Content/Stories/Demo.story` — three lines, one choice, MoveTo + CameraCut branch.

## Editor

When **PlaySession** starts, editor loads Demo.story into `StoryRuntime` on PlayWorld:

- Dock tab «Сценарий»: speaker, line, choice buttons.
- Viewport live strip overlay with current dialogue while playing.

## Tests

`SakuraStoryRuntimeTest` — JSON round-trip, line/choice/action order, `Stop` cancel.

```bat
cmake --build build --config Debug --target SakuraStoryRuntimeTest
ctest --test-dir build -C Debug -R Engine.Story.Runtime --output-on-failure
```

## Commands

```bat
cmake --build build --config Debug --target Engine SakuraEditor
```
