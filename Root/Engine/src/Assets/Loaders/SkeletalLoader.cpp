#include "Assets/Loaders/SkeletalLoader.h"
#include "Characters/CharacterAnimationIO.h"
#include "Assets/Resources/AnimationClipResource.h"
#include "Assets/Resources/SkeletalMeshResource.h"
#include "Assets/Resources/SkeletonResource.h"
#include "Assets/Resources/SkinBinding.h"

#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/skeleton_utils.h"
#include "ozz/base/maths/quaternion.h"
#include "ozz/base/maths/transform.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/base/memory/unique_ptr.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <variant>

namespace
{
constexpr uint64_t MaxAssetFileBytes = 256ull * 1024ull * 1024ull;

DirectX::SimpleMath::Matrix ToMatrix(const fastgltf::math::fmat4x4& Source)
{
    DirectX::SimpleMath::Matrix Result;
    for (int Row = 0; Row < 4; ++Row)
    {
        for (int Column = 0; Column < 4; ++Column)
        {
            Result.m[Row][Column] = Source[Row][Column];
        }
    }
    return Result;
}

ozz::math::Transform ToOzzTransform(const fastgltf::Node& Node)
{
    ozz::math::Transform Result = ozz::math::Transform::identity();
    if (const fastgltf::TRS* Trs = std::get_if<fastgltf::TRS>(&Node.transform))
    {
        Result.translation = ozz::math::Float3(Trs->translation[0], Trs->translation[1], Trs->translation[2]);
        Result.rotation = ozz::math::Quaternion(Trs->rotation[0], Trs->rotation[1], Trs->rotation[2], Trs->rotation[3]);
        Result.scale = ozz::math::Float3(Trs->scale[0], Trs->scale[1], Trs->scale[2]);
        return Result;
    }

    const fastgltf::math::fmat4x4& Matrix = std::get<fastgltf::math::fmat4x4>(Node.transform);
    DirectX::SimpleMath::Matrix Simple = ToMatrix(Matrix);
    DirectX::SimpleMath::Vector3 Scale;
    DirectX::SimpleMath::Quaternion Rotation;
    DirectX::SimpleMath::Vector3 Translation;
    Simple.Decompose(Scale, Rotation, Translation);
    Result.translation = ozz::math::Float3(Translation.x, Translation.y, Translation.z);
    Result.rotation = ozz::math::Quaternion(Rotation.x, Rotation.y, Rotation.z, Rotation.w);
    Result.scale = ozz::math::Float3(Scale.x, Scale.y, Scale.z);
    return Result;
}

void BuildRawJoint(
    const fastgltf::Asset& Asset,
    size_t NodeIndex,
    const std::unordered_map<size_t, std::vector<size_t>>& ChildrenByParent,
    ozz::animation::offline::RawSkeleton::Joint& OutJoint)
{
    const fastgltf::Node& Node = Asset.nodes[NodeIndex];
    OutJoint.name = Node.name.empty() ? ("Joint_" + std::to_string(NodeIndex)) : std::string(Node.name);
    OutJoint.transform = ToOzzTransform(Node);

    auto Iterator = ChildrenByParent.find(NodeIndex);
    if (Iterator == ChildrenByParent.end())
    {
        return;
    }

    OutJoint.children.resize(Iterator->second.size());
    for (size_t ChildOrder = 0; ChildOrder < Iterator->second.size(); ++ChildOrder)
    {
        BuildRawJoint(Asset, Iterator->second[ChildOrder], ChildrenByParent, OutJoint.children[ChildOrder]);
    }
}

bool TryLoadGltfBinary(
    const std::filesystem::path& AbsolutePath,
    fastgltf::Asset& OutAsset,
    AssetDiagnostic& OutDiagnostic,
    const AssetKey& Key)
{
    std::error_code ErrorCode;
    const auto FileSize = std::filesystem::file_size(AbsolutePath, ErrorCode);
    if (ErrorCode || FileSize > MaxAssetFileBytes)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "SkeletalLoader",
            "GLB missing or exceeds 256MB",
            Key,
            AbsolutePath.string(),
            "SkeletalLoader");
        return false;
    }

    std::ifstream Input(AbsolutePath, std::ios::binary);
    std::vector<uint8_t> Bytes(static_cast<size_t>(FileSize));
    Input.read(reinterpret_cast<char*>(Bytes.data()), static_cast<std::streamsize>(FileSize));
    if (!Input)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "SkeletalLoader",
            "Failed to read GLB",
            Key,
            AbsolutePath.string(),
            "SkeletalLoader");
        return false;
    }

    auto DataBufferResult = fastgltf::GltfDataBuffer::FromBytes(
        reinterpret_cast<const std::byte*>(Bytes.data()),
        Bytes.size());
    if (!DataBufferResult)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "SkeletalLoader",
            "Failed to create GLB buffer",
            Key,
            AbsolutePath.string(),
            "SkeletalLoader");
        return false;
    }

    fastgltf::Parser Parser;
    auto AssetResult = Parser.loadGltfBinary(DataBufferResult.get(), AbsolutePath.parent_path(), fastgltf::Options::None);
    if (!AssetResult)
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "SkeletalLoader",
            std::string("fastgltf parse failed: ") + std::string(fastgltf::getErrorMessage(AssetResult.error())),
            Key,
            AbsolutePath.string(),
            "SkeletalLoader");
        return false;
    }

    for (const fastgltf::Buffer& Buffer : AssetResult->buffers)
    {
        if (std::holds_alternative<fastgltf::sources::URI>(Buffer.data))
        {
            OutDiagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::UnsupportedFeature,
                "SkeletalLoader",
                "External URI buffers are not allowed",
                Key,
                AbsolutePath.string(),
                "SkeletalLoader");
            return false;
        }
    }

    OutAsset = std::move(AssetResult.get());
    return true;
}

std::shared_ptr<const SkeletonResource> BuildSkeletonResource(
    const fastgltf::Asset& Asset,
    size_t SkinIndex,
    AssetDiagnostic& OutDiagnostic,
    const AssetKey& Key,
    const std::string& Path)
{
    if (SkinIndex >= Asset.skins.size())
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::NotFound, "SkeletalLoader", "Skin index out of range", Key, Path, "SkeletalLoader");
        return nullptr;
    }

    const fastgltf::Skin& Skin = Asset.skins[SkinIndex];
    if (Skin.joints.empty())
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "SkeletalLoader", "Skin has no joints", Key, Path, "SkeletalLoader");
        return nullptr;
    }

    std::unordered_map<size_t, std::vector<size_t>> ChildrenByParent;
    std::unordered_map<size_t, size_t> ParentByNode;
    for (size_t NodeIndex = 0; NodeIndex < Asset.nodes.size(); ++NodeIndex)
    {
        for (size_t ChildIndex : Asset.nodes[NodeIndex].children)
        {
            ChildrenByParent[NodeIndex].push_back(ChildIndex);
            ParentByNode[ChildIndex] = NodeIndex;
        }
    }

    std::unordered_map<size_t, bool> JointSet;
    for (size_t JointNode : Skin.joints)
    {
        JointSet[JointNode] = true;
    }

    size_t RootJoint = Skin.joints.front();
    if (Skin.skeleton.has_value())
    {
        RootJoint = *Skin.skeleton;
    }
    else
    {
        for (size_t JointNode : Skin.joints)
        {
            auto ParentIterator = ParentByNode.find(JointNode);
            if (ParentIterator == ParentByNode.end() || JointSet.find(ParentIterator->second) == JointSet.end())
            {
                RootJoint = JointNode;
                break;
            }
        }
    }

    while (ParentByNode.find(RootJoint) != ParentByNode.end())
    {
        RootJoint = ParentByNode.at(RootJoint);
    }
    ozz::animation::offline::RawSkeleton RawSkeleton;
    RawSkeleton.roots.resize(1);
    BuildRawJoint(Asset, RootJoint, ChildrenByParent, RawSkeleton.roots[0]);

    if (!RawSkeleton.Validate())
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "SkeletalLoader", "RawSkeleton validation failed", Key, Path, "SkeletalLoader");
        return nullptr;
    }

    ozz::animation::offline::SkeletonBuilder Builder;
    ozz::unique_ptr<ozz::animation::Skeleton> Built = Builder(RawSkeleton);
    if (!Built)
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InternalError, "SkeletalLoader", "SkeletonBuilder failed", Key, Path, "SkeletalLoader");
        return nullptr;
    }

    auto Resource = std::make_shared<SkeletonResource>();
    Resource->Skeleton = std::shared_ptr<const ozz::animation::Skeleton>(
        Built.release(),
        ozz::Deleter<ozz::animation::Skeleton>());
    return Resource;
}

bool RejectNonLinearAnimation(const fastgltf::Asset& Asset, AssetDiagnostic& OutDiagnostic, const AssetKey& Key, const std::string& Path)
{
    for (const fastgltf::Animation& Animation : Asset.animations)
    {
        for (const fastgltf::AnimationSampler& Sampler : Animation.samplers)
        {
            if (Sampler.interpolation != fastgltf::AnimationInterpolation::Linear)
            {
                OutDiagnostic = AssetDiagnostic::Fail(
                    AssetErrorCode::UnsupportedFeature,
                    "SkeletalLoader",
                    "Only LINEAR animation interpolation is supported",
                    Key,
                    Path,
                    "SkeletalLoader");
                return false;
            }
        }

        for (const fastgltf::AnimationChannel& Channel : Animation.channels)
        {
            if (Channel.path == fastgltf::AnimationPath::Weights)
            {
                OutDiagnostic = AssetDiagnostic::Fail(
                    AssetErrorCode::UnsupportedFeature,
                    "SkeletalLoader",
                    "Morph target animation is not supported",
                    Key,
                    Path,
                    "SkeletalLoader");
                return false;
            }
        }
    }

    return true;
}

std::shared_ptr<const AnimationClipResource> BuildAnimationClip(
    const fastgltf::Asset& Asset,
    size_t AnimationIndex,
    const ozz::animation::Skeleton& Skeleton,
    AssetDiagnostic& OutDiagnostic,
    const AssetKey& Key,
    const std::string& Path)
{
    if (AnimationIndex >= Asset.animations.size())
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::NotFound, "SkeletalLoader", "Animation index out of range", Key, Path, "SkeletalLoader");
        return nullptr;
    }

    const fastgltf::Animation& Source = Asset.animations[AnimationIndex];
    ozz::animation::offline::RawAnimation RawAnimation;
    RawAnimation.name = Source.name.empty() ? ("Anim_" + std::to_string(AnimationIndex)) : std::string(Source.name);
    RawAnimation.tracks.resize(static_cast<size_t>(Skeleton.num_joints()));

    float Duration = 0.f;
    std::unordered_map<std::string, int32_t> JointNameToIndex;
    for (int32_t JointIndex = 0; JointIndex < Skeleton.num_joints(); ++JointIndex)
    {
        JointNameToIndex[Skeleton.joint_names()[JointIndex]] = JointIndex;
    }

    for (const fastgltf::AnimationChannel& Channel : Source.channels)
    {
        if (!Channel.nodeIndex.has_value() || Channel.samplerIndex >= Source.samplers.size())
        {
            continue;
        }

        const fastgltf::Node& TargetNode = Asset.nodes[*Channel.nodeIndex];
        const std::string JointName = TargetNode.name.empty()
            ? ("Joint_" + std::to_string(*Channel.nodeIndex))
            : std::string(TargetNode.name);
        auto JointIterator = JointNameToIndex.find(JointName);
        if (JointIterator == JointNameToIndex.end())
        {
            continue;
        }

        const fastgltf::AnimationSampler& Sampler = Source.samplers[Channel.samplerIndex];
        const fastgltf::Accessor& InputAccessor = Asset.accessors[Sampler.inputAccessor];
        const fastgltf::Accessor& OutputAccessor = Asset.accessors[Sampler.outputAccessor];

        if (Sampler.interpolation != fastgltf::AnimationInterpolation::Linear || InputAccessor.count != OutputAccessor.count
            || InputAccessor.count == 0 || InputAccessor.count > 18001)
        {
            OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "SkeletalLoader",
                "Character GLB animations require matching LINEAR keyframes; bake STEP or CUBICSPLINE animation before import", Key);
            return nullptr;
        }
        std::vector<float> Times(InputAccessor.count);
        fastgltf::copyFromAccessor<float>(Asset, InputAccessor, Times.data());
        if (!Times.empty())
        {
            Duration = (std::max)(Duration, Times.back());
        }

        ozz::animation::offline::RawAnimation::JointTrack& Track = RawAnimation.tracks[static_cast<size_t>(JointIterator->second)];
        if (Channel.path == fastgltf::AnimationPath::Translation)
        {
            std::vector<fastgltf::math::fvec3> Values(OutputAccessor.count);
            fastgltf::copyFromAccessor<fastgltf::math::fvec3>(Asset, OutputAccessor, Values.data());
            Track.translations.resize(Values.size());
            for (size_t KeyIndex = 0; KeyIndex < Values.size(); ++KeyIndex)
            {
                Track.translations[KeyIndex].time = Times[KeyIndex];
                Track.translations[KeyIndex].value = ozz::math::Float3(Values[KeyIndex][0], Values[KeyIndex][1], Values[KeyIndex][2]);
            }
        }
        else if (Channel.path == fastgltf::AnimationPath::Rotation)
        {
            std::vector<fastgltf::math::fvec4> Values(OutputAccessor.count);
            fastgltf::copyFromAccessor<fastgltf::math::fvec4>(Asset, OutputAccessor, Values.data());
            Track.rotations.resize(Values.size());
            for (size_t KeyIndex = 0; KeyIndex < Values.size(); ++KeyIndex)
            {
                Track.rotations[KeyIndex].time = Times[KeyIndex];
                Track.rotations[KeyIndex].value = ozz::math::Quaternion(
                    Values[KeyIndex][0],
                    Values[KeyIndex][1],
                    Values[KeyIndex][2],
                    Values[KeyIndex][3]);
            }
        }
        else if (Channel.path == fastgltf::AnimationPath::Scale)
        {
            std::vector<fastgltf::math::fvec3> Values(OutputAccessor.count);
            fastgltf::copyFromAccessor<fastgltf::math::fvec3>(Asset, OutputAccessor, Values.data());
            Track.scales.resize(Values.size());
            for (size_t KeyIndex = 0; KeyIndex < Values.size(); ++KeyIndex)
            {
                Track.scales[KeyIndex].time = Times[KeyIndex];
                Track.scales[KeyIndex].value = ozz::math::Float3(Values[KeyIndex][0], Values[KeyIndex][1], Values[KeyIndex][2]);
            }
        }
    }

    for (int JointIndex = 0; JointIndex < Skeleton.num_joints(); ++JointIndex)
    {
        auto& Track = RawAnimation.tracks[JointIndex];
        const auto Rest = ozz::animation::GetJointLocalRestPose(Skeleton, JointIndex);
        if (Track.translations.empty())
        {
            Track.translations.push_back({0.f, Rest.translation});
        }
        if (Track.rotations.empty())
        {
            Track.rotations.push_back({0.f, Rest.rotation});
        }
        if (Track.scales.empty())
        {
            Track.scales.push_back({0.f, Rest.scale});
        }
    }
    RawAnimation.duration = Duration > 0.f ? Duration : 1.f;
    if (!RawAnimation.Validate())
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "SkeletalLoader", "RawAnimation validation failed", Key, Path, "SkeletalLoader");
        return nullptr;
    }

    ozz::animation::offline::AnimationBuilder Builder;
    ozz::unique_ptr<ozz::animation::Animation> Built = Builder(RawAnimation);
    if (!Built)
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InternalError, "SkeletalLoader", "AnimationBuilder failed", Key, Path, "SkeletalLoader");
        return nullptr;
    }

    auto Resource = std::make_shared<AnimationClipResource>();
    Resource->Name = RawAnimation.name;
    for (const auto* JointName : Skeleton.joint_names())
    {
        Resource->JointNames.emplace_back(JointName);
    }
    Resource->Animation = std::shared_ptr<const ozz::animation::Animation>(
        Built.release(),
        ozz::Deleter<ozz::animation::Animation>());
    return Resource;
}

bool BuildSkinBinding(
    const fastgltf::Asset& Asset,
    size_t SkinIndex,
    const fastgltf::Primitive& Primitive,
    SkinBinding& OutBinding,
    AssetDiagnostic& OutDiagnostic,
    const AssetKey& Key,
    const std::string& Path)
{
    if (SkinIndex >= Asset.skins.size())
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::NotFound, "SkeletalLoader", "Skin missing", Key, Path, "SkeletalLoader");
        return false;
    }

    const fastgltf::Skin& Skin = Asset.skins[SkinIndex];
    OutBinding.JointNodeIndices.assign(Skin.joints.begin(), Skin.joints.end());
    for (const auto JointNode : Skin.joints)
    {
        std::string Name(Asset.nodes[JointNode].name);
        if (Name.empty())
        {
            Name = "Joint_" + std::to_string(JointNode);
        }
        OutBinding.JointNames.push_back(std::move(Name));
    }
    OutBinding.InfluencesPerVertex = SkinBinding::MaxInfluencesPerVertex;

    OutBinding.InverseBindMatrices.assign(Skin.joints.size(), DirectX::SimpleMath::Matrix::Identity);
    if (Skin.inverseBindMatrices.has_value())
    {
        const fastgltf::Accessor& MatrixAccessor = Asset.accessors[*Skin.inverseBindMatrices];
        std::vector<fastgltf::math::fmat4x4> Matrices(MatrixAccessor.count);
        fastgltf::copyFromAccessor<fastgltf::math::fmat4x4>(Asset, MatrixAccessor, Matrices.data());
        OutBinding.InverseBindMatrices.resize(Matrices.size());
        for (size_t Index = 0; Index < Matrices.size(); ++Index)
        {
            OutBinding.InverseBindMatrices[Index] = ToMatrix(Matrices[Index]);
        }
    }

    const auto* JointsAttribute = Primitive.findAttribute("JOINTS_0");
    const auto* WeightsAttribute = Primitive.findAttribute("WEIGHTS_0");
    if (JointsAttribute == Primitive.attributes.end() || WeightsAttribute == Primitive.attributes.end())
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "SkeletalLoader", "Skinned primitive missing JOINTS_0/WEIGHTS_0", Key, Path, "SkeletalLoader");
        return false;
    }

    if (Primitive.findAttribute("JOINTS_1") != Primitive.attributes.end()
        || Primitive.findAttribute("WEIGHTS_1") != Primitive.attributes.end())
    {
        OutDiagnostic = AssetDiagnostic::Fail(
            AssetErrorCode::UnsupportedFeature,
            "SkeletalLoader",
            "More than 4 bone influences per vertex is not supported",
            Key,
            Path,
            "SkeletalLoader");
        return false;
    }

    const fastgltf::Accessor& JointsAccessor = Asset.accessors[JointsAttribute->accessorIndex];
    const fastgltf::Accessor& WeightsAccessor = Asset.accessors[WeightsAttribute->accessorIndex];

    std::vector<fastgltf::math::u16vec4> Joints(JointsAccessor.count);
    if (JointsAccessor.componentType == fastgltf::ComponentType::UnsignedShort)
    {
        fastgltf::copyFromAccessor<fastgltf::math::u16vec4>(Asset, JointsAccessor, Joints.data());
    }
    else if (JointsAccessor.componentType == fastgltf::ComponentType::UnsignedByte)
    {
        std::vector<fastgltf::math::u8vec4> ByteJoints(JointsAccessor.count);
        fastgltf::copyFromAccessor<fastgltf::math::u8vec4>(Asset, JointsAccessor, ByteJoints.data());
        for (size_t Index = 0; Index < ByteJoints.size(); ++Index)
        {
            Joints[Index] = fastgltf::math::u16vec4(
                ByteJoints[Index][0],
                ByteJoints[Index][1],
                ByteJoints[Index][2],
                ByteJoints[Index][3]);
        }
    }
    else
    {
        OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::UnsupportedFeature, "SkeletalLoader", "Unsupported JOINTS component type", Key, Path, "SkeletalLoader");
        return false;
    }

    std::vector<fastgltf::math::fvec4> Weights(WeightsAccessor.count);
    fastgltf::copyFromAccessor<fastgltf::math::fvec4>(Asset, WeightsAccessor, Weights.data());

    OutBinding.JointIndices.resize(Joints.size() * SkinBinding::MaxInfluencesPerVertex);
    OutBinding.JointWeights.resize(Weights.size() * SkinBinding::MaxInfluencesPerVertex);
    for (size_t VertexIndex = 0; VertexIndex < Joints.size(); ++VertexIndex)
    {
        for (uint32_t Influence = 0; Influence < SkinBinding::MaxInfluencesPerVertex; ++Influence)
        {
            OutBinding.JointIndices[VertexIndex * SkinBinding::MaxInfluencesPerVertex + Influence] = Joints[VertexIndex][Influence];
            OutBinding.JointWeights[VertexIndex * SkinBinding::MaxInfluencesPerVertex + Influence] = Weights[VertexIndex][Influence];
        }
    }

    return true;
}
}

SkeletalLoader::SkeletalLoader(ModelLoader& InSharedModelLoader)
    : SharedModelLoader(InSharedModelLoader)
{
}

std::shared_ptr<const void> SkeletalLoader::Load(const AssetLoadContext& Context, AssetDiagnostic& OutDiagnostic)
{
    const std::filesystem::path AbsolutePath(Context.Entry.AbsolutePath);
    if (Context.Entry.Metadata.Type == AnimationClipAssetType && AbsolutePath.extension() == ".animation")
    {
        std::string Error;
        const auto Resource = ReadCharacterAnimation(AbsolutePath, Error);
        if (!Resource)
        {
            OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "CharacterAnimation", Error, Context.Key);
        }
        return Resource;
    }
    fastgltf::Asset Asset;
    if (!TryLoadGltfBinary(AbsolutePath, Asset, OutDiagnostic, Context.Key))
    {
        return nullptr;
    }

    if (!RejectNonLinearAnimation(Asset, OutDiagnostic, Context.Key, Context.Entry.RelativePath))
    {
        return nullptr;
    }

    AssetType RequestedType = Context.Entry.Metadata.Type;
    int32_t SelectorIndex = 0;
    if (Context.SubAsset.has_value())
    {
        RequestedType = Context.SubAsset->Type;
        SelectorIndex = Context.SubAsset->Selector.Index;
        if (SelectorIndex < 0)
        {
            SelectorIndex = 0;
        }
    }

    if (RequestedType == SkeletonAssetType)
    {
        return BuildSkeletonResource(Asset, static_cast<size_t>(SelectorIndex), OutDiagnostic, Context.Key, Context.Entry.RelativePath);
    }

    if (RequestedType == AnimationClipAssetType)
    {
        if (Asset.skins.empty())
        {
            OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "SkeletalLoader", "Animation requires a skin/skeleton", Context.Key, Context.Entry.RelativePath, "SkeletalLoader");
            return nullptr;
        }

        std::shared_ptr<const SkeletonResource> Skeleton = BuildSkeletonResource(Asset, 0, OutDiagnostic, Context.Key, Context.Entry.RelativePath);
        if (!Skeleton || !Skeleton->Skeleton)
        {
            return nullptr;
        }

        return BuildAnimationClip(Asset, static_cast<size_t>(SelectorIndex), *Skeleton->Skeleton, OutDiagnostic, Context.Key, Context.Entry.RelativePath);
    }

    if (RequestedType == SkeletalMeshAssetType || RequestedType == SkinBindingAssetType || RequestedType == ModelAssetType)
    {
        if (Asset.skins.empty())
        {
            OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "SkeletalLoader", "No skins in GLB", Context.Key, Context.Entry.RelativePath, "SkeletalLoader");
            return nullptr;
        }

        std::shared_ptr<const ModelDocument> Document = SharedModelLoader.LoadOrGetSharedDocument(
            AbsolutePath,
            Context.Entry.Metadata.SourceFingerprint,
            OutDiagnostic);
        if (!Document)
        {
            OutDiagnostic.Key = Context.Key;
            return nullptr;
        }

        auto Resource = std::make_shared<SkeletalMeshResource>();
        auto Binding = std::make_shared<SkinBinding>();
        const auto SkinIndex = static_cast<size_t>(SelectorIndex);
        for (const auto& Node : Asset.nodes)
        {
            if (!Node.skinIndex || *Node.skinIndex != SkinIndex || !Node.meshIndex)
            {
                continue;
            }
            const auto MeshIndex = *Node.meshIndex;
            if (MeshIndex >= Document->Meshes.size() || !Document->Meshes[MeshIndex].Mesh)
            {
                OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "SkeletalLoader", "Mesh missing from shared document", Context.Key);
                return nullptr;
            }
            SkinBinding MeshBinding;
            for (const auto& Primitive : Asset.meshes[MeshIndex].primitives)
            {
                SkinBinding Part;
                if (!BuildSkinBinding(Asset, SkinIndex, Primitive, Part, OutDiagnostic, Context.Key, Context.Entry.RelativePath))
                {
                    return nullptr;
                }
                Binding->JointNames = Part.JointNames;
                Binding->JointNodeIndices = Part.JointNodeIndices;
                Binding->InverseBindMatrices = Part.InverseBindMatrices;
                MeshBinding.JointIndices.insert(MeshBinding.JointIndices.end(), Part.JointIndices.begin(), Part.JointIndices.end());
                MeshBinding.JointWeights.insert(MeshBinding.JointWeights.end(), Part.JointWeights.begin(), Part.JointWeights.end());
            }
            const auto& Mesh = *Document->Meshes[MeshIndex].Mesh;
            for (const uint32_t SourceIndex : Mesh.SourceVertexIndices)
            {
                const size_t Start = static_cast<size_t>(SourceIndex) * SkinBinding::MaxInfluencesPerVertex;
                if (Start + SkinBinding::MaxInfluencesPerVertex > MeshBinding.JointIndices.size())
                {
                    OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "SkeletalLoader", "Skin vertex mapping is invalid", Context.Key);
                    return nullptr;
                }
                for (size_t Influence = 0; Influence < SkinBinding::MaxInfluencesPerVertex; ++Influence)
                {
                    Binding->JointIndices.push_back(MeshBinding.JointIndices[Start + Influence]);
                    Binding->JointWeights.push_back(MeshBinding.JointWeights[Start + Influence]);
                }
            }
            const auto VertexOffset = static_cast<uint32_t>(Resource->Mesh.Vertices.size());
            const auto IndexOffset = static_cast<uint32_t>(Resource->Mesh.Indices.size());
            Resource->Mesh.Vertices.insert(Resource->Mesh.Vertices.end(), Mesh.Vertices.begin(), Mesh.Vertices.end());
            for (const auto Index : Mesh.Indices)
            {
                Resource->Mesh.Indices.push_back(Index + VertexOffset);
            }
            for (auto Submesh : Mesh.Submeshes)
            {
                Submesh.IndexOffset += IndexOffset;
                Resource->Mesh.Submeshes.push_back(Submesh);
            }
        }
        if (Resource->Mesh.Vertices.empty() || Binding->JointIndices.size() != Resource->Mesh.Vertices.size() * 4)
        {
            OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "SkeletalLoader", "Skin has no matching mesh vertices", Context.Key);
            return nullptr;
        }
        if (RequestedType == SkinBindingAssetType)
        {
            return Binding;
        }

        Resource->Binding = Binding;
        return Resource;
    }

    OutDiagnostic = AssetDiagnostic::Fail(AssetErrorCode::TypeMismatch, "SkeletalLoader", "Unsupported skeletal asset type", Context.Key, Context.Entry.RelativePath, "SkeletalLoader");
    return nullptr;
}
