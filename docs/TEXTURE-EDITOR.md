# Texture editor

Double-click a Texture asset in the native React Content Browser, or use Open in a shared asset reference field. The detached window is reused by project and asset identity and participates in project shutdown and asset-operation guards.

The workspace presents the texture on the left and independently scrolling source/compression/LOD details on the right. RGBA, RGB, individual R/G/B and alpha views, checker/dark/light backgrounds, fit, 1:1, numeric zoom and drag panning affect presentation only. Individual channels are displayed as opaque grayscale; RGB ignores source alpha. File/Edit mutation actions are disabled because inspection has no editable document yet.

The existing typed `assetDocument.execute` bridge supports Texture open/get/preview. It returns read-only snapshots loaded asynchronously through Engine AssetManager/TextureLoader, with real decoded dimensions, RGBA8 channel count, decoded memory size and color space. Failed loads return a diagnostic. Mutation operations are rejected. Preview PNGs are cached per window and bounded to 1024 pixels per side to respect the 8 MiB bridge response limit. Display sizing uses source dimensions; larger sources explicitly show the reduced preview dimensions.

Compression and mip settings are unavailable until supported by the texture processing pipeline and shared save/undo commands. No settings or source pixels are written by this viewer. Window, channel and zoom controls are UI-only and excluded from MCP mutations; existing asset catalogs provide identities and source paths.

Verification helper: launch Electron with `--screenshot-texture=M:/Sacura-Novel-Engine-3D/build/TextureEditor.png` and `SACURA_NATIVE_HOST` / `SACURA_NATIVE_PROJECT` set. It tests native snapshots, read-only rejection, window reuse and rendered channel/zoom/background behavior and saves wide/compact screenshots.
