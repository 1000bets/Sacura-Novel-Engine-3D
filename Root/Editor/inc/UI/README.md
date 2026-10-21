# Editor UI boundary

The visual reference is `engine-web-prototype/frontend/src/Editor.jsx` and
`editorTheme.css`. The current native shell uses the screenshot's Sakura Night
palette; Classic is retained as a source-checked resource.

- Presentation contains owned view data and layout metrics, without Engine/world pointers.
- Adapters load story documents into view data or apply workspace arrangement.
  Scene mutations still use SceneWorkspace and EditorCommandStack.
- Views own Qt widgets: one-row StudioHeaderView, HierarchyPanelView,
  WorkspaceView regions, StoryWorkspaceView and StoryInspectorView.
- Styles own the QSS, palette resources, vector icon strokes, graph painting and fonts.
  Views never embed stylesheets. Runtime game UI remains in Engine/UI.

The base layout has a 44 px header, 36 px tabs, 32 px viewport/graph command bars,
210/280 px side rails and 6 px splitters. Rail sizes are applied after the final
host layout so DPI and initial widget size cannot shrink them.

The story canvas edits the native story v1 profile (Line / Choice / Action / End).
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
layout. The code does not implement the additional JS Event/function/value-node
families, variable authoring, persisted viewport zoom or play-from-line.

Play freezes authoring and loads the in-memory draft into the Engine-owned
StoryRuntime bound to PlayWorld. It does not save implicitly. Engine Content is
read-only; opening another document/closing prompts for dirty stories. Content,
playback and render settings remain available from the Windows menu. New stories
are ordinary Game assets with metadata, created without hand-written JSON.

Tests use Qt offscreen and native screenshot helpers. QWidget capture intentionally
captures only application widgets; it does not capture a GPU swapchain or desktop.
