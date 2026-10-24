# Sacura Novel Engine 3D — Agent Notes

## Session logging

- `PrintString` retains stdout and the bounded in-memory history, and writes timestamped UTC/thread-tagged records to a flushed per-session `Saved/Logs/Engine-*.log`. Engine path initialization starts the startup log; opening a project copies the startup history to the project's `Saved/Logs` and continues there. Failure to create or write a log reports to stderr without preventing startup. Logs are diagnostics, never authored Content or export data.
- Electron native viewport sessions write separate `Saved/Logs/Electron-*.log` files under the project, capturing host stdout/stderr, renderer console and renderer termination. Frame protocol statistics are excluded. Presentation failures must report to the renderer console once per distinct error. This is passive diagnostics; no MCP mutation command is required.

## React and Electron architecture

- The editor and standalone game Player must use React + Electron, including in-game UI. Qt hosts, widgets, MCP executables and build integration have been removed.
- Remove Qt from Engine, EditorCore, native hosts, MCP transport, build prerequisites and distribution. Do not retain Qt behind an optional build switch or replace it with Qt-compatible wrapper classes.
- Preserve authored documents, persistent identities, shared command validation, Undo/Redo, explicit saves, Python gameplay, StoryRuntime and GPU rendering. React presents game state and sends typed actions; the native Game thread owns simulation. The Player must not expose editor mutation commands.
- Atomic files, PNG previews, Base64 and localization use native C++ services without Qt. Verify that deployed executables do not import Qt libraries.
- The explicitly retained migrations are Python Narrative, isolated prefab editing with overrides/apply/revert/unpack, and game export. Other Qt-only editors are removed; do not recreate them. Enhanced Input runtime remains.
- Narrative uses `story.addNarrative`, `story.setNarrativeInput`, `story.connectData` and `story.validate`, with the Python catalog, typed pins, named exits and shared StoryGraphDocument history.
- Prefab workspaces run an isolated native process, use `assetDocument.execute` with revision/conflict checks, and keep Inspector left, viewport center, Hierarchy right. Scene instance commands are `prefab.create/instantiate/inspect/modify`. Apply writes the shared asset and clears scene undo history.
- Player uses `player-main.cjs`, React NativeGamePlayer and the same native host with `SACURA_PLAYER=1`. Only game state, advance, choice, pause and startup audio commands are accepted. Electron forwards keyboard, pointer and standard gamepad input to Enhanced Input.
- Export is coordinated by Electron `game-export.cjs` and EditorCore GameExporter. Publish a new folder only after copying the complete Electron application, native runtime, Python and saved project. Preserve unknown descriptor fields and authored Content/Config; omit Saved and caches. See docs/DISTRIBUTION.md.

## Parallel task and build coordination

- Agents working in parallel, including separate chats sharing this workspace, are responsible for coordinating task execution and compilation themselves. Do not ask the user whether to continue work, wait for another task, or postpone a build solely for coordination.
- Use available agent messaging or shared workspace coordination files to communicate active work and build ownership. Before configuring or building in a shared build directory, check for an active build and coordinate access; do not run conflicting builds or terminate another task's build. While waiting, continue independent work and resume the required build when the directory is available.
- Preserve other tasks' changes. Coordination does not authorize spawning additional agents or expand the scope of the assigned task.

## Tool restrictions

- Session exception: the user explicitly authorized Computer Use for the current template parity task. This exception does not change the default restriction for other tasks.

- Input actions default to requiring a neutral value after pause, context changes and source removal. Absolute pointer-position actions set `bRequireNeutral=false`: a screen coordinate has no release state and must remain usable after menus. Keep the default for keys and sticks. The setting belongs to the authored InputAction document and shared InputSession, not template-specific native code.

- Computer Use is prohibited for this project. Do not use UI automation to inspect, operate or test applications. Use builds, automated tests, logs and project-provided command-line or screenshot helpers instead.

## Editor settings, console and MCP

- Do not add viewport or graph help popovers listing navigation controls or keyboard shortcuts.

- Scripting lists individual PythonScript assets and searches their paths and source contents. Results open the original file through the ordinary Python asset workspace, preserving its editing, history and explicit saves. It uses existing assetDocument.execute/open reads and never concatenates modules into a synthetic file.

- Native Story Flow cards reuse Inspector fields for node type, speaker, dialogue/prompt and actions, and Narrative literal controls for typed inputs. Inline edits use the existing story.updateNode/story.setNarrativeInput commands, retain Undo/Redo, and are disabled during Play/read-only. Text commits on blur or Enter (Ctrl+Enter for multiline); Escape cancels the draft.

- Story Flow selection preserves the graph viewport by default. Automatic focus is an opt-in toolbar icon toggle; explicit focus and fit actions remain independent. This is UI-only navigation state, excluded from native mutation commands and authored story documents.

- Scene hierarchy rename edits the row directly through `object.rename`; prefab actions open from the compact per-object hierarchy button, not the Inspector. Clipboard subtree payloads use the shared `serializedJson` contract with a 1 MiB UTF-8 limit and parsed JSON validation, separate from short text fields.

- The Windows menu lists only registered editor panels. Asset documents must never appear there, whether open, closed or restored from saved workspace layout.

- Content Browser file and folder cards share fixed dimensions and aligned preview/name/metadata rows. Loading a thumbnail, selecting a card, long names or search paths must not change card size. Truncate overflowing labels and retain the full path in a tooltip.

- Mini-dialogs and popovers belong to the invoking workspace window, including detached asset editors. Portal targets, viewport measurements, focus and event listeners must use the trigger's ownerDocument/defaultView (or the captured active workspace document for action dialogs), never the main document/window. Color panels remain compact local dialogs; they are not dockable workspaces.

- Project and editor settings use ordinary workspace tabs in Electron, registered in the shared window catalog. Edit menu and toolbar entries reveal those tabs; they use the standard close, drag, dock and detach behavior. Do not wrap these settings in modal dialogs.

- Electron Project Settings are available from Edit and persist explicitly through typed `project.settings.get/set` bridge commands and `ProjectSession::UpdateSettings`. Startup paths are project-relative, validated, and applied at next project open; writes reject Play and external descriptor changes. Editor preferences remain per-user.

- Boolean editor properties use checkboxes instead of Yes/No dropdowns. A toggle applies immediately through the existing command path and retains Undo/Redo and read-only/Play restrictions.

- Standalone asset editors use ordinary native Windows windows (Electron BrowserWindow, standard title bar, minimize/maximize/close, resizable frame) with File/Edit/View menus and shared Editor styling. Do not present a full asset workspace as a restricted dialog. Menu and toolbar entries reuse the same actions and shortcuts; document operations retain their existing typed bridge command path. Window presentation controls are UI-only. Main-window shutdown and asset-window reuse must include these windows and preserve unsaved-document prompts.

- The Electron Inspector presents components in a single list of collapsible sections, without component overview filters or their search field. Each component header and all of its fields share one bordered card so property ownership remains visible. Property refresh must retain the Inspector scroll position; never scroll to a component as a side effect of editing a field.

- The main mesh Inspector presents the mesh asset, actual material slots and object physics as separate sections. Material assignment is direct per slot, with explicit inherited-material reset and a single assign-all action; do not expose raw slot collection editing as the primary workflow. Collision participation and dynamic simulation are separate controls. Disabling a physics body preserves its authored settings. Material and physics changes use shared EditorCore commands and the typed `mesh_material_assign` / `mesh_physics_set` tools. Undo/Redo resolves restored scene components by persistent ID, never by a stale runtime identity.

- Inspector components exposing the reflected `material_slots: engine.MaterialSlots` property reuse NativeMeshInspector, including CharacterComponent. Hide raw material slot/global assignment fields for these components. Use the existing mesh.inspect/mesh.material command path and shared asset thumbnails; do not introduce a separate Character material mutation path.

- Every editor window, asset editor, detached workspace and settings dialog must use the shared Engine/Editor design system: palette tokens, backgrounds, typography, controls, spacing and interaction states. New windows must explicitly participate in the shared CSS tokens and reusable style roles. Do not leave native light surfaces or default unstyled controls inside the themed editor. Verify the actual native window, including scroll contents and detached presentation, using the project screenshot helpers or automated native tests before completing UI work.

- Every new editor capability must consider its MCP integration in the same change: register a typed command in the native bridge contract, or expose it through the existing reflected component / published action catalogs. Document explicit exclusions. Electron and native commands share this registry and validation; do not implement separate mutation paths.
- Scene mutations use `EditorCommandStack`, persistent object/component IDs and existing reflection property validation. Reject edit-world mutation during Play. Save is explicit. Never expose unrestricted shell execution through the console or MCP.
- Project settings persist in the existing `.project` descriptor, preserving unknown fields and validating startup paths. Startup changes apply at next project open. Editor preferences use per-user JSON preferences; the console reads bounded, thread-safe `PrintString` history.

## Engine vs Project vs Editor

Architecture follows an Unreal-like split: Engine and Editor are built and staged once; user Projects live outside the engine tree and are opened at runtime.

```text
Installation / Stage/
├── Bin/
│   └── SakuraElectronHost.exe
└── Engine/
    ├── Content/     — engine assets (/Engine/…)
    ├── Shaders/
    └── Config/

User project (outside engine sources):
MyGame/
├── MyGame.project
├── Content/         — project assets (/Game/…)
│   └── Scripts/     — Python gameplay source assets
└── Config/
```

Rules:

- **Engine** (logical EngineRuntime, CMake target `Engine`) does not know a concrete Project, does not embed user Content/Scripts, and does not depend on Editor. Engine has no UI toolkit dependency.
- **EditorCore** (CMake `EditorCore`) depends on Engine + ufbx — import tools, `SceneDocument`, `EditorCommandStack` / scene edit commands, abstract reflected `editor.EditorAction`. Its operations require no application UI. It is built independently of SAKURA_BUILD_EDITOR.
- **Editor** uses React workspaces in Electron, `SakuraElectronHost`, EditorCore and ImGuizmo. The native bridge owns validation and command dispatch on the Game thread.
- **Project** is not a CMake target. No `add_subdirectory(Project)`. Descriptor is `*.project` JSON (`name`, `engineVersion`, `startupScene`, optional `startupStory`).
- Paths: `EnginePaths` (install root from executable / `Bin` parent) and `ProjectPaths` (open project root). Never resolve user assets via `../../../Content` from the exe.
- Open flow: `ProjectSession::OpenProject` → `ProjectDescriptor` + `ProjectPaths` → `Engine::LoadProjectContent` (returns `AssetDiagnostic`; Game Content + Scripts + rescan) → load `startupScene` via `SceneSerializer` into `Engine::AdoptScene`. Health: `Ready` / `Degraded` (scan or startupScene issues) / `Failed`.
- Scene document format `sakura.scene` v1 (`SceneSerializer`): stable object/component string IDs, parent refs, transform/flags, reflected component `type`/`typeVersion`/`properties`. Runtime `ObjectHandle` and GPU handles are not persisted. Atomic write: temp file + verify + rename. Subtree helpers: `SerializeSubtreeToJson` / `DeserializeSubtreeFromJson` (delete undo), `SerializeComponentToJson` / `DeserializeComponentFromJson`.
- Story document format `sakura.story` v1 (`StoryDocumentIO` under `Root/Engine/inc/Story/`): nodes with kinds `Line`, `Choice`, `Action`, `End`; action kinds `Wait`, `MoveTo`, `CameraCut`. **`StoryRuntime`** (Engine, UI-independent): `LoadFromFile` / `Reset` / `AdvanceDialogue` / `SelectChoice` / `Tick` for timed blocking actions; `Stop` cancels running actions. `MoveTo` resolves a persistent object ID and lerps its local transform; `CameraCut` blends the primary `CameraComponent` owner transform (optional copy from an object resolved by persistent ID). Sample: `Samples/SampleProject/Content/Stories/Demo.story`. Headless test: `SakuraStoryRuntimeTest` (`Engine.Story.Runtime`). Editor playtest: `PlaySession` PlayWorld + dock tab «Сценарий» driven by `StoryRuntime`.
- EditorCore `SceneDocument`: path, dirty flag, `AdoptScene` / Open/Save/SaveAs/Close; failed Open keeps the previous document. Saving a scene does not rewrite `.project`. Engine owns the main scene via unique_ptr; SceneDocument stores a generation-checked non-owning identity. Project close stops play and releases the Engine-owned scene; document/session close order is safe.
- EditorCore commands (`EditorCommandStack` + `Make*Command`) own rename, transform, create/delete, reparent and component/property changes. Hierarchy and Inspector mutations use the stack; selection uses generation-checked object identities. Reject parent cycles and retain persistent identities through Undo/Redo.
- `LightComponent` is reflected (`engine.LightComponent`) so factory / inspector / scene IO work; `light_type` is the reflected `engine.RenderLightType` enum (Directional, Point, Spot). Scene JSON writes enum names and still accepts legacy numeric values 0/1/2. Inspectors derive dropdown options from enum metadata, not hardcoded property names. Light creation APIs take `RenderLightType` directly; templates, generators and shipped sample scenes write named enum values. Numeric values are reserved for legacy input compatibility and GPU encoding.
- Closed-choice component properties use reflected enum types end-to-end, including creation commands and runtime consumers: physics motion/shape, navigation generation mode and navigation agent state. Inspectors use enum metadata and localized labels; scene writers and sample generators use names while legacy numeric scene input remains readable. Collision masks, navigation area IDs and open profile identifiers are not enums. Static-mesh collision asset records retain their numeric shape schema, converted explicitly at the component boundary.
- Create flow: `ProjectGenerator::CreateProject` (folders + `.project` + real empty `startupScene` file). `ProjectName` must be a single path segment inside the parent directory; `startupScene` must stay inside the project root.
- Each project and isolated asset workspace owns a native process. Do not treat Python `sys.path` changes as isolation; project Python is trusted code, not a sandbox.
- Editor shell UI migrates toward the current `engine-web-prototype/frontend/src/Editor.jsx`: menubar, play toolbar, **left Hierarchy**, center viewport + bottom dock tabs, **right Inspector**, status. The user's exact-frontend migration request supersedes the earlier opposite placement. Editor-only UI presentation/adapters live under `Root/Editor/inc/src/UI`; pure styles and frontend palette tokens live under `Root/Editor/resources/ui/styles`. Current shell groundwork does not establish full visual parity. Forward render live stats (`Engine::GetRenderStatistics`) and postprocess knobs (Exposure / IBL / Bloom / FXAA via `RenderSettings`) live in bottom dock tabs «Настройки статистики» and «Настройки постобработки», not under the viewport.
- Searchable action palettes reuse `StudioActionMenu` + `StudioMenuItem`; row layout/painting lives in `ActionMenuStyle`, surface/search/group styles in `controls.qss`. Share domain catalogs across entry points (`BuildStoryNodeMenu`, `BuildStoryEventMenu`) instead of copying action lists into views. Callbacks use existing document commands, copied IDs/positions and lifetime guards. Native story types not supported by the document/runtime remain disabled with explanations. Audit and extension rules: `docs/UI-COMPONENTS-AUDIT.md`.
- Sample: `Samples/SampleProject/` (not part of Engine Content).
- Source packer (no build / Diligent / PhysX / Electron): `Scripts/PackEngineSourceApp/PackEngineSourceApp.py` or `PackEngineSourceApp.exe` → RAR under `Scripts/PackEngineSourceApp/Output/` (needs WinRAR `Rar.exe`). Rebuild exe: `python -m PyInstaller --onefile --console --name PackEngineSourceApp Scripts/PackEngineSourceApp/PackEngineSourceApp.py`.
- CMake staging: `sakura_stage_engine_runtime(<target>)` → `${CMAKE_BINARY_DIR}/Stage/{Bin,Engine/...}`.
- Windows authoring distribution uses electron-builder. Game export produces `Game.exe` plus `resources/app`, `resources/native` and `resources/Game`; explicit Player `--project` remains supported. Debug builds are for development; distribute Release builds. See `docs/DISTRIBUTION.md`.
- Engine assets are also deployed to `<TargetExecutableDirectory>/Engine/{Content,Shaders,Config}` for direct build-directory launches. Asset deployment runs on every target build, including builds without relinking; Content includes builtin Material/HLSL files and their metadata. Creation integration tests resolve EnginePaths from the executable for both direct and staged layouts, without substituting source-tree Engine Content or shader paths.

### Runtime hosting and scene ownership

- Electron owns application windows and React UI; the native host owns the Game thread, child HWND and Win32 loop. Engine remains the only frame producer. GPU shared textures are presented by the Electron preload.
- `Engine::AdoptScene(unique_ptr<Scene>)` transfers main-world ownership and stops any current simulation. `PlaySession` exclusively owns its cloned PlayWorld; the main world remains in Engine. `GetActiveScene` selects the simulated world without transferring ownership.
- `Engine::StartGame` runs the main world in Player; `PlaySession::StartPlay` runs a copy in Editor. Both use `Engine::TickWorld` for scene components and StoryRuntime. Pausing freezes both; Step advances both once.
- Engine is the only frame producer. EndFrame submits one immutable RenderFrameData containing all active RenderViewFrame snapshots. Editor uses one native viewport: Edit mode renders EditWorld through the editor camera; Play switches that surface and Hierarchy to PlayWorld and the primary game camera. Frame indices increase monotonically. The renderer still supports multiple surfaces for tests and future tools.
- Register/configure/resize/unregister surfaces through Engine. Surface 1 initializes the shared device; additional native swapchains require D3D12 or Vulkan (OpenGL remains a single-surface fallback). Native-surface destruction synchronously detaches and waits for GPU idle before returning to the native host; resize is enqueued. Zero-sized/hidden surfaces suspend. StopPresenting joins Render Thread before host destruction.
- `Object::PersistentId` survives serialization, deletion undo, reorder and rename. Runtime ObjectHandle is never persisted. New IDs use GUIDs; loaded scene IDs are preserved.
- Story actions persist `targetObjectId` and retain generation-checked identities while running. Legacy `targetObjectName` is resolved once on load and must be unique. A destroyed target fails the action without dereferencing stale memory.
- Startup story is an optional project-relative `startupStory` path restricted to the project root. Engine owns StoryRuntime; UI hosts do not hardcode sample content.
- Diligent implementation and backend headers are private under `Root/Engine/src/Rendering/RHI`; only that directory invokes the Diligent API. Public gameplay/editor headers expose scene data and resource identities.

### Prefabs

- Prefab implementation is integrated from `origin/prefabs` (`28d9444`), adapted to the current module layout. Engine owns `PrefabLibrary`, graph expansion, variants, nested instances, overrides and scene materialization. EditorCore owns prefab commands; Editor owns the prefab window and override panel.
- `.prefab` is a registered Content asset (`sakura.prefab` v1). Scenes serialize linked instances with asset identity and modifications. Missing assets retain their original records. Runtime identities never enter documents; instance object/component IDs derive from stable source IDs. Reflected object references must resolve inside the materialized instance after all objects and components exist.
- Prefab editor panel order is Inspector on the left, viewport in the center, Hierarchy on the right (explicit user correction). Preserve this arrangement; do not change it to match the main editor shell.
- Double-click opens an isolated prefab scene with Hierarchy, viewport, Inspector and its own undo history. The scene Hierarchy offers creation from selection; dragging a prefab into the viewport instantiates it through the command stack. Instance controls expose overrides, apply/revert and unpack. Applying overrides writes the shared asset explicitly and clears the affected local undo history.
- Preview render surfaces hold generation-checked scene identities. Engine remains the only frame producer; previews resolve assets through the shared resolver and detach their render surface before scene destruction. Closing the main window must honor unsaved prefab editors.
- The native bridge exposes typed `prefab.create`, `prefab.instantiate`, `prefab.inspect` and `prefab.modify` commands through the native bridge, using the same EditorCore commands and Engine utility as the UI. Mutations reject Play. Selective per-property apply/revert is available through override indices; scene-object/component edits remain available through the ordinary tools.
- Prefab files are editable local assets. Dependency-complete authoring-package export/import is not implemented by this integration; the A → B → C package acceptance must not be claimed from prefab round-trip tests alone.

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

- The four `builtin-quest/shooter/novel/horror` templates are ordinary authored projects. Installation seeds live under `Engine/Config/ProjectTemplates`, outside asset mounts, and are copied into the new project's Content. Gameplay rules live in the copied `Content/Scripts/mechanics.py`; `template_game.py` connects those rules to scene identities, Enhanced Input and Python Narrative. Engine and Player must never branch on these template IDs to simulate gameplay. Existing projects are not upgraded implicitly.
- Published script methods `GetGameView() -> str` and `ExecuteGameAction(Action: str)` provide the shared runtime HUD contract. The view is a JSON object bounded to 64 KiB; native `game.action` accepts only currently published, enabled actions. Paused execution additionally requires the published action's explicit `allowWhilePaused` flag (save/load/restart use this). React displays this state without owning rules. `pointerLock` requests relative mouse capture after a user gesture, released on pause/stop. The same contract is available in Editor Play and Player.
- Web template parity uses the shipped `frontend/public/game-templates/*.sacura-template.json` packages as the authored-data oracle and executes the original component methods for behavior traces. `Scripts/ExportWebTemplateOracle.mjs`, `ExportWebTemplateGeometry.mjs` and `ExportWebMechanicTraces.mjs` produce reference fixtures; `GenerateGameplayTemplates.py` authors native scenes, source widgets, input mappings and project-owned meshes. Preserve the source characters, dimensions, story text, rewards and puzzle conditions; do not replace them with approximate genre presets. A passing startup smoke test alone does not establish parity.
- Python `RequestStoryNode` queues a validated transition for the next StoryRuntime tick so component callbacks cannot close a currently executing Narrative generator reentrantly. Transitions cancel running work while keeping the Narrative session usable and its variables available. `GetStoryNode` returns the active document node identity. Template saves store explicit JSON state under project Saved and re-enter the saved node; they do not serialize Python executable objects or runtime scene identities.
- Input actions default to `bClampMagnitude=true`. Mouse delta and pointer-position actions must explicitly disable this flag to retain pixel deltas and aspect-correct coordinates; gamepad/movement axes keep the default. The flag persists through the shared InputDocument and reflected asset factory.
- Verify template rules with `Root/Verification/TemplateMechanicsTest.py`, generated projects with `desktop-electron/tests/template-gameplay.test.cjs`, and GPU/React Player presentation with `desktop-electron/scripts/template-players-check.cjs`. These CLI checks create disposable projects and reports under `build` on the repository drive. Do not use Computer Use.

- The Python asset creation dialog defaults to Game/Scripts when invoked outside that tree, preserves a selected Scripts subfolder, and displays the actual destination before submission. Its module-name validation must match the native generator; never offer a destination rejected by the Content/Scripts constraint.

- Only Content/Scripts is supported for project Python sources. Do not add migration or fallback for legacy project-root Scripts; there are no legacy projects to support. Repository maintenance Scripts/ is separate.

- Python source files are ordinary UTF-8 `.py` assets (`PythonScript`) with stable metadata under `Content/Scripts`. Creation uses `editor.CreatePythonScriptAssetFactory`; generated type IDs are written into source and remain unchanged when a file is renamed. Asset scanning does not execute code; imports and type registration happen at project open.
- Python source names are package-qualified and exempt from global asset leaf-name uniqueness, allowing separate `__init__.py` and helper modules. Other asset types retain their existing naming rules. Module moves do not rewrite Python imports.
- Double-click opens a regular Electron asset workspace tab with Python highlighting, line numbers, search, indentation, Save and Undo/Redo. The workspace uses EditorCore `PythonScriptDocument`, atomic writes and external-edit conflict detection. The typed `assetDocument.execute` bridge and `python_document` MCP/console tool edit drafts without executing them. Mutations reject Play; dirty documents participate in close prompts.
- The source editor does not imply hot reload, arbitrary editor-script execution, pip management, a debugger or a language server. Reopen the project in a new process to apply Python changes.
- Windows builds default to official CPython 3.12.10 x64, downloaded with a pinned SHA-256 by `ThirdParty/Python.cmake`; no system Python installation is required. Editor, Electron host and Player deploy its DLLs and standard library with isolated `._pth` paths. Game export carries the runtime. Python is shared with the engine installation, not duplicated inside authored projects. Explicit `SAKURA_PYTHON_ROOT` overrides require compatible Python 3.12.

Scripts live only under `<ProjectRoot>/Content/Scripts/`. After project open, that directory is added to `sys.path`; packages resolve as `import gameplay.player` → `Content/Scripts/gameplay/player.py`. Engine owns the interpreter; it does not hardcode project module names.

### Python Narrative nodes

- Python Narrative is part of StoryRuntime and the existing Story graph, not a separate mechanics workspace. Project scripts declare `@register_node(type_id=..., pure=...)` classes derived from `engine.NarrativeNode`; no component lifecycle callbacks are invoked for these classes.
- `Inputs` and `Outputs` contain typed `pin` declarations. Pure `Evaluate` returns a dictionary; action `Execute` returns `NarrativeResult` with a declared `Exits` branch. Actions can yield `Context.Wait(seconds)`; StoryRuntime advances them on Game Thread and closes generators on Stop/Reset/failure. Python coroutines are not accepted by this scheduler.
- Pure dependencies evaluate once per action invocation. Data cycles and incompatible pin types are rejected; outputs of actions are readable only after execution. Context variables belong to a story run and reset with it. The pure context disallows effects, but trusted project Python is not a sandbox.
- Published component methods appear as generated Narrative action nodes and can also be called through `Context.Invoke`. Persistent component references resolve against the bound PlayWorld. Calls use the existing published method validation and active script-instance contract.
- Story `Narrative` nodes persist `narrativeType`, literal `inputs`, `sources` (input to `[nodeId, outputName]`) and named exits as `choices` with stable exit IDs. Missing script types remain editable/preservable but prevent playback. Graph edits use StoryGraphDocument history. UI derives fields and pins from the same Narrative catalog as the runtime.

- Narrative authoring MCP/console commands (`narrative_*`) use the active `StoryGraphController` document, shared command history and Play lock. Typed literal changes use `StoryGraphDocument::SetNarrativeInput`; replacing a data wire with a literal is one undoable edit. Register the tools after constructing the story controller. Document opening/creation and source editing remain UI-only; coverage is in `Editor.Narrative`.

## Rendering architecture

- Viewport component icons, camera outlines and arrows belong to the viewport's immutable ImGui render snapshot, alongside the transform gizmo. Do not display them in a separate tool window. Project their positions from the same editor camera used for that frame so scene presentation and editor markers share GPU submission and window clipping. Icon selection remains keyed to scene identities; image resources are cached on the Game thread.

- Graphics abstraction: **Diligent Engine Core** (not raw D3D12/Vulkan/OpenGL, not SDL_GPU).
- Application windows / runtime UI: **React + Electron**. The native host owns a Win32 message loop; SDL3 remains for low-level facilities.
- Our code owns `Rendering` (Renderer, RenderScene, SceneExtractor, passes, materials). Gameplay never includes Diligent headers or native GPU API types (`VkInstance`, `ID3D12Device`, etc.).
- Only `Rendering/RHI` may talk to Diligent. Editor ImGuizmo uses DiligentTools' bundled ImGui on the Game thread; GPU overlay submit goes through RHI `ImGuiDiligentRenderer` (never from the UI host).
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
- Native runtime child windows are owned by the Electron host; the legacy SDL host uses `WindowSubsystem`. Rendering receives `NativeWindowInfo` only.
- Resize path: Native host resize (or legacy SDL event) → Game Thread → Render Command → `Renderer::Resize` → SwapChain.
- Live resize must not depend on the host's next timer tick: `Engine::ResizeRenderSurface` publishes a fresh render snapshot without advancing simulation; a resize inside an active frame uses its upcoming `EndFrame`. The pinned Diligent D3D swapchain receives the reproducible `PatchDiligentWindowResize.cmake` patch so the previous presentation stretches over the HWND until the new-size frame arrives, instead of exposing a background strip. Keep native API details inside RHI/dependency integration.
- During continuous resizing, `RenderViewportWidget::IsInteractiveResize` stays active until 120 ms after the last size change. A D3D12 swapchain verified to use stretch keeps its buffers and continues rendering with the current camera aspect; the final size is allocated on the settle notification. Other backends coalesce resize requests before drawing. Resize snapshots replace obsolete pending frames without blocking the UI; ordinary ticks retain bounded backpressure. ImGui framebuffer scaling follows the actual buffer extent. Check continuous dragging, not only isolated resize-and-wait steps (`SakuraEditorRenderSmokeTest --resize-regression`).
- Forward rendering: GGX metallic/roughness PBR, directional/point/spot lights; inverse-transpose normal transforms. Material schemaVersion 2 supports independent Lit/Unlit/Toon shading and Opaque/Masked/Translucent/Additive blending; legacy schemaVersion 1 remains readable. No GPU skinning.
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
- The Electron native host (or the legacy SDL `WindowSubsystem`) owns native windows on Game Thread; resize is forwarded as a Render Command.
- GPU resources (`GpuBuffer`, `RenderMesh`, pipelines, shaders) live only in Rendering; Game code uses `MeshHandle` / `MaterialHandle`.

Lifecycle: Game Thread initializes JobSystem → starts Render Thread → waits READY → loop. Shutdown: stop frames → join Render Thread → JobSystem shutdown → subsystems → (later) destroy SDL after GPU teardown.

Use `AssertGameThread()` / `AssertRenderThread()` / `AssertWorkerThread()` on thread-sensitive APIs. Logging via `PrintString` (English).

## Physics and navigation ownership

- Gameplay talks to `PhysicsWorld` and SDK-neutral `IPhysicsWorld` / `PhysicsTypes`. Only `Root/Engine/src/Physics/Backends` invokes PhysX. `PhysXWorld` implements `IPhysicsWorld`; replacing a backend must not change scene components or script calls. Native SDK objects never cross the interface or enter scene documents.
- Each Scene owns its physics world. EditWorld does not simulate; PlayWorld has independent actors, contacts, controllers and crowd state. The process shares the PhysX foundation/physics lifetime only. Public world operations and queries run on Game Thread; simulation completes before publishing poses or contact batches.
- Physics uses metres, kilograms, seconds and Y-up. Fixed simulation is 60 Hz, at most eight substeps per frame. Paused Step advances one fixed physics step. `AddForce` / `AddTorque` are frame forces applied to all substeps of that frame; impulses are one-shot. CharacterControllerComponent owns an upright capsule whose object origin is at its feet; capsule half height is half the cylinder length and Center must be zero.
- PhysicsBodyComponent supports box, sphere, capsule and static/kinematic cooked triangle collision from CPU StaticMesh data. Triggers, simulation collision and scene queries have separate flags; mutual layer/mask filtering applies to simulation and character movement. Movable bodies publish world poses through parent conversion and preserve authored local scale. Story MoveTo/CameraCut retains exclusive transform control; dynamic bodies temporarily become kinematic and navigation movement cancels.
- `NavigationWorld` owns per-profile immutable Recast/Detour meshes and transient DetourCrowd state. Native Recast/Detour headers stay under `src/Navigation/Recast`. The pinned SDK is fetched once into `Root/Engine/ThirdParty/navigation/_src`; its ChunkyTriMesh helper include directory must remain private because its demo `imgui.h` conflicts with Diligent's bundled ImGui.
- Bounds define a union of build regions, with identical settings for volumes in the same profile. Geometry comes from enabled solid physics colliders marked AffectsNavigation, excluding characters and triggers. Area 0 is null, area 1 is default, 2–16 are custom; query masks use bit `(Area - 1)` and area costs are at least 1. Modifier priority controls overlapping volumes. Link IDs derive from persistent component IDs; link traversal requires explicit gameplay completion at the directional endpoint.
- Static keeps the baked mesh during gameplay; Dynamic tracks geometry/modifiers/links; DynamicModifiersOnly keeps its initial collision geometry and tracks modifiers/links. Editor auto-build is separate from initial Play preparation. Builds capture owned CPU snapshots before scheduling JobSystem work; workers never read Scene/GameObject/Component. Scene closure cancels safely. Publication, generation changes and route invalidation happen on Game Thread.
- Local edits rebuild dirty tiles into a new mesh while reusing unchanged tile data. Geometry topology, settings or bounds changes require full rebuild. Published meshes are immutable; dynamic in-flight builds may finish an earlier valid snapshot and then build the latest state so continuously moving geometry cannot starve publication. A valid previous dynamic mesh remains queryable during rebuilding.
- Scene JSON optionally stores gravity and checksummed baked tiles (`sakura.navigation` v1). Tile data is validated before Detour receives it; incompatible SDK/poly-reference formats are rejected. CPU nav data can be shared by EditWorld/PlayWorld, but runtime body IDs, crowds and movement requests are never persisted. Reflection JSON and Inspector enumerate inherited fields so CharacterController properties survive editing and cloning.
- NavigationAgentComponent follows a generation-tagged route through a physical character controller. DetourCrowd supplies avoidance velocity; PhysX remains authoritative for actual position. Off-mesh traversal enters WaitingForLink and never teleports implicitly. Story `NavigateTo` owns a request, waits for physical arrival and propagates cancellation, failed/partial routes and deleted targets.
- Editor physics/navigation creation and component edits use existing command history. Overlays go through immutable ImGui draw snapshots and the existing rendering path. Physical P toggles navmesh independent of keyboard layout. Sample and usage guide: `Samples/PhysicsNavigation/README.md`. Local coverage: `Engine.Physics.Navigation`, `Editor.Physics.Navigation`.

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
| ufbx | FBX Import source data | FetchContent → `Root/Editor/ThirdParty/ufbx/_src/`; target `Sakura::Ufbx` via `Sakura::EditorCoreThirdParty` |
| ImGuizmo | Editor transform manipulator | FetchContent → `Root/Editor/ThirdParty/ImGuizmo/`; target `Sakura::ImGuizmo`, using DiligentTools' bundled ImGui |

Fetched Diligent/PhysX/asset/`_src`, Editor ImGuizmo trees are gitignored. Do not copy their `.cpp`/`.h` around the project by hand.

- Dependencies use pinned sources built through CMake. No UI toolkit SDK is required; Electron dependencies are installed through npm.
- On Windows, the root CMake configuration enables `core.longpaths=true` through process-scoped `GIT_CONFIG_COUNT` / `GIT_CONFIG_KEY_*` / `GIT_CONFIG_VALUE_*`, preserving inherited entries. All child Git dependency downloads and submodule checkouts inherit it. Do not change global Git configuration or Windows registry settings for dependency downloads. This handles Git paths, not arbitrary path limits in other build tools.

Pinned version cache vars: `SAKURA_SDL3_GIT_TAG`, `SAKURA_DILIGENT_*_GIT_TAG`, `SAKURA_PHYSX_GIT_TAG`, `SAKURA_FASTGLTF_GIT_TAG`, `SAKURA_OZZ_GIT_TAG`, `SAKURA_NLOHMANN_JSON_GIT_TAG`, `SAKURA_STB_GIT_TAG`, `SAKURA_UFBX_GIT_TAG`, `SAKURA_IMGUIZMO_GIT_TAG`, `SAKURA_PYBIND11_GIT_TAG`. Bump deliberately (keep Diligent modules on the same version).

- Engine links `Sakura::EngineThirdParty` (SDL3, Diligent backends, PhysX, Tools/FX helpers, `Sakura::AssetThirdParty`). Do **not** add a CMake target named `imgui` — DiligentTools must keep its bundled ImGui for `Diligent-Imgui` only.
- EditorCore links `Sakura::EditorCoreThirdParty` (ufbx) plus Engine; no UI toolkit requirement.
- Executables that need Diligent backend DLLs: `sakura_copy_engine_runtime_dlls(<target>)`.
- `sakura_copy_engine_runtime_dlls(<target>)` deploys native runtime dependencies. Electron packaging excludes obsolete Qt DLLs and plugins from reused build directories.
- Include usage: `#include <fastgltf/core.hpp>`, `#include "ozz/animation/runtime/skeleton.h"`, `#include "stb_image.h"`, `#include <nlohmann/json.hpp>`, `#include "ufbx.h"`.

## Asset System (Blocks 1–6)

```text
Content + .meta → AssetRegistry → AssetManager (LoadAsync / PumpCompletions)
  → CPU resources (Model/StaticMesh/Texture/Material/Skeletal*)
  → AssetGpuUploader → RenderThread → RenderResourceManager (GPU mesh/texture)
```

- `AssetRegistry` owns Content scan / GUID↔path / subassets; Game Thread.
- Authored documents are first-class assets: `SceneAssetType` for `*.scene` and `StoryAssetType` for `*.story`. They use ordinary Content files plus `.meta`, participate in move/rename/delete and are opened by registered type-specific editor handlers.
- `AssetManager` dedupes by `AssetKey`+generation; JobSystem workers do I/O; publish only in `PumpCompletions` on Game Thread. No Diligent dependency.
- Loaders: `ModelLoader` (self-contained GLB only), `TextureLoader` (stb RGBA8), `MaterialLoader` (JSON schemaVersion 1 legacy adapter / 2), `MaterialInstanceLoader` (schemaVersion 1), `SkeletalLoader` (GLB→ozz offline builders; LINEAR only; max 4 influences).
- Shared GLB parse cache lives in `ModelLoader` for Model/StaticMesh/Skeletal subassets of the same file.
- `Engine::InitializeHeadless(GameContentRoot)` — Game thread + Memory + Jobs + Registry.Scan (Engine+Game mounts) + AssetManager; no window/render. Project scripts: sibling `Content/Scripts/` or explicit `SetScriptsRoot`.
- Full `Initialize` also creates Assets after Jobs; `Tick` calls `Assets.PumpCompletions()`; `Shutdown` calls `Assets.Shutdown()` before `Render.Stop()`.
- Editor `AssetTools` (EditorCore): `AssetImporter` dispatches by extension; staging outside Content; path conflict → `ImportConflict` (no overwrite). FBX→GLB via ufbx + fastgltf `Exporter` (static geometry only).
- Headless verification target: `SakuraAssetTest` (`Root/Tests/AssetSystem_Test.cpp`) links Engine + EditorCore (no UI application needed).
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
| Load Material | Surface / PostProcess / UserInterface, typed parameters, inherited instances, Standard PBR maps, legacy schemaVersion 1 | Shader Graph, deferred, decals, compute shaders, GPU skinning |
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
  → React Inspector (typed native bridge via PropertyAccess)
```

- **TypeId** / **PropertyId** are stable strings (`"engine.CameraComponent"`, `"field_of_view"`), not RTTI hashes or registration indices.
- Author-facing declarations do not take a schema version: `ENGINE_CLASS(ClassName, BaseClassName, TypeId)`, root/abstract variants, `ENGINE_STRUCT(StructName, TypeId)`, `ENGINE_REFLECT_ENUM(EnumType, TypeId, ...)`, and Python `@register_class(type_id=...)`. Internal registration uses version 1 for compatibility with existing reflection JSON. Document format versions and migration checks remain separate; this does not introduce automatic field migration.
- Authoring is in headers next to members via `Reflection/ReflectionMacros.h`. Nested static registrars only store string literals + binder function pointers (`noexcept`, no heap). Binder bodies run in `InitializeNative` when the class is complete.
- After the class/struct closing brace in the **same header**: `ENGINE_CLASS_END` / `ENGINE_STRUCT_END`, then `ENGINE_IMPLEMENT_FIELD` / `ENGINE_IMPLEMENT_PROPERTY` / `ENGINE_IMPLEMENT_READONLY` (MSVC-safe member-pointer bind).
- `RF_DISPLAY_NAME`, `RF_UI_RANGE`, `RF_CATEGORY` feed `PropertyAttributes` alongside `PropertyFlags`.
- `Class` is metadata (`Object` subclass), **not** a scene object. Instances get `Object::AssignClass` / `GetClass` / `GetTypeId` from the factory.
- Native property value types and incomplete type families are inventoried below. A catalog entry, C++ wrapper or JSON conversion branch alone does not imply complete reflected-property support.
- No on-disk type / schema cache in v1.
- MSVC static registration can be stripped from static `Engine.lib` if unused: link tests/tools with `/WHOLEARCHIVE:Engine` (see `Root/CMakeLists.txt` for `SakuraReflectionTest`, `SakuraReflectionPythonTest`, `SakuraAssetTest`, `SakuraTest`). `ReflectionAnchor.cpp` includes reflected headers and `ForceTouchRegistrars()`.
- Headless verification: `SakuraReflectionTest` (native) and `SakuraReflectionPythonTest` (JSON + Python carrier + inspector list API).
- Engine hooks: `InitializeCommon` calls `InitializeNative` after Jobs, then `ScriptingSubsystem::Initialize` + import `ScriptsRoot` modules; `Shutdown` destroys script instances → finalize interpreter → Reflection shutdown (after Assets, before Render/Jobs). CDOs still need `MemorySubsystem`.

### Reflected property types

The agreed reflected property categories are:

1. `bool`.
2. `int` / `int64_t` (the existing shared `engine.int64` reflection type).
3. `float`.
4. `double`.
5. `std::string`.
6. `Vector3`.
7. `Quaternion`.
8. `EulerAngles` (Euler angles, expressed in degrees for authoring).
9. `Struct` (generic reflected structures, including `Color`, `LightSettings`, `MaterialSlotOverrides` and `MaterialPostProcessEffects`).
10. `ObjectHandle`.
11. `ClassRef<T>`.
12. `AssetRef<T>`.
13. `TypeId`.
14. `Enum`.

`Struct` is a generic property category with per-structure stable TypeIds and reflected fields. Color and the two material collections must use that common mechanism instead of separate property categories and handwritten per-structure codecs. A specialized Inspector presentation, such as the Color picker, may remain an adapter over reflected struct fields. Preserve the semantic identity of each structure; sharing Struct support does not merge their TypeIds. Quaternion and EulerAngles remain distinct property types with explicit conversion between their rotation representations.

### Reflected types and implementation status

Keep this inventory synchronized with native codecs and metadata, `ReflectionJson`, `ReflectionInspector` and Python field conversion changes. Class registration and a value stored in a reflected property are separate capabilities.

- Native scalar codecs support bool, int/int64, float, double and std::string. Vector3, Quaternion and EulerAngles have distinct value identities; EulerAngles stores Pitch/Yaw/Roll in degrees and converts explicitly to/from Quaternion. Inspector edits all three rotation/vector representations. Python preserves double precision instead of narrowing through float.
- `TypeKind::Struct` and `ReflectedValue::Struct` provide generic named fields. `ENGINE_STRUCT` / `ENGINE_STRUCT_END` register structure metadata and a default factory; the primary `ReflectionValueCodec` traverses those fields. `engine.Color`, `engine.LightSettings`, `engine.MaterialSlotOverride`, `engine.MaterialSlots`, `engine.MaterialPostProcessEffect` and `engine.PostProcessEffects` use this path. Color and the material collections have no dedicated codec specializations. Color retains its picker as a presentation adapter over reflected r/g/b/a fields.
- Struct collection fields use generic `std::vector<ElementType>` support (`TypeKind::Array` and an Array carrier), including nested structures. Their element TypeId is explicit. This supplies the entries in both material collections; it does not imply support for other STL containers. Inspector supports adding/removing elements and editing nested fields; edits commit the root property through existing command callbacks. RF_COLLECTION_INDEX marks unique integer index fields; generic validation rejects duplicates and the array editor chooses an unused index when adding an element. Nested numeric ranges remain validated. Runtime dynamic-material/render-snapshot pointers are not reflected or persisted.
- Struct JSON uses named objects; arrays use JSON arrays. Existing Color `[r,g,b,a]` and material-collection array representations remain readable through legacy input adapters. New writes use the common struct representation and retain existing semantic TypeIds/property IDs. Unknown or duplicate fields, invalid enums, incompatible class references and nested range violations are rejected. Native struct conversion publishes a candidate only after successful conversion.
- `ObjectHandle` is a supported reference property with generation validation. Runtime values retain the generation-checked identity; scene JSON stores only a tagged persistent object/component ID. Scene loading and subtree restoration allocate/attach all objects and components before applying properties with an explicit scene resolver. PlayWorld cloning resolves inside the clone; subtree paste remaps internal references. Component restoration resolves its own identity before attachment. Destroyed/missing targets read as empty references; a non-empty JSON reference requires an explicit resolver. Never persist runtime IDs, generations or addresses.
- `ClassRef<BaseType>` has a stable reflected identity containing the base TypeId, TypeId string JSON and a compatible-class picker. Native and Python writes reject incompatible or missing classes. Empty class references are supported.
- `AssetRef<ResourceType>` and the underlying `AssetKey` use the common `engine.AssetRef` value representation, preserving asset and optional subasset GUIDs in JSON and Inspector. Asset picker constraints use `RF_ASSET_TYPE` / `RF_ASSET_TYPES`; resource loaders retain C++ resource-type validation. Legacy component GUID-string fields and `RF_COMPANION_PROPERTY` remain supported without being rewritten implicitly.
- `TypeId` is a native property value with a string JSON representation and a catalog picker. TypeId metadata identity remains distinct from a reference to an object instance. PropertyId remains metadata only.
- Enums register named values with `ENGINE_REFLECT_ENUM(EnumType, TypeId, ENGINE_ENUM_VALUE(EnumType, Name), ...)`. Enum descriptors are published into the catalog; generic codecs validate declared values. JSON writes names and accepts names or declared numeric values; Inspector offers named choices; Python uses names. Existing int64 enum adapters remain compatible and can migrate separately.
- Python fields support the agreed categories through primitive values, Vector3 objects, Quaternion/Euler tuples, struct dictionaries, array lists, enum names, TypeId/ClassRef strings, asset-key dictionaries and generation-checked `engine.Object` wrappers. Struct/array reads return copies; write the updated root property to commit changes. Default factories supply omitted struct fields and empty collections/references. Arbitrary unsupported TypeIds do not silently produce Empty values.
- Current carrier kinds are Empty, Bool, Int64, Float64, String, TypeId, ObjectId, Bytes, Struct, Array, ObjectReference, AssetReference and Enum. The legacy ObjectId carrier is not a scene-reference serialization format. Bytes is used only by explicit math codecs; it does not automatically expose arbitrary POD.
- Native reflected class catalogs still include Engine Object/Component and component subclasses, EditorCore's abstract EditorAction, and concrete Editor actions. Project Python classes are published at import with ScriptComponent backing. GameObject's name/transform/flags/hierarchy use dedicated scene/document code; GameObject itself is not a registered reflected Class.
- Unsupported property families remain raw pointers/references, smart pointers, distinct unsupported signed integer widths, unsigned integers, character types, long double, Vector2/Vector4/Matrix/Transform and other unregistered aggregates, alternate string/path types, non-vector containers, optional/variant/pairs/tuples, function pointers/callables/delegates/events. PropertyId, Guid/AssetId/SubAssetId/AssetType, ScriptInstanceHandle and GPU handles have no standalone property codecs; asset GUIDs are supported as part of AssetRef/AssetKey. Typedefs work when they alias a supported C++ type.

### Python carrier model (`SAKURA_ENABLE_PYTHON`)

- Input is not a component lifecycle callback. Do not add `OnInput` or broadcast input events through Scene/Component. Enhanced-style input must use explicit action/phase delegate subscriptions with explicit unbinding and cleanup on owner/session teardown. Python callbacks need not be reflected `@method` methods. The Input implementation owns this integration; removing the old broadcast path does not imply the delegate API is already implemented.

- Python scripting has two authoring contracts: ordinary object components and Narrative node classes. Do not introduce a separate mechanic-script runtime. Narrative supports direct published component-method calls and custom node classes with declared pins. The native Editor must not expose the placeholder Games Mechanics workspace; the web prototype is unchanged.
- Project Python components appear in Inspector Add Component and Create/Components from the published class catalog, and attach through `MakeAddComponentCommand`. Source stays in project Scripts and is edited externally. Python `OnCreate` / `OnUpdate` / `OnDestroy` run only during Play, with legacy `on_create` / `on_update` / `on_destroy` fallback (PascalCase takes precedence). No hot reload or coroutine scheduler is provided.
- `@method(Parameters={"Amount": float}, Result=float)` explicitly publishes a synchronous instance method. Class metadata stores named typed arguments and an optional result; `ScriptingSubsystem::InvokeMethod` validates arguments before execution and validates the result. All arguments are required; async/generator methods, variadic arguments and argument defaults are rejected. Unpublished methods are internal. `engine.Object.GetComponents(TypeName)` returns generation-checked component identities; `Invoke(MethodName, **Arguments)` dispatches against an active instance. Published methods do not execute in EditWorld. Narrative uses these descriptors for its generated call nodes. Ordinary Python attributes are temporary Play state; reflected `field()` values remain authored scene properties. Sample: `Samples/SampleProject/Content/Scripts/health.py`.

```text
Python @register_class / field()
  → ScriptingSubsystem publishes Class (Origin=Python, NativeBackingType=engine.ScriptComponent)
  → native ScriptComponent carrier holds managed PropertyId→ReflectedValue + ScriptInstanceHandle
  → BeginPlay/Tick/EndPlay → Python OnCreate/OnUpdate/OnDestroy (legacy snake_case fallback, no user __init__)
```

- CMake option `SAKURA_ENABLE_PYTHON` (default ON). If Python3 `Development.Embed` is missing, configure falls back to OFF with a warning. pybind11 (pinned `SAKURA_PYBIND11_GIT_TAG`, default `v2.13.6`) lives in `Root/Engine/ThirdParty/pybind11/_src` (gitignored). Hint install root with `SAKURA_PYTHON_ROOT` / `-DSAKURA_PYTHON_ROOT=...`. Runtime needs `python310.dll` on `PATH` (or next to the exe) and a valid `PYTHONHOME`.
- When ON: Engine links `pybind11::embed` + `Python3::Python`, defines `SAKURA_ENABLE_PYTHON=1`. Reflection core headers stay free of `py::object`.
- Embedded module name: `engine` (`ScriptComponent` façade, `register_class`, `field`, `Object` handle with generation, Vector3/Transform copy-in/copy-out).
- Reflected Python `field()` maps builtin bool/int/float/str to engine.bool/engine.int64/engine.float/engine.string and accepts supported TypeId strings for the other categories. Native metadata, validation and the generic struct/array conversion define support; direct Object/Transform/material/physics/navigation bindings are separate APIs.
- Published Python types are kept in an internal TypeId→`py::object` map at register time. `BindScriptComponent` must not scan `sys.modules`. Resolve lifecycle callbacks via `getattr` on the **type**, then bind to the instance — avoid `hasattr`/`attr` on the instance for `on_*` (deadlocks with field descriptors under embed).
- Project scripts path: `<ProjectRoot>/Content/Scripts` via `ProjectSession` / `Engine::SetScriptsRoot`. Sample: `Samples/SampleProject/Content/Scripts/door_controller.py` (`game.DoorController`).
- Exception in `on_update` → mark ScriptFailed, stop further updates; `on_destroy` at most once.
- **No hot reload** in v1: import at project open / headless init when ScriptsRoot is set.
- Build without Python: `-DSAKURA_ENABLE_PYTHON=OFF` — native reflection tests still pass; Python cases are skipped in `SakuraReflectionPythonTest`.

### JSON + Inspector

- `Reflection/Json/ReflectionJson` uses `{ "type", "typeVersion", "properties" }` with generic nested struct/array values and explicit reference representations described above. Type/version mismatches, unknown fields and value validation failures are rejected before property writes. Optional per-type migration hooks remain available; arbitrary native setter side effects are not transactional.
- React Inspector edits reflected root/nested properties through validated native commands and the shared scene history. Specialized presentation does not add native codec categories.
- Inspector refresh preserves component disclosure and scroll state, reconciles persistent component identities and never retains stale scene objects after Undo/Redo.
- Story canvas navigation is a local editor preference, keyed by canonical project descriptor path and stable story asset identity in per-user preferences. Persist logical scene origin and zoom after explicit navigation, debounce disk writes and flush pending state on story switches/destruction. Restore through show/resize after dock layout is applied; hidden/layout-only events must not overwrite the saved view. Invalid settings fall back to the initial view. Do not mark the story dirty or add navigation to runtime/document commands.
- Asset properties keep stable GUID and subasset IDs in serialized scene data, while editor UI resolves them through `AssetRegistry` and displays virtual paths. Reflected asset fields use `RF_ASSET_TYPE`; `RF_COMPANION_PROPERTY` groups a subasset ID with its owning asset; `RF_EDITOR_HIDDEN` keeps implementation fields out of the Inspector. Users must never type asset GUIDs in the Inspector.
- `ContentBrowserWidget` adapts the native registry/operations to the frontend «Проводник» layout through `AssetExplorerView` and `AssetExplorerTileDelegate`: Assets (`/Game`) and read-only Engine folder trees, breadcrumb navigation, search, draggable asset tiles, selection details and import controls. Registered-type filters live in the search field menu; no filter shows direct folder contents, search and active type filters include descendants. Folder navigation clears search and synchronizes the tree. `AssetImportSources` collects supported files from dropped/selected folders with relative subfolders, rejects files above 100 MiB and does not follow symlinks; `AssetImportDialog` previews destinations before import. Empty/unsupported-only directories are not imported. The existing importer commits individual files, reporting partial failures without overwriting existing files. Pure styling lives in `resources/ui/styles/explorer.qss` and `UI/Styles/AssetExplorerStyle` using the shared palette/icons. It displays only final usable assets: storage containers such as `Model` are hidden when they own subassets, and every non-empty GLB mesh is shown as its own `StaticMesh` tile. Final asset names are case-insensitively unique across the Game project; import derives stable names from the destination file and adds numeric suffixes on conflicts. Game Content accepts imports through the Import button or external file drop. Both open `AssetImportDialog` for destination names/folder and format-specific options before invoking `AssetImporter`; Engine Content is read-only. Game folders support create / F2 rename / Delete (nested folders only; `/Game` and Engine stay protected), including empty folders discovered on disk. Supported UI imports are self-contained GLB, static FBX (converted to GLB, optional missing-normal generation), PNG and JPEG. Model import writes one `StaticMesh` subasset record per non-empty GLB mesh (`selector.kind="mesh"`, source mesh index). Game assets support F2/context-menu rename, confirmed Delete, and drag-and-drop moves onto Game folders; siblings sharing one backing file move together, deleting the last leaf removes the data/metadata pair, GUIDs stay stable across rename/move, and deletion unregisters CPU slots and retires cached GPU resources. It exports `application/x-sakura-asset`; dropping a StaticMesh into the Scene viewport creates a `GameObject` with `MeshRendererComponent`, while dropping a Material assigns it to the selected mesh object.
- Clipboard shortcuts use the system Electron clipboard. Text inputs, spin boxes and editable combos retain standard Ctrl+C/Ctrl+V behavior. Hierarchy and the Edit-mode viewport copy the selected object subtree as `application/x-sakura-scene-subtree`; paste duplicates it as a sibling through `MakePasteSubtreeCommand`, remaps every object/component persistent ID and remains undoable. Content Browser copy stores `application/x-sakura-asset`; paste duplicates the backing file and metadata into the selected Game folder with fresh asset/subasset GUIDs and unique final names. Engine Content remains read-only as a paste destination.
- Scene gizmo uses ImGuizmo with exclusive Translate / Rotate / Scale modes (`W` / `E` / `R` physical scan codes and matching viewport-header buttons next to Camera Speed). It is available only while the single viewport is in Edit mode. During a drag the selected object receives preview transforms directly; release restores the old value and commits one `MakeSetTransformCommand`, preserving a single undo step. Edit-mode object picking constructs a camera ray on the CPU and selects the nearest visible `MeshRendererComponent` by its transformed world AABB; it does not require PhysX colliders and does not run during Play or gizmo manipulation. Inspector transforms expose position, Euler rotation in degrees and scale. The component selector is a filter: `All` is the default and displays every component in its own settings card; selecting a component type only hides non-matching cards. Edit-mode viewport camera is Unreal-like: RMB look + WASD/QE fly (Shift faster, Ctrl slower, wheel adjusts speed), MMB pan, wheel dolly, Alt+LMB orbit, Alt+RMB dolly; Play mode switches to the game camera and disables editor navigation. Create / Hierarchy menus are built from reflected `editor.EditorAction` types (`EditorAction::CollectPublishedActions`); builtins live under `Root/Editor/inc/Actions/` with categories `Objects` / `Components`. Project-side native actions can subclass `EditorAction`, register via `ENGINE_CLASS`, and call `ForceTouchEditorRegistrars` (or link their TU) before `InitializeNative`.

## Coding conventions (engine)

- Transform gizmo defaults to Translate, so selecting an object shows axis arrows immediately; Q explicitly selects without a gizmo, W/E/R change tools. Inspector position/rotation/scale numbers support horizontal LMB scrubbing (Shift fine, Ctrl fast), click-to-type and Escape cancellation. Each scrub uses one command group with live scene updates and one undo step; selection changes cancel it and Play finishes it before cloning the world. Transform commits/cancellation update only transform fields and the gizmo (`RefreshTransformUi`); never rebuild component cards or the hierarchy on drag release, which causes inspector layout/scroll jumps. Undo/redo uses the same lightweight refresh for commands marked `IsTransformOnly`, including groups whose every child is transform-only; mixed/structural commands retain the full refresh. Never infer command effects from display names.

- Always bind keyboard controls to physical key positions using native scan codes, independent of the active keyboard layout. This includes editor navigation and gameplay controls. Do not fall back to character-based key values for these controls. Keep standard text input and platform UI shortcuts under normal browser/platform handling.
- Editor gizmo overlay is a non-activating, input-transparent tool window used only for ImGuizmo input/state. The native viewport owns mouse/keyboard input and forwards mouse events in viewport coordinates to the gizmo. Pass SimpleMath matrices directly to ImGuizmo without transposing; both store translation in the last four-float row. ImGuizmo draw data is snapshotted into `ImGuiOverlaySnapshot` on the Game thread (`Engine::SetRenderSurfaceImGuiOverlay`), carried on `RenderViewFrame`, and drawn by `ImGuiDiligentRenderer` on the Render Thread after FXAA onto the swapchain. There is no QPainter ImGui drawer.
- ImGui's current-context pointer is thread-local through `Engine/ThirdParty/ImGuiConfig/SakuraImGuiConfig.h`. `Diligent-Imgui` propagates `IMGUI_USER_CONFIG` publicly to every consumer, including ImGuizmo. Game and Render threads own separate contexts/atlases; only snapshots cross the boundary. Never compile a consumer with a different ImGui configuration or replace this with a process-global context.
- Viewport overlay synchronization is non-reentrant, including geometry/show/resize notifications. Nested host callbacks must not start another ImGui frame while the current overlay is being built; every completed build restores the previous current context.

- Identifiers: PascalCase, no abbreviations, no type-wrapper suffixes (`Ptr`, `Ref`, `Handle`).
- Booleans may use a `b` prefix (`bActive`, `bRunning`).
- Always use braces for `if` / loops; no single-line conditionals.
- Prefer complete names over short ones.
- Debug logging messages in English via `PrintString`.
- Dominant build configuration: **Debug**.
- Commit author and committer must both be the user's configured Git identity. Never use an agent identity or add an agent co-author. Do not change git config to accomplish this.
- Compilation is allowed whenever needed for implementation, diagnostics or verification; no special user command is required.
- CMake 3.25+ Windows/MSVC presets build native dependencies without Qt. Use Debug for development, Release for distribution. `SAKURA_BUILD_EDITOR=OFF` disables editor services and the default Electron host; explicit host builds require ImGuizmo.

## Architecture follow-up and rendering roadmap

- The architecture cleanup preserves PhysX, DiligentFX, ozz, Python and the current backend choices; dependency scope is unchanged.
- Minimal Forward implementation contracts are documented below; the current source layout and local validation workflow are documented in `README.md`. Do not describe newly written GPU code as runtime-verified until the current sources have been built and exercised.
- Continue using forward rendering. Deferred and ray tracing require a separate explicit decision.
- Regression coverage includes standalone component ticking, safe close during Play, persistent scene IDs, story target deletion, ambiguous legacy names and actual React dialogue/choice commands.

### Minimal Forward renderer contracts

- All swapchains use IsPrimary=false. Renderer explicitly calls FinishFrame and ReleaseStaleResources once after every surface has presented; individual Present calls must not invalidate another surface’s dynamic buffers.
- One device/context and Render Thread serve every surface. Each surface owns RGBA16F HDR, D32 depth, weighted OIT accumulation/revealage, tone-mapped intermediate, a 4096-square shadow atlas and a four-frame duration-query ring. No GPU state is accessed from Game Thread.
- Lighting budget: 32 visible lights and 16 shadow tiles of 1024 pixels per view. Directional/spot use one tile; point uses six. Overflow is counted in RenderStatistics. Directional shadows use a camera-relative finite volume, not cascades.
- IBL uses a built-in sky baked into irradiance and GGX-prefiltered reflection cubemaps plus an integrated BRDF LUT. This is global environment lighting; custom HDR import and local probes are outside this minimal implementation.
- OPAQUE/MASK render with depth writes; MASK uses the same alpha test for shadow casting. BLEND uses weighted blended OIT with depth testing and no depth writes, then composites in linear HDR. Weighted OIT is approximate, does not model refraction, and translucent surfaces do not cast transmission shadows.
- Post: linear HDR composition → exposure, threshold bloom and ACES approximation → FXAA → sRGB swapchain. React UI is composed separately. No TAA/motion vectors, deferred renderer, animation or GPU skinning in this change.
- CPU visibility uses conservative world AABB/frustum tests separately for every camera and shadow view; local lights are range-culled. Opaque draws are front-to-back. GPU timings measure shadow, opaque, transparency and post passes without blocking; delayed samples carry their GPU frame index. CPU submission time, draw/triangle counts, light-budget overflow and asset GPU memory estimates are exposed through Engine::GetRenderStatistics.
- Asset retirement retains identities until the fence has completed the last potentially referencing submitted frame (signal value FrameIndex + 1). Cancellation after upload also retires the created object. IDs are never recycled across renderer sessions. Project close clears upload caches, cancels CPU loads and schedules GPU retirement. Resize/detach/shutdown deliberately wait for GPU idle; ordinary frames do not.
- `Samples/RenderValidation/RenderValidation.project` is the reproducible technical scene; `GenerateScene.py` regenerates its GLBs, materials, alpha texture and metadata. `SakuraForwardRendererTest` / `Engine.Render.Forward` exercises two surfaces, culling, shadow allocation, suspend/resize/reattach and retirement. `--vulkan` selects Vulkan for an additional run.

### Editor component icons

- Component exposes virtual GetEditorIconPath (empty by default) and GetEditorIconPriority (0 by default). Paths are deployed image file paths; Engine does not load editor graphics or depend on Editor.
- Editor viewport displays one screen-sized icon at each active visual object's world position. The component with the highest icon priority wins; equal priorities preserve component order. LightComponent uses priority 100 and Lucide sun/lightbulb/flashlight for Directional/Point/Spot.
- Icons are editor overlay markers, visible through geometry, clipped to the camera frustum and hidden in Play/Player. Clicking a marker selects its generation-checked object identity; overlapping markers prefer the nearest object. Resources and upstream licenses belong to Editor only.

### Local verification and MCP connections

- Tests under Root/Tests and Tests, including Root/Tests/TestTargets.cmake, are local-only and ignored by Git. Root/CMakeLists.txt includes local test targets only when BUILD_TESTING is enabled and that file exists; a clean checkout builds Engine/Editor/Player without local tests.
- MCP connection configurations are local-only and ignored by Git. Do not commit connection settings or credentials.
- Stop the native host and join GPU work before destroying Electron windows. Scene/project shutdown follows renderer shutdown; screenshot checks use the same path.

## Root-system refactoring contracts

- Owned source locations mirror their runtime responsibility. `Root/EditorCore/{inc,src}` contains `Assets`, `Documents`, `Commands` and abstract `Actions`. `Root/Editor/{inc,src}` contains `Shell`, `Panels`, `Viewport`, `Launcher` and concrete editor actions. EditorCore remains available with `SAKURA_BUILD_EDITOR=OFF`; editor dependencies stay in `Root/Editor/ThirdParty` and are configured before EditorCore.
- `Core/Object` owns Object, ObjectHandle and the live-object registry currently named MemorySubsystem. `World` owns Scene and GameObject; `World/Components`, `World/Serialization`, `World/Simulation` and `World/Resources` contain the corresponding behavior. Serialized TypeId, PropertyId and persistent object IDs do not change with C++ include paths.
- `Rendering/Threading` owns RenderThread, RenderCommandQueue and RenderFrameQueue. `Rendering/Frame` owns RenderFrameData. `Rendering/Assets` owns CPU-to-GPU upload. Engine composes these systems; PresentationController owns surface view configuration and frame extraction. Every host, including SDL, submits `RenderFrameData.Views`; there is no fallback single-scene field.
- Scene ticks and gameplay start/stop use generation-checked component snapshots. Objects/components created during a phase enter the next snapshot. Removal is flagged immediately and destruction is deferred until the callback phase ends; queued child/parent deletion must resolve identities again before deleting. Components may remove themselves without invalidating their running callback.
- Native OnCreate/OnDestroy remain construction/attachment callbacks. BeginPlay/EndPlay are gameplay callbacks. Python `OnCreate`, `OnUpdate`, `OnDestroy` (with legacy snake_case fallback) run only for playing component instances: opening/deserializing an EditWorld or restoring undo does not create Python gameplay instances. PlayWorld cloning occurs before BeginPlay. EndPlay and component destruction unbind once.
- Transforms are read through const GetTransform and changed through SetTransform. MoveTo and CameraCut exclusively control their destination transform while running; competing setters are ignored until completion, cancellation, failure or Stop releases control. MoveTo and explicit CameraCut coordinates are local to the controlled object's parent. CameraCut copying an object transform converts its world matrix into the camera parent's local space. Scene::FindPrimaryCamera is the common enabled/active camera resolver for story and extraction.
- Scene extraction and asset resolution inspect every mesh/light component. Camera selection considers every camera component, prefers the first enabled primary camera, and otherwise uses the first enabled active camera. Extraction caches matrices and decomposed transforms only inside one extraction call; it retains no world pointers between frames or on Render Thread.
- `Core/IO/AtomicFileWriter` performs authored text replacement through an exclusive adjacent temporary file, flush and atomic OS replacement. A failed replacement preserves the previous target. Scene, Story, project descriptors and asset metadata share this implementation. Import staging remains outside Content.
- Custom CPU resource loading requires `AssetManager::RegisterLoader<ResourceType>` to bind the C++ resource type to its string AssetType. Unknown or mismatched resource types are rejected before casting. Native resource types, including SkinBinding, retain their established asset identifiers.
- EditorCore AssetOperations owns Content file mutations and Engine mount read-only checks. SceneDocument follows its AssetId after moves. Moving a startup scene/story or its ancestor updates the project descriptor separately from scene Save; descriptor failure rolls the move back. Deleting an open scene or its folder is rejected until it is closed. ContentBrowserWidget collects requests and displays diagnostics.
- SceneWorkspace owns the document, command history and generation-checked scene selection without UI operations. Electron composes the shell and delegates workspace identity to this service; shell implementation units are separated by responsibility.
- Editor command groups compensate successfully applied children on failure. A command returning false must leave its own state unchanged. Saved document revisions are distinct from current revisions; Undo/Redo restore the associated revision. Undo/Redo cannot run inside an open group. A failed compensation is reported explicitly rather than treated as successful rollback.
- Mandatory native reflection initialization failure shuts Engine down. Script initialization/import failures produce project Degraded health and retain all diagnostics; failed initialization cannot become Ready. Editor and Player check Engine readiness before project flow.
- Engine publicly exposes math, JSON and ozz runtime requirements used by public headers. SDL, PhysX, Python embedding, Diligent, fastgltf and import implementation dependencies remain private usage requirements. Static library link dependencies still propagate as required; do not delete them to reduce include exposure.
- Local regression sources and TestTargets.cmake remain ignored. Their existence is optional for clean checkout builds. Use Debug builds, headless tests, automated GPU/Electron smoke tests and project screenshot helpers for verification; Computer Use remains prohibited.

## Materials and Material Instances

- `Material` (`.material`, schemaVersion 2) owns its domain, shading/blending/render settings, parameter definitions/defaults, shader source identity and deformation bounds. `MaterialInstance` (`.materialinstance`, schemaVersion 1) owns a single Material/Instance parent and typed overrides only. Materials are assignable directly. Legacy schemaVersion 1 is adapted in memory to Standard PBR; opening never rewrites assets or GUIDs. Explicit editor save upgrades the document.
- Material domains are Surface, PostProcess and UserInterface. Surface uses independent Lit/Unlit/Toon and Opaque/Masked/Translucent/Additive settings. Opaque/Masked write depth and may cast shadows. Translucent uses weighted blending; Additive runs after transparency composition and before postprocess. Their shadow casting is disabled. Back-facing double-sided surfaces orient their shading basis consistently in color and masked shadow passes.
- `Materials/MaterialDocumentIO` resolves inheritance and HLSL/includes on workers through owned registry snapshots. Parameter IDs are stable HLSL identifiers; display names/groups are metadata. Types are Float, Integer, Boolean, Vector2/3/4, Color, Texture2D and StaticSwitch. Limits are eight textures, eight static switches and 4 KiB of ordinary constants. Explicit empty texture overrides differ from inheritance. Cycles/missing parents/incompatible types fail; unused overrides remain authored and produce warnings.
- Material HLSL is embedded in the `shader` field of its `.material` document. External ShaderSource assets, companion files and external include relocation are not supported. Content moves preserve the material identity; runtime file tracking watches material documents and their parent chain.
- HLSL contract: `EvaluateSurface`, optional `ModifyVertex`, `EvaluatePostProcess`, `EvaluateUserInterface`. Engine generates typed `GetMaterialParameters`, texture sampling helpers (`Sample<Name>` / `Sample<Name>Level`, `Has<Name>`), static switch macros and entry points. Shader code has no Diligent/native API access. Vertex deformation requires conservative `boundsExpansion`, applied to camera and shadow bounds. Surface and shadow passes use the same vertex/opacity evaluation.
- Shader and pipeline creation belongs exclusively to Render Thread/RHI and never occurs inside Draw. Supported backend async creation is polled without Game Thread waiting. OpenGL uses bundled glslang/SPIRV-Cross through the RHI adapter. In-memory variant keys include root/shader/include revision, switch values, contract version, backend, vertex layout and target formats; ordinary values and display metadata do not create variants. Data revisions include the full inheritance chain separately.
- `DynamicMaterialInstance` belongs to Game Thread. C++ and Python can change/reset ordinary parameters without modifying assets/scenes; switches remain fixed. It follows successful asset updates while retaining runtime overrides. Editor preview instances use independent draft definitions. Python exports `create_dynamic_material_instance`, `set_parameter`, `get_parameter`, `reset_parameter`, `assign_dynamic_material`, `assign_postprocess_material` and `assign_ui_material`.
- `MeshRendererComponent.material_asset_id` remains the default across slots. `material_slots` stores per-slot overrides; real submesh index ranges are extracted/drawn. `RF_ASSET_TYPES` accepts multiple type identifiers while retaining the old single-type attribute. Both material asset types are accepted by Inspector/drop targets.
- `RenderObject` has immutable `MaterialRenderSnapshot` values and GPU resource identities, never editable material/Component references or a fixed PBR-field schema. Textures have separate Linear/sRGB GPU residency over shared CPU data, retained by snapshot lifetime leases. Shader/pipeline/image resources are retired behind GPU completion; project unload cancels pending compilation/readback and clears caches safely.
- `CameraComponent.post_process_effects` serializes ordered material/enabled/intensity entries. BeforeToneMapping runs in HDR after transparency/additive and before bloom/tone mapping. AfterToneMapping runs in linear LDR before FXAA/sRGB. Effects ping-pong between targets, never sample the active destination; zero intensity bypasses and intermediate intensity lerps. Editor has a separate material-postprocess preview toggle.
- Engine Content includes Standard PBR, Unlit, Toon, HDR/LDR postprocess examples and UI Image/Panel. Normal/MR/Occlusion are Linear; base color/emissive textures are sRGB. MR uses G roughness/B metallic, occlusion R. Tangent space is positive Y; imported/generated tangents include handedness, mirrored seams split, degenerate UV disables normal mapping without NaN.
- EditorCore material documents support dirty state, undo/redo, atomic save and explicit external-conflict handling without a UI application. Material authoring is provided through EditorCore documents; future asset editors integrate through registered adapters. Content creation is transactional and Engine Content stays read-only. Invalid HLSL preserves last successful rendering. Surface falls back to diagnostic rendering, invalid post effects are skipped, UI has an initial diagnostic image. Successful data/variant publication rejects obsolete requests.
- Startup material preparation is asynchronous before Editor Play/Player game start, including visible non-preview UI bindings held through Game Thread weak state registrations and the authored materials retained by scene cloning. No blocking shader compilation or asset loading in Draw. Tests and MCP configuration remain local/gitignored. Builds/checks need no special authorization word. UI verification uses native automated tests and screenshot helpers; Computer Use remains prohibited.

- `engine.ArrowComponent` is a reflected editor direction marker along local +X, with color/length/thickness/visibility. It serializes like other components, is added through reflected editor actions, and draws only in Edit viewport overlays; Player/Play rendering never extracts it. Billboard icon priority remains independent of direction-marker geometry.
- Future material editing adapters must apply only the draft that completed compilation; edits during preparation require another Apply. Their previews must coalesce ancestor/include changes without dirtying local overrides. Parent shader edits are not write conflicts for an instance document. External texture changes prepare fingerprints on workers, update Game metadata, and replace bindings only after successful upload; moved/deleted texture watches cannot publish into another asset.
- Pinned DiligentCore receives the reproducible `PatchDiligentOpenGLQuery.cmake` FetchContent patch: query names from glGenQueries are not labelable objects until initialized, so labels require glIsQuery. GPU timing and Debug validation remain enabled; no third-party source copy or manual dependency edit is required.
- Pinned DiligentFX receives `PatchDiligentFXShaderList.cmake` through FetchContent and before configuring cached sources. Its shader-list generator assembles the complete header in memory and publishes it with `file(CONFIGURE)` instead of repeated WRITE/APPEND. Reconfiguration repairs a damaged generated list and preserves its timestamp when unchanged; do not hand-edit `build/.../shaders_inc/shaders_list.h`.
- Material and MaterialInstance activate a single native `MaterialEditorWindow` per asset through `RegisterAssetEditor`. EditorCore `MaterialEditingSession` owns validated drafts, history and revision-checked compilation; Editor owns fields, prompts and dependency watching; Engine/RHI owns compilation and immutable preview data. Worker preparation captures registry/document/source copies, never UI objects or scene state. Publish only the successfully compiled requested revision; retain the last successful preview on failure. Save stays explicit and uses MaterialDocument conflict checks. Engine Content and inherited HLSL stay read-only; writable Game materials own their shader code. Main-window close and Escape honor dirty material prompts. Console/MCP `material_document` uses the same open session and requires the current revision for changes; reject edits during Play. Domain reassignment and separate Apply are not exposed; source edits update the embedded shader string. Actual UI/API and portability limitations: `docs/MATERIAL-EDITOR.md`; `Root/EditorCore/MaterialEditorUiContract.md` remains the broader target contract.

# Обязательные продуктовые правила Sacura

Перед изменениями прочитай [требования продукта](docs/PRODUCT-REQUIREMENTS.md).
Они имеют приоритет над прежними архитектурными предложениями, заметками по
дизайну и жанровыми примерами. Явные инструкции пользователя остаются главным
источником требований.

Для каждого изменения проверь три принципа по
[списку проверки](docs/CHANGE-CHECKLIST.md): простота, переносимость и
редактируемость, универсальность. В результате работы укажи фактическую
проверку и незакрытые ограничения; неприменимость объясняй, а не считай успехом.

- Основной путь: выбрать готовую механику → добавить → настроить понятные
  параметры → запустить. Внутреннюю логику раскрывай по запросу в том же
  редакторе и на тех же данных. Типовые сценарии не требуют JSON, ручных ID,
  путей свойств или кода.
- Пользовательские Events, функции, механики и шаблоны — локальные редактируемые
  данные. Перенос с зависимостями является обязательной возможностью, а не
  задачей после рефакторинга. Контракт: [пакет v1](docs/AUTHORING-PACKAGE-V1.md).
  Экспорт проекта или готовой игры не заменяет экспорт авторской заготовки.
- Импорт предварительно проверяет пакет, показывает состав и привязки,
  разрешает конфликты без молчаливой перезаписи и применяется целиком либо
  оставляет проект прежним. Не исполняй JavaScript и установочные скрипты из
  пользовательских пакетов; расширения движка с кодом устанавливаются отдельно.
- Все жанры используют общую модель Events, групп действий и функций.
  Герой, HP, оружие и победа над врагами нужны только использующим их механикам.
  Движение, физика и рендер остаются низкоуровневыми системами, без цепочек
  асинхронных Events на каждый кадр.
- Сейчас не реализуй магазин, оплату, аккаунты или облачную синхронизацию ради
  обмена пакетами. Будущий магазин использует тот же локальный формат.

Приёмка переноса: [«Дверь с ключом», A → B → C](docs/ACCEPTANCE-DOOR-WITH-KEY.md).
Не объявляй её выполненной без проверки работы, редактирования и независимости
копий. Техническая универсальность не компенсирует усложнение обычной механики.

## Enhanced Input

- Engine owns one `InputSession` per Player/Play session. `InputAction` and `InputMappingContext` are Content assets (`.inputaction`, `.inputcontext`), identified by metadata GUIDs. Runtime device state and delegates are never scene data. Electron keyboard/mouse/gamepad adapters feed the same Game-thread session; focus loss, pause, context replacement, disconnect and Stop clear or cancel applicable state.
- Actions expose typed values, phases, triggers, modifiers, chords and context priority/consumption. Gameplay subscribes explicitly through `BindAction` / `Unbind`, with owner lifetime cleanup; never broadcast arbitrary input to every component. Python callbacks are ordinary methods, without requiring reflected method decorators.
- `.project` input settings hold startup contexts and explicit Story Event bindings. User remapping is separate in `Saved/InputProfile.json`. Engine defaults are dialogue advancement and pause; user contexts can replace the active set. Story input invokes the existing Event runtime, not a second executor.
- EditorCore owns validated input documents, undo/redo, explicit atomic saves and input-package dependency closure/remapping. Editor supplies the native input workspace; Console/MCP use the same document/package operations through typed `input_*` tools. UI-only settings and physical recording exclusions are documented in `docs/INPUT.md`.
- Input packages contain editable action/context dependencies, not project Python handlers or Story documents. Input A → B → C coverage must not be presented as the complete door-mechanic acceptance. Local tests: `Engine.Input`, `Editor.Input`; physical gamepad behavior needs hardware verification.


## Characters

- New Electron `character.create` instances use `/Engine/Characters/StarterHumanoid/StarterHumanoid.character` on one CharacterComponent, with two ordinary material-slot assignments. Do not attach a second static renderer as the default character body. Gameplay templates attach the same animated character as one scaled visual child, preserving gameplay roots and collision. Idle/Walk are authored starter clips; HipHopDance preserves the supplied FBX motion. Clip choices in component property snapshots come from the loaded CharacterDefinition and mutations retain component.setProperty/setAsset and scene history.

- Character authoring uses registered `.character` assets (`sakura.character` v1), stable asset/subasset IDs and the existing AssetRegistry/AssetManager. Engine owns CharacterDefinition, CharacterComponent and ozz CPU pose evaluation; EditorCore owns CharacterDocument/CharacterDocuments, import and undoable scene placement; Editor owns CharacterWorkspaceView. No browser storage or Engine-to-Editor dependency.
- CharacterDocuments shares drafts/history between UI and typed `character_*` tools. Save is explicit and atomic, checks external changes and invalidates loaded resources. Character edits reject Play. Closing the editor accounts for dirty MCP-only drafts. Asset deletion preserves unresolved scene/story references; it is not a scene-history command.
- GLB skeletal loading preserves all selected-skin mesh parts and skin weights through tangent seam vertex splitting via SourceVertexIndices. CPU pose snapshots are immutable shared resources in RenderFrameData; only Render Thread creates/retires their GPU meshes. Preview scenes use Engine render surfaces and detach before destruction. No GPU skinning is introduced.
- FBX humanoid animation import uses ufbx and bakes editable `.animation` resources (`sakura.animation` v1), validating required bone roles and target skeleton compatibility. Import errors must not silently replace existing assets or keep a partially accepted character draft.
- Story speakers may store `character:<asset GUID>` while legacy literal names remain valid. StoryActionKind::CharacterAnimation changes an instance animation and optionally waits; execution stays in StoryRuntime, including the native event adapter. UI and MCP story edits use StoryGraphDocument history. Details, limitations and commands: docs/CHARACTERS.md.

## Static Mesh defaults

- Engine owns `StaticMeshDefaults`; per-subasset settings persist in the model metadata under `loadSettings.staticMeshDefaults.<subasset GUID>` and load into immutable CPU mesh resources. Instance material overrides take precedence; shared defaults never overwrite authored instance slots.
- EditorCore owns `StaticMeshDocument` history, Surface-material validation and explicit atomic metadata saves with external-change detection. UI and `static_mesh_document` MCP/console share the same document. Mesh changes reject Play; preview camera and collision visibility are UI-only.
- Content Browser placement in the main scene and prefab editor adds an undoable PhysicsBody with `use_mesh_defaults=true` when saved mesh collision is enabled. Existing bodies opt in explicitly; no runtime component creation is implicit. PhysicsBody keeps motion, mass and filters while inheriting shape dimensions and collision enablement. Preview surfaces detach before scene destruction. Usage and limits: `docs/STATIC-MESH-EDITOR.md`; coverage: `Editor.StaticMesh`.

- Static Mesh collision defaults contain an ordered `collision.elements` array with per-element shape, center, XYZ rotation in degrees and dimensions. PhysX attaches these shapes to one actor; body-level contact Begin/End spans all touching shape pairs. Debug and navigation geometry include every element. Legacy single-shape mesh defaults remain readable. Rotated Box/Capsule primitives require positive uniform object scale; triangle geometry bakes local rotation before object scale. Character controllers do not accept compound shapes.

- Static Mesh authoring must support direct collision selection and transform gizmos in its isolated preview scene, with the same camera navigation as the main viewport. One pointer drag is one document-history edit; transient previews must not enter saved metadata. Collision authoring outlines are independent of simulation enablement and are clipped against the camera frustum. Selected elements must be visually distinguished; show only shape-relevant fields and keep material slots in their own tab. Preview navigation and selection remain UI-only; serialized transforms use the shared StaticMeshDocument/MCP path.

- Builtin geometric primitives live as ordinary GLB assets with stable metadata in `Root/Engine/Content/BasicShapes` (`/Engine/BasicShapes`). Reproduce them with `Scripts/GenerateBasicShapes.py`; preserve identifiers. Units are metres, Y-up, centered pivots, slot 0 uses StandardPBR. Cube/Sphere/Capsule/Plane use primitive collision; Cylinder/Cone use static/kinematic triangle collision. Use existing asset registry, placement commands and runtime deployment, not special object types. Details: `docs/BASIC-SHAPES.md`.


## Sky environment

- Reflected SkySphereComponent and SkyLightComponent are scene data. Extraction copies immutable RenderSky values; no world references enter RHI. Sky Sphere owns visible procedural zenith/horizon/ground and sun radiance; Sky Light owns IBL color/intensity. Sun direction is the sphere object's world +Y, independent of position/scale. Direct lighting/shadows require a separate Directional Light.
- Each render surface retains its own IBL cache. Background and irradiance/reflection use the same procedural function; intensity-only edits do not rebuild maps. Preserve legacy IBL for scenes without either component. Multiple active skies/lights resolve deterministically by owner persistent ID. Editor creation and component properties use existing action/reflection catalogs, command history and MCP. Usage and DiligentFX investigation: `docs/SKY.md`.


## Viewport diagnostic visualization

- `RenderVisualization` and its shared descriptor catalog define viewport modes for Engine, Editor UI and `viewport_visualization` console/MCP. These are per-view presentation settings, never scene/project data. Camera and asset previews keep independent modes; Player defaults to Lit.
- The renderer is forward: BaseColor/WorldNormal/Roughness/Metallic/material AO/Emissive/Opacity are shader diagnostic outputs, not a G-buffer. Depth, HDR scene color, shadow atlas and weighted OIT accumulation/revealage inspect the actual per-surface textures. Diagnostic views bypass material postprocess, exposure, bloom, tone mapping, FXAA and the editor grid; lit rendering retains its usual path. Document display mappings, depth range and transparency limits in `docs/BUFFER-VISUALIZATION.md`.

## Reflected asset creation

- Enum creation settings use reflected enum types rather than integer fields with numeric legends. `AssetCreationFactory::DescribeSettings` publishes descriptor names as `enumValues`; React uses the shared schema select and submits enum names through the existing validated creation path. Header-defined enum registrars use one external inline variable so inclusion by multiple translation units does not register the same recipe repeatedly.

- A Material is one `.material` document containing its HLSL string in `shader`; MaterialInstance remains a separate `.materialinstance` with parent/overrides. Do not create `.Source.hlsl` companions, source-asset selectors or compatibility readers for external material sources. Source editing, Undo/Redo, compilation and saving use the same MaterialDocument revision. Builtin material shaders are embedded in their documents. Only builtin Sakura shader headers may be included.
- React Content Browser file/folder actions use shared EditorCore `AssetOperations`, preserving asset metadata and persistent identity during rename/move and generating new identities on duplication. Support context menus and focused-browser shortcuts for copy/paste, cut/move, duplicate, rename, delete, folder creation and refresh. Discover actual Content directories so empty folders remain visible. Deletion requires a concrete confirmation; Engine Content and Play are read-only for mutations. Do not rename/move/delete files owned by open detached editors. Typed Electron `assets.*` commands use AssetOperations.

- Creatable assets are exposed by concrete reflected `AssetCreationFactory` objects in EditorCore. Discover them through the published reflection catalog; do not maintain separate creation-type lists in React, the Electron bridge or MCP. Builtin linker anchors retain registration translation units; they are not the runtime catalog.
- Factory editable properties describe creation settings, defaults, labels, ranges and asset-type filters. The factory declares required settings and implements creation. Parent selection is an ordinary reflected AssetKey setting, never a frontend MaterialInstance special case. Instantiate a fresh factory per request, apply shared PropertyAccess validation, and preserve the class default object.
- React Content Browser right-click creation and typed `asset_creation_types` / `asset_create` tools share this implementation. Validate Game Content destination, extension, global name uniqueness and settings before creation. Refresh and select the created asset without replacing the current scene/story or their unsaved edits. Reject creation during Play. Resource types requiring imported source data remain import-only until they provide a valid creation factory. Extension guide: `docs/ASSET-CREATION-FACTORIES.md`.

## Asset thumbnails

- Editor-only `AssetThumbnails` supplies shared 128px previews to Content Browser, reflected asset references, material slots, texture/parent pickers and character references. Reuse this binding for new asset selectors; preserve asset/subasset identity and existing assignment commands.
- Thumbnail generation uses JobSystem with an owned AssetRegistry snapshot. Workers read CPU asset data only; they never create worlds, touch scene objects or submit GPU work. ImageBuffer results publish on Game thread into a bounded memory cache and generation-checked asset identities. Never retain UI element pointers across asynchronous work.
- Textures use decoded images, meshes/models use neutral CPU geometry previews, characters use their model, and materials use a sphere with resolved BaseColor/BaseColorTexture. Other asset types and unsupported/failed previews use a type card. These previews do not execute custom material shaders or simulate scenes, prefabs or animation. They are presentation only, excluded from MCP; asset identity and mutation remain in the existing typed tools.
- Generated thumbnails are disposable presentation state, not authored Content or scene data. Cache keys include asset/subasset identity, source timestamp/size and metadata. Invalidate shared previews after authored asset changes. No thumbnail files are written beside source assets.

## Asset reference fields and compact toolbars

- Every asset reference, regardless of asset type or owning editor, uses the shared `AssetReferenceField`: thumbnail and picker plus Open, Browse in Content Browser, and Clear. The same actions are available from the field context menu; double-clicking the thumbnail opens the asset. Do not introduce a bare asset select in a specialized editor. Missing references remain clearable; empty references cannot navigate. Read-only references still allow navigation while assignment/clearing follows existing edit permissions.
- Asset identity always includes asset GUID and optional subasset GUID. Generic reflected references derive allowed types and companion identity from PropertyDescriptor attributes (`AssetTypeFilter`, `AllowedAssetTypes`, `CompanionProperty`); never determine behavior from a caption, extension or a hardcoded Material/StaticMesh UI branch. Specialized document collections use the same reference widget and their existing validated document commands. Do not replace document history with direct scene/property writes.
- UI asset navigation shares typed `asset_open` and `asset_focus` commands through the registered asset editor dispatch and Content Browser. Types without an asset editor fall back to revealing the asset in Content Browser. Clear uses the existing picker/assignment command and its validation, not reflected reset-to-default. Reset and clearing are separate operations; inherited material reset retains its explicit existing semantics.
- Document toolbars show a single icon-only Save action (floppy disk), with an accessible name and Ctrl+S tooltip. Undo/Redo/Reload stay in menus/shortcuts rather than text buttons. Viewport transform tools use the same icon-only Move/Rotate/Scale/Select buttons, roles and shortcut tooltips in the main scene and asset previews.

## React integration priority

- React/Electron UI from `feature/electoron_ui` takes priority in presentation conflicts when integrating into `feature/narrative_and_ui`. Preserve recent Engine/EditorCore capabilities, asset identities and document commands.
- CharacterComponent retains the asset-backed character/animation runtime and the reflected description field required by Electron starter projects. Shared command identity must continue resolving objects/components after Undo restores persistent IDs.

## Native React editor integration

### Universal React workspace tabs

- Tab dragging uses pointer capture and physical document hit testing, including portaled content and native child-window content bounds. Do not use HTML5 drag-and-drop for workspace tabs: its OS forbidden-drop cursor incorrectly describes supported detachment outside windows. Escape cancels; release outside docking hosts detaches. Asset drag-and-drop remains separate. Regression helpers must send mouse gestures onto panel contents, across native windows and outside window bounds, rather than directly dispatching synthetic drop events on tab strips.

- Every editor workspace/tool is a tab, including the scene viewport, Hierarchy, Inspector, Content Browser, history and asset editors. Main-window areas and native detached windows host the same tab groups; groups may split horizontally or vertically. A new tool must register a tab instead of creating an independent window lifecycle. Transient confirmations and file pickers remain dialogs.
- `workspaceLayout.js` owns identity, placement, group topology and layout migration. `EditorDocking.jsx` preserves the same portal container and mounted React state while moving between documents; `EditorWorkspace.jsx` supplies the common tab strip, drop targets, splits and detached native windows. Placement changes are user preferences, not scene edits; they are explicitly excluded from document MCP mutation tools. Document mutations continue through the existing typed bridge/EditorCore commands.
- `asset-windows.cjs` is the native asset document-session registry. Sessions and Undo histories belong to document IDs, never window IDs. Shared-frame metadata routes each frame/ACK to its document; register the session before its first GPU frame. Moving a tab must neither reopen its document nor recreate its native host. Closing a dirty tab flushes source drafts and honors save/discard/cancel, including window/project shutdown.
- Detached windows are same-origin presentation hosts with no independent Node or privileged IPC access. The primary renderer retains document/scene ownership. Register window event ownership and use the actual owner document for menus, selection, CodeMirror and native surface bounds. Persist tab placement, selected tabs, window bounds and split ratios. Preserve open asset documents when resetting the layout.
- Regression coverage: `editor-windows.test.js`, `editor-windows-ui.test.js` and the project-owned Electron `--workspace-tabs-check` helper. Test actual cross-window DOM transfer, live GPU frames, draft identity, document Undo/Redo and cancelled close; screenshots alone are insufficient.


- Main native viewport flight speed is presentation state, configured beside the transform/grid/help tools through the existing nativeTools transport. Keep the 0.1–100 m/s validation consistent in UI, Electron and ViewportController; default is 4 m/s and Shift multiplies flight speed by three. This does not edit game cameras or scene documents and is excluded from document MCP mutations.

- Native scene clipboard uses typed `object.copy` / `object.paste` commands and serialized subtree snapshots; paste and cut deletion use EditorCommandStack. Scene menus and shortcuts must target the native selection, never the prototype object collection. Viewport input must let editor clipboard and Undo/Redo shortcuts reach the editor shell.

- Reflected `engine.Color` properties use the shared React `ColorField` swatch and Color Panel instead of generic struct/vector fields. Material `Color` parameters and reflected factory settings reuse it; `Vector4` remains a vector. Preserve the source object/array representation, linear floating-point/HDR values and existing command validation/history. sRGB preview and HEX conversion are presentation only; opening/cancelling the panel does not mutate documents, and Apply commits one edit through the existing callback. The picker is UI-only; reflected property and material document tools remain its MCP mutation path.

- Preserve the React Inspector presentation: searchable component overview, expandable reflected component sections and common fields. Asset references still require the common thumbnail/picker/Open/Reveal/Clear contract, including typed AssetKey properties and paired asset/subasset strings.
- Native React assets use AssetRegistry identities through NativeAssetsContext. Reveal opens the containing Content Browser folder, clears the search and selects the asset. Thumbnail images come from the shared EditorCore AssetPreview CPU generator. Browser thumbnails are asynchronous, session scoped and disposable.
- React Content Browser mesh dragging carries only asset/subasset identities in `application/x-sacura-native-asset`. The main native viewport accepts it only in editable scene mode and resolves the drop position through its current camera using nativeScenePlacement. Placement uses the existing `assets.place` command, command history, selection and mesh collision defaults; do not add direct scene mutation or browser geometry creation. Asset preview viewports and Play reject scene drops.

- Material and MaterialInstance Content Browser drops use the same identity-only payload with `kind: material`. `scene.pickMesh` is a read-only, normalized-coordinate viewport query sharing native scene selection's mesh bounds hit test; it returns persistent object/component IDs, never the current selection as a fallback. Dropping on a mesh assigns all slots in one existing `mesh.material` command (`slot: -1`) with Undo/Redo and material-domain validation. Empty-space drops do not mutate the scene. Preview windows and Play reject drops. The existing mesh material tools remain the MCP mutation path; viewport hit testing is presentation-specific. Regression helper: `--material-drop-check`.
- The separate React placement menu uses the published EditorAction catalog through editor.actions/editor.executeAction. Builtin object/component action declarations and registrar anchors live in EditorCore. Do not hardcode mutation presets in React; presentation translations/icons may be mapped by stable action ID. Builtin geometric assets use the existing static-mesh placement command.
- Native asset document mutations use StaticMeshDocument and MaterialEditingSession with document revision checks, explicit saves and independent histories. Project close/switch guards include dirty asset documents. React asset editors are document tabs in the universal workspace registry, reused by project/asset/subasset identity. Each document owns an isolated native host and preview scene independently of its containing window; it never edits the main scene as a preview. Close/project-switch guards include these windows and their unsaved-document prompts. GPU scene previews use Engine frame production and the existing shared-texture transport. Surface materials bind to preview meshes, PostProcess materials bind to the preview camera, and UserInterface materials use the existing GPU MaterialImageRequest renderer through Engine/RenderThread. Do not assign non-Surface domains to MeshRendererComponent or import private renderer headers into the Electron host. Collision wireframes and gizmos belong to the immutable native viewport overlay; each completed gesture goes through assetDocument.execute edit with the captured document revision and creates one undo step. Material-instance fields come from resolved native parameter definitions, preserve typed override records and support explicit inherited-value reset.
- Test the Electron integration with the project-owned --asset-transfer-check helper, typed bridge tests and the native host. Keep generated test projects and captures under build on M:, including TEMP/TMP overrides for native tests. UI helper diagnostics remain in English.

- Native shared-frame presentation retains a bounded, window-scoped pool by resource/extent. Native D3D12 slots are reused, but their Electron SharedImage import must be renewed for each published frame after the preceding consumer ACK; retaining the same import across external texture writes freezes its initial contents in Chromium. Native resources and handles are not recreated per frame. Producer slots are acknowledged only after the consumer GPU blit completes and its VideoFrame is closed, through the window-scoped native-frame-consumed channel. Do not use sharedTexture transfer completion as a consumed-frame signal. Resize/project-close must drain pending frames and release the retained imports. Visibility/focus restoration republishes viewport bounds, including after minimizing detached asset windows.

- React material editors prioritize the HLSL workspace (72% initial width). Keep the native preview above the independently scrolling material inspector on the right, with resizable dividers and collapsible compiler output beneath the code. MaterialInstance source remains inherited/read-only. Source drafts flush through assetDocument.execute before document actions and window close; presentation changes do not introduce a separate mutation path or MCP command.

- React Texture assets open as ordinary document tabs through the shared workspace/asset identity registry. Texture inspection uses read-only `assetDocument.execute` snapshots and the Engine TextureLoader, never browser file paths or a separate decoder. Channel/background/zoom/pan controls are presentation only and excluded from MCP mutations. Preview transport is bounded to 1024 pixels per side under the bridge response limit; metadata retains decoded source dimensions. Compression and mip controls must remain unavailable until the actual loader/cooking pipeline and shared saved-document commands support them.

- Component captions come from reflection Class metadata (`ENGINE_CLASS` optional final display-name literal, `Class::GetDisplayName`). Unnamed types use the shared PascalCase fallback. React/native bridge reflection catalogs consume that metadata; never maintain per-view component-name dictionaries. Type IDs remain unchanged in commands and saved documents.

- React Inspector component addition uses the shared searchable action popup in the trigger's owner document, including detached workspaces. Selecting a catalog entry invokes the existing `object.addComponent` command with Undo/Redo; search and popup presentation are UI-only. Close the popup when the selected object changes or Play starts.

- The transform Inspector has a UI-only scale axis lock, initially unlocked. Locked numeric scale edits preserve X:Y:Z ratios against the values at the start of the edit and commit all axes through the existing single transform command. Clearing and retyping a field must not lose the original ratio. If the edited baseline axis is zero, set all scale axes to the entered value. Position and rotation remain independent.

- Transform presentation must not clamp scale to an arbitrary authoring range (such as 0.05–30). Preserve finite small, negative and large values; numeric scale inputs accept arbitrary fractional steps. Native transform validation remains authoritative for singular scales.

- Transform numeric inputs use the shared NumericInput arithmetic parser (no eval or JavaScript execution), with click-to-edit, pointer scrubbing, Shift fine adjustment, Ctrl coarse adjustment and Escape rollback. The typed Electron `transform.edit` begin/update/commit/cancel lifecycle previews by persistent object ID and world identity, then restores the baseline and executes one EditorCore transform command. Unrelated mutations cancel the preview; reads do not. React coalesces pending previews and cancels on unmount. Expression text is local UI state; MCP continues to accept evaluated numbers via existing transform tools.

- Native Inspector asset references open supported resources through the shared inline Open action; do not add a separate mesh-opening button. Empty material slot identities are JSON null, never the zero GUID (which falsely presents an unavailable asset). Opening is UI-only and uses the existing asset document dispatcher; material mutations retain mesh.material / mesh_material_assign.


## Editor localization

- English is the editor source language and default; Russian is a translation. The shared catalog is `Root/EditorCore/resources/Localization/Editor.json`, consumed by EditorCore and React. Catalog keys are immutable identities; never regenerate a migrated key when its source changes. New entries use descriptive `Editor.*` keys. Reflection captions use stable type/property IDs, not per-view dictionaries.
- `EditorLocalization` owns native locale/overrides in per-user JSON under APPDATA/SakuraNovel. Electron invokes the same service through the bounded `--editor-localization` CLI. The deployed catalog lives in Engine/Config/EditorLocalization.json. Browser-only use retains localStorage as a fallback.
- Editor language is independent of game text. Translate declared UI captions and reflection metadata, never authored names, paths, dialogue, property values, identifiers or logs. New UI uses stable catalog keys. Language changes must not reconstruct documents.
- Settings include translation search, edits, JSON export and import preview. Packages carry keys, English source and Russian text; imports validate source revisions and interpolation parameters before applying any entry. Overrides do not modify installed catalogs. A source mismatch falls back to the shipped translation/source until reviewed. Do not claim this editor catalog implements localization of story documents or a game's runtime text.
- Regression coverage: frontend `localization.test.js` and `editor-localization.test.js`; native desktop `--localization-check`. Keep generated projects, profiles and captures under M:/Sacura-Novel-Engine-3D/build. Details: `docs/EDITOR-LOCALIZATION.md`.

- Reopening an asset must select its existing document tab, reveal its dock region, and raise its owning native window. Restore minimized windows and recover offscreen title bars onto a display work area. Reuse the document session and Undo history; presentation focus uses the restricted workspace-focus-window IPC, not a document mutation command.


## Default reflected asset documents

- Asset activation opens an ordinary document tab. Registered specialized presentations in `assetEditorRegistry.js` take priority; otherwise `NativeReflectedAssetEditor` renders the native reflected schema. Unsupported formats open read-only asset information with an explicit diagnostic, never an unrestricted JSON file editor.
- EditorCore `ReflectedAssetDocument` registers document factories by open AssetType identifiers. Each adapter owns format IO, domain validation and history; shared reflection generates nested property/collection metadata and validates property-path edits. React must not duplicate Input field catalogs or write files directly. Native typed `assetDocument.execute` operation `property` accepts `{path, operation, value?, index?, destination?}`; collection operations are insert/remove/move. Save/undo/redo/reload retain revision checks, Play restrictions, Engine read-only behavior and existing document-window close guards.
- InputAction/InputMappingContext and their nested values use ordinary ENGINE_STRUCT metadata. The Input adapter preserves original JSON format and unknown fields, generates IDs for new bindings, validates through InputSession and keeps explicit saves. Specialized UI registration is presentation only; it must reuse the document command path.
- Fixed `std::array<T, N>` reflected values expose element type and fixed length; the property editor allows element edits but no resizing. Native `int` decoding rejects overflow; int64 transport uses decimal strings. This extends the supported container inventory beyond std::vector without enabling arbitrary containers.

### React widget libraries

- Game UI authoring uses the prototype WidgetWorkspace in normal Electron asset tabs. The UI toolbar and Windows/Widgets panel list WidgetLibrary assets; Content Browser creation and double-click use the shared factory and asset-document registry. Do not restore the removed Qt widget editor.
- A .widgets asset under Content (default destination Content/UI) is a sakura.widgets version 1 JSON library: widgets contains layouts with stable local IDs, ui maps dialogue/hud/mainMenu/pauseMenu to compatible widget IDs. The .meta GUID identifies the library across moves. Each library owns its defaults; never select the first discovered library implicitly. Unknown document fields are preserved. Texture elements use imageAssetId GUIDs, not machine-specific paths or embedded image files.
- WidgetLibraryDocument owns validation, independent Undo/Redo, explicit atomic saves and external-change detection. assetDocument.execute open/get/edit/undo/redo/save/reload is the shared UI/MCP contract; edits require the current revision and reject Engine Content and Play. Dragging previews locally and commits one document edit at release. Dirty document tabs use the existing close guards. Canvas fit, preview resolution, selection and layout preview are presentation state, excluded from MCP mutations and saved assets.
- Python GetGameView may publish widgetLibrary as the library asset GUID and variables as its UI binding values. The native host resolves only saved library defaults into the existing widget/widgets payload for Editor Play and Player; actions remain restricted by the published actions catalog and game.action validation. The React layout preview does not execute project gameplay. Prototype-only event/component authoring is not exposed as if it were supported by native gameplay. Existing template-owned widget payloads remain supported when widgetLibrary is absent.
- Widget files are limited to 512 KiB, 128 widgets and 512 elements per widget, with checked parent graphs and typed default assignments. Runtime image presentation uses the existing CPU texture loader and bounded PNG transport (512 pixels per side); resolved game UI is limited to 6 MiB. Saved assets retain full-resolution texture references and export with Content. Tests: desktop-electron/tests/widget-library.test.cjs and the --widget-editor-check project-owned Electron helper.

- Reflected property controls must not display implementation hints such as int64 decimal-string transport notes. Keep transport details in developer documentation, not the property UI.
