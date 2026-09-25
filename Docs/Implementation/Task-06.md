# Task 06 — Render readiness and GPU operation lifetime

Date: 2026-09-20

## Changes

### RenderThread
- States: `Stopped / Starting / Ready / Failed / Stopping`.
- READY only if `Renderer::Initialize` returns true; otherwise `Failed` + `GetLastError()`.
- `WaitUntilReady()` returns bool (false on Failed/Stopped).
- Start resets command/frame queues (`ResetForReuse`) so Start–Stop–Start works on the same instance.
- Shutdown uses `WaitUntilIdle` (queue empty **and** executing count 0), not only `WaitUntilEmpty`.

### RenderCommandQueue
- `Enqueue` returns `RenderCommandEnqueueResult` (Accepted / RejectedShutdown / RejectedEmpty).
- `BeginExecute` / `EndExecute` + `WaitUntilIdle` for CPU completion of dequeued work.
- `ResetForReuse` clears shutdown flag.

### RenderFrameQueue
- `ResetForReuse` for re-Start.

### Renderer
- `Initialize` returns `bool`.

### AssetGpuUploader
- Session id bumped on `Clear`; late callbacks ignore stale session and do not recreate map entries.
- Failed enqueue / missing RenderThread → terminal `Failed`.
- Captures session id into upload jobs.

## Tests
- `SakuraRenderLifetimeTest` (CPU queue only, no Engine link) — Failures: 0.
- `SakuraTest` GPU smoke — All tests passed (30 frames).

## Commands
```bat
cmake --build build --config Debug --target SakuraRenderLifetimeTest
cmake --build build --config Debug --target SakuraTest
```

## Remaining risks
- WaitUntilEmpty still means “container empty”, not GPU fence — documented; use WaitUntilIdle for CPU command completion.
- Resource destroy vs in-flight frames not fully fence-synced yet (frame queue ReleaseConsumedFrame is still a stub).
- Fault-injection of backend failure not automated (requires bad device/shader path).
