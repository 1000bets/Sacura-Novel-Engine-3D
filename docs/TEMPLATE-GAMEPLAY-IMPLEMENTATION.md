# Native template gameplay migration

The authored-data acceptance source is the four shipped packages under
`engine-web-prototype/frontend/public/game-templates`. Behavior references are
`templates/components/*.js` and the web capability implementations. Oracle traces
execute the original HauntedRoom and movement functions. Existing user projects
are not rewritten when templates change.

Required scenarios:

- Quest: collectible brass key, inventory, two independent instances of the same
  door logic; unlocking does not animate; interaction and story-driven opening do.
- Shooter: FPS movement/look, hitscan pistol and travelling projectile launcher,
  separate magazines/reserves, cooldown/reload, health/armor/heal/death, medkit
  pickup/use, five pursuing/attacking enemies, rewards, explicit battle start/stop,
  all-targets objective, victory and Game Over. HUD reads the same gameplay state.
- Novel: support reward, relationship-dependent choice, continuously updated
  relationship counter and an independent one-time snapshot, both dialogue routes.
- Horror: third-person movement/look and object interaction; fuse, power, recorder,
  locked exit, shadow/lamp changes, mirror toggle, journal/cabinet/bell examination,
  terminal escape after the one-second door animation. There is only one authored ending.

## Project structure

The launcher copies installation seeds from `Root/Engine/Config/ProjectTemplates`
into a new project. `Scripts/GenerateGameplayTemplates.py` regenerates the authored
scene, story, material and input seeds. These are not Engine Content assets and
their template IDs are never interpreted by gameplay runtime code.

- `Content/Scripts/mechanics.py`: ordinary Python classes for inventory, locks,
  health, weapons, enemies, rewards and puzzle state. Each controller owns fresh
  instances; stopping and restarting Play resets them.
- `Content/Scripts/template_game.py`: registered `game.TemplateController`, scene
  lookup through persistent IDs, movement and physics queries, input subscriptions,
  runtime presentation and the `game.TemplateAction` Narrative node.
- `Content/Stories/Opening.story`: editable story, conditions and calls into Python.
- `Content/Input`: editable actions and mapping context; raw mouse axes use
  `bClampMagnitude=false` while movement retains the normal bounded axis contract.

Create a new project from a launcher card, then start Play. Existing projects are
left intact. Python changes require reopening the project in a new process.
`MoveSpeed`, `LookSensitivity` and `RequireRecord` are reflected controller settings.
Weapon balance, enemy attributes, rewards and puzzle rules are in `mechanics.py`.

## Controls and walkthroughs

Quest: click the key and doors, or use their HUD buttons. The left lock needs the
key. Advancing the story unlocks the warehouse separately from opening its door.

Shooter: advancing the introduction starts battle before displaying the next line. WASD and arrow keys move,
mouse looks, left click captures the pointer and shoots, Escape releases it.
1/2 switches weapons, R reloads, E interacts, H heals. The original HUD exposes
HP, score, current magazine, medkit use and battle stop. Weapons operate independently
of the battle runner. Each kill awards 100 points. Victory or defeat interrupts
the current dialogue. The inventory starts with one medkit.

Novel: supporting the character awards relationship points, records a snapshot,
and exposes a second support option. The second reward changes the live value,
not the snapshot. A cold response leaves the second support option visible but disabled.

Horror: WASD moves, right mouse drag looks, left click examines a nearby object.
Take the fuse from the entrance desk, visit the red power box, inspect the recorder
on the blue table, then return to the green exit. The mirror toggles the silhouette;
journal, desk, cabinet, silhouette and bell provide their original messages.
Tape reels invoke the recorder interaction. Interaction range is 3.2 units.
The camera shortens its distance when an obstacle intervenes.

## Shared APIs

`engine.FindSceneObject(Context, PersistentId)` resolves an object in the context's
scene, returning `None` when absent. `SetObjectActive(Object, bool)` changes its
activity; `SetObjectLook(Object, Yaw, Pitch, Roll=0)` uses radians. Runtime identity and
target-type validation remain native. Physics uses the existing Raycast and
SphereSweep API; no template-specific physics or combat was added to Engine.

A published `GetGameView() -> str` returns a JSON object up to 64 KiB with `title`,
`values` (`label`, string/number `value`), `actions` (`id`, `label`, `enabled`),
`text`, `objective`, `controls`, `reticle`, and `pointerLock`. All fields are optional.
The bridge adds persistent object/component IDs. The React HUD only presents it.
The typed `game.action` command invokes the published
`ExecuteGameAction(Action: str)` after checking that the action remains enabled
and the game is not paused, unless that action explicitly publishes `allowWhilePaused`.
The contract is shared by Editor Play and Player. Source widgets use the same
React WidgetRenderer as the web prototype. Save/load/restart use ordinary project
Python and explicit JSON snapshots under Saved/Gameplay.json. `RequestStoryNode`
queues story transitions safely; `GetStoryNode` reads the current node identity.

## Verification

Run from the repository root after building the Debug host and UI:

```powershell
python Root/Verification/TemplateMechanicsTest.py
python Root/Verification/WebTemplateParityTest.py
node --test desktop-electron/tests/template-gameplay.test.cjs
node --test engine-web-prototype/frontend/tests/native-player-interactions.test.js
node desktop-electron/scripts/template-players-check.cjs
```

The native suite creates fresh projects and exercises both dialogue branches,
door animation, actual keyboard/mouse input, raycast kills, reload, battle
stop, victory, defeat, the complete horror route, pause restrictions, JSON saves and
Play restart. The Player helper checks native GPU frames, pause/resume, authored
HUD state and rejection of editor commands, and captures 1280x800 and 640x720
windows for each template. Reports stay under `build/template-players-*`.
The pure Python suite additionally covers armor, healing, cooldown, delayed
projectile hits, repeated rewards and puzzle guards. These automated suites do not
use Computer Use.

Scene meshes are generated from the web primitive geometry. Characters now use
the shared skinned StarterHumanoid asset instead of nine primitive mesh parts.
This visual replacement was explicitly requested after the original parity work.
The first-person player mesh is hidden; third-person and enemy meshes remain visible.
The controller ports the web swept-AABB sliding rules as project Python; bullet
queries use native physics. Browser JavaScript is only used for authoring and
oracle generation, never as the native gameplay runtime.

### Verification limits (2026-10-24)

The current run passed 17 Python checks, five native gameplay scenarios and 23
bridge/transport regression checks. All four fresh Player projects passed GPU
presentation, pause/resume and HUD checks at both window sizes. Ready-to-open
projects and their captures are under `M:/Prototypes/Sacura-Web-Templates-20261024`.
Existing user projects are not overwritten or automatically migrated.

These results do not establish complete web/native parity. Lighting uses the
native PBR renderer rather than the web hemisphere-light model. Primitive picking
now uses the exported mesh triangles, scale and live door rotation. Character
picking still uses a bounding box; interactive markers also need comparison.
Projectile/tracer presentation also requires further comparison. Reopening an
already open door now retains the original animation wait, with regression coverage.
The available tools in this session do not include the
engine MCP functions, so the requested MCP acceptance pass was not performed.

### Interactive regression findings (2026-10-24)

Earlier interactive comparisons covered the Quest ending, both Novel response
branches and save restoration, and Shooter weapon switching, firing, reload and
defeat. They exposed failures not caught by native command tests: pointer capture
after pointer lock prevented firing, empty browser key codes lost physical keyboard
input, and the native UI lacked the end screen and restart button. These are now
fixed. A DOM regression test covers firing with pointer lock, physical key fallback
on a Russian keyboard layout, project window title and the end-screen restart action.
Choice speakers and dialogue hints are also restored. Native gameplay tests check
the choice speaker and ending title through the actual bridge.

This was not a complete manual playthrough of every branch. Horror was entered in
the web prototype, but its entire native route was verified through the native
bridge suite rather than interactive play. The user subsequently authorized
Computer Use for this session. Native Horror was entered and its out-of-range
interaction checked. The available Windows input API supports key taps but has
no key-down/key-up or hold-duration API, limiting continuous movement playthroughs.

Interactive Quest testing exposed pointer coordinates being blocked after the
main menu: the first door click picked up the key at the screen center instead.
InputAction now supports `bRequireNeutral` (default true); absolute pointer actions
disable it while keys and sticks retain release protection. This is authored input
configuration, not a new editor mutation command; the existing input document
serialization preserves the setting. No separate MCP mutation endpoint was added.
After rebuilding and restarting, Computer Use verified locked-door feedback,
key pickup and door opening by clicking the models. A native bridge regression
checks pointer selection across pause/resume. Another oracle test compares 108
primitive ray intersections against Three.js, including rotated and scaled meshes.

Template colors now use project MaterialInstance assets referencing the shipped
Engine materials. When deploying over a previous build, remove stale generated
`.material` files and their sidecars from the deployed built-in template material
folders if their counterparts no longer exist in source; CMake's directory copy
does not remove obsolete files. Do not apply this cleanup to user projects.

### Animated character checkpoint

The starter Character contains both imported mesh parts (28,374 render vertices)
and separate Idle, Walk and HipHopDance clip identities inside its GLB. The supplied
Hip Hop Dancing FBX provided the model and dance; Idle and Walk are separately
authored skeletal motion, not extracted source clips. PrepareStarterCharacter.py
normalizes compatible skin bindings and authors these resources. Initial FBX to GLB
conversion used the official facebookincubator/FBX2glTF v0.9.7 tool with bake30 and
binary output; the conversion executable stays under ignored build tooling.

The Debug build, Engine.Character.Runtime pose/deformation checks, six Character
workspace UI checks, two native Character bridge checks and Player dance-frame
capture passed. The subsequent full gameplay regression did not pass: three of
six scenarios passed, Shooter weapon switching failed, Horror movement failed to
reach its target, and the five-target Shooter scenario timed out. These failures
remain unresolved at this checkpoint; the earlier five/six-scenario passing runs
predate the skinned character replacement and must not be treated as current acceptance.
