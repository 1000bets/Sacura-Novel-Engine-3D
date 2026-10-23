# Interactive C++ viewport — Electron 45 / Windows D3D12

Branch: `feature/electoron_ui`. Stage: native viewport interaction only.

## Run

From `desktop-electron` in PowerShell:

```powershell
npm install
npm --prefix ../engine-web-prototype/frontend install
npm run native:configure
npm run native:build
npm run build:ui
npm run native:start
```

The configure preset must keep `SAKURA_BUILD_EDITOR=ON`: the host reuses the
existing Editor-owned ImGuizmo library. It links Engine + EditorCore + ImGuizmo,
not the Qt Editor target. Sources and dependencies remain in their original owners.

`native:start` makes a temporary copy of SampleProject, arranges its two real mesh
objects in that copy, and opens the existing React shell. Source assets are not
modified. Close the window to stop/join the Engine and remove the temporary copy.
The viewport button **Открыть C++ проект…** opens a real `.project` through a main
process file dialog. It stops the previous process before creating the next one,
so Python, scene state and GPU ownership do not cross project boundaries.

Development: `npm run desktop:dev`; optionally set `SACURA_NATIVE_PROJECT` to an
absolute `.project` path. With no project or missing host, desktop shows an error
and the open button. It never falls back to Three.js.

Build the standalone directory: `npm run desktop:build`. Output:
`release/win-unpacked/Sacura Desktop Experiment.exe`. The package includes native
host, Engine assets/shaders, backend DLLs, Qt and Python under `resources/native`.
This is an experimental **Debug** native build, not an optimized release installer.

## Presentation architecture

The first stage's Engine host, child HWND, swapchain and Render Thread are reused.
A visible child HWND cannot compose ordinary React overlays because of native
window airspace. The HWND is now hidden and retained as the existing swapchain's
native surface. The actual scene is rendered once by the existing Renderer:

```text
Scene / EditorCore / ImGuizmo (Game thread)
  → immutable RenderFrameData
  → existing Renderer / Diligent D3D12 (Render thread)
  → GPU copy of completed backbuffer into shared texture ring
  → process-local NT handle duplicated into Electron main
  → sharedTexture.importSharedTexture / sendSharedTexture
  → isolated preload: getVideoFrame → WebGL2 image blit → Chromium compositor
  → existing React overlays
```

The WebGL pass draws only the imported image. It has no scene, meshes, materials,
camera or Three.js. All 3D and gizmo geometry comes from C++/Diligent. No recurring
PNG/JPEG/base64 pixel transfer or CPU pixel readback is used. Diagnostic screenshots
are one-shot readbacks in the CLI test only. GPU copies still exist; this is not
a claim of zero GPU copies.

Each frame's pipe packet is 56 binary bytes (size, token, handle, frame index and
timestamps). The reverse packet is an 8-byte release token. JSON IPC carries only
input, bounds, commands and low-rate status, never render frame contents.

### Synchronization and lifetime

- Export lives entirely under `Root/Engine/src/Rendering/RHI`.
- Shared D3D12 resource: default heap + `HEAP_FLAG_SHARED`, RGBA8 UNORM,
  `ALLOW_SIMULTANEOUS_ACCESS | ALLOW_RENDER_TARGET`, one mip/sample.
- In this tested configuration, omitting `ALLOW_RENDER_TARGET` produced invalid
  Chromium mailboxes, white output and WebGL context loss. Correcting the flag
  produced the actual scene, confirmed by compositor screenshots and frame counts.
- Diligent transitions source/destination for copy, then shared texture to COMMON.
  A conservative `WaitForIdle` on Render Thread completes producer GPU work before
  publishing the handle. This adds a CPU fence wait, not a CPU copy of pixels.
- Three slots per extent, at most nine across resize generations. Busy slots are
  never reused before the renderer completes its GL blit, closes the VideoFrame
  and sends a consumed-frame ACK. Main keeps a bounded resource/extent pool and renews the Electron import for
  each publication after the previous consumer ACK. Reusing a Chromium import
  across external writes leaves its initial image cached. The native D3D12
  texture and duplicated handle remain pooled; pixels never pass through CPU memory. Old imports
  retire after pending frames finish; shutdown releases the retained imports.
- Old sizes retire only after consumer release. Shutdown suspends production,
  drains imports, joins Render Thread and destroys the native surface. A timeout
  reports/kills a stuck host; normal tested shutdown returned exit code 0.
- Canvas GPU objects are cleared/deleted on detach and project replacement.

### Input, camera, selection and transforms

React forwards physical pixel coordinates (`CSS × devicePixelRatio`), DOM button
bits, physical key codes and modifiers. Main validates origin/frame, kinds, finite
numbers, size limits and allowed fields. The sandboxed renderer has no Node or
Engine access. The separate host owns QApplication/Game thread; existing Render
Thread owns all GPU work.

| Control | Native behavior |
|---|---|
| Left click | nearest visible mesh's transformed local AABB |
| Alt + left drag | orbit |
| Middle drag | pan |
| Right drag + WASD / QE | mouse look and fly; Shift accelerates |
| Wheel / F | dolly / center selected object |
| Q / W / E / R | select / translate / rotate / scale |
| Existing toolbar | tool, world/local axes, snapping, grid |
| Ctrl+Z / Ctrl+Y | EditorCore transform undo / redo |

Selection uses generation-checked `ObjectHandle`. ImGuizmo previews a transform,
then commits through `MakeSetTransformCommand`; focus/capture loss cancels an
unfinished edit and clears held input. Its draw lists become the existing immutable
`ImGuiOverlaySnapshot`, rendered by RHI before texture export.

### Resize, HiDPI, visibility and clipping

The `SceneViewport` adapter chooses native on desktop and lazy-loads the unchanged
`LocationScene`/Three.js on web. Existing Editor overlays remain siblings of that
adapter. Only the viewport integration and a smoke-test layout hook changed in
Editor.jsx; the editor was not rewritten.

DOM extent/DPR changes publish bounds only when changed. During live resizing the
Engine's existing interactive resize path stretches the previous swapchain image;
the final extent is allocated after 120 ms without a size change. Resize does not
advance simulation. Chromium performs rectangular clipping and HTML composition;
the projection uses the full element size, including when partially clipped.

Hidden/zero-size viewport and minimized window suspend the surface and reset input.
Showing it resumes frames. Changing either C++ project or the React project releases
the prior C++ process; the new React project must explicitly open its native project.

## Verification and measurements

CLI integration runner (no Computer Use, browser driver or DOM injection):

```powershell
npm run native:smoke
$env:SACURA_NATIVE_SOAK_SECONDS='120'
$env:SACURA_NATIVE_TEST_OUTPUT='F:/projects/Sacura-Novel-Engine-3D/build/electron-interactive-150'
npm run native:smoke
node scripts/native-start.cjs --smoke --force-device-scale-factor=1.25
```

The project-owned React smoke hook invokes the same input adapter and updates the
same dock layout state as normal interaction. The test verifies native process/HWND,
resolved GPU meshes, rendered VideoFrame counters, resize, ray selection, actual
ImGuizmo drag, undo/redo, mouse look/keyboard movement, hiding/showing, minimize,
live zoom 1.25/0.8/1, dock positions left/right/bottom, continuous dock extent changes,
and process replacement with zero outstanding imports. The screenshot contains
actual meshes, the native gizmo and a React badge; pixel assertions verify composition.

Recorded on Windows, RTX 5080, Electron 45.0.0, Debug Engine, system DPI 144:

| Metric | 120-second run |
|---|---:|
| Measured interval | 120.658 s |
| GPU-image draws submitted in Chromium | 7,529 |
| Mean submission FPS | 62.40 |
| Frame interval p50 / p95 | 16.0 / 16.4 ms |
| GPU texture ready → JS draw submission p50 / p95 | 14.08 / 14.52 ms |
| Native CPU / summed GPU pass time, final sample | 0.403 / 0.261 ms |
| Shared textures during steady interval | 3 |
| Shared texture pixel storage | 8,121,384 bytes (~7.75 MiB) |

Native GPU pass timing excludes export copy/fence and Chromium composition. Display
latency is **not photon latency**: the current measurement stops at JS GL submission,
before compositor/vsync/scanout. Input marker timing in JSON is diagnostic only:
it is attached by the host transport, not proven to identify the first frame visibly
reflecting an event. FPS likewise counts submitted images, not monitor refreshes.

The ring remained bounded throughout the soak; Chromium GPU process private memory
went from 224,380 to 138,864 KiB, renderer private memory from 115,848 to 125,712 KiB
between the first/last samples. Two minutes is not proof of leak freedom or an
hours-long stability guarantee.

Artifacts (repository `build/`, ignored):

- `electron-interactive-150/report.json`, `viewport.png`, and `electron-interactive-150.log`.
- `web-viewport-tests.log`: **28/28** camera/navigation/selection/resize tests passed.
- `electron-store-tests.log`: **2/2** existing project storage tests passed.
- `qt-resize-regression.log`: **PASS**, 66 input/overlay updates, frame 1014,
  12.231 seconds; sampled workspace backing store had zero stale pixels.
- `electron-package-build.log`: standalone Windows package build.
- `electron-interactive-packaged-125/report.json` and `viewport.png`: **PASS** from
  the packaged executable with its packaged native host and forced Chromium scale
  1.25 (native window DPI stayed 144). The 15.096 s measurement submitted 936 images:
  62.00 FPS; frame interval p50/p95 16.0/16.5 ms; ready-to-submit 15.11/15.68 ms.
  The composited screenshot contained 10,060 pixels of the React overlay's green
  background. Replacement/close returned 0 and drained the previous host's imports.

## Known limits

- Windows D3D12 only; no D3D11 backend was added. Electron's Windows importer uses
  its own graphics stack. Cross-adapter systems, Vulkan export, macOS and Linux are
  not validated. SharedTexture remains experimental; upstream v45 pixel tests are
  currently gated to macOS ARM64, so our Windows runtime evidence matters.
- CPU wait at each export and repeated import are deliberately conservative.
  Explicit cross-API fences and persistent imports are subsequent optimization work.
- Tested: native DPI 144, Chromium scaling, automated dock layout changes and a
  separate forced-scale run. Physical Windows display-setting changes, mixed-DPI
  monitor dragging and a human dragging every dock configuration are not established
  by these CLI tests. No broad claim of artifact-free behavior on every driver.
- Single selection uses AABBs, not triangle/ID-buffer picking. Transform state is
  in memory, with undo/redo. React Save/Inspector/Hierarchy/Story do not operate on
  the C++ SceneDocument; Play/game overlays remain React preview systems.
- The existing web editor help may describe additional web-only actions (multi-select,
  deletion, gameplay). The native viewport's visible control legend lists its scope.
- Device loss is surfaced as an error; automatic GPU recovery is not implemented.
- Existing shader compiler warnings and the existing development CSP warning remain.

## Changed implementation areas

- Engine: `SharedGpuFrame.h`, RHI `SharedFrameExporter.*`, Renderer attachment,
  `RenderThread::SetSharedFrameStream`, CMake source list. No Qt UI source changes.
- Host: `native/Host.cpp`, `FramePipe.h`, `ViewportController.*`, native CMake target.
- Electron: `main.cjs`, `preload.cjs`, `native-viewport.cjs`, packaging configuration,
  launcher and integration-test scripts.
- React: new `SceneViewport.jsx`, `NativeViewport.jsx`, small Editor.jsx integration.

Existing frontend package changes were preserved. Qt action/inspector source changes
that appeared independently in the workspace during this work were not edited or
reverted by this task; the Qt regression results above refer to the tested build.

## Official references and next stage

API contracts: [sharedTexture](https://www.electronjs.org/docs/latest/api/shared-texture),
[v45 implementation](https://github.com/electron/electron/blob/v45.0.0/shell/common/api/electron_api_shared_texture.cc),
[v45 pixel tests](https://github.com/electron/electron/blob/v45.0.0/spec/api-shared-texture.spec.ts),
[D3D12 shared heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/shared-heaps).
Earlier API comparison, Node-API and offscreen-rendering research is retained in
[the first-stage report](NATIVE-VIEWPORT.md). Offscreen rendering exports Chromium
frames; it is not this Engine→Chromium path. A Node-API addon is not needed here.

Recommended next stage, not implemented automatically: unify project/document
ownership and saving; expose native selection/commands to Inspector/Hierarchy;
then add exact picking, explicit GPU fences, device-loss handling and multi-adapter
coverage before expanding runtime/Story integration.
