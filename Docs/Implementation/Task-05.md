# Task 05 — Unified reflection factory path

Date: 2026-09-20

## Changes

- `ReflectionSubsystem::CreateInstance` assigns `Class` and copies editable properties from CDO via `PropertyAccess::CopyPropertiesFrom` (skipped while building the CDO itself).
- `PropertyAccess::CopyPropertiesFrom` / `ResetToClassDefaults` added.
- `GameObject::AddComponent<T>()` with no constructor args uses `CreateInstance(TypeId{T::StaticReflectionTypeId()})` when reflection is initialized; falls back to `NewObject` only for custom ctor args or missing reflection.
- `CreateComponent(TypeId)` shares the same CreateInstance path.

## Tests

- Extended `SakuraReflectionTest`: AddComponent assigns Class, FOV defaults from CDO, ResetToClassDefaults.
- `SakuraSceneTest` still green (AddComponent path used there too).

## Commands

```bat
cmake --build build --config Debug --target SakuraReflectionTest
build\Root\debug\SakuraReflectionTest.exe
```

## Results

- Reflection tests passed; Scene Failures: 0.

## Invariants

- CDO construction still goes through Class::Factory without copying from a prior CDO.
- AssignClass happens before property copy.
- AddExistingComponent still owns OnCreate lifecycle once at attach time.

## Remaining risks

- Types without ENGINE_CLASS still use NewObject without Class (documented fallback).
- FindProperty does not walk base Class lists; copy walks BaseClass and each local Properties vector.
- JSON round-trip not re-proven in this task beyond existing Python/JSON suites (not run here).
