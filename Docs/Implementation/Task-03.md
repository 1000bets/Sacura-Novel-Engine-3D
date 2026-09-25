# Task 03 — AssetManager job lifetime

Date: 2026-09-20

## Changes

- `AssetManager` owns outstanding CPU loads via `JobGroup OutstandingLoads`.
- Shutdown sequence: `bAcceptingLoads=false` → session cancel flag → mark slots cancelled → `OutstandingLoads.Wait()` (no slots mutex held) → pump/clear → bump `SessionId` → drop registry/jobs.
- `CompletionEvent::SessionId` rejects late completions after re-Initialize/shutdown.
- Per-slot `CancelFlag` (`shared_ptr<atomic<bool>>`) observed by already-running workers; `CancelLoad(Key)` cancels one consumer without clearing shared dependency slots.
- Worker wraps `Load` in try/catch → always enqueues terminal `Failed`/`Cancelled`/`Ready` (no stuck `Loading` on loader exceptions). JobSystem’s outer catch is no longer the only barrier.
- Destructor calls `Shutdown` if still initialized (safe with a live `JobSystem`).
- Test seams: `SetWorkerLoadFault`, existing release gate.

## Tests (`TestAssetManagerLifetimeAndCancel`)

- Cancel while gated → `Cancelled`
- Injected loader exception → `Failed` + InternalError
- Shutdown + re-Initialize bumps session; subsequent load Ready
- `AssetManager` destructor while `JobSystem` still alive
- Missing material dependency → Failed

## Commands

```bat
cmake --build build --config Debug --target SakuraAssetTest
build\Root\debug\SakuraAssetTest.exe
```

## Results

- Debug: **Failures: 0**
- ASan: not run (MSVC Debug)

## Invariants

- Do not wait on OutstandingLoads while holding `SlotsMutex`.
- Do not stop the shared JobSystem from AssetManager; only wait for this manager’s JobGroup.
- Cancel of one WaitingParent must not cancel a shared dependency used by others (CancelLoad is key-local).

## Remaining risks

- Workers still capture `this`/loader pointers; safety relies on Wait-before-teardown, not shared_ptr ownership of AssetManager.
- `Context.Registry` can still race with Game-thread registry mutation during a load (unchanged).
- GPU upload lifetime remains task 06/11.
