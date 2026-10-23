#pragma once
#include "Engine.h"
#include "Documents/SceneWorkspace.h"
#include <nlohmann/json.hpp>
#include <unordered_set>
struct ImGuiContext;
class NativeBridge;
class ViewportController
{
public:
    ViewportController(Engine& Runtime, const ProjectSession& Session);
    ~ViewportController();
    void Resize(int Width, int Height, float Scale);
    void Input(const nlohmann::json& Input);
    void Tick(float Dt);
    void SetAssetPreview(NativeBridge* Value) { AssetPreview = Value; }
    nlohmann::json Status() const;
    void ResetInput();
    void SetTool(const nlohmann::json& Options);
    SceneWorkspace& GetWorkspace() { return Workspace; }
private:
    void Pick(float X, float Y);
    void BuildAssetOverlay();
    NativeBridge* AssetPreview = nullptr;
    bool bPreviewFramed = false;
    nlohmann::json CollisionDraft;
    bool bCollisionDragging = false;
    uint64_t CollisionStartRevision = 0;
    void FocusSelection();
    void BuildOverlay(float Dt);
    RenderCamera Camera() const;
    Engine& Runtime;
    SceneWorkspace Workspace;
    ImGuiContext* Gui = nullptr;
    Vector3 Position{4.f, 3.f, 6.f}, Target{0.f, 0.f, 0.f};
    float Width = 64, Height = 64, Scale = 1;
    float MouseX = 0, MouseY = 0, PressX = 0, PressY = 0;
    int Buttons = 0, Navigation = 0, Operation = 0;
    bool Alt = false, Shift = false, Ctrl = false, DraggingGizmo = false, WorldAxes = false, Snap = false, Grid = true;
    Transform DragStart;
    std::unordered_set<std::string> Keys;
    uint64_t InputCount = 0, Edits = 0;
    bool Visible = false;
    RenderSettings ViewSettings;
};
