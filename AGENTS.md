# Sacura Novel Engine 3D — Agent Notes

## Rendering architecture

- Graphics abstraction: **Diligent Engine Core** (not raw D3D12/Vulkan/OpenGL, not SDL_GPU).
- Window / input / events: **SDL3**.
- Our code owns `Rendering` (Renderer, RenderScene, SceneExtractor, passes, materials). Gameplay never includes Diligent headers or native GPU API types (`VkInstance`, `ID3D12Device`, etc.).
- Only `Rendering/RHI` may talk to Diligent.
- Shaders: **HLSL** as the primary language.
- Backend selection: `Auto` | `D3D12` | `Vulkan` | `OpenGL`. On Windows, `Auto` resolves to D3D12 (fallback Vulkan → OpenGL). Selection lives only in `RenderDevice::ResolveBackend` / `InitializeBackend`.
- Pipeline model for the current horizon: **forward** rendering. Deferred is out of scope until explicitly planned.

### Stage 3 — Rendering Core (Diligent)

Module layout under `Root/Engine`:

```text
inc/src/Rendering/
  Renderer.*          — lifecycle, clear, draw from RenderFrameData
  RenderDevice.*      — Diligent device / context / swapchain ownership
  RenderContext.h     — per-frame execution state only
  Resources/
    GpuBuffer.*       — Vertex / Index / Constant buffers
    RenderMesh.*      — GPU mesh (VB + IB + counts)
    RenderResourceManager.* — MeshHandle → RenderMesh
shaders/TestMesh.hlsl — VSMain + PSMain (HLSL only; Diligent compiles/translates)
```

- `Renderer` is created and used **only** on Render Thread (`AssertRenderThread` on public methods).
- SDL3 window stays in `WindowSubsystem`; Rendering receives `NativeWindowInfo` only.
- Resize path: SDL event → Game Thread → Render Command → `Renderer::Resize` → SwapChain.
- First milestone draw: indexed colored quad + `FrameConstants` / `ObjectConstants` (World × ViewProjection).
- Debug builds use Diligent validation; startup logs Backend / GPU / Resolution / formats via `PrintString`.
- Executables need Diligent backend DLLs **and** `SDL3.dll` next to the binary (POST_BUILD on `SakuraTest`).

### Game World vs Render World

```text
Scene → SceneExtractor (Game Thread) → RenderScene → RenderFrameData → Render Thread → Renderer
```

- `SceneExtractor` runs only on Game Thread (`AssertGameThread`). It may read Scene / GameObject / Components.
- `RenderScene` / `RenderObject` / `RenderCamera` / `RenderLight` hold POD + resource handles (`MeshHandle`, `MaterialHandle`). **No** pointers/references to GameObject, Component, or Scene.
- Renderer / mock consumer on Render Thread reads only `const RenderFrameData` (`AssertRenderThread`). Deleting a GameObject after extraction must not invalidate an already submitted frame.
- `RenderScene::Clear()` clears contents but keeps vector capacity for reuse.
- Parallel job-based extraction is a later optimization; Extractor must stay free of hidden global mutable state.

## Threading ownership

Roles: `ThreadRole::{ Unknown, Game, Render, Worker }` (`Core/Threading`).

| Thread | Owns | Must not |
|--------|------|----------|
| **Game** (main/app thread) | Scene, GameObject, Components, gameplay/app state, SDL events | Draw / Dispatch / Present / GPU state transitions |
| **Render** (dedicated) | Renderer, Diligent immediate context, GPU submit, Present, render passes | Read Scene / GameObject / Component / gameplay objects |
| **Worker** (JobSystem pool) | No persistent world state; only explicit job payloads | GPU execution; owning Scene or Renderer |

Cross-thread work:

- CPU parallel work → `JobSystem` / `Job` / `JobGroup` (Game and Render may schedule; they are not workers).
- Game → Render work → `RenderThread::Enqueue` / `RenderCommandQueue` (not JobSystem).
- Frame handoff → immutable `RenderFrameData` via `RenderFrameQueue` (max 1–2 frames ahead, backpressure).
- **Diligent** is owned by `RenderDevice` / `Renderer` on the **Render Thread** only (`ImmediateContext`, Present, Draw).
- `WindowSubsystem` (SDL3) owns the OS window on the Game Thread; resize is forwarded as a Render Command.
- GPU resources (`GpuBuffer`, `RenderMesh`, pipelines, shaders) live only in Rendering; Game code uses `MeshHandle` / `MaterialHandle`.

Lifecycle: Game Thread initializes JobSystem → starts Render Thread → waits READY → loop. Shutdown: stop frames → join Render Thread → JobSystem shutdown → subsystems → (later) destroy SDL after GPU teardown.

Use `AssertGameThread()` / `AssertRenderThread()` / `AssertWorkerThread()` on thread-sensitive APIs. Logging via `PrintString` (English).

## Third-party dependencies

Third-party code lives in **exactly one** of two places — never in a shared `Root/ThirdParty`:

| Owner | Path | When to use |
|-------|------|-------------|
| Engine | `Root/Engine/ThirdParty/` | Runtime / game / shared with Editor via Engine |
| Editor | `Root/Editor/ThirdParty/` | Editor-only tools |

Do not duplicate the same library in both. If Editor needs a capability that Engine already wraps, depend on Engine — do not re-link Diligent from Editor.

### Current Engine deps

| Dependency | Role | Integration |
|------------|------|-------------|
| SimpleMath / DirectXMath | Transforms, math | Vendored under `Root/Engine/ThirdParty/SimpleMath` |
| SDL3 | Window, input, events | FetchContent (`Root/Engine/ThirdParty/CMakeLists.txt`) |
| DiligentCore | RHI | FetchContent → `Root/Engine/ThirdParty/DiligentCore/` |
| DiligentTools | Texture/Asset loaders, Diligent ImGui bridge | FetchContent → `Root/Engine/ThirdParty/DiligentTools/` |
| DiligentFX | High-level FX framework | FetchContent → `Root/Engine/ThirdParty/DiligentFX/` |
| DiligentSamples | Official tutorials/samples (sources) | FetchContent → `Root/Engine/ThirdParty/DiligentSamples/`; build with `-DSAKURA_BUILD_DILIGENT_SAMPLES=ON` |
| Dear ImGui | UI (docking `v*-docking`) | FetchContent → `Root/Engine/ThirdParty/imgui/_src/`; target `Sakura::ImGui` (not named `imgui`, so DiligentTools keeps its bundled ImGui for `Diligent-Imgui`) |
| PhysX | Physics (NVIDIA PhysX 5.6.1) | FetchContent → `Root/Engine/ThirdParty/PhysX/`; linked as `Sakura::PhysX` |
| fastgltf | glTF/GLB load + export | FetchContent → `Root/Engine/ThirdParty/asset/fastgltf/_src/`; via `Sakura::AssetThirdParty` |
| ozz-animation | Skeletal sampling / blending / model-space | FetchContent → `Root/Engine/ThirdParty/asset/ozz-animation/_src/`; target `ozz_animation` |
| stb_image | PNG/JPEG decode to RAM | FetchContent → `Root/Engine/ThirdParty/asset/stb/_src/`; target `Sakura::StbImage` |
| nlohmann/json | `.meta` and JSON documents | FetchContent → `Root/Engine/ThirdParty/asset/nlohmann_json/_src/`; `nlohmann_json::nlohmann_json` |

### Current Editor deps

| Dependency | Role | Integration |
|------------|------|-------------|
| ufbx | FBX Import source data | FetchContent → `Root/Editor/ThirdParty/ufbx/_src/`; target `Sakura::Ufbx` via `Sakura::EditorThirdParty` |

Fetched Diligent/PhysX/ImGui/asset/`_src` trees are gitignored. Do not copy their `.cpp`/`.h` around the project by hand.

Pinned version cache vars: `SAKURA_SDL3_GIT_TAG`, `SAKURA_DILIGENT_*_GIT_TAG`, `SAKURA_IMGUI_GIT_TAG`, `SAKURA_PHYSX_GIT_TAG`, `SAKURA_FASTGLTF_GIT_TAG`, `SAKURA_OZZ_GIT_TAG`, `SAKURA_NLOHMANN_JSON_GIT_TAG`, `SAKURA_STB_GIT_TAG`, `SAKURA_UFBX_GIT_TAG`. Bump deliberately (keep Diligent modules on the same version).

- Engine links `Sakura::EngineThirdParty` (SDL3, Diligent backends, ImGui, PhysX, Tools/FX helpers, `Sakura::AssetThirdParty`).
- Editor links `Sakura::EditorThirdParty` (ufbx) plus Engine.
- Executables that need Diligent backend DLLs: `sakura_copy_engine_runtime_dlls(<target>)`.
- Include usage: `#include <fastgltf/core.hpp>`, `#include "ozz/animation/runtime/skeleton.h"`, `#include "stb_image.h"`, `#include <nlohmann/json.hpp>`, `#include "ufbx.h"`.

## Coding conventions (engine)

- Identifiers: PascalCase, no abbreviations, no type-wrapper suffixes (`Ptr`, `Ref`, `Handle`).
- Booleans may use a `b` prefix (`bActive`, `bRunning`).
- Always use braces for `if` / loops; no single-line conditionals.
- Prefer complete names over short ones.
- Debug logging messages in English via `PrintString`.
- Dominant build configuration: **Debug**.
