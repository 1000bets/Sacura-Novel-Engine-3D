# Native authoring: contracts and migration

Status: first native execution slice. Product requirements in
[PRODUCT-REQUIREMENTS.md](PRODUCT-REQUIREMENTS.md) take precedence.

## Ownership and boundaries

Engine owns operation contracts, validation, execution and world state. Each
simulated Scene owns an independent execution session. EditWorld never executes
authoring logic. Editor Play and SakuraPlayer use the same Scene lifecycle and
game clock. Pause freezes work; Step advances it once. EndPlay cancels work
before components and physics are destroyed. No application window is required.

EditorCore owns editable documents, revisions, commands, dependency collection
and transactional import plans. Editor owns UI only: views, presentation models
and adapters to EditorCore. Engine and EditorCore do not depend on Editor.

Low-level rendering, physics and navigation retain their existing native systems.
Authoring operations request their services on Game Thread; they do not drive
physics or GPU commands through asynchronous per-frame scripts.

## Values, identities and operation contracts

Use the existing owning ReflectedValue carrier. The first native slice accepts
Boolean, Integer, finite Number, String and Empty (void results). No implicit
coercion or narrowing. Structured values and reference parameters require an
explicit subsequent schema extension; unsupported values fail validation.

An installed operation has a stable namespaced string ID, positive version,
display label, supported calling contexts, named typed parameters with optional
defaults, and a declared result. An ID/version pair cannot be overwritten.
Unknown arguments, unsupported versions/contexts, invalid defaults and wrong
results fail with a path and diagnostic before publishing a success.

Existing IDs, GUIDs and reflection TypeId/PropertyId do not change. Authored
object references use persistent IDs and resolve in the receiving world. Runtime
contexts retain only generation-checked ObjectHandles, never persisted addresses
or runtime IDs. A removed, inactive or foreign-world owner cannot keep executing.

Installed operation implementations are trusted native code. Project/package
data cannot install implementations or run code during validation or import.

## Execution and cancellation

Tasks are cooperatively ticked on Game Thread. Submission validates the entire
call before constructing its task. Each task receives a revocable context.
Completion validates its result; failure or cancellation invalidates the context
before cleanup, exactly once. A retained context cannot write state after Stop,
owner destruction, restart or Scene destruction. Work started during a tick
becomes eligible on the next tick. Recursive ticking is rejected.

The session owns typed variables, monotonically increasing run IDs and bounded
terminal results/signals. Its clock accepts finite nonnegative deltas only.
Wait uses game time, never wall time. Invalid deltas change nothing. A failed
submission does not start work. Native task exceptions become diagnostics.
Callbacks must not destroy their executing Scene/runtime or recursively mutate
the scheduler; native extensions must honor this execution boundary.

This slice provides an operation scheduler, not the full Event/rule engine.
Before/during/after story bindings, parallel groups, rules, signal subscriptions,
component methods and JS continuations will all compose this same executor.

## JavaScript and reusable packages

New components use JavaScript, as required on 20 October 2026. Python remains
supported but is not a substitute or an automatic translation target. Legacy
graph/BT resources are preserved as compatibility data, without offering new
graph/BT components in the creation UI. Ordinary function/story/Event graphs
keep their own editors.

Native JS integration must implement the existing component/function SDK with
explicit arguments, instance state, named references and installed operations.
It must bound memory/computation, revoke asynchronous continuations and preserve
the last successful compiled source separately from the draft. No DOM, network,
Node APIs or arbitrary package-module loading.

The interchange contract remains sacura-authoring-package v3; v1/v2 need explicit
compatibility adapters. A native document format does not claim automatic web
project compatibility. Unsupported dependencies are reported before any mutation.
Import is data-only, previews its dependency closure and external bindings, then
applies one undoable transaction. Imported JS remains unapproved until review
and successful compilation. Stable method IDs and dependency tables are remapped;
source is not rewritten by string replacement. Author/license/provenance survive
A → B → C and exports retain editable sources. This acceptance is not complete
until an edited native package runs in C without A/B.

## UI migration contract

The live source of truth is engine-web-prototype/frontend/src/main.jsx → AuthGate
→ ProjectHome → Editor, not historical App/Studio screens. Match the frontend's
layout, spacing, controls, language and states at the same viewport size, including
Hierarchy on the left and Inspector on the right. This explicit migration request
supersedes the older opposite placement described in AGENTS.md.

New Editor UI lives under inc/src/UI, separated into Views, Presentation and
Adapters. Pure styles live under resources/ui/styles; palette tokens are separate
from widget behavior. No business logic, file writes or GPU access in views, and
no setStyleSheet literals inside view implementations. Runtime game widgets stay
in Engine/UI. Migrate existing panels incrementally through adapters; do not
destroy functioning native world/asset services to reproduce their appearance.

Visual acceptance uses project screenshot helpers and automated Qt tests only.
Compare identical dimensions and state against frontend references. A styled
placeholder or runtime test does not establish visual parity.
The shell helper captures Qt-owned widgets, never an area of the desktop.
Native swapchain pixels require renderer readback and are not established by
the widget-only screenshot; GPU startup is checked separately through logs.

## Verification and staged delivery

AGENTS.md keeps Root/Tests and TestTargets.cmake local/gitignored. Restore the
local suite there; never silently report an empty CTest run as success.
Scripts/TestEngine.ps1 checks for a nonempty manifest before building/testing.
Python embedding DLL lookup is configured per CTest process from its imported
Python target; no global PATH or machine configuration changes are required.
The Qt foundation test deploys the imported offscreen platform plugin and does
not inspect or automate a desktop application.
Baseline coverage includes scene identity, command revisions, failed document
open, project path confinement and story cancellation. Executor coverage includes
validation before side effects, result contracts, waits, independent sessions,
pause/step, owner deletion, restart and retained-context rejection.

The native Classic and Sakura palettes are copied from frontend themes.js and
checked by Scripts/VerifyEditorUiSources.py (52 tokens). The base shell now uses
Sakura to match the supplied screenshot: one compact header, searchable hierarchy,
location/game strip, story canvas with variable sidebar and a contextual inspector.
Pure drawing, fonts and QSS live in UI/Styles and resources/ui/styles. The canvas
shows actual story v1 nodes/connections through an owned presentation model; it
supports selection, pan, zoom and fit. Story v1 now has a complete editable document
slice: four native node kinds, transition ports, choices/actions in the inspector,
persisted layout, atomic save and independent undo/redo in EditorCore. The UI
controller passes the current draft to StoryRuntime in PlayWorld without saving.
See [native story graph](NATIVE-STORY-GRAPH.md) for architecture, workflow, schema
extensions, local coverage and remaining limits. JS Event/function/value nodes,
variable authoring and play-from-line remain open. The launcher and specialized
inspectors have not all been given frontend visual parity.

The Qt suite includes gesture-level graph editing and a hidden-shell integration
check for document history routing, save and draft playback. Debug screenshot
helpers capture widget content; they do not capture the GPU swapchain or establish
pixel equivalence. The source palette/style boundary check still covers 52 tokens.

Next slices: JS/component editor and minimal A→B→C package; story/Event authoring;
input/interaction; media/characters/cameras; widgets/save checkpoints/player
packaging. Their state and file contracts must be versioned before implementation.
Scene serialization is not a game save; arbitrary running JS stacks are not saves.
