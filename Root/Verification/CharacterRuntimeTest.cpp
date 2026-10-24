#include "Engine.h"
#include "Core/EnginePaths.h"
#include "Core/Threading/ThreadContext.h"
#include "Characters/CharacterDefinition.h"
#include "Characters/CharacterPose.h"
#include "Assets/Loaders/SkeletalLoader.h"
#include "Assets/Loaders/ModelLoader.h"
#include <cmath>
#include <iostream>

int main(int ArgumentCount, char** Arguments)
{
    SetCurrentThreadRole(ThreadRole::Game);
    EnginePaths::InitializeFromExecutable(std::filesystem::u8path(Arguments[0]));
    Engine Runtime;
    Runtime.InitializeHeadless({});
    int Result = 0;
    try
    {
        const auto Require = [](bool Value, const std::string& Message)
        {
            if (!Value)
            {
                throw std::runtime_error(Message);
            }
        };
        Require(ArgumentCount == 1 && Runtime.IsInitialized(), Runtime.GetInitializationError());
        auto& Registry = Runtime.GetAssetRegistry();
        AssetRegistryEntry Character;
        Require(Registry.TryGetByPath("/Engine/Characters/StarterHumanoid/StarterHumanoid.character", Character), "Starter character is missing");
        CharacterDefinition Definition;
        std::string Error;
        Require(CharacterDefinition::Read(Character.AbsolutePath, Definition, Error), Error);
        ModelLoader Models;
        SkeletalLoader Loader(Models);
        const auto Load = [&](const AssetKey& Key)
        {
            AssetLoadContext Context;
            const SubAssetRecord* SubAsset = nullptr;
            Require(Registry.TryResolveKey(Key, Context.Entry, &SubAsset), "Character dependency is missing");
            Context.Key = Key;
            BindLoadContextSubAsset(Context, SubAsset);
            AssetDiagnostic Diagnostic;
            auto Resource = Loader.Load(Context, Diagnostic);
            Require(Resource != nullptr, Diagnostic.Message);
            return Resource;
        };
        const auto Mesh = std::static_pointer_cast<const SkeletalMeshResource>(Load(Definition.Model));
        const auto Skeleton = std::static_pointer_cast<const SkeletonResource>(Load(Definition.Skeleton));
        CharacterPoseCache PoseCache;
        Require(Mesh->Mesh.Submeshes.size() == 2, "Character must retain both source mesh parts");
        Require(Mesh->Mesh.Vertices.size() > 15000, "Character must use the full imported mesh");
        for (const std::string Identifier : {"idle", "walk", "dance"})
        {
            const auto* Motion = Definition.FindAnimation(Identifier);
            Require(Motion != nullptr, "Character animation is missing: " + Identifier);
            const auto Clip = std::static_pointer_cast<const AnimationClipResource>(Load(Motion->Clip));
            const auto First = BuildSkinnedCharacterPose(*Mesh, *Skeleton, Clip.get(), 0.1f, true, Error);
            Require(First != nullptr, Error);
            const auto Second = BuildSkinnedCharacterPose(*Mesh, *Skeleton, Clip.get(), 0.6f, true, Error);
            Require(Second != nullptr, Error);
            Require(First->Indices == Mesh->Mesh.Indices && First->Vertices.size() == Mesh->Mesh.Vertices.size(),
                "Animation must preserve the prepared mesh topology");
            const auto Cached = PoseCache.BuildPose(Mesh, Skeleton, Clip, 0.1f, true, Error);
            Require(Cached != nullptr, Error);
            Require(PoseCache.BuildPose(Mesh, Skeleton, Clip, 0.1f, true, Error) == Cached,
                "Identical characters must share an immutable pose within a frame");
            Require(PoseCache.BuildPose(Mesh, Skeleton, Clip, 0.6f, true, Error) != Cached,
                "Different animation times must retain independent poses");
            float MaximumChange = 0.f;
            for (size_t Index = 0; Index < First->Vertices.size(); ++Index)
            {
                const auto Position = Second->Vertices[Index].Position;
                Require(std::isfinite(Position.x) && std::isfinite(Position.y) && std::isfinite(Position.z), "Character pose contains invalid coordinates");
                const auto& Vertex = Second->Vertices[Index];
                Require(std::abs(Vertex.Normal.LengthSquared() - 1.f) < 0.001f, "Skinned normals must remain normalized");
                if (Vertex.bHasTangent)
                {
                    const Vector3 Tangent(Vertex.Tangent.x, Vertex.Tangent.y, Vertex.Tangent.z);
                    Require(std::isfinite(Tangent.LengthSquared()) && std::abs(Tangent.LengthSquared() - 1.f) < 0.001f
                        && std::abs(Vertex.Normal.Dot(Tangent)) < 0.001f && std::abs(Vertex.Tangent.w) == 1.f,
                        "Skinned tangent frames must remain finite, orthogonal and normalized");
                }
                MaximumChange = std::max(MaximumChange, Vector3::Distance(First->Vertices[Index].Position, Position));
            }
            Require(MaximumChange > 0.0001f, "Character animation does not deform the mesh: " + Identifier);
            const auto Height = Second->Bounds.Maximum.y - Second->Bounds.Minimum.y;
            Require(Height > 1.f && Height < 2.5f, "Character pose has incorrect units or skinning");
            const uint64_t PreviousFrame = GetGameFrameIndex();
            SetGameFrameIndex(PreviousFrame + 1);
            Require(PoseCache.BuildPose(Mesh, Skeleton, Clip, 0.1f, true, Error) != Cached,
                "The pose cache must release its previous frame entries");
            std::cout << Identifier << ": " << First->Vertices.size() << " vertices, pose change " << MaximumChange << '\n';
        }
    }
    catch (const std::exception& Failure)
    {
        std::cerr << Failure.what() << '\n';
        Result = 1;
    }
    Runtime.Shutdown();
    return Result;
}
