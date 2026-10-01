#include "Rendering/SceneExtractor.h"

#include "Core/Threading/ThreadContext.h"
#include "World/Scene.h"
#include "World/Components/CameraComponent.h"
#include "World/GameObject.h"
#include "World/Components/LightComponent.h"
#include "World/Components/MeshRendererComponent.h"

#include <unordered_map>

struct SceneExtractor::ExtractionContext
{
    std::unordered_map<const GameObject*, Matrix> WorldMatrices;
    std::unordered_map<const GameObject*, Transform> WorldTransforms;
    std::vector<const GameObject*> Ancestors;

    Matrix GetWorldMatrix(const GameObject& ObjectInstance)
    {
        const auto Cached = WorldMatrices.find(&ObjectInstance);
        if (Cached != WorldMatrices.end())
        {
            return Cached->second;
        }
        Ancestors.clear();
        const GameObject* Current = &ObjectInstance;
        Matrix World = Matrix::Identity;
        while (Current != nullptr)
        {
            const auto Existing = WorldMatrices.find(Current);
            if (Existing != WorldMatrices.end())
            {
                World = Existing->second;
                break;
            }
            Ancestors.push_back(Current);
            Current = Current->GetParent();
        }
        for (auto Ancestor = Ancestors.rbegin(); Ancestor != Ancestors.rend(); ++Ancestor)
        {
            World = (*Ancestor)->GetTransform().GetMatrix() * World;
            WorldMatrices.emplace(*Ancestor, World);
        }
        return World;
    }

    const Transform& GetWorldTransform(const GameObject& ObjectInstance)
    {
        const auto Existing = WorldTransforms.find(&ObjectInstance);
        if (Existing != WorldTransforms.end())
        {
            return Existing->second;
        }
        Transform WorldTransform;
        GetWorldMatrix(ObjectInstance).Decompose(WorldTransform.Scale, WorldTransform.Rotation, WorldTransform.Position);
        return WorldTransforms.emplace(&ObjectInstance, WorldTransform).first->second;
    }
};

void SceneExtractor::Extract(const Scene& SourceScene, RenderScene& Output) const
{
    AssertGameThread();

    Output.Clear();
    ExtractionContext Context;
    Context.WorldMatrices.reserve(SourceScene.GetObjectCount());
    ExtractRenderableObjects(SourceScene, Output, Context);
    ExtractCamera(SourceScene, Output, Context);
    ExtractLights(SourceScene, Output, Context);
}

void SceneExtractor::ExtractRenderableObjects(const Scene& SourceScene, RenderScene& Output, ExtractionContext& Context) const
{
    for (GameObject* Object : SourceScene.GetAllObjects())
    {
        if (Object == nullptr || !Object->IsActiveInHierarchy() || !Object->IsVisual())
        {
            continue;
        }

        for (Component* ComponentInstance : Object->GetAllComponents())
        {
            MeshRendererComponent* MeshRenderer = dynamic_cast<MeshRendererComponent*>(ComponentInstance);
            if (MeshRenderer == nullptr || !MeshRenderer->IsEnabled() || !MeshRenderer->bVisible)
            {
                continue;
            }

            RenderObject Extracted;
            Extracted.WorldMatrix = Context.GetWorldMatrix(*Object);
            Extracted.Mesh = MeshRenderer->Mesh;
            Extracted.MaterialSnapshot = MeshRenderer->MaterialSnapshot;
            Extracted.BaseColor = MeshRenderer->BaseColorFactor;
            if (!Extracted.MaterialSnapshot && !MeshRenderer->MaterialDiagnostic.empty())
            {
                Extracted.BaseColor = Color(1.f, 0.f, 1.f, 1.f);
                Extracted.DiagnosticMessage = MeshRenderer->MaterialDiagnostic;
            }
            AxisAlignedBounds Bounds = MeshRenderer->LocalBounds;
            if (Extracted.MaterialSnapshot)
            {
                const float Expansion = Extracted.MaterialSnapshot->Material->Definition.BoundsExpansion;
                Bounds.Minimum -= Vector3(Expansion, Expansion, Expansion);
                Bounds.Maximum += Vector3(Expansion, Expansion, Expansion);
            }
            Extracted.Bounds = Bounds.TransformedBy(Extracted.WorldMatrix);
            Extracted.bVisible = true;
            Extracted.bMissingAsset = MeshRenderer->bMissingAsset;
            Extracted.bUsePlaceholder = MeshRenderer->bMissingAsset || (MeshRenderer->bPendingAsset && !MeshRenderer->Mesh.IsValid());
            if (MeshRenderer->bMissingAsset)
            {
                Extracted.DiagnosticMessage = "Missing or failed mesh asset on '" + Object->GetName() + "'";
            }
            if (MeshRenderer->Submeshes.empty())
            {
                Output.Objects.push_back(Extracted);
            }
            else
            {
                for (const auto& Submesh : MeshRenderer->Submeshes)
                {
                    RenderObject SlotObject = Extracted;
                    SlotObject.IndexOffset = Submesh.IndexOffset;
                    SlotObject.IndexCount = Submesh.IndexCount;
                    const auto Slot = MeshRenderer->SlotSnapshots.find(Submesh.MaterialSlot);
                    if (Slot != MeshRenderer->SlotSnapshots.end())
                    {
                        SlotObject.MaterialSnapshot = Slot->second;
                        AxisAlignedBounds SlotBounds = MeshRenderer->LocalBounds;
                        const float Expansion = Slot->second->Material->Definition.BoundsExpansion;
                        SlotBounds.Minimum -= Vector3(Expansion, Expansion, Expansion);
                        SlotBounds.Maximum += Vector3(Expansion, Expansion, Expansion);
                        SlotObject.Bounds = SlotBounds.TransformedBy(SlotObject.WorldMatrix);
                    }
                    Output.Objects.push_back(std::move(SlotObject));
                }
            }
        }
    }
}

void SceneExtractor::ExtractCamera(const Scene& SourceScene, RenderScene& Output, ExtractionContext& Context) const
{
    CameraComponent* PrimaryCamera = SourceScene.FindPrimaryCamera();
    GameObject* PrimaryOwner = nullptr;
    if (PrimaryCamera != nullptr)
    {
        PrimaryOwner = PrimaryCamera->GetGameObject();
    }

    if (PrimaryCamera == nullptr || PrimaryOwner == nullptr)
    {
        Output.Camera.bValid = false;
        return;
    }

    const Transform WorldTransform = Context.GetWorldTransform(*PrimaryOwner);
    const Vector3 Position = WorldTransform.Position;
    const Vector3 Forward = Vector3::Transform(Vector3::Forward, WorldTransform.Rotation);
    const Vector3 Up = Vector3::Transform(Vector3::Up, WorldTransform.Rotation);

    for (const auto& Effect : PrimaryCamera->PostProcessEffects.Entries)
    {
        if (Effect.bEnabled && Effect.Intensity > 0.f && Effect.Snapshot)
        {
            Output.PostProcessEffects.push_back({Effect.Snapshot, Effect.Intensity});
        }
    }
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

void SceneExtractor::ExtractLights(const Scene& SourceScene, RenderScene& Output, ExtractionContext& Context) const
{
    for (GameObject* Object : SourceScene.GetAllObjects())
    {
        if (Object == nullptr || !Object->IsActiveInHierarchy())
        {
            continue;
        }

        for (Component* ComponentInstance : Object->GetAllComponents())
        {
            LightComponent* Light = dynamic_cast<LightComponent*>(ComponentInstance);
            if (Light == nullptr || !Light->IsEnabled())
            {
                continue;
            }

            RenderLight Extracted;
            Extracted.Type = Light->Type;
            const Transform& WorldTransform = Context.GetWorldTransform(*Object);
            Extracted.Position = WorldTransform.Position;
            Extracted.Direction = Vector3::Transform(Vector3::Forward, WorldTransform.Rotation);
            Extracted.LightColor = Light->LightColor;
            Extracted.Intensity = Light->Intensity;
            Extracted.Range = Light->Range;
            Extracted.InnerConeAngle = Light->InnerConeAngle;
            Extracted.OuterConeAngle = Light->OuterConeAngle;
            Extracted.bCastShadows = Light->bCastShadows;
            Output.Lights.push_back(Extracted);
        }
    }
}
