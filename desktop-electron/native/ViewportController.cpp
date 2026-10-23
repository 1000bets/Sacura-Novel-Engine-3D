#include "ViewportController.h"
#include "NativeBridge.h"
#include "Commands/EditorCommands.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Components/MeshRendererComponent.h"
#include "World/Components/LightComponent.h"
#include "Project/ProjectSession.h"
#include "Core/Threading/ThreadContext.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

using Json = nlohmann::json;
namespace {
Json V(const Vector3& P) { return {P.x, P.y, P.z}; }
bool ProjectLightIcon(const GameObject& Object, const RenderCamera& Camera, float Width, float Height,
    float Scale, ImVec2& Position)
{
    const Vector3 Clip = Vector3::Transform(Object.GetWorldPosition(), Camera.ViewProjection);
    if (!std::isfinite(Clip.x) || !std::isfinite(Clip.y) || !std::isfinite(Clip.z)
        || Clip.z <= 0.f || Clip.z >= 1.f) return false;
    Position = ImVec2((Clip.x + 1.f) * Width * .5f,
        (1.f - Clip.y) * Height * .5f - 18.f * Scale);
    const float Margin = 13.f * Scale;
    return Position.x >= -Margin && Position.x <= Width + Margin
        && Position.y >= -Margin && Position.y <= Height + Margin;
}
void DrawLightIcon(ImDrawList& Draw, const ImVec2& Center, float Scale, bool Selected)
{
    const auto P = [=](float X, float Y) { return ImVec2(Center.x + X * Scale, Center.y + Y * Scale); };
    const ImU32 Gold = IM_COL32(255, 211, 99, 255);
    Draw.AddLine(P(0, 11), P(0, 17), IM_COL32(255, 211, 99, 180), 1.5f * Scale);
    Draw.AddCircleFilled(Center, 12.f * Scale, IM_COL32(28, 30, 39, 225), 24);
    Draw.AddCircle(Center, 12.f * Scale,
        Selected ? IM_COL32(255, 226, 143, 255) : IM_COL32(188, 157, 90, 220), 24, 1.5f * Scale);
    Draw.AddCircleFilled(P(0, -3), 5.f * Scale, Gold, 16);
    Draw.AddLine(P(-3, 1), P(-2, 5), Gold, 2.f * Scale);
    Draw.AddLine(P(3, 1), P(2, 5), Gold, 2.f * Scale);
    Draw.AddLine(P(-2, 5), P(2, 5), Gold, 2.f * Scale);
    Draw.AddLine(P(-2, 8), P(2, 8), Gold, 2.f * Scale);
    Draw.AddLine(P(-9, -3), P(-7, -3), Gold, 1.5f * Scale);
    Draw.AddLine(P(9, -3), P(7, -3), Gold, 1.5f * Scale);
    Draw.AddLine(P(0, -10), P(0, -8), Gold, 1.5f * Scale);
}
ImGuiOverlaySnapshot Capture(const ImDrawData* Source)
{
    ImGuiOverlaySnapshot Result;
    if (!Source || !Source->Valid) return Result;
    static_assert(sizeof(ImDrawVert) == sizeof(ImGuiOverlayVertex));
    static_assert(sizeof(ImDrawIdx) == sizeof(uint16_t));
    Result.DisplayWidth = Source->DisplaySize.x; Result.DisplayHeight = Source->DisplaySize.y;
    for (int I = 0; I < Source->CmdListsCount; ++I) {
        const auto* List = Source->CmdLists[I];
        auto& Copy = Result.Lists.emplace_back();
        Copy.Vertices.resize(List->VtxBuffer.Size); Copy.Indices.resize(List->IdxBuffer.Size);
        std::memcpy(Copy.Vertices.data(), List->VtxBuffer.Data, Copy.Vertices.size() * sizeof(ImDrawVert));
        std::memcpy(Copy.Indices.data(), List->IdxBuffer.Data, Copy.Indices.size() * sizeof(ImDrawIdx));
        for (const auto& Command : List->CmdBuffer) if (!Command.UserCallback && Command.ElemCount)
            Copy.Commands.push_back({Command.IdxOffset, Command.ElemCount, Command.VtxOffset,
                Command.ClipRect.x, Command.ClipRect.y, Command.ClipRect.z, Command.ClipRect.w});
    }
    Result.bValid = !Result.Lists.empty(); return Result;
}
}
ViewportController::ViewportController(Engine& InRuntime, const ProjectSession& Session)
    : Runtime(InRuntime), Workspace(InRuntime)
{
    AssertGameThread();
    std::string Error;
    if (!Workspace.AdoptSessionScene(Session, Error)) throw std::runtime_error(Error);
    Gui = ImGui::CreateContext(); ImGui::SetCurrentContext(Gui);
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    unsigned char* Pixels; int W, H; ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&Pixels, &W, &H);
}
ViewportController::~ViewportController() { AssertGameThread(); ImGui::DestroyContext(Gui); }
void ViewportController::Resize(int W, int H, float InScale) { AssertGameThread(); Visible = W>0&&H>0; if(Visible){Width=W;Height=H;} Scale = std::max(0.25f, InScale); }
RenderCamera ViewportController::Camera() const
{
    RenderCamera C; C.bValid = true; C.Position = Position;
    C.View = Matrix::CreateLookAt(Position, Target, Vector3::Up);
    C.AspectRatio = Width / Height;
    if (AssetPreview && AssetPreview->GetPreviewScene())
    {
        C.NearPlane = std::max(.001f, (Position - Target).Length() / 1000.f);
        C.FarPlane = std::max(1000.f, (Position - Target).Length() * 10.f);
    }
    C.Projection = Matrix::CreatePerspectiveFieldOfView(C.FieldOfView * 0.0174532925f, C.AspectRatio, C.NearPlane, C.FarPlane);
    C.ViewProjection = C.View * C.Projection;
    return C;
}
void ViewportController::ResetInput()
{
    AssertGameThread();
    bCollisionDragging = false; CollisionDraft = nullptr;
    Buttons = 0; Navigation = 0; Keys.clear(); Alt = Shift = Ctrl = false;
    // A lost pointer/focus cancels an uncommitted drag, never leaves a stuck gizmo.
    if (DraggingGizmo) if (auto* Object = Workspace.GetSelectedObject(Runtime.GetActiveScene())) Object->SetTransform(DragStart);
    DraggingGizmo = false;
}
void ViewportController::SetTool(const Json& Options)
{
    AssertGameThread();
    const std::string Tool = Options.value("tool", "translate");
    if (Options.contains("cameraSpeed"))
    {
        const float RequestedSpeed = Options.at("cameraSpeed").get<float>();
        if (!std::isfinite(RequestedSpeed) || RequestedSpeed < 0.1f || RequestedSpeed > 100.f)
        {
            throw std::invalid_argument("Camera speed must be between 0.1 and 100 metres per second");
        }
        CameraSpeed = RequestedSpeed;
    }
    Operation = Tool == "rotate" ? 1 : Tool == "scale" ? 2 : Tool == "select" ? 3 : 0;
    Snap = Options.value("snap", false); WorldAxes = Options.value("space", "local") == "world";
    Grid = Options.value("grid", true);
    if (Options.contains("visualization"))
    {
        const auto* Mode = FindRenderVisualization(Options.at("visualization").get<std::string>());
        if (Mode) { ViewSettings.Visualization = Mode->Mode; }
    }
    if (Options.contains("depthRange"))
    {
        ViewSettings.VisualizationDepthRange = std::clamp(Options.at("depthRange").get<float>(), 0.01f, 100000.0f);
    }
}
void ViewportController::FocusSelection()
{
    if (AssetPreview && AssetPreview->GetPreviewMesh())
    {
        AssetHandle<StaticMeshResource> Resource;
        if (Runtime.GetAssetManager().TryGetLoaded(AssetPreview->GetPreviewMesh()->GetMeshAssetKey(), Resource))
        {
            Target = (Resource->Bounds.Minimum + Resource->Bounds.Maximum) * 0.5f;
            const float Radius = std::max(0.1f, (Resource->Bounds.Maximum - Resource->Bounds.Minimum).Length() * 0.5f);
            Position = Target + Vector3(1.5f, 1.f, -2.5f) * Radius;
            bPreviewFramed = true;
        }
        return;
    }
    if (auto* Object = Workspace.GetSelectedObject(Runtime.GetActiveScene())) {
        const auto Previous = Target; Target = Object->GetWorldPosition(); Position += Target - Previous;
    }
}
void ViewportController::Input(const Json& E)
{
    AssertGameThread();
    ++InputCount;
    const std::string Kind = E.at("kind");
    if (Kind == "reset") { ResetInput(); return; }
    Alt = E.value("alt", false); Shift = E.value("shift", false); Ctrl = E.value("ctrl", false);
    if (Kind == "key") {
        const std::string Code = E.at("code"); const bool Down = E.at("down");
        if (Down) Keys.insert(Code); else Keys.erase(Code);
        if (Down && !E.value("repeat", false)) {
            if (Code == "KeyF") FocusSelection();
            if (Code == "Escape") { ResetInput(); Workspace.ClearSelection(); }
            if (!Runtime.GetPlaySession().IsSimulating() && Ctrl && Code == "KeyZ") { ResetInput(); Shift ? Workspace.GetCommands().Redo() : Workspace.GetCommands().Undo(); }
            if (!Runtime.GetPlaySession().IsSimulating() && Ctrl && Code == "KeyY") { ResetInput(); Workspace.GetCommands().Redo(); }
            if (!Navigation && !Ctrl && !Alt) {
                if (Code == "KeyW") Operation = 0; if (Code == "KeyE") Operation = 1;
                if (Code == "KeyR") Operation = 2; if (Code == "KeyQ") Operation = 3;
            }
        }
        return;
    }
    if (Kind == "wheel") {
        Vector3 Offset = Position - Target;
        const float Factor = std::exp(std::clamp(E.value("delta", 0.f), -1000.f, 1000.f) * 0.001f);
        const float Distance = std::clamp(Offset.Length() * Factor, 0.2f, 500.f);
        Offset.Normalize(); Position = Target + Offset * Distance; return;
    }
    if (Kind != "pointer") return;
    const float X = E.at("x"), Y = E.at("y");
    const float Dx = (X - MouseX) / Scale, Dy = (Y - MouseY) / Scale;
    MouseX = X; MouseY = Y;
    const std::string Phase = E.at("phase");
    Buttons = E.value("buttons", 0);
    if (Phase == "down") {
        PressX = X; PressY = Y;
        const int Button = E.value("button", 0);
        Navigation = Button == 2 ? 1 : Button == 1 ? 2 : Alt ? 3 : 0;
    }
    if (Phase == "move" && Navigation) {
        Vector3 Forward = Target - Position; const float Distance = Forward.Length(); Forward.Normalize();
        Vector3 Right = Forward.Cross(Vector3::Up); Right.Normalize();
        if (Navigation == 2) {
            const Vector3 Up = Right.Cross(Forward);
            const Vector3 Delta = (-Right * Dx + Up * Dy) * std::max(0.001f, Distance * 0.0015f);
            Position += Delta; Target += Delta;
        } else {
            float Yaw = std::atan2(Forward.x, Forward.z) - Dx * 0.005f;
            float Pitch = std::clamp(std::asin(Forward.y) - Dy * 0.005f, -1.54f, 1.54f);
            const Vector3 Direction(std::sin(Yaw) * std::cos(Pitch), std::sin(Pitch), std::cos(Yaw) * std::cos(Pitch));
            if (Navigation == 3) Position = Target - Direction * Distance;
            else Target = Position + Direction * Distance;
        }
    }
    if (Phase == "up") {
        ImGui::SetCurrentContext(Gui);
        if (!Navigation && !DraggingGizmo && !ImGuizmo::IsOver() && E.value("button", 0) == 0 && std::hypot(X - PressX, Y - PressY) < 5 * Scale) Pick(X, Y);
        Navigation = 0;
    }
}
void ViewportController::Pick(float X, float Y)
{
    const auto C = Camera();
    if (AssetPreview && AssetPreview->GetPreviewScene())
    {
        const auto* Defaults = AssetPreview->GetPreviewCollision();
        int Selected = -1;
        float Closest = 24.f * Scale;
        if (Defaults)
        {
            for (size_t Index = 0; Index < Defaults->CollisionElements.size(); ++Index)
            {
                const auto Clip = Vector3::Transform(Defaults->CollisionElements[Index].Center, C.ViewProjection);
                const float Distance = std::hypot(X - (Clip.x + 1.f) * Width * 0.5f, Y - (1.f - Clip.y) * Height * 0.5f);
                if (Clip.z > 0.f && Clip.z < 1.f && Distance < Closest) { Closest = Distance; Selected = static_cast<int>(Index); }
            }
        }
        AssetPreview->SelectCollision(Selected);
        return;
    }
    GameObject* SelectedLight = nullptr;
    float LightDistance = (13.f * Scale) * (13.f * Scale);
    if (!Runtime.GetPlaySession().IsSimulating()) for (auto* Object : Runtime.GetActiveScene()->GetAllObjects()) {
        if (!Object->IsActiveInHierarchy()) continue;
        const auto* Light = Object->GetComponent<LightComponent>();
        if (!Light || !Light->IsEnabled()) continue;
        ImVec2 Icon;
        if (!ProjectLightIcon(*Object, C, Width, Height, Scale, Icon)) continue;
        const float Distance = (X - Icon.x) * (X - Icon.x) + (Y - Icon.y) * (Y - Icon.y);
        if (Distance < LightDistance) { LightDistance = Distance; SelectedLight = Object; }
    }
    if (SelectedLight) { Workspace.Select(Runtime.GetActiveScene(), SelectedLight->GetObjectHandle()); return; }
    auto* Selected = FindMeshAt(X, Y);
    if (Selected)
    {
        Workspace.Select(Runtime.GetActiveScene(), Selected->GetObjectHandle());
    }
    else
    {
        Workspace.ClearSelection();
    }
}
nlohmann::json ViewportController::PickMesh(float Horizontal, float Vertical) const
{
    AssertGameThread();
    auto* Object = FindMeshAt(Horizontal * Width, Vertical * Height);
    if (!Object)
    {
        return nullptr;
    }
    return {{"objectId", Object->GetPersistentId()},
        {"componentId", Object->GetComponent<MeshRendererComponent>()->GetPersistentId()}};
}
GameObject* ViewportController::FindMeshAt(float Horizontal, float Vertical) const
{
    const Matrix InverseViewProjection = Camera().ViewProjection.Invert();
    const Vector3 Near = Vector3::Transform(Vector3(Horizontal / Width * 2 - 1, 1 - Vertical / Height * 2, 0), InverseViewProjection);
    const Vector3 Far = Vector3::Transform(Vector3(Horizontal / Width * 2 - 1, 1 - Vertical / Height * 2, 1), InverseViewProjection);
    Vector3 Direction = Far - Near;
    Direction.Normalize();
    GameObject* Selected = nullptr;
    float Closest = std::numeric_limits<float>::max();
    for (auto* Object : Runtime.GetActiveScene()->GetAllObjects())
    {
        auto* Mesh = Object->GetComponent<MeshRendererComponent>();
        if (!Object->IsActiveInHierarchy() || !Object->IsVisual() || !Mesh || !Mesh->bVisible || !Mesh->IsEnabled())
        {
            continue;
        }
        const Matrix Inverse = Object->GetWorldMatrix().Invert();
        const Vector3 Origin = Vector3::Transform(Near, Inverse);
        const Vector3 Ray = Vector3::TransformNormal(Direction, Inverse);
        float Low = 0.f;
        float High = Closest;
        const float Origins[3]{Origin.x, Origin.y, Origin.z};
        const float Directions[3]{Ray.x, Ray.y, Ray.z};
        const float Minimum[3]{Mesh->LocalBounds.Minimum.x, Mesh->LocalBounds.Minimum.y, Mesh->LocalBounds.Minimum.z};
        const float Maximum[3]{Mesh->LocalBounds.Maximum.x, Mesh->LocalBounds.Maximum.y, Mesh->LocalBounds.Maximum.z};
        bool bHit = true;
        for (int Axis = 0; Axis < 3; ++Axis)
        {
            if (std::abs(Directions[Axis]) < 1e-7f)
            {
                if (Origins[Axis] < Minimum[Axis] || Origins[Axis] > Maximum[Axis])
                {
                    bHit = false;
                }
            }
            else
            {
                float Entry = (Minimum[Axis] - Origins[Axis]) / Directions[Axis];
                float Exit = (Maximum[Axis] - Origins[Axis]) / Directions[Axis];
                if (Entry > Exit)
                {
                    std::swap(Entry, Exit);
                }
                Low = std::max(Low, Entry);
                High = std::min(High, Exit);
            }
        }
        if (bHit && Low <= High && Low < Closest)
        {
            Closest = Low;
            Selected = Object;
        }
    }
    return Selected;
}
void ViewportController::Tick(float Dt)
{
    AssertGameThread();
    if (Navigation == 1) {
        Vector3 Forward = Target - Position; Forward.Normalize();
        Vector3 Right = Forward.Cross(Vector3::Up); Right.Normalize();
        Vector3 Move = Vector3::Zero;
        if (Keys.count("KeyW")) Move += Forward; if (Keys.count("KeyS")) Move -= Forward;
        if (Keys.count("KeyD")) Move += Right; if (Keys.count("KeyA")) Move -= Right;
        if (Keys.count("KeyE")) Move += Vector3::Up; if (Keys.count("KeyQ")) Move -= Vector3::Up;
        if (Move.LengthSquared() > 0)
        {
            Move.Normalize();
            Move *= Dt * CameraSpeed;
            if (Shift)
            {
                Move *= 3.f;
            }
            Position += Move;
            Target += Move;
        }
    }
    if (AssetPreview)
    {
        AssetPreview->TickPreview();
        if (AssetPreview->GetPreviewScene() && !bPreviewFramed) { FocusSelection(); }
    }
    BuildOverlay(Dt);
    RenderSettings Settings = ViewSettings; Settings.bGroundGrid = Grid;
    Runtime.ConfigureRenderSurface(RenderSurfaceId{1}, !Runtime.GetPlaySession().IsSimulating(), Runtime.GetPlaySession().IsSimulating() ? RenderCamera{} : Camera(), Settings);
}
void ViewportController::BuildOverlay(float Dt)
{
    ImGui::SetCurrentContext(Gui);
    auto& IO = ImGui::GetIO(); IO.DisplaySize = ImVec2(Width, Height); IO.DeltaTime = std::max(0.001f, Dt);
    IO.MousePos = ImVec2(MouseX, MouseY); IO.MouseDown[0] = (Buttons & 1) && !Navigation && !Alt;
    ImGui::NewFrame(); ImGuizmo::SetImGuiContext(Gui); ImGuizmo::BeginFrame();
    ImGuizmo::SetRect(0, 0, Width, Height); ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    if (AssetPreview && AssetPreview->GetPreviewScene())
    {
        BuildAssetOverlay();
        ImGui::Render();
        Runtime.SetRenderSurfaceImGuiOverlay(RenderSurfaceId{1}, Capture(ImGui::GetDrawData()));
        return;
    }
    auto* Object = Workspace.GetSelectedObject(Runtime.GetActiveScene());
    if (!Runtime.GetPlaySession().IsSimulating()) {
        const auto View = Camera();
        auto* Draw = ImGui::GetForegroundDrawList();
        for (auto* LightObject : Runtime.GetActiveScene()->GetAllObjects()) {
            if (!LightObject->IsActiveInHierarchy()) continue;
            const auto* Light = LightObject->GetComponent<LightComponent>();
            if (!Light || !Light->IsEnabled()) continue;
            ImVec2 Icon;
            if (ProjectLightIcon(*LightObject, View, Width, Height, Scale, Icon))
                DrawLightIcon(*Draw, Icon, Scale, LightObject == Object);
        }
    }
    if (Object && Operation != 3 && !Navigation && !Runtime.GetPlaySession().IsSimulating()) {
        const auto C = Camera(); Matrix World = Object->GetWorldMatrix();
        const auto Op = Operation == 1 ? ImGuizmo::ROTATE : Operation == 2 ? ImGuizmo::SCALE : ImGuizmo::TRANSLATE;
        const float Step = Operation == 1 ? 15.f : Operation == 2 ? 0.1f : 0.5f;
        float Steps[]{Step, Step, Step};
        const Transform Before = Object->GetTransform();
        const bool Changed = ImGuizmo::Manipulate(&C.View._11, &C.Projection._11, Op,
            WorldAxes ? ImGuizmo::WORLD : ImGuizmo::LOCAL, &World._11, nullptr, Snap ? Steps : nullptr);
        if (ImGuizmo::IsUsing() && !DraggingGizmo) { DragStart = Before; DraggingGizmo = true; }
        if (Changed) {
            Matrix Local = World;
            if (Object->GetParent()) Local *= Object->GetParent()->GetWorldMatrix().Invert();
            Transform T; if (Local.Decompose(T.Scale, T.Rotation, T.Position)) Object->SetTransform(T);
        }
    }
    if (DraggingGizmo && !(Buttons & 1)) {
        if (Object) {
            const auto Final = Object->GetTransform(); Object->SetTransform(DragStart);
            if (Workspace.GetCommands().Execute(MakeSetTransformCommand(Runtime.GetActiveScene(), Object->GetObjectHandle(), Final))) ++Edits;
        }
        DraggingGizmo = false;
    }
    if (Object) {
        const auto C = Camera();
        const Vector3 Clip = Vector3::Transform(Object->GetWorldPosition(), C.ViewProjection);
        if (Clip.z > 0 && Clip.z < 1) ImGui::GetForegroundDrawList()->AddCircle(ImVec2((Clip.x + 1) * Width / 2, (1 - Clip.y) * Height / 2), 9 * Scale, IM_COL32(255, 200, 60, 255), 20, 2);
    }
    ImGui::Render(); Runtime.SetRenderSurfaceImGuiOverlay(RenderSurfaceId{1}, Capture(ImGui::GetDrawData()));
}
Json ViewportController::Status() const
{
    AssertGameThread();
    Json Result{{"cameraPosition", V(Position)}, {"cameraTarget", V(Target)}, {"inputCount", InputCount}, {"edits", Edits},
        {"cameraSpeed", CameraSpeed},
        {"preview", AssetPreview && AssetPreview->GetPreviewScene()}, {"selectedCollision", AssetPreview ? AssetPreview->GetSelectedCollision() : -1}, {"tool", Operation}, {"navigation", Navigation}, {"heldKeys", Keys.size()}, {"canUndo", const_cast<SceneWorkspace&>(Workspace).GetCommands().CanUndo()}, {"selection", nullptr}, {"pickTargets",Json::array()}, {"lightIcons",Json::array()}};
    const auto C = Camera();
    Result["collisionTargets"] = Json::array();
    if (AssetPreview && AssetPreview->GetPreviewCollision())
    {
        for (const auto& Element : AssetPreview->GetPreviewCollision()->CollisionElements)
        {
            const auto Clip = Vector3::Transform(Element.Center, C.ViewProjection);
            Result["collisionTargets"].push_back({{"x", (Clip.x + 1.f) * Width * .5f}, {"y", (1.f - Clip.y) * Height * .5f}});
        }
    }
    for (auto* Object : Runtime.GetActiveScene()->GetAllObjects()) if (Object->GetComponent<MeshRendererComponent>()) {
        const auto P = Vector3::Transform(Object->GetWorldPosition(),C.ViewProjection);
        Result["pickTargets"].push_back({{"id",Object->GetPersistentId()},{"x",(P.x+1)*Width/2},{"y",(1-P.y)*Height/2}});
    }
    if (!Runtime.GetPlaySession().IsSimulating()) for (auto* Object : Runtime.GetActiveScene()->GetAllObjects()) {
        if (!Object->IsActiveInHierarchy()) continue;
        const auto* Light = Object->GetComponent<LightComponent>();
        if (!Light || !Light->IsEnabled()) continue;
        ImVec2 Icon;
        if (ProjectLightIcon(*Object, C, Width, Height, Scale, Icon))
            Result["lightIcons"].push_back({{"id",Object->GetPersistentId()},{"x",Icon.x},{"y",Icon.y}});
    }
    if (auto* Object = Workspace.GetSelectedObject(Runtime.GetActiveScene())) Result["selection"] = {
        {"id", Object->GetPersistentId()}, {"name", Object->GetName()}, {"position", V(Object->GetTransform().Position)},
        {"scale", V(Object->GetTransform().Scale)}};
    return Result;
}

void ViewportController::BuildAssetOverlay()
{
    const auto* Defaults = AssetPreview->GetPreviewCollision();
    if (!Defaults) { return; }
    Json Document = Defaults->ToJson();
    if (bCollisionDragging) { Document = CollisionDraft; }
    const auto CameraValue = Camera();
    auto* Draw = ImGui::GetForegroundDrawList();
    const auto DrawLine = [&](Vector3 Start, Vector3 End, ImU32 Color)
    {
        Start = Vector3::Transform(Start, CameraValue.ViewProjection);
        End = Vector3::Transform(End, CameraValue.ViewProjection);
        if (Start.z <= 0.f || End.z <= 0.f || Start.z >= 1.f || End.z >= 1.f) { return; }
        Draw->AddLine(ImVec2((Start.x + 1.f) * Width * .5f, (1.f - Start.y) * Height * .5f),
            ImVec2((End.x + 1.f) * Width * .5f, (1.f - End.y) * Height * .5f), Color, 2.f * Scale);
    };
    for (size_t Index = 0; Index < Document["collision"]["elements"].size(); ++Index)
    {
        const auto& Element = Document["collision"]["elements"][Index];
        const Vector3 Center(Element["center"][0], Element["center"][1], Element["center"][2]);
        const Vector3 Rotation(Element["rotation"][0], Element["rotation"][1], Element["rotation"][2]);
        const auto Orientation = Quaternion::CreateFromYawPitchRoll(Rotation.y * .0174532925f, Rotation.x * .0174532925f, Rotation.z * .0174532925f);
        const auto MatrixValue = Matrix::CreateFromQuaternion(Orientation) * Matrix::CreateTranslation(Center);
        ImU32 Color = IM_COL32(84, 220, 130, 240);
        if (static_cast<int>(Index) == AssetPreview->GetSelectedCollision()) { Color = IM_COL32(255, 208, 80, 255); }
        const auto Segment = [&](const Vector3& Start, const Vector3& End) { DrawLine(Vector3::Transform(Start, MatrixValue), Vector3::Transform(End, MatrixValue), Color); };
        const int Shape = Element["shape"];
        if (Shape == 3)
        {
            AssetHandle<StaticMeshResource> Resource;
            if (Runtime.GetAssetManager().TryGetLoaded(AssetPreview->GetPreviewMesh()->GetMeshAssetKey(), Resource))
            {
                for (size_t Triangle = 0; Triangle + 2 < Resource->Indices.size(); Triangle += 3)
                {
                    for (size_t Edge = 0; Edge < 3; ++Edge)
                    {
                        Segment(Resource->Vertices[Resource->Indices[Triangle + Edge]].Position,
                            Resource->Vertices[Resource->Indices[Triangle + (Edge + 1) % 3]].Position);
                    }
                }
            }
        }
        else if (Shape == 0)
        {
            const Vector3 Extent(Element["halfExtents"][0], Element["halfExtents"][1], Element["halfExtents"][2]);
            for (int Corner = 0; Corner < 8; ++Corner)
            {
                Vector3 Start(-Extent.x, -Extent.y, -Extent.z);
                if (Corner & 1) { Start.x = Extent.x; }
                if (Corner & 2) { Start.y = Extent.y; }
                if (Corner & 4) { Start.z = Extent.z; }
                for (int Axis = 0; Axis < 3; ++Axis)
                {
                    if (Corner & (1 << Axis)) { continue; }
                    auto End = Start;
                    (&End.x)[Axis] = (&Extent.x)[Axis];
                    Segment(Start, End);
                }
            }
        }
        else
        {
            const float Radius = Element["radius"];
            float HalfHeight = 0.f;
            if (Shape == 2) { HalfHeight = Element["capsuleHalfHeight"]; }
            for (int Plane = 0; Plane < 3; ++Plane)
            {
                for (int Step = 0; Step < 48; ++Step)
                {
                    const auto Point = [&](int Sample)
                    {
                        const float Angle = Sample * 6.2831853f / 48.f;
                        Vector3 Result = Vector3::Zero;
                        (&Result.x)[Plane] = std::cos(Angle) * Radius;
                        (&Result.x)[(Plane + 1) % 3] = std::sin(Angle) * Radius;
                        if (Result.y >= 0.f) { Result.y += HalfHeight; }
                        else { Result.y -= HalfHeight; }
                        return Result;
                    };
                    Segment(Point(Step), Point(Step + 1));
                }
            }
        }
        const auto Clip = Vector3::Transform(Center, CameraValue.ViewProjection);
        if (Clip.z > 0.f && Clip.z < 1.f) { Draw->AddCircle(ImVec2((Clip.x + 1.f) * Width * .5f, (1.f - Clip.y) * Height * .5f), 7.f * Scale, Color); }
    }
    const int Selected = AssetPreview->GetSelectedCollision();
    if (Selected >= 0 && Selected < static_cast<int>(Document["collision"]["elements"].size()) && Operation != 3 && !Navigation && AssetPreview->CanEditPreviewCollision())
    {
        auto& Element = Document["collision"]["elements"][Selected];
        float Translation[]{Element["center"][0], Element["center"][1], Element["center"][2]};
        float Rotation[]{Element["rotation"][0], Element["rotation"][1], Element["rotation"][2]};
        float Scaling[]{1.f, 1.f, 1.f};
        Matrix Manipulated;
        ImGuizmo::RecomposeMatrixFromComponents(Translation, Rotation, Scaling, &Manipulated._11);
        auto Manipulation = ImGuizmo::TRANSLATE;
        if (Operation == 1) { Manipulation = ImGuizmo::ROTATE; }
        if (Operation == 2) { Manipulation = ImGuizmo::SCALE; }
        const bool bSupportsScale = Element["shape"] != 3 || Operation != 2;
        if (bSupportsScale && ImGuizmo::Manipulate(&CameraValue.View._11, &CameraValue.Projection._11, Manipulation, ImGuizmo::LOCAL, &Manipulated._11))
        {
            ImGuizmo::DecomposeMatrixToComponents(&Manipulated._11, Translation, Rotation, Scaling);
            Element["center"] = {Translation[0], Translation[1], Translation[2]};
            Element["rotation"] = {Rotation[0], Rotation[1], Rotation[2]};
            if (Operation == 2)
            {
                for (int Axis = 0; Axis < 3; ++Axis) { Element["halfExtents"][Axis] = std::max(.0001f, Element["halfExtents"][Axis].get<float>() * std::abs(Scaling[Axis])); }
                float RadiusScale = std::max(std::abs(Scaling[0]), std::abs(Scaling[2]));
                if (Element["shape"] == 1) { RadiusScale = std::max(RadiusScale, std::abs(Scaling[1])); }
                Element["radius"] = std::max(.0001f, Element["radius"].get<float>() * RadiusScale);
                Element["capsuleHalfHeight"] = std::max(.0001f, Element["capsuleHalfHeight"].get<float>() * std::abs(Scaling[1]));
            }
            if (!bCollisionDragging) { CollisionStartRevision = AssetPreview->GetPreviewRevision(); }
            CollisionDraft = Document;
            bCollisionDragging = true;
        }
    }
    if (bCollisionDragging && !(Buttons & 1))
    {
        if (AssetPreview->CommitPreviewCollision(CollisionDraft, CollisionStartRevision)) { ++Edits; }
        bCollisionDragging = false;
        CollisionDraft = nullptr;
    }
}
