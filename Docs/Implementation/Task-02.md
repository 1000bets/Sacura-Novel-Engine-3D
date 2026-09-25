# Task 02 — AssetLoadContext SubAsset ownership

Date: 2026-09-20

## Problem

`AssetRegistry::TryResolveKey` returned `const SubAssetRecord*` into `OutEntry.Metadata.SubAssets`. `AssetManager` copied `Entry` into `AssetLoadContext` but kept the old pointer, then `ScheduleWorkerLoad` captured `Context` by value. After the producer stack frame ended, the pointer dangled.

## Fix

- `AssetLoadContext::SubAsset` is now `std::optional<SubAssetRecord>` (owned value).
- `BindLoadContextSubAsset(Context, ResolvedSubAsset)` copies the record while the registry stack entry is still live.
- All `AssetManager` context construction sites use the binder (dependency walk, `BeginLoadWhenReady`, `RequestLoad`).
- `ModelLoader` / `SkeletalLoader` use `Context.SubAsset.has_value()`.

## Test seam

- `AssetManager::SetWorkerLoadReleaseFlag` / `ClearWorkerLoadReleaseFlag` — worker spins until flag is true before `Load`, so the test can force producer return before worker dereference.

## Tests added (`AssetSystem_Test.cpp`)

- `TestAssetLoadContextOwnsSubAssetByValue` — destroy producer entry; Type / SubAssetId / Selector.Index survive copy/move.
- `TestSubAssetLoadAfterProducerReturns` — gate worker, load StaticMesh subasset, then whole Model + repeat request.

## Commands

```bat
cmake --build build --config Debug --target SakuraAssetTest
ctest --test-dir build -C Debug -R Engine.Assets.WithEditorImport --output-on-failure
```

## Results

- Debug: **Passed** (0.34s).
- AddressSanitizer: **not run** on this MSVC Debug configuration.

## Invariants

- Worker must not rely on pointers into Game-thread temporary `AssetRegistryEntry` metadata.
- External `.meta` schema unchanged.
- `Context.Registry` remains a non-owning pointer (path lookups); SubAsset no longer aliases registry memory.

## Remaining risks

- `Context.Registry` can still be used from workers while Game Thread mutates the registry (pre-existing; task 03/shutdown may harden).
- Worker load gate is a test-only hook; do not use in production paths.
