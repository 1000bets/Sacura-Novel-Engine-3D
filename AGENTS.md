# Sacura Novel Engine 3D — Agent Notes

## Tool restrictions

- Computer Use is prohibited for this project. Do not use UI automation to inspect, operate or test applications. Use builds, automated tests, logs and project-provided command-line or screenshot helpers instead.

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

- **Engine** (logical EngineRuntime, CMake target `Engine`) does not know a concrete Project, does not embed user Content/Scripts, and does not depend on Editor. Qt6 Core/Gui/Widgets is a public Engine dependency for runtime UI.
- **EditorCore** (CMake `EditorCore`) depends on Engine + ufbx — import tools, `SceneDocument`, `EditorCommandStack` / scene edit commands. Its operations require no QApplication; Qt linkage is inherited from Engine. It is built independently of SAKURA_BUILD_EDITOR.
- **Editor** (logical QtEditorFramework, CMake `Editor`) depends on EditorCore + Qt6 Widgets + ImGuizmo — shell UI, adapters, native viewport host and edit-only transform manipulator. Entry: `SakuraEditor` (`Root/Editor/apps/SakuraEditorMain.cpp`).
- **Project** is not a CMake target. No `add_subdirectory(Project)`. Descriptor is `*.project` JSON (`name`, `engineVersion`, `startupScene`, optional `startupStory`).
- Paths: `EnginePaths` (install root from executable / `Bin` parent) and `ProjectPaths` (open project root). Never resolve user assets via `../../../Content` from the exe.
- Open flow: `ProjectSession::OpenProject` → `ProjectDescriptor` + `ProjectPaths` → `Engine::LoadProjectContent` (returns `AssetDiagnostic`; Game Content + Scripts + rescan) → load `startupScene` via `SceneSerializer` into `Engine::AdoptScene`. Health: `Ready` / `Degraded` (scan or startupScene issues) / `Failed`.
- Scene document format `sakura.scene` v1 (`SceneSerializer`): stable object/component string IDs, parent refs, transform/flags, reflected component `type`/`typeVersion`/`properties`. Runtime `ObjectHandle` and GPU handles are not persisted. Atomic write: temp file + verify + rename. Subtree helpers: `SerializeSubtreeToJson` / `DeserializeSubtreeFromJson` (delete undo), `SerializeComponentToJson` / `DeserializeComponentFromJson`.
- Story document format `sakura.story` v1 (`StoryDocumentIO` under `Root/Engine/inc/Story/`): nodes with kinds `Line`, `Choice`, `Action`, `End`; action kinds `Wait`, `MoveTo`, `CameraCut`. **`StoryRuntime`** (Engine, UI-independent): `LoadFromFile` / `Reset` / `AdvanceDialogue` / `SelectChoice` / `Tick` for timed blocking actions; `Stop` cancels running actions. `MoveTo` resolves a persistent object ID and lerps its local transform; `CameraCut` blends the primary `CameraComponent` owner transform (optional copy from an object resolved by persistent ID). Sample: `Samples/SampleProject/Content/Stories/Demo.story`. Headless test: `SakuraStoryRuntimeTest` (`Engine.Story.Runtime`). Editor playtest: `PlaySession` PlayWorld + dock tab «Сценарий» and viewport strip overlay driven by `StoryRuntime`.
- EditorCore `SceneDocument`: path, dirty flag, `AdoptScene` / Open/Save/SaveAs/Close; failed Open keeps the previous document. Saving a scene does not rewrite `.project`. Engine owns the main scene via unique_ptr; SceneDocument stores a generation-checked non-owning identity. Project close stops play and releases the Engine-owned scene; document/session close order is safe.
- EditorCore commands (`EditorCommandStack` + `Make*Command`): rename, transform, create/delete (subtree JSON undo), reparent (cycle reject), add/remove component, set/reset property. All Hierarchy/Inspector mutations go through the stack; stack marks the document dirty. Selection uses `ObjectHandle`, never Qt ownership of `GameObject`.
- `LightComponent` is reflected (`engine.LightComponent`) so factory / inspector / scene IO work; `light_type` is int64 (0=Directional, 1=Point, 2=Spot).
- Create flow: `ProjectGenerator::CreateProject` (folders + `.project` + real empty `startupScene` file). `ProjectName` must be a single path segment inside the parent directory; `startupScene` must stay inside the project root.
- **One project per process (v1):** File → Open / New while a project is already open launches a new `SakuraEditor --project …` process. Do not treat `ScriptsRoot` / `sys.path` swap as Python isolation. Type registration is for the trusted open project only; UI must not promise a sandbox.
- CLI: `SakuraEditor --project "D:/Games/MyGame/MyGame.project"`; without `--project` → Qt Studio launcher (`ProjectBrowserDialog`: New / Open / Recent / templates).
- **SakuraPlayer** (`Root/Engine/apps/SakuraPlayerMain.cpp`): Qt runtime host with Engine-owned `RenderViewportWidget` and `StoryWidget`. Uses QApplication + QTimer, `InitializeHeadless` + `StartPresenting`, `StartGame`, and `.project` startupStory. No automatic dialogue advancement or implicit first-choice selection. Links Engine and deploys Qt through the Engine runtime helper.
- Screenshot helper: `SakuraEditor --screenshot-launcher <path.png>` / `--screenshot-editor <path.png>` (for UI iteration).
- Editor shell UI mirrors React prototype (`Sacura-Novel-Engine-3D_ReactEngine` / `Editor.jsx`): menubar, play toolbar, **left Inspector**, center viewport + bottom dock tabs, **right Hierarchy** (scene tree / story outline), status — graphite + muted rose. Forward render live stats (`Engine::GetRenderStatistics`) and postprocess knobs (Exposure / IBL / Bloom / FXAA via `RenderSettings`) live in bottom dock tabs «Настройки статистики» and «Настройки постобработки», not under the viewport.
- Sample: `Samples/SampleProject/` (not part of Engine Content).
- Source packer (no build / Diligent / PhysX / Qt): `Scripts/PackEngineSourceApp/PackEngineSourceApp.py` or `PackEngineSourceApp.exe` → RAR under `Scripts/PackEngineSourceApp/Output/` (needs WinRAR `Rar.exe`). Rebuild exe: `python -m PyInstaller --onefile --console --name PackEngineSourceApp Scripts/PackEngineSourceApp/PackEngineSourceApp.py`.
- CMake staging: `sakura_stage_engine_runtime(<target>)` → `${CMAKE_BINARY_DIR}/Stage/{Bin,Engine/...}`.

### Runtime hosting and scene ownership

- Qt owns the application loop and native viewport for both Editor and Player. Shared game UI lives in `Root/Engine/inc/src/UI`; Editor-specific panels stay in Editor. SDL remains available for existing low-level host tests.
- `Engine::AdoptScene(unique_ptr<Scene>)` transfers main-world ownership and stops any current simulation. `PlaySession` exclusively owns its cloned PlayWorld; the main world remains in Engine. `GetActiveScene` selects the simulated world without transferring ownership.
- `Engine::StartGame` runs the main world in Player; `PlaySession::StartPlay` runs a copy in Editor. Both use `Engine::TickWorld` for scene components and StoryRuntime. Pausing freezes both; Step advances both once.
- Engine is the only frame producer. EndFrame submits one immutable RenderFrameData containing all active RenderViewFrame snapshots. Editor uses one native viewport: Edit mode renders EditWorld through the editor camera; Play switches that surface and Hierarchy to PlayWorld and the primary game camera. Frame indices increase monotonically. The renderer still supports multiple surfaces for tests and future tools.
- Register/configure/resize/unregister surfaces through Engine. Surface 1 initializes the shared device; additional native swapchains require D3D12 or Vulkan (OpenGL remains a single-surface fallback). Native-surface destruction synchronously detaches and waits for GPU idle before returning to Qt; resize is enqueued. Zero-sized/hidden surfaces suspend. StopPresenting joins Render Thread before host destruction.
- `Object::PersistentId` survives serialization, deletion undo, reorder and rename. Runtime ObjectHandle is never persisted. New IDs use GUIDs; loaded scene IDs are preserved.
- Story actions persist `targetObjectId` and retain generation-checked identities while running. Legacy `targetObjectName` is resolved once on load and must be unique. A destroyed target fails the action without dereferencing stale memory.
- Startup story is an optional project-relative `startupStory` path restricted to the project root. Engine owns StoryRuntime; UI hosts do not hardcode sample content.
- Diligent implementation and backend headers are private under `Root/Engine/src/Rendering/RHI`; only that directory invokes the Diligent API. Public gameplay/editor headers expose scene data and resource identities.

### Asset mounts

`AssetRegistry` supports two roots:

| Mount | Virtual prefix | Disk root |
|-------|----------------|-----------|
| Engine | `/Engine/...` | `EnginePaths::Content()` |
| Game | `/Game/...` (default if unprefixed) | `ProjectPaths::Content()` |

Importer writes to the Game mount (`GetContentRoot()` / `GetGameContentRoot()`).

All authored asset and document files live under one of these Content roots. Asset types are open string identifiers represented by `AssetType`, never a closed enum. Built-in identifiers are ordinary constants such as `SceneAssetType`; project or plugin types use their own stable identifier such as `game.Dialogue`. `AssetRegistry::RegisterAssetType` registers the identifier, display name, extensions and Content Browser visibility before scanning. `AssetRegistry::ScanContent` discovers every registered Content extension and creates a missing `.meta` sidecar with a stable GUID, asset type and source fingerprint. A recognized data file without `.meta` is therefore adopted into the asset system instead of being hidden from the Content Browser. Unsupported extensions are not assets until their type is registered. Final Game asset names, including document names and subasset names, are case-insensitively unique across the project.

Content Browser filters are generated from registered type descriptors. Double-click is the common asset activation contract: it emits the asset identity, string type identifier and virtual path, then dispatches through `EditorMainWindow::RegisterAssetEditor`. Scene activation prompts for unsaved changes and opens through `SceneDocument`; Story activation loads into the Story workspace. Types without a registered editor remain valid assets and report that no editor is available. Runtime loaders are registered by identifier through `AssetManager::RegisterLoader`; custom resource types may use `LoadAsync<CustomResource>` without modifying an Engine enum. Runtime/CPU resource loading and editor document opening are separate consumers of the same `AssetRegistry` identity.

### Python scripts

Scripts live only under `<ProjectRoot>/Scripts/`. After project open, that directory is added to `sys.path`; packages resolve as `import gameplay.player` → `Scripts/gameplay/player.py`. Engine owns the interpreter; it does not hardcode project module names.

## Rendering architecture

- Graphics abstraction: **Diligent Engine Core** (not raw D3D12/Vulkan/OpenGL, not SDL_GPU).
- Application windows / runtime UI: **Qt6**. SDL3 remains for legacy host tests.
- Our code owns `Rendering` (Renderer, RenderScene, SceneExtractor, passes, materials). Gameplay never includes Diligent headers or native GPU API types (`VkInstance`, `ID3D12Device`, etc.).
- Only `Rendering/RHI` may talk to Diligent.
- Shaders: **HLSL** as the primary language.
- Backend selection: `Auto` | `D3D12` | `Vulkan` | `OpenGL`. On Windows, `Auto` resolves to D3D12 (fallback Vulkan → OpenGL). Selection lives only in `RenderDevice::ResolveBackend` / `InitializeBackend`.
- Pipeline model for the current horizon: **forward** rendering. Deferred is out of scope until explicitly planned.

### Stage 3 — Rendering Core (Diligent)

Module layout under `Root/Engine`:

```text
src/Rendering/RHI/
  Renderer.*          — lifecycle, clear, draw from RenderFrameData
  RenderDevice.*      — Diligent device / context / swapchain ownership
  RenderContext.h     — per-frame execution state only
  GpuBuffer.*        — Vertex / Index / Constant buffers
  RenderMesh.*       — GPU mesh (VB + IB + counts)
  RenderResourceManager.* — resource identities, residency and retirement
  EnvironmentLighting.* — irradiance, prefiltered environment and BRDF LUT
shaders/Forward.hlsl — PBR, shadow and weighted transparency entry points
shaders/Postprocess.hlsl — composite, tone mapping/bloom and FXAA
```

- `Renderer` is created and used **only** on Render Thread (`AssertRenderThread` on public methods).
- Qt runtime windows are owned by UI widgets; the legacy SDL host uses `WindowSubsystem`. Rendering receives `NativeWindowInfo` only.
- Resize path: Qt resize (or legacy SDL event) → Game Thread → Render Command → `Renderer::Resize` → SwapChain.
- Forward rendering: GGX metallic/roughness PBR, directional/point/spot lights; inverse-transpose normal transforms. Material v1 supports alphaMode OPAQUE/MASK/BLEND, alphaCutoff, emissive, doubleSided and castShadows in addition to existing factors/baseColorTexture. No GPU skinning.
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
- Qt runtime widgets (or the legacy SDL `WindowSubsystem`) own native windows on Game Thread; resize is forwarded as a Render Command.
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
| Qt 6 | Engine runtime UI and application windows | `Root/Engine/ThirdParty/Qt.cmake`; public via `Sakura::EngineQtThirdParty`. Local SDK: `Root/Engine/ThirdParty/Qt/`; hints: SAKURA_QT_ROOT / Qt6_DIR. |
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
| ufbx | FBX Import source data | FetchContent → `Root/Editor/ThirdParty/ufbx/_src/`; target `Sakura::Ufbx` via `Sakura::EditorCoreThirdParty` |
| ImGuizmo | Editor transform manipulator | FetchContent → `Root/Editor/ThirdParty/ImGuizmo/`; target `Sakura::ImGuizmo`, using DiligentTools' bundled ImGui |

Fetched Diligent/PhysX/asset/`_src`, Editor ImGuizmo and local `Engine/ThirdParty/Qt/` trees are gitignored. Do not copy their `.cpp`/`.h` around the project by hand.

Pinned version cache vars: `SAKURA_SDL3_GIT_TAG`, `SAKURA_DILIGENT_*_GIT_TAG`, `SAKURA_PHYSX_GIT_TAG`, `SAKURA_FASTGLTF_GIT_TAG`, `SAKURA_OZZ_GIT_TAG`, `SAKURA_NLOHMANN_JSON_GIT_TAG`, `SAKURA_STB_GIT_TAG`, `SAKURA_UFBX_GIT_TAG`, `SAKURA_IMGUIZMO_GIT_TAG`, `SAKURA_PYBIND11_GIT_TAG`. Bump deliberately (keep Diligent modules on the same version).

- Engine links `Sakura::EngineThirdParty` (SDL3, Diligent backends, PhysX, Tools/FX helpers, `Sakura::AssetThirdParty`). Do **not** add a CMake target named `imgui` — DiligentTools must keep its bundled ImGui for `Diligent-Imgui` only.
- EditorCore links `Sakura::EditorCoreThirdParty` (ufbx) plus Engine; no QWidget operations or QApplication requirement.
- Editor links EditorCore + `Sakura::EditorThirdParty` and inherits Qt from Engine. AUTOMOC is enabled for Engine runtime widgets and Editor widgets.
- Executables that need Diligent backend DLLs: `sakura_copy_engine_runtime_dlls(<target>)`.
- `sakura_copy_engine_runtime_dlls(<target>)` also deploys Qt runtime libraries and Windows platform/image plugins for Engine consumers.
- Include usage: `#include <fastgltf/core.hpp>`, `#include "ozz/animation/runtime/skeleton.h"`, `#include "stb_image.h"`, `#include <nlohmann/json.hpp>`, `#include "ufbx.h"`, `#include <QWidget>` (Engine UI and Editor UI).

## Asset System (Blocks 1–6)

```text
Content + .meta → AssetRegistry → AssetManager (LoadAsync / PumpCompletions)
  → CPU resources (Model/StaticMesh/Texture/Material/Skeletal*)
  → AssetGpuUploader → RenderThread → RenderResourceManager (GPU mesh/texture)
```

- `AssetRegistry` owns Content scan / GUID↔path / subassets; Game Thread.
- Authored documents are first-class assets: `SceneAssetType` for `*.scene` and `StoryAssetType` for `*.story`. They use ordinary Content files plus `.meta`, participate in move/rename/delete and are opened by registered type-specific editor handlers.
- `AssetManager` dedupes by `AssetKey`+generation; JobSystem workers do I/O; publish only in `PumpCompletions` on Game Thread. No Diligent dependency.
- Loaders: `ModelLoader` (self-contained GLB only), `TextureLoader` (stb RGBA8), `MaterialLoader` (JSON schemaVersion 1), `SkeletalLoader` (GLB→ozz offline builders; LINEAR only; max 4 influences).
- Shared GLB parse cache lives in `ModelLoader` for Model/StaticMesh/Skeletal subassets of the same file.
- `Engine::InitializeHeadless(GameContentRoot)` — Game thread + Memory + Jobs + Registry.Scan (Engine+Game mounts) + AssetManager; no window/render. Project scripts: sibling `Scripts/` or explicit `SetScriptsRoot`.
- Full `Initialize` also creates Assets after Jobs; `Tick` calls `Assets.PumpCompletions()`; `Shutdown` calls `Assets.Shutdown()` before `Render.Stop()`.
- Editor `AssetTools` (EditorCore): `AssetImporter` dispatches by extension; staging outside Content; path conflict → `ImportConflict` (no overwrite). FBX→GLB via ufbx + fastgltf `Exporter` (static geometry only).
- Headless verification target: `SakuraAssetTest` (`Root/Tests/AssetSystem_Test.cpp`) links Engine + EditorCore (no QApplication needed).
- Content fingerprint: FNV-1a 64-bit (`ContentHash`); mismatch → `AssetChanged`. Not asset identity.
- Engine defines `NOMINMAX` (required for fastgltf on MSVC).
- `AssetRegistry::ScanContent` returns failure when mount iteration fails or any `.meta` ingest error was recorded (diagnostics remain in `GetScanDiagnostics()`).

### Supported profile (v1)

| Operation | Supported | Rejected / out of scope |
|-----------|-----------|-------------------------|
| Import GLB | Self-contained buffers/images | External URI, network, path traversal |
| Import PNG/JPEG | Copy + `.meta` | Files >256MB, images >8192/side |
| Import FBX | Static meshes, transforms, UV/normals, material slots | Skinning/animation (UnsupportedFeature) |
| Load Model/StaticMesh | Triangles, owned CPU buffers | Non-triangle primitives without conversion path |
| Load Texture | RGBA8 CPU, sRGB default | — |
| Load Material | baseColor/metallic/roughness, baseColorTexture, emissive, OPAQUE/MASK/BLEND, cutoff and sidedness | Normal/metallic-roughness texture maps |
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
- Asset properties keep stable GUID and subasset IDs in serialized scene data, while editor UI resolves them through `AssetRegistry` and displays virtual paths. Reflected asset fields use `RF_ASSET_TYPE`; `RF_COMPANION_PROPERTY` groups a subasset ID with its owning asset; `RF_EDITOR_HIDDEN` keeps implementation fields out of the Inspector. Users must never type asset GUIDs in the Inspector.
- `ContentBrowserWidget` follows an Unreal-like sources/assets layout: Engine and Game folder trees, compact registered-type filter buttons, search and draggable asset tiles. `All` shows the direct contents of the selected folder; an active type filter shows every matching final asset recursively under that folder. It displays only final usable assets: storage containers such as `Model` are hidden when they own subassets, and every non-empty GLB mesh is shown as its own `StaticMesh` tile. Final asset names are case-insensitively unique across the Game project; import derives stable names from the destination file and adds numeric suffixes on conflicts. Game Content accepts imports through the Import button or external file drop. Both open `AssetImportDialog` for destination names/folder and format-specific options before invoking `AssetImporter`; Engine Content is read-only. Supported UI imports are self-contained GLB, static FBX (converted to GLB, optional missing-normal generation), PNG and JPEG. Model import writes one `StaticMesh` subasset record per non-empty GLB mesh (`selector.kind="mesh"`, source mesh index). Game assets support F2/context-menu rename, confirmed Delete, and drag-and-drop moves onto Game folders; siblings sharing one backing file move together, deleting the last leaf removes the data/metadata pair, GUIDs stay stable across rename/move, and deletion unregisters CPU slots and retires cached GPU resources. It exports `application/x-sakura-asset`; dropping a StaticMesh into the Scene viewport creates a `GameObject` with `MeshRendererComponent`, while dropping a Material assigns it to the selected mesh object.
- Scene gizmo uses ImGuizmo with combined `TRANSLATE | ROTATE | SCALE` (no W/E/R mode switch) and is available only while the single viewport is in Edit mode. During a drag the selected object receives preview transforms directly; release restores the old value and commits one `MakeSetTransformCommand`, preserving a single undo step. Edit-mode object picking constructs a camera ray on the CPU and selects the nearest visible `MeshRendererComponent` by its transformed world AABB; it does not require PhysX colliders and does not run during Play or gizmo manipulation. Inspector transforms expose position, Euler rotation in degrees and scale. Edit-mode viewport camera is Unreal-like: RMB look + WASD/QE fly (Shift faster, Ctrl slower, wheel adjusts speed), MMB pan, wheel dolly, Alt+LMB orbit, Alt+RMB dolly; Play mode switches to the game camera and disables editor navigation.

## Coding conventions (engine)

- Editor gizmo overlay is a non-activating, input-transparent tool window. The native viewport owns mouse/keyboard input and forwards mouse events in viewport coordinates to the gizmo. Pass SimpleMath matrices directly to ImGuizmo without transposing; both store translation in the last four-float row. Windows fly navigation uses physical WASD/QE scan codes so the keyboard layout does not change camera controls.

- Identifiers: PascalCase, no abbreviations, no type-wrapper suffixes (`Ptr`, `Ref`, `Handle`).
- Booleans may use a `b` prefix (`bActive`, `bRunning`).
- Always use braces for `if` / loops; no single-line conditionals.
- Prefer complete names over short ones.
- Debug logging messages in English via `PrintString`.
- Dominant build configuration: **Debug**.
- CMake presets (Windows/MSVC): `CMakePresets.json` (`windows-msvc-debug` / `windows-msvc-release` / `windows-msvc-debug-engine-only`). Machine paths via `CMakeUserPresets.json` (see `.example`) using `SAKURA_QT_ROOT` / `SAKURA_PYTHON_ROOT`. Editor optional: `-DSAKURA_BUILD_EDITOR=OFF`. Tests: `ctest --preset windows-msvc-debug` (excludes label `probe`).

## Architecture follow-up and rendering roadmap

- The architecture cleanup preserves PhysX, DiligentFX, ozz, Python and the current backend choices; dependency scope is unchanged.
- Minimal Forward implementation and remaining validation are documented in `Docs/ForwardRenderer.md`; `Docs/RenderingRoadmap.md` remains the longer-term roadmap. Do not describe newly written GPU code as runtime-verified until the current sources have been built and exercised.
- Continue using forward rendering. Deferred and ray tracing require a separate explicit decision.
- Regression coverage includes standalone component ticking, safe close during Play, persistent scene IDs, story target deletion, ambiguous legacy names and actual Qt dialogue/choice button interactions.

### Minimal Forward renderer contracts

- All swapchains use IsPrimary=false. Renderer explicitly calls FinishFrame and ReleaseStaleResources once after every surface has presented; individual Present calls must not invalidate another surface’s dynamic buffers.
- One device/context and Render Thread serve every surface. Each surface owns RGBA16F HDR, D32 depth, weighted OIT accumulation/revealage, tone-mapped intermediate, a 4096-square shadow atlas and a four-frame duration-query ring. No GPU state is accessed from Qt/Game Thread.
- Lighting budget: 32 visible lights and 16 shadow tiles of 1024 pixels per view. Directional/spot use one tile; point uses six. Overflow is counted in RenderStatistics. Directional shadows use a camera-relative finite volume, not cascades.
- IBL uses a built-in sky baked into irradiance and GGX-prefiltered reflection cubemaps plus an integrated BRDF LUT. This is global environment lighting; custom HDR import and local probes are outside this minimal implementation.
- OPAQUE/MASK render with depth writes; MASK uses the same alpha test for shadow casting. BLEND uses weighted blended OIT with depth testing and no depth writes, then composites in linear HDR. Weighted OIT is approximate, does not model refraction, and translucent surfaces do not cast transmission shadows.
- Post: linear HDR composition → exposure, threshold bloom and ACES approximation → FXAA → sRGB swapchain. Qt UI is composed separately. No TAA/motion vectors, deferred renderer, animation or GPU skinning in this change.
- CPU visibility uses conservative world AABB/frustum tests separately for every camera and shadow view; local lights are range-culled. Opaque draws are front-to-back. GPU timings measure shadow, opaque, transparency and post passes without blocking; delayed samples carry their GPU frame index. CPU submission time, draw/triangle counts, light-budget overflow and asset GPU memory estimates are exposed through Engine::GetRenderStatistics.
- Asset retirement retains identities until the fence has completed the last potentially referencing submitted frame (signal value FrameIndex + 1). Cancellation after upload also retires the created object. IDs are never recycled across renderer sessions. Project close clears upload caches, cancels CPU loads and schedules GPU retirement. Resize/detach/shutdown deliberately wait for GPU idle; ordinary frames do not.
- `Samples/RenderValidation/RenderValidation.project` is the reproducible technical scene; `GenerateScene.py` regenerates its GLBs, materials, alpha texture and metadata. `SakuraForwardRendererTest` / `Engine.Render.Forward` exercises two surfaces, culling, shadow allocation, suspend/resize/reattach and retirement. `--vulkan` selects Vulkan for an additional run.
