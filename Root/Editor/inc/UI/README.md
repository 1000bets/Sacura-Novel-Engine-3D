# Editor UI boundary

The visual reference is `engine-web-prototype/frontend/src/Editor.jsx` and
`editorTheme.css`. The current native shell uses the screenshot's Sakura Night
palette; Classic is retained as a source-checked resource.

- Presentation contains owned view data and layout metrics, without Engine/world pointers.
- Adapters load story documents into view data or apply workspace arrangement.
  Scene mutations still use SceneWorkspace and EditorCommandStack.
- Views own Qt widgets: responsive StudioHeaderView, HierarchyPanelView,
  WorkspaceView regions, StoryWorkspaceView and StoryInspectorView.
- Styles own the QSS, palette resources, vector icon strokes, graph painting and fonts.
  Views never embed stylesheets. Runtime game UI remains in Engine/UI.

The header centers menu item content with the brand and command buttons. It uses
Qt height-for-width layout: navigation and commands share one row when they fit,
and wrap to two rows otherwise, using font metrics and logical pixels. Its minimum
single-row height is 44 px. The Flower2 brand mark uses the frontend's Lucide
vector geometry, painted at the current device scale; attribution is embedded in
`resources/ui/icons/LUCIDE-LICENSE`.

The base layout has 36 px tabs, 32 px viewport/graph command bars,
210/280 px side rails and 6 px splitters. Rail sizes are applied after the final
host layout so DPI and initial widget size cannot shrink them.

The story canvas edits the native story v1 profile (Line / Choice / Action / End).
Its reference is `StoryFlow.jsx` / `storyFlow.css`, rather than the older `enode`
canvas. Moment cards are 360 logical pixels wide. `StoryGraphStyle::Layout`
measures pre-wrapped dialogue and answer text with the painting font; card bounds,
choice output anchors and initial unsaved layout use those same measurements.
Execution ports are 16 px squares with triangular glyphs and a separate hover/hit
area. The grid uses 28 px spacing. Authored node positions are preserved.

`story.qss` maps `storyInspector.css`: a 48 px heading, 30 px controls, 38 px
section summaries, 120 px dialogue input and 16 px horizontal padding. Document
typography lives in StoryInspectorStyle; widgets own layout and field bindings.
Only the final spacer absorbs extra height. Collapsing sections never writes a
document revision. The native entry checkbox lives in “Параметры истории”.

StoryGraphController translates view intents into StoryGraphDocument commands in
EditorCore; that document owns revision snapshots, persistent data and atomic IO.
SceneWorkspace and its history remain separate. Global Save/Undo/Redo route to the
active editing context, while inspector shortcuts flush pending values first.

Nodes can be added, dragged and deleted; output-to-input gestures connect or
replace transitions, and selected edges can be deleted. Choice rows have separate
outputs. The inspector edits type, text, speaker, continuations, choices and
Wait/MoveTo/CameraCut/NavigateTo parameters, including persistent scene-object
pickers. Positions survive save/reopen. Pan/zoom survive commands within a session.
The canvas uses wheel panning, Ctrl+wheel zoom, context menus and the compact JS
layout. The code does not implement JS function/value-node families, variable
authoring, persisted viewport zoom or play-from-line.

Finite Events use the three StoryFlow phases, sequential/parallel batches and
reusable definitions with sequential steps and parallel actions. The event
inspector edits placement overrides separately from the source definition.
Event drag/drop and Delete share document history with node and wire edits.
StoryEventStyle owns event metrics; action fields and package dialogs are separate
views. Local event packages externalize scene objects as named inputs and apply
as one undoable import after preview. See docs/NATIVE-STORY-EVENTS.md for supported
actions, the declared package profile and remaining JS functionality.

Play freezes authoring and loads the in-memory draft into the Engine-owned
StoryRuntime bound to PlayWorld. It does not save implicitly. Engine Content is
read-only; opening another document/closing prompts for dirty stories. Content,
playback and render settings remain available from the Windows menu. New stories
are ordinary Game assets with metadata, created without hand-written JSON.

Tests use Qt offscreen and native screenshot helpers. QWidget capture intentionally
captures only application widgets; it does not capture a GPU swapchain or desktop.

Shared native controls mirror `EditorSelect.jsx` and `objectInspector.css`.
`StudioSelect` keeps QComboBox models/signals but uses a read-only trigger and a
searchable popup with checked selection, keyboard navigation and screen bounds.
Its snapshot retains persistent model indexes; search never edits the authored
value. Rapid close/reopen reverses the existing popup animation. The speaker list
uses speakers already present in the story; it is not a character asset editor.
An empty speaker remains empty in the document while displaying “Рассказчик”.

`StudioCheckBox` retains Qt toggle/accessibility behavior and animates its vector
checkmark. `AnimateDisclosure` clips opening/closing fields without changing
document history. Hierarchy branches use vector chevrons and Qt tree animation;
scene handles, reparenting and filtering remain with their existing adapters.

`ObjectInspectorView` owns identity/transform rows, action buttons and component
disclosures. The shell supplies scene command callbacks, reflected fields and
generation-checked selection. The entire inspector scrolls as one form; component
disclosure state survives undo/redo refreshes. No raw component TypeId is used as
its primary caption; the identity stays in data and tooltips.

`ControlStyle` owns vector painting and resolves the frontend font stack to an
installed Inter or Segoe UI face (system sans fallback elsewhere). `controls.qss`
owns styling. Resolving an actual face avoids Windows Qt interpreting a CSS family
list as a missing font; QTextDocument also receives the resolved font explicitly.
