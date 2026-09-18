# Sacura Novel Engine 3D — Agent Notes

## Rendering architecture

- Graphics abstraction: **Diligent Engine Core** (not raw D3D12/Vulkan/OpenGL, not SDL_GPU).
- Window / input / events: **SDL3**.
- Our code owns `Rendering` (Renderer, RenderScene, SceneExtractor, passes, materials). Gameplay never includes Diligent headers or native GPU API types (`VkInstance`, `ID3D12Device`, etc.).
- Only `Rendering/RHI` may talk to Diligent.
- Shaders: **HLSL** as the primary language.
- Backend selection: `Auto` | `D3D12` | `Vulkan` | `OpenGL`. On Windows, `Auto` resolves to D3D12.
- Pipeline model for the current horizon: **forward** rendering. Deferred is out of scope until explicitly planned.

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
| DiligentTools | Texture/Asset loaders, Diligent ImGui bridge | FetchContent → `Root/Engine/ThirdParty/DiligentTools/` |
| DiligentFX | High-level FX framework | FetchContent → `Root/Engine/ThirdParty/DiligentFX/` |
| DiligentSamples | Official tutorials/samples (sources) | FetchContent → `Root/Engine/ThirdParty/DiligentSamples/`; build with `-DSAKURA_BUILD_DILIGENT_SAMPLES=ON` |
| Dear ImGui | UI (docking `v*-docking`) | FetchContent → `Root/Engine/ThirdParty/imgui/_src/`; target `Sakura::ImGui` (not named `imgui`, so DiligentTools keeps its bundled ImGui for `Diligent-Imgui`) |
| PhysX | Physics (NVIDIA PhysX 5.6.1) | FetchContent → `Root/Engine/ThirdParty/PhysX/`; linked as `Sakura::PhysX` |

Fetched Diligent/PhysX/ImGui trees are gitignored. Do not copy their `.cpp`/`.h` around the project by hand.

Pinned version cache vars: `SAKURA_SDL3_GIT_TAG`, `SAKURA_DILIGENT_*_GIT_TAG`, `SAKURA_IMGUI_GIT_TAG`, `SAKURA_PHYSX_GIT_TAG`. Bump deliberately (keep Diligent modules on the same version).

- Engine links `Sakura::EngineThirdParty` (SDL3, Diligent backends, ImGui, PhysX, Tools/FX helpers).
- Editor links `Sakura::EditorThirdParty` plus Engine.
- Executables that need Diligent backend DLLs: `sakura_copy_engine_runtime_dlls(<target>)`.

## Coding conventions (engine)

- Identifiers: PascalCase, no abbreviations, no type-wrapper suffixes (`Ptr`, `Ref`, `Handle`).
- Booleans may use a `b` prefix (`bActive`, `bRunning`).
- Always use braces for `if` / loops; no single-line conditionals.
- Prefer complete names over short ones.
- Debug logging messages in English.
- Dominant build configuration: **Debug**.
