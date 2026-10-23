#include "World/Components/CharacterComponent.h"
#include "Characters/CharacterPose.h"
#include "World/GameObject.h"
#include "World/Scene.h"
#include "Core/Threading/ThreadContext.h"
#include <cmath>

void CharacterComponent::Resolve(AssetManager& Manager)
{
    AssertGameThread();
    Assets = &Manager;
    if (!bPreview)
    {
        AssetHandle<CharacterDefinition> Loaded;
        if (Character != LoadedCharacter)
        {
            Definition.reset();
            PosedMesh.reset();
            LoadedCharacter = Character;
            AnimationTime = 0.f;
        }
        Manager.LoadAsync<CharacterDefinition>(Character);
        if (!Manager.TryGetLoaded(Character, Loaded))
        {
            Definition.reset();
            PosedMesh.reset();
            bPoseDirty = true;
            Diagnostic = Manager.GetLastDiagnostic(Character).Message;
            bMissingAsset = Manager.GetLoadState(Character) == AssetLoadState::Failed;
            bPendingAsset = !bMissingAsset;
            return;
        }
        if (Definition != Loaded.GetSharedResource())
        {
            Definition = Loaded.GetSharedResource();
            bPoseDirty = true;
            AnimationTime = 0.f;
        }
    }
    if (!IsPlaying())
    {
        UpdatePose(0.f);
    }
}

bool CharacterComponent::PlayAnimation(const std::string& Identifier)
{
    AssertGameThread();
    if (!Definition || !Definition->FindAnimation(Identifier))
    {
        return false;
    }
    Animation = Identifier;
    PlayingAnimation.clear();
    AnimationTime = 0.f;
    UpdatePose(0.f);
    return true;
}

void CharacterComponent::BeginPlay()
{
    AnimationTime = 0.f;
    PlayingAnimation.clear();
    bHasPreviousPosition = false;
}

void CharacterComponent::EndPlay()
{
    AnimationTime = 0.f;
    PlayingAnimation.clear();
    bHasPreviousPosition = false;
}

void CharacterComponent::Tick(float DeltaTime)
{
    if (GetGameObject() && GetGameObject()->GetScene() && GetGameObject()->GetScene()->GetAssetManager())
    {
        Resolve(*GetGameObject()->GetScene()->GetAssetManager());
    }
    UpdatePose(DeltaTime);
}

void CharacterComponent::SetPreview(std::shared_ptr<const CharacterDefinition> Value)
{
    AssertGameThread();
    Definition = std::move(Value);
    if (!Definition)
    {
        PosedMesh.reset();
        bVisible = false;
    }
    bPoseDirty = true;
    bPreview = true;
    AnimationTime = 0.f;
    PlayingAnimation.clear();
    UpdatePose(0.f);
}

void CharacterComponent::AdvancePreview(float DeltaTime)
{
    UpdatePose(DeltaTime);
}

void CharacterComponent::UpdatePose(float DeltaTime)
{
    AssertGameThread();
    if (!Definition)
    {
        return;
    }
    bVisible = bPreview || Definition->bEnabled;
    BaseColorFactor = Definition->Color;
    std::string Selected = Animation;
    if (Selected.empty())
    {
        Selected = Definition->IdleAnimation;
    }
    if (DeltaTime > 0.f && GetGameObject())
    {
        const auto Position = GetGameObject()->GetWorldTransform().Position;
        if (bAutomaticWalk && bHasPreviousPosition && Vector3::DistanceSquared(Position, PreviousPosition) > 0.000001f
            && Definition->FindAnimation(Definition->WalkAnimation))
        {
            Selected = Definition->WalkAnimation;
        }
        PreviousPosition = Position;
        bHasPreviousPosition = true;
    }
    if (DeltaTime == 0.f && !bPoseDirty && PosedMesh && PlayingAnimation == Selected)
    {
        return;
    }
    bPoseDirty = true;
    if (PlayingAnimation != Selected)
    {
        PlayingAnimation = Selected;
        AnimationTime = 0.f;
    }
    const auto* Motion = Definition->FindAnimation(Selected);
    Diagnostic.clear();
    if (!Selected.empty() && !Motion)
    {
        Diagnostic = "Selected character animation is missing or disabled";
    }
    if (Motion)
    {
        AnimationTime += std::max(0.f, DeltaTime) * Motion->Speed;
    }
    if (!Definition->Model.IsValid())
    {
        std::string Pose = "idle";
        if (Motion)
        {
            Pose = Motion->Pose;
        }
        float PoseTime = AnimationTime;
        if (Motion && !Motion->bLoop)
        {
            PoseTime = std::min(PoseTime, 1.f);
        }
        PosedMesh = BuildStandardCharacterPose(Pose, PoseTime);
        bPoseDirty = false;
    }
    else if (Assets)
    {
        AssetHandle<SkeletalMeshResource> Model;
        AssetHandle<SkeletonResource> Skeleton;
        AssetHandle<AnimationClipResource> Clip;
        Assets->LoadAsync<SkeletalMeshResource>(Definition->Model);
        Assets->LoadAsync<SkeletonResource>(Definition->Skeleton);
        if (!Assets->TryGetLoaded(Definition->Model, Model) || !Assets->TryGetLoaded(Definition->Skeleton, Skeleton))
        {
            Diagnostic = Assets->GetLastDiagnostic(Definition->Model).Message + Assets->GetLastDiagnostic(Definition->Skeleton).Message;
            PosedMesh.reset();
            bPendingAsset = Diagnostic.empty();
            bMissingAsset = !Diagnostic.empty();
            return;
        }
        float Ratio = 0.f;
        bool bInPlace = false;
        if (Motion && Motion->Clip.IsValid())
        {
            Assets->LoadAsync<AnimationClipResource>(Motion->Clip);
            if (!Assets->TryGetLoaded(Motion->Clip, Clip))
            {
                Diagnostic = Assets->GetLastDiagnostic(Motion->Clip).Message;
            }
            else
            {
                const float Duration = Clip->Animation->duration();
                Ratio = std::min(AnimationTime / Duration, 1.f);
                if (Motion->bLoop)
                {
                    Ratio = std::fmod(AnimationTime, Duration) / Duration;
                }
                bInPlace = Motion->bInPlace;
            }
        }
        PosedMesh = BuildSkinnedCharacterPose(*Model, *Skeleton, Clip.Get(), Ratio, bInPlace, Diagnostic);
        bPoseDirty = Motion && Motion->Clip.IsValid() && !Clip.Get();
    }
    bMissingAsset = !PosedMesh;
    bPendingAsset = false;
    if (PosedMesh)
    {
        LocalBounds = PosedMesh->Bounds;
        Submeshes = PosedMesh->Submeshes;
    }
}
