#include "ViewportController.h"
#include "Commands/EditorCommands.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Components/MeshRendererComponent.h"
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
    C.Projection = Matrix::CreatePerspectiveFieldOfView(C.FieldOfView * 0.0174532925f, C.AspectRatio, C.NearPlane, C.FarPlane);
    C.ViewProjection = C.View * C.Projection;
    return C;
}
void ViewportController::ResetInput()
{
    AssertGameThread();
    Buttons = 0; Navigation = 0; Keys.clear(); Alt = Shift = Ctrl = false;
    // A lost pointer/focus cancels an uncommitted drag, never leaves a stuck gizmo.
    if (DraggingGizmo) if (auto* Object = Workspace.GetSelectedObject(Runtime.GetActiveScene())) Object->SetTransform(DragStart);
    DraggingGizmo = false;
}
void ViewportController::SetTool(const Json& Options)
{
    AssertGameThread();
    const std::string Tool = Options.value("tool", "translate");
    Operation = Tool == "rotate" ? 1 : Tool == "scale" ? 2 : Tool == "select" ? 3 : 0;
    Snap = Options.value("snap", false); WorldAxes = Options.value("space", "local") == "world";
    Grid = Options.value("grid", true);
}
void ViewportController::FocusSelection()
{
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
            float Yaw = std::atan2(Forward.x, Forward.z) + Dx * 0.005f;
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
    const Matrix Inv = C.ViewProjection.Invert();
    const Vector3 Near = Vector3::Transform(Vector3(X / Width * 2 - 1, 1 - Y / Height * 2, 0), Inv);
    const Vector3 Far = Vector3::Transform(Vector3(X / Width * 2 - 1, 1 - Y / Height * 2, 1), Inv);
    Vector3 Direction = Far - Near; Direction.Normalize();
    GameObject* Selected = nullptr; float Closest = std::numeric_limits<float>::max();
    for (auto* Object : Runtime.GetActiveScene()->GetAllObjects()) {
        auto* Mesh = Object->GetComponent<MeshRendererComponent>();
        if (!Object->IsActiveInHierarchy() || !Object->IsVisual() || !Mesh || !Mesh->bVisible || !Mesh->IsEnabled()) continue;
        const Matrix Inverse = Object->GetWorldMatrix().Invert();
        const Vector3 Origin = Vector3::Transform(Near, Inverse), Ray = Vector3::TransformNormal(Direction, Inverse);
        float Low = 0, High = Closest;
        const float O[3]{Origin.x, Origin.y, Origin.z}, D[3]{Ray.x, Ray.y, Ray.z};
        const float Min[3]{Mesh->LocalBounds.Minimum.x, Mesh->LocalBounds.Minimum.y, Mesh->LocalBounds.Minimum.z};
        const float Max[3]{Mesh->LocalBounds.Maximum.x, Mesh->LocalBounds.Maximum.y, Mesh->LocalBounds.Maximum.z};
        bool Hit = true;
        for (int I = 0; I < 3; ++I) {
            if (std::abs(D[I]) < 1e-7f) { if (O[I] < Min[I] || O[I] > Max[I]) Hit = false; }
            else { float A = (Min[I] - O[I]) / D[I], B = (Max[I] - O[I]) / D[I]; if (A > B) std::swap(A, B); Low = std::max(Low, A); High = std::min(High, B); }
        }
        if (Hit && Low <= High && Low < Closest) { Closest = Low; Selected = Object; }
    }
    if (Selected) Workspace.Select(Runtime.GetActiveScene(), Selected->GetObjectHandle()); else Workspace.ClearSelection();
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
        if (Move.LengthSquared() > 0) { Move.Normalize(); Move *= Dt * (Shift ? 12.f : 4.f); Position += Move; Target += Move; }
    }
    BuildOverlay(Dt);
    RenderSettings Settings; Settings.bGroundGrid = Grid;
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
    auto* Object = Workspace.GetSelectedObject(Runtime.GetActiveScene());
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
        {"tool", Operation}, {"navigation", Navigation}, {"heldKeys", Keys.size()}, {"canUndo", const_cast<SceneWorkspace&>(Workspace).GetCommands().CanUndo()}, {"selection", nullptr}, {"pickTargets",Json::array()}};
    const auto C = Camera();
    for (auto* Object : Runtime.GetActiveScene()->GetAllObjects()) if (Object->GetComponent<MeshRendererComponent>()) {
        const auto P = Vector3::Transform(Object->GetWorldPosition(),C.ViewProjection);
        Result["pickTargets"].push_back({{"id",Object->GetPersistentId()},{"x",(P.x+1)*Width/2},{"y",(1-P.y)*Height/2}});
    }
    if (auto* Object = Workspace.GetSelectedObject(Runtime.GetActiveScene())) Result["selection"] = {
        {"id", Object->GetPersistentId()}, {"name", Object->GetName()}, {"position", V(Object->GetTransform().Position)},
        {"scale", V(Object->GetTransform().Scale)}};
    return Result;
}
