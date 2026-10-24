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
            float MaximumChange = 0.f;
            for (size_t Index = 0; Index < First->Vertices.size(); ++Index)
            {
                const auto Position = Second->Vertices[Index].Position;
                Require(std::isfinite(Position.x) && std::isfinite(Position.y) && std::isfinite(Position.z), "Character pose contains invalid coordinates");
                MaximumChange = std::max(MaximumChange, Vector3::Distance(First->Vertices[Index].Position, Position));
            }
            Require(MaximumChange > 0.0001f, "Character animation does not deform the mesh: " + Identifier);
            const auto Height = Second->Bounds.Maximum.y - Second->Bounds.Minimum.y;
            Require(Height > 1.f && Height < 2.5f, "Character pose has incorrect units or skinning");
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
