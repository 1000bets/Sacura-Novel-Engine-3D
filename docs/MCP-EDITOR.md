# Editor automation

The former Qt EditorMcpServer and SakuraMcpBridge executables have been removed with the Qt editor. Their old socket configuration and MCP commands are no longer available.

Electron uses the versioned, typed native bridge defined in `desktop-electron/bridge/contract.json` and `contract.d.ts`. `NativeBridge` validates and dispatches commands on the Game thread. Scene mutations share EditorCore command history, persistent identities and Play restrictions; asset documents use revision and external-change checks. Transport workers do not access the world.

Python Narrative authoring uses `story.addNarrative`, `story.setNarrativeInput`, `story.connectData`, `story.validate` and existing story save/undo/redo commands. Prefab instances use `prefab.create`, `prefab.instantiate`, `prefab.inspect` and `prefab.modify`; isolated prefab workspaces use `assetDocument.execute`. Export is invoked from trusted Electron main via `project.export`, then packaged by `game-export.cjs`.

Standalone Player exposes only `bridge.describe`, `game.state`, `game.action`, `game.advance`, `game.choose`, `game.pause` and `audio.resolveStartup`. `game.action` invokes an enabled action from a playing Python component's published game view; it rejects paused sessions and unavailable actions. It rejects editor mutations even when sent directly to the native bridge. No unrestricted shell or arbitrary Python execution command is exposed.

An external MCP adapter for the Electron bridge is not implemented by this migration. Do not describe the removed Qt MCP integration as operational.
