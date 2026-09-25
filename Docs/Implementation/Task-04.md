# Task 04 — Scene ownership and world transforms

Date: 2026-09-20

## Changes

- `Scene::~Scene` clears parent/child links before deleting the flat list (safe for any creation order).
- `DestroyGameObject` returns `bool`, rejects objects not owned by this Scene, destroys subtree depth-first.
- `QueueDestroyGameObject` / `FlushPendingDestroys` for deferred destroy during iteration (Play loop / task 12).
- `FindByHandle` for stale observer checks via existing `ObjectHandle`.
- `GameObject::SetParent` returns `bool`; cycle / cross-scene / self rejected in Release (no assert-only).
- `AddExistingComponent` rejects already-attached or duplicate pointer.
- Unified world API: `GetWorldMatrix` / `GetWorldTransform` / `GetWorldPosition` / `GetWorldRotation` / `GetWorldForward` / `GetWorldUp` / `IsActiveInHierarchy`.
- `SceneExtractor` uses world transforms for mesh, camera, and lights; skips inactive ancestors. Camera drops non-uniform scale via rotation extract from world matrix.

## Tests

- New `SakuraSceneTest` / CTest `Engine.Scene.Ownership`.

## Commands

```bat
cmake --build build --config Debug --target SakuraSceneTest
build\Root\debug\SakuraSceneTest.exe
```

## Results

- Debug: **Failures: 0**

## Coordinate notes

- Local matrix order remains Scale * Rotation * Translation (existing `Transform::GetMatrix`).
- Parent composition: `ChildLocal * ParentWorld` via reverse chain multiply (same as prior mesh path).
- Camera look uses world Forward/Up; SimpleMath Forward = (0,0,-1).

## Remaining risks

- Generation is not bumped on destroy (FindByHandle uses pointer identity via live list only — stale pointers to freed memory are still UB if cached raw `GameObject*`). Prefer handles.
- `AddComponent` still bypasses reflection factory (task 05).
