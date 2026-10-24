# Runtime scene transitions

`Engine::RequestScene` replaces the active gameplay scene in Player or Editor Play. It does not modify the scene document, startup scene, project descriptor or saved assets. This is a complete world replacement, not additive scene streaming.

```cpp
std::string Error;
if (!Runtime.RequestScene("/Game/Scenes/House.scene", Error, "/Game/Stories/HouseOpening.story"))
{
    PrintString(Error);
}
```

Python exposes the same Engine implementation:

```python
engine.RequestScene('/Game/Scenes/House.scene', '/Game/Stories/HouseOpening.story')
```

The second argument is optional. Without it, the previous story is cleared. The project's startup story is not restarted. Scene and story paths must identify registered Content assets of the corresponding types; absolute filesystem paths are not an API contract.

Requests require an active game session. Invalid asset identities/types and duplicate pending requests return `false` with an error in C++, or raise `ValueError` in Python. A successful request means queued, not loaded. Only one transition may be pending.

The Engine processes the request at the end of its tick after input, world, StoryRuntime and subsystem callbacks return. This permits calls from component callbacks, input delegates, published gameplay methods and Python Narrative generators without deleting the executing component or closing the executing generator reentrantly. A request does not interrupt the calling function; it should return or yield normally.

Deserialization, story validation and physics/navigation preparation happen before replacing the active world. Material preparation may span frames while the old scene continues. Load/preparation failures retain the old world. Destination BeginPlay is staged while the previous scene is retained, so a failed simulation startup can restore it. After successful startup the old story is stopped, the previous world ends play and is destroyed, and the requested story starts against the new world. Requests made during destination startup or old-world teardown are rejected while the transition is pending.

The existing InputSession remains owned by the game session. A transition cancels active actions, blocks held inputs until neutral, and restores startup input contexts before destination components start; their `OnCreate` methods may activate gameplay contexts and bind actions. Scene-bound script delegates are removed when their components end play. Native delegates owned by the host/session remain. Player and Editor pause state is preserved.

In Editor Play, only PlayWorld is replaced. EditWorld, authored scene state and editor Undo/Redo remain unchanged. Stop Play returns to the original EditWorld. In Player, the Engine-owned main world is replaced. Stop cancels a queued transition.

World objects, scene-local Python instances and Narrative variables do not transfer between scenes. Resolve destination objects in the destination scene; references to destroyed objects become invalid. Shared render frames retain immutable snapshots, and the Engine remains the only frame producer.

Completion/failure can be inspected from C++ using `IsSceneTransitionPending()` and `GetSceneTransitionError()`, or from Python:

```python
if not engine.IsSceneTransitionPending():
    Error = engine.GetSceneTransitionError()
    if Error:
        print(Error)
```

An accepted request clears the previous deferred error. Deferred failures also write an English diagnostic through native `PrintString`. An error starting the destination story is reported after the scene has switched; the new scene continues with its story cleared.

This API is gameplay functionality, so it adds no editor mutation command or UI. Project-published component methods and Python Narrative nodes may call it and remain accessible through their existing action catalogs and MCP tools. Editor `scene.open` remains a separate document operation and is not a gameplay transition.

Validation: `Engine.Scene.Transitions` covers Player, Editor Play, input/component callbacks, Python Narrative generator shutdown, optional destination stories, invalid assets, failed deserialization, duplicate requests, Stop cancellation and pause preservation.
