#include "Rendering/PresentationController.h"

#include "Core/Threading/ThreadContext.h"

bool PresentationController::Contains(RenderSurfaceId Surface, const NativeWindowInfo& Window) const
{
    AssertGameThread();
    const auto Existing = Presentations.find(Surface.Value);
    return Existing != Presentations.end() && Existing->second.Window.WindowHandle == Window.WindowHandle;
}

void PresentationController::Register(RenderSurfaceId Surface, const NativeWindowInfo& Window)
{
    AssertGameThread();
    Presentations[Surface.Value].Window = Window;
}

bool PresentationController::Unregister(RenderSurfaceId Surface)
{
    AssertGameThread();
    return Presentations.erase(Surface.Value) != 0;
}

void PresentationController::Resize(RenderSurfaceId Surface, uint32_t Width, uint32_t Height)
{
    AssertGameThread();
    auto Existing = Presentations.find(Surface.Value);
    if (Existing != Presentations.end())
    {
        Existing->second.Window.Width = Width;
        Existing->second.Window.Height = Height;
    }
}

void PresentationController::Configure(RenderSurfaceId Surface, bool bEditScene, const RenderCamera& Camera, const RenderSettings& Settings)
{
    AssertGameThread();
    auto Existing = Presentations.find(Surface.Value);
    if (Existing != Presentations.end())
    {
        Existing->second.bEditScene = bEditScene;
        Existing->second.Camera = Camera;
        Existing->second.Settings = Settings;
    }
}

void PresentationController::SetOverlay(RenderSurfaceId Surface, ImGuiOverlaySnapshot Overlay)
{
    AssertGameThread();
    auto Existing = Presentations.find(Surface.Value);
    if (Existing != Presentations.end())
    {
        Existing->second.Overlay = std::move(Overlay);
    }
}

void PresentationController::Clear()
{
    AssertGameThread();
    Presentations.clear();
}

std::unique_ptr<RenderFrameData> PresentationController::BuildFrame(uint64_t FrameIndex, Scene* ActiveWorld, Scene* EditWorld, float TimeSeconds)
{
    AssertGameThread();
    auto Frame = std::make_unique<RenderFrameData>();
    Frame->FrameIndex = FrameIndex;
    for (auto& Entry : Presentations)
    {
        PresentationState& Presentation = Entry.second;
        if (Presentation.Window.Width == 0 || Presentation.Window.Height == 0)
        {
            continue;
        }
        RenderViewFrame View;
        View.TimeSeconds = TimeSeconds;
        View.Surface = RenderSurfaceId{Entry.first};
        View.Settings = Presentation.Settings;
        Scene* World = ActiveWorld;
        if (Presentation.bEditScene)
        {
            World = EditWorld;
        }
        if (World != nullptr)
        {
            Extractor.Extract(*World, View.Scene);
        }
        if (Presentation.Camera.bValid)
        {
            View.Scene.Camera = Presentation.Camera;
        }
        RenderCamera& Camera = View.Scene.Camera;
        if (Camera.bValid)
        {
            Camera.AspectRatio = static_cast<float>(Presentation.Window.Width) / static_cast<float>(Presentation.Window.Height);
            Camera.Projection = Matrix::CreatePerspectiveFieldOfView(
                Camera.FieldOfView * 0.0174532925f, Camera.AspectRatio, Camera.NearPlane, Camera.FarPlane);
            Camera.ViewProjection = Camera.View * Camera.Projection;
        }
        View.Overlay = std::move(Presentation.Overlay);
        Frame->Views.push_back(std::move(View));
    }
    return Frame;
}
