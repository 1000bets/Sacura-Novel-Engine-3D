# Sacura Novel Engine 3D — Agent Notes

## Engine vs Project vs Editor

Architecture follows an Unreal-like split: Engine and Editor are built and staged once; user Projects live outside the engine tree and are opened at runtime.

```text
Installation / Stage/
├── Bin/
│   └── SakuraEditor.exe
└── Engine/
    ├── Content/     — engine assets (/Engine/…)
    ├── Shaders/
    └── Config/

User project (outside engine sources):
MyGame/
├── MyGame.project
├── Content/         — project assets (/Game/…)
├── Scripts/         — Python gameplay scripts
└── Config/
```

Rules:

- **Engine** does not know a concrete Project, does not embed user Content/Scripts, and does not depend on Editor.
- **Editor** depends on Engine only (`Editor → Engine`). Entry: `SakuraEditor` (`Root/Editor/apps/SakuraEditorMain.cpp`).
- **Project** is not a CMake target. No `add_subdirectory(Project)`. Descriptor is `*.project` JSON (`name`, `engineVersion`, `startupScene`).
- Paths: `EnginePaths` (install root from executable / `Bin` parent) and `ProjectPaths` (open project root). Never resolve user assets via `../../../Content` from the exe.
- Open flow: `ProjectSession::OpenProject` → `ProjectDescriptor` + `ProjectPaths` → `Engine::LoadProjectContent` (Game Content + Scripts + rescan).
- Create flow: `ProjectGenerator::CreateProject` (folders + `.project`).
- CLI: `SakuraEditor --project "D:/Games/MyGame/MyGame.project"`; without `--project` → Qt Studio launcher (`ProjectBrowserDialog`: New / Open / Recent / templates).
- Screenshot helper: `SakuraEditor --screenshot-launcher <path.png>` / `--screenshot-editor <path.png>` (for UI iteration).
- Editor shell UI mirrors React prototype (`Sacura-Novel-Engine-3D_ReactEngine` / `Editor.jsx`): menubar, play toolbar, hierarchy, viewport, bottom dock tabs, inspector, status — graphite + muted rose.
- Sample: `Samples/SampleProject/` (not part of Engine Content).
- Source packer (no build / Diligent / PhysX / Qt): `Scripts/PackEngineSourceApp/PackEngineSourceApp.py` or `PackEngineSourceApp.exe` → RAR under `Scripts/PackEngineSourceApp/Output/` (needs WinRAR `Rar.exe`). Rebuild exe: `python -m PyInstaller --onefile --console --name PackEngineSourceApp Scripts/PackEngineSourceApp/PackEngineSourceApp.py`.
- CMake staging: `sakura_stage_engine_runtime(<target>)` → `${CMAKE_BINARY_DIR}/Stage/{Bin,Engine/...}`.

### Asset mounts

`AssetRegistry` supports two roots:

| Mount | Virtual prefix | Disk root |
|-------|----------------|-----------|
| Engine | `/Engine/...` | `EnginePaths::Content()` |
| Game | `/Game/...` (default if unprefixed) | `ProjectPaths::Content()` |

Importer writes to the Game mount (`GetContentRoot()` / `GetGameContentRoot()`).

### Python scripts

Scripts live only under `<ProjectRoot>/Scripts/`. After project open, that directory is added to `sys.path`; packages resolve as `import gameplay.player` → `Scripts/gameplay/player.py`. Engine owns the interpreter; it does not hardcode project module names.

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
| DiligentTools | Texture/Asset loaders, Diligent ImGui bridge (Tools-internal only) | FetchContent → `Root/Engine/ThirdParty/DiligentTools/` |
| DiligentFX | High-level FX framework | FetchContent → `Root/Engine/ThirdParty/DiligentFX/` |
| DiligentSamples | Official tutorials/samples (sources) | FetchContent → `Root/Engine/ThirdParty/DiligentSamples/`; build with `-DSAKURA_BUILD_DILIGENT_SAMPLES=ON` |
| PhysX | Physics (NVIDIA PhysX 5.6.1) | FetchContent → `Root/Engine/ThirdParty/PhysX/`; linked as `Sakura::PhysX` |
| fastgltf | glTF/GLB load + export | FetchContent → `Root/Engine/ThirdParty/asset/fastgltf/_src/`; via `Sakura::AssetThirdParty` |
| ozz-animation | Skeletal sampling / blending / model-space | FetchContent → `Root/Engine/ThirdParty/asset/ozz-animation/_src/`; target `ozz_animation` |
| stb_image | PNG/JPEG decode to RAM | FetchContent → `Root/Engine/ThirdParty/asset/stb/_src/`; target `Sakura::StbImage` |
| nlohmann/json | `.meta` and JSON documents | FetchContent → `Root/Engine/ThirdParty/asset/nlohmann_json/_src/`; `nlohmann_json::nlohmann_json` |
| pybind11 | Python embed scripting | FetchContent → `Root/Engine/ThirdParty/pybind11/_src/`; optional via `SAKURA_ENABLE_PYTHON` |

### Current Editor deps

| Dependency | Role | Integration |
|------------|------|-------------|
| ufbx | FBX Import source data | FetchContent → `Root/Editor/ThirdParty/ufbx/_src/`; target `Sakura::Ufbx` via `Sakura::EditorThirdParty` |
| Qt 6 | Editor UI (`Qt6::Core`, `Qt6::Gui`, `Qt6::Widgets`) | `find_package(Qt6)` in `Root/Editor/ThirdParty/CMakeLists.txt`; hint via `SAKURA_QT_ROOT`, `Qt6_DIR`, or `CMAKE_PREFIX_PATH`. Optional local aqt tree: `Root/Editor/ThirdParty/Qt/<ver>/<arch>` (gitignored). |

Fetched Diligent/PhysX/asset/`_src` and local `Editor/ThirdParty/Qt/` trees are gitignored. Do not copy their `.cpp`/`.h` around the project by hand.

Pinned version cache vars: `SAKURA_SDL3_GIT_TAG`, `SAKURA_DILIGENT_*_GIT_TAG`, `SAKURA_PHYSX_GIT_TAG`, `SAKURA_FASTGLTF_GIT_TAG`, `SAKURA_OZZ_GIT_TAG`, `SAKURA_NLOHMANN_JSON_GIT_TAG`, `SAKURA_STB_GIT_TAG`, `SAKURA_UFBX_GIT_TAG`, `SAKURA_PYBIND11_GIT_TAG`. Bump deliberately (keep Diligent modules on the same version).

- Engine links `Sakura::EngineThirdParty` (SDL3, Diligent backends, PhysX, Tools/FX helpers, `Sakura::AssetThirdParty`). Do **not** add a CMake target named `imgui` — DiligentTools must keep its bundled ImGui for `Diligent-Imgui` only.
- Editor links `Sakura::EditorThirdParty` (ufbx + Qt6 Core/Gui/Widgets) plus Engine. `CMAKE_AUTOMOC` is ON for the Editor library.
- Executables that need Diligent backend DLLs: `sakura_copy_engine_runtime_dlls(<target>)`.
- Executables that link Editor: `sakura_deploy_qt_runtime(<target>)` (windeployqt on Windows).
- Include usage: `#include <fastgltf/core.hpp>`, `#include "ozz/animation/runtime/skeleton.h"`, `#include "stb_image.h"`, `#include <nlohmann/json.hpp>`, `#include "ufbx.h"`, `#include <QWidget>` (Editor only).

## Asset System (Blocks 1–6)

```text
Content + .meta → AssetRegistry → AssetManager (LoadAsync / PumpCompletions)
  → CPU resources (Model/StaticMesh/Texture/Material/Skeletal*)
  → AssetGpuUploader → RenderThread → RenderResourceManager (GPU mesh/texture)
```

- `AssetRegistry` owns Content scan / GUID↔path / subassets; Game Thread.
- `AssetManager` dedupes by `AssetKey`+generation; JobSystem workers do I/O; publish only in `PumpCompletions` on Game Thread. No Diligent dependency.
- Loaders: `ModelLoader` (self-contained GLB only), `TextureLoader` (stb RGBA8), `MaterialLoader` (JSON schemaVersion 1), `SkeletalLoader` (GLB→ozz offline builders; LINEAR only; max 4 influences).
- Shared GLB parse cache lives in `ModelLoader` for Model/StaticMesh/Skeletal subassets of the same file.
- `Engine::InitializeHeadless(GameContentRoot)` — Game thread + Memory + Jobs + Registry.Scan (Engine+Game mounts) + AssetManager; no window/render. Project scripts: sibling `Scripts/` or explicit `SetScriptsRoot`.
- Full `Initialize` also creates Assets after Jobs; `Tick` calls `Assets.PumpCompletions()`; `Shutdown` calls `Assets.Shutdown()` before `Render.Stop()`.
- Editor `AssetTools`: `AssetImporter` dispatches by extension; staging outside Content; path conflict → `ImportConflict` (no overwrite). FBX→GLB via ufbx + fastgltf `Exporter` (static geometry only).
- Headless verification target: `SakuraAssetTest` (`Root/Tests/AssetSystem_Test.cpp`).
- Content fingerprint: FNV-1a 64-bit (`ContentHash`); mismatch → `AssetChanged`. Not asset identity.
- Engine defines `NOMINMAX` (required for fastgltf on MSVC).

### Supported profile (v1)

| Operation | Supported | Rejected / out of scope |
|-----------|-----------|-------------------------|
| Import GLB | Self-contained buffers/images | External URI, network, path traversal |
| Import PNG/JPEG | Copy + `.meta` | Files >256MB, images >8192/side |
| Import FBX | Static meshes, transforms, UV/normals, material slots | Skinning/animation (UnsupportedFeature) |
| Load Model/StaticMesh | Triangles, owned CPU buffers | Non-triangle primitives without conversion path |
| Load Texture | RGBA8 CPU, sRGB default | — |
| Load Material | baseColor/metallic/roughness + optional texture AssetRef | Full PBR |
| Load Skeletal | GLB→ozz in RAM (no `.ozz` on disk), LINEAR tracks, ≤4 influences | STEP/CUBICSPLINE/morph silent drop |
| GPU upload | Mesh/texture via Render Thread | Sync load inside Draw |

No disk asset cache / DerivedData / `.meshbin`.

## Reflection System

```text
ENGINE_CLASS / ENGINE_STRUCT / ENGINE_REFLECT_* (header registrars)
  → PendingRegistry (static intrusive recipes, early dynamic init)
  → ReflectionSubsystem::InitializeNative (bind when types are complete)
  → Class / TypeDescriptor / PropertyDescriptor catalog + CDO
  → PropertyAccess Get/Set (Inspector / Deserialize / Script / Default)
  → (optional) ScriptingSubsystem + Python Class publish
  → ReflectionJson serialize/deserialize
  → Editor ReflectionInspector (Qt Widgets via PropertyAccess)
```

- **TypeId** / **PropertyId** are stable strings (`"engine.CameraComponent"`, `"field_of_view"`), not RTTI hashes or registration indices.
- Authoring is in headers next to members via `Reflection/ReflectionMacros.h`. Nested static registrars only store string literals + binder function pointers (`noexcept`, no heap). Binder bodies run in `InitializeNative` when the class is complete.
- After the class/struct closing brace in the **same header**: `ENGINE_CLASS_END` / `ENGINE_STRUCT_END`, then `ENGINE_IMPLEMENT_FIELD` / `ENGINE_IMPLEMENT_PROPERTY` / `ENGINE_IMPLEMENT_READONLY` (MSVC-safe member-pointer bind).
- `RF_DISPLAY_NAME`, `RF_UI_RANGE`, `RF_CATEGORY` feed `PropertyAttributes` alongside `PropertyFlags`.
- `Class` is metadata (`Object` subclass), **not** a scene object. Instances get `Object::AssignClass` / `GetClass` / `GetTypeId` from the factory.
- Builtin value types: `engine.bool`, `engine.int64`, `engine.float`, `engine.double`, `engine.string`, `engine.Vector3`, `engine.Quaternion`, `engine.Color` (Color stays a distinct TypeId from Vector4).
- No on-disk type / schema cache in v1.
- MSVC static registration can be stripped from static `Engine.lib` if unused: link tests/tools with `/WHOLEARCHIVE:Engine` (see `Root/CMakeLists.txt` for `SakuraReflectionTest`, `SakuraReflectionPythonTest`, `SakuraAssetTest`, `SakuraTest`). `ReflectionAnchor.cpp` includes reflected headers and `ForceTouchRegistrars()`.
- Headless verification: `SakuraReflectionTest` (native) and `SakuraReflectionPythonTest` (JSON + Python carrier + inspector list API).
- Engine hooks: `InitializeCommon` calls `InitializeNative` after Jobs, then `ScriptingSubsystem::Initialize` + import `ScriptsRoot` modules; `Shutdown` destroys script instances → finalize interpreter → Reflection shutdown (after Assets, before Render/Jobs). CDOs still need `MemorySubsystem`.

### Python carrier model (`SAKURA_ENABLE_PYTHON`)

```text
Python @register_class / field()
  → ScriptingSubsystem publishes Class (Origin=Python, NativeBackingType=engine.ScriptComponent)
  → native ScriptComponent carrier holds managed PropertyId→ReflectedValue + ScriptInstanceHandle
  → OnCreate/Tick/OnDestroy → Python on_create/on_update/on_destroy (no user __init__)
```

- CMake option `SAKURA_ENABLE_PYTHON` (default ON). If Python3 `Development.Embed` is missing, configure falls back to OFF with a warning. pybind11 (pinned `SAKURA_PYBIND11_GIT_TAG`, default `v2.13.6`) lives in `Root/Engine/ThirdParty/pybind11/_src` (gitignored). Hint install root with `SAKURA_PYTHON_ROOT` / `-DSAKURA_PYTHON_ROOT=...`. Runtime needs `python310.dll` on `PATH` (or next to the exe) and a valid `PYTHONHOME`.
- When ON: Engine links `pybind11::embed` + `Python3::Python`, defines `SAKURA_ENABLE_PYTHON=1`. Reflection core headers stay free of `py::object`.
- Embedded module name: `engine` (`ScriptComponent` façade, `register_class`, `field`, `Object` handle with generation, Vector3/Transform copy-in/copy-out).
- Published Python types are kept in an internal TypeId→`py::object` map at register time. `BindScriptComponent` must not scan `sys.modules`. Resolve lifecycle callbacks via `getattr` on the **type**, then bind to the instance — avoid `hasattr`/`attr` on the instance for `on_*` (deadlocks with field descriptors under embed).
- Project scripts path: `<ProjectRoot>/Scripts` via `ProjectSession` / `Engine::SetScriptsRoot`. Sample: `Samples/SampleProject/Scripts/door_controller.py` (`game.DoorController`).
- Exception in `on_update` → mark ScriptFailed, stop further updates; `on_destroy` at most once.
- **No hot reload** in v1: import at project open / headless init when ScriptsRoot is set.
- Build without Python: `-DSAKURA_ENABLE_PYTHON=OFF` — native reflection tests still pass; Python cases are skipped in `SakuraReflectionPythonTest`.

### JSON + Inspector

- `Reflection/Json/ReflectionJson` — `{ "type", "typeVersion", "properties" }`; AssetRef as id string only; ClassRef as TypeId string; unknown fields/types fail with no partial publish; optional per-type migration hooks (e.g. 1→2).
- Editor `ReflectionInspector` — `QWidget` panel (Qt6 Widgets) for bool/float/string/Vector3/Color via `PropertyAccess`; `SetInspectedObject` / `Rebuild`; reset from CDO; range rejection; no hardcoded Camera panel. Requires a `QApplication` before constructing widgets.

## Coding conventions (engine)

- Identifiers: PascalCase, no abbreviations, no type-wrapper suffixes (`Ptr`, `Ref`, `Handle`).
- Booleans may use a `b` prefix (`bActive`, `bRunning`).
- Always use braces for `if` / loops; no single-line conditionals.
- Prefer complete names over short ones.
- Debug logging messages in English via `PrintString`.
- Dominant build configuration: **Debug**.
