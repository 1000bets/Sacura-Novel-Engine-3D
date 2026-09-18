#include "Rendering/SceneExtractor.h"

#include "Core/Threading/ThreadContext.h"
#include "Game/Scene.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/LightComponent.h"
#include "Gameplay/MeshRendererComponent.h"

#include <vector>

Matrix SceneExtractor::ComputeWorldMatrix(const GameObject& Object) const
{
    std::vector<const GameObject*> Chain;
    const GameObject* Current = &Object;
    while (Current != nullptr)
    {
        Chain.push_back(Current);
        Current = Current->GetParent();
    }

    Matrix World = Matrix::Identity;
    for (auto Iterator = Chain.rbegin(); Iterator != Chain.rend(); ++Iterator)
    {
        World = (*Iterator)->GetTransform().GetMatrix() * World;
    }

    return World;
}

void SceneExtractor::Extract(const Scene& SourceScene, RenderScene& Output) const
{
    AssertGameThread();

    Output.Clear();
    ExtractRenderableObjects(SourceScene, Output);
    ExtractCamera(SourceScene, Output);
    ExtractLights(SourceScene, Output);
}

void SceneExtractor::ExtractRenderableObjects(const Scene& SourceScene, RenderScene& Output) const
{
    for (GameObject* Object : SourceScene.GetAllObjects())
    {
        if (Object == nullptr || !Object->IsActive() || !Object->IsVisual())
        {
            continue;
        }

        MeshRendererComponent* MeshRenderer = Object->GetComponent<MeshRendererComponent>();
        if (MeshRenderer == nullptr || !MeshRenderer->IsEnabled() || !MeshRenderer->bVisible)
        {
            continue;
        }

        RenderObject Extracted;
        Extracted.WorldMatrix = ComputeWorldMatrix(*Object);
        Extracted.Mesh = MeshRenderer->Mesh;
        Extracted.Material = MeshRenderer->Material;
        Extracted.Bounds = MeshRenderer->LocalBounds.TransformedBy(Extracted.WorldMatrix);
        Extracted.bVisible = true;
        Output.Objects.push_back(Extracted);
    }
}

void SceneExtractor::ExtractCamera(const Scene& SourceScene, RenderScene& Output) const
{
    CameraComponent* PrimaryCamera = nullptr;
    GameObject* PrimaryOwner = nullptr;

    for (GameObject* Object : SourceScene.GetAllObjects())
    {
        if (Object == nullptr || !Object->IsActive())
        {
            continue;
        }

        CameraComponent* Camera = Object->GetComponent<CameraComponent>();
        if (Camera == nullptr || !Camera->IsEnabled())
        {
            continue;
        }

        if (PrimaryCamera == nullptr || Camera->bPrimary)
        {
            PrimaryCamera = Camera;
            PrimaryOwner = Object;
            if (Camera->bPrimary)
            {
                break;
            }
        }
    }

    if (PrimaryCamera == nullptr || PrimaryOwner == nullptr)
    {
        Output.Camera.bValid = false;
        return;
    }

    const Transform& CameraTransform = PrimaryOwner->GetTransform();
    const Vector3 Position = CameraTransform.Position;
    const Vector3 Forward = CameraTransform.GetForward();
    const Vector3 Up = CameraTransform.GetUp();

    RenderCamera& Camera = Output.Camera;
    Camera.Position = Position;
    Camera.NearPlane = PrimaryCamera->NearPlane;
    Camera.FarPlane = PrimaryCamera->FarPlane;
    Camera.FieldOfView = PrimaryCamera->FieldOfViewDegrees;
    Camera.AspectRatio = PrimaryCamera->AspectRatio;
    Camera.View = Matrix::CreateLookAt(Position, Position + Forward, Up);
    Camera.Projection = Matrix::CreatePerspectiveFieldOfView(
        PrimaryCamera->FieldOfViewDegrees * (3.14159265f / 180.f),
        PrimaryCamera->AspectRatio,
        PrimaryCamera->NearPlane,
        PrimaryCamera->FarPlane);
    Camera.ViewProjection = Camera.View * Camera.Projection;
    Camera.bValid = true;
}

void SceneExtractor::ExtractLights(const Scene& SourceScene, RenderScene& Output) const
{
    for (GameObject* Object : SourceScene.GetAllObjects())
    {
        if (Object == nullptr || !Object->IsActive())
        {
            continue;
        }

        LightComponent* Light = Object->GetComponent<LightComponent>();
        if (Light == nullptr || !Light->IsEnabled())
        {
            continue;
        }

        const Transform& LightTransform = Object->GetTransform();

        RenderLight Extracted;
        Extracted.Type = Light->Type;
        Extracted.Position = LightTransform.Position;
        Extracted.Direction = LightTransform.GetForward();
        Extracted.LightColor = Light->LightColor;
        Extracted.Intensity = Light->Intensity;
        Extracted.Range = Light->Range;
        Extracted.InnerConeAngle = Light->InnerConeAngle;
        Extracted.OuterConeAngle = Light->OuterConeAngle;
        Extracted.bCastShadows = Light->bCastShadows;
        Output.Lights.push_back(Extracted);
    }
}
