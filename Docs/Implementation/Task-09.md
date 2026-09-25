# Task 09 — Qt viewport and RenderView cameras

Date: 2026-09-20

## Changes

### RenderView / RenderSurface (Runtime)
- `RenderViewId`, `RenderSurfaceId`, `RenderViewCamera`, `RenderView` in `Rendering/RenderView.h`.
- `RenderViewCamera::BuildRenderCamera(Aspect, Out)` builds independent `RenderCamera` (look-at + perspective). Viewport cameras are **not** Scene components.
- `RenderSurface` header stub for per-window swapchain ownership (device still uses primary swapchain from `StartPresenting` in this slice).

### Engine presentation
- `Engine::StartPresenting(NativeWindowInfo)` / `IsPresenting` / `StopPresenting` — starts RenderThread against a Qt HWND after headless init (no second SDL window).

### Qt layer
- `EditorViewportWidget` — native `QWidget` (`WA_NativeWindow` / `WA_PaintOnScreen`), tracks `winId` changes, DPI size, emits `NativeSurfaceChanged` / `ViewportResized`.
- `EditorMainWindow` — two side-by-side viewports with different `RenderViewCamera` (front vs angled FOV). Maintenance tick: `Tick` + extract scene + override camera from focused viewport + inject default mesh if scene empty + `SubmitFrame`.
- First ready viewport calls `StartPresenting`; resize forwards `ResizeRenderer`.

## Tests
- `SakuraRenderViewTest` — independent cameras / FOV / ViewProjection differ.

## Commands
```bat
cmake --build build --config Debug --target SakuraRenderViewTest Editor SakuraEditor
ctest --test-dir build -C Debug -R Engine.Render.ViewCamera --output-on-failure
```

## Acceptance notes
- **GPU Present into Qt:** wired via Diligent primary swapchain on first viewport HWND; dual-swapchain (true independent Present per view) not yet — second view currently shares camera override by focus only.
- **GPU readback / framebuffer capture:** not automated in this slice (manual visual check of clear+default mesh). Mark as remaining.

## Remaining risks
- Single primary swapchain: only the first `StartPresenting` HWND is presented; second viewport does not own a Diligent surface yet.
- `winId` recreation mid-session needs swapchain recreate (signal exists; recreate path incomplete).
- Input routing / capture for viewport focus is ClickFocus only.
