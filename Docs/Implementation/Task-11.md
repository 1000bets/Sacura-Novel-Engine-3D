# Task 11 — Assets to rendering (static mesh path)

Date: 2026-09-20

## Changes

### Mesh / GPU path
- `RenderVertex`: Position + Normal + TexCoord + VertexColor (UV/orientation preserved from CPU `StaticMeshResource`).
- `RenderResourceManager::CreateMeshFromCpuData` uploads normals/UVs.
- `TestMesh.hlsl` unlit path: `baseColor * vertexColor * optional baseColorTexture`. **LightComponent does not affect this material in v1.**
- `ObjectConstants` carries `BaseColor` + `UseTexture`; mutable `g_BaseColorTexture` SRV.

### MeshRendererComponent
- Reflected `engine.MeshRendererComponent` with serializable `mesh_asset_id` / `mesh_sub_asset_id` / `material_asset_id` (stable Guid strings). Runtime `MeshHandle` / texture handles are not persisted.
- Flags `bMissingAsset` / `bPendingAsset`; missing/failed asset → magenta placeholder mesh + `PrintString` diagnostic (not silent default-quad mask).

### Resolve / upload
- `SceneAssetResolver` + `Engine::GetAssetGpuUploader()`: `LoadAsync` → `RequestUploadMesh/Texture` → resident handles before extract.
- `GpuUploader.Clear()` on StopPresenting / Shutdown so stale GPU handles are invalidated with session.

### Scene extract
- `RenderObject` carries `BaseColor`, `BaseColorTexture`, placeholder/missing flags for the renderer.

## Tests
- Round-trip / command tests still cover reflected MeshRenderer via serializer when present.
- Full GLB→pixels GPU visual: exercised via editor present path; automated framebuffer readback not added (same gap as Task 09).

## Commands
```bat
cmake --build build --config Debug --target Engine SakuraEditor SakuraTest
ctest --test-dir build -C Debug -R "Engine.Assets|Editor.Commands|Engine.Play|Engine.Story" --output-on-failure
```

## Remaining risks
- No committed multi-node checker GLB fixture in Samples yet (tests generate temporary GLB under AssetSystem).
- Hierarchy drop→reparent still incomplete (Task 10).
- Unlit only; PBR/lights not wired to materials.
