#pragma once

// One context per owning thread. Contexts, atlases and draw lists must still
// remain thread-owned; only copied ImGuiOverlaySnapshot data crosses threads.
// This config is propagated by Diligent-Imgui to all ImGui consumers.
struct ImGuiContext;
extern thread_local ImGuiContext* SakuraImGuiContext;
#define GImGui SakuraImGuiContext
