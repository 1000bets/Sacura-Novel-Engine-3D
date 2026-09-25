#include "Rendering/SceneExtractor.h"

#include "Core/Threading/ThreadContext.h"
#include "Game/Scene.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/LightComponent.h"
#include "Gameplay/MeshRendererComponent.h"

Matrix SceneExtractor::ComputeWorldMatrix(const GameObject& Object) const
{
    return Object.GetWorldMatrix();
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
        if (Object == nullptr || !Object->IsActiveInHierarchy() || !Object->IsVisual())
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
        Extracted.SurfaceMaterial = MeshRenderer->SurfaceMaterial;
        Extracted.BaseColorTexture = MeshRenderer->BaseColorTexture;
        Extracted.BaseColor = MeshRenderer->BaseColorFactor;
        Extracted.Bounds = MeshRenderer->LocalBounds.TransformedBy(Extracted.WorldMatrix);
        Extracted.bVisible = true;
        Extracted.bMissingAsset = MeshRenderer->bMissingAsset;
        Extracted.bUsePlaceholder = MeshRenderer->bMissingAsset || (MeshRenderer->bPendingAsset && !MeshRenderer->Mesh.IsValid());
        if (MeshRenderer->bMissingAsset)
        {
            Extracted.DiagnosticMessage = "Missing or failed mesh asset on '" + Object->GetName() + "'";
        }
        Output.Objects.push_back(Extracted);
    }
}

void SceneExtractor::ExtractCamera(const Scene& SourceScene, RenderScene& Output) const
{
    CameraComponent* PrimaryCamera = nullptr;
    GameObject* PrimaryOwner = nullptr;

    for (GameObject* Object : SourceScene.GetAllObjects())
    {
        if (Object == nullptr || !Object->IsActiveInHierarchy())
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

    // Cameras use world position/orientation; non-uniform parent scale is dropped via rotation extract.
    const Vector3 Position = PrimaryOwner->GetWorldPosition();
    const Vector3 Forward = PrimaryOwner->GetWorldForward();
    const Vector3 Up = PrimaryOwner->GetWorldUp();

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
        if (Object == nullptr || !Object->IsActiveInHierarchy())
        {
            continue;
        }

        LightComponent* Light = Object->GetComponent<LightComponent>();
        if (Light == nullptr || !Light->IsEnabled())
        {
            continue;
        }

        RenderLight Extracted;
        Extracted.Type = Light->Type;
        Extracted.Position = Object->GetWorldPosition();
        Extracted.Direction = Object->GetWorldForward();
        Extracted.LightColor = Light->LightColor;
        Extracted.Intensity = Light->Intensity;
        Extracted.Range = Light->Range;
        Extracted.InnerConeAngle = Light->InnerConeAngle;
        Extracted.OuterConeAngle = Light->OuterConeAngle;
        Extracted.bCastShadows = Light->bCastShadows;
        Output.Lights.push_back(Extracted);
    }
}
