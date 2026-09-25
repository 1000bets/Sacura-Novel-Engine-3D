# Task 15 — Runtime UI and lifecycle boundaries

Date: 2026-09-24.

## Changes

- Qt Core/Gui/Widgets is owned and publicly linked by Engine. The local SDK moved from Editor/ThirdParty/Qt to Engine/ThirdParty/Qt; Qt.cmake migrates matching legacy CMake cache entries on reconfiguration.
- Player uses QApplication, QTimer and Engine runtime widgets. StoryWidget is shared with Editor Game and supports explicit advancement and any choice. Editor panels remain in Editor.
- Engine owns the main scene through unique_ptr; PlaySession owns its cloned scene. SceneDocument observes a generation-checked object identity. Closing a project stops simulation before destroying scenes.
- Player StartGame and Editor PlaySession share TickWorld, including story execution. Engine owns the story runtime.
- Engine alone extracts and submits frames. Editor configures the camera before Tick; Game uses the scene camera. Resizing is enqueued onto Render Thread.
- The current one-surface renderer uses one shared native viewport for Scene/Game. The previous second viewport did not have an independent swapchain. Simultaneous views are explicitly scheduled in RenderingRoadmap.md.
- Persistent object/component IDs replace serialization-order IDs. Scene and subtree restoration preserve them; story actions persist targetObjectId and resolve runtime generations before access. Legacy names are accepted only when uniquely resolvable on load.
- startupStory is an optional project-relative field. Editor story tree reads the actual file. Sample-specific paths and implicit dialogue progression are removed from application code.
- Backend implementation and headers live under private src/Rendering/RHI. EditorCore builds independently of the editor application option.
- PhysX, DiligentFX, ozz, Python and backend dependency scope are unchanged.

## Verification

New regression cases cover standalone ticking, stopping simulation, ownership during close, persistent identities, renamed/deleted action targets, ambiguous legacy names, startupStory paths and Qt choice interactions. C++ compilation and execution of the updated tests require the separately authorized build. Old binaries are not evidence for the updated sources.

Static validation passed for source manifests, public backend boundaries, frame submission ownership, sample story references and CMake control-block balance. Qt moc successfully generated metadata for the runtime viewport and editor headers; git diff --check passed. Full GPU/UI behavior, Qt deployment on a clean machine and native-widget composition remain runtime verification items.

## Follow-up

See ../RenderingRoadmap.md for the rendering implementation sequence and acceptance criteria. Full renderer features are planned, not implemented by this architecture change.
