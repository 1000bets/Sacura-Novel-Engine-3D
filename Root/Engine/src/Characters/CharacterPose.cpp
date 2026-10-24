#include "Core/Threading/CpuProfileScope.h"
#include "Characters/CharacterPose.h"
#include "Core/Threading/ThreadContext.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/simd_math.h"
#include <algorithm>
#include <cmath>

std::shared_ptr<const StaticMeshResource> CharacterPoseCache::BuildPose(std::shared_ptr<const SkeletalMeshResource> Model,
    std::shared_ptr<const SkeletonResource> Skeleton, std::shared_ptr<const AnimationClipResource> Animation,
    float Ratio, bool bInPlace, std::string& Error)
{
    AssertGameThread();
    const uint64_t CurrentFrame = GetGameFrameIndex();
    if (FrameIndex != CurrentFrame)
    {
        Entries.clear();
        FrameIndex = CurrentFrame;
    }
    for (const auto& Cached : Entries)
    {
        if (Cached.Model == Model && Cached.Skeleton == Skeleton && Cached.Animation == Animation
            && Cached.Ratio == Ratio && Cached.bInPlace == bInPlace)
        {
            return Cached.Pose;
        }
    }
    auto Pose = BuildSkinnedCharacterPose(*Model, *Skeleton, Animation.get(), Ratio, bInPlace, Error);
    if (Pose)
    {
        Entries.push_back({std::move(Model), std::move(Skeleton), std::move(Animation), Ratio, bInPlace, Pose});
    }
    return Pose;
}

static void AppendCharacterBox(StaticMeshResource& Mesh, const Vector3& Center, const Vector3& Extents, const Matrix& Pose)
{
    Vector3 Corners[8];
    for (int Corner = 0; Corner < 8; ++Corner)
    {
        Vector3 Position = Center - Extents;
        if ((Corner & 1) != 0)
        {
            Position.x += Extents.x * 2.f;
        }
        if ((Corner & 2) != 0)
        {
            Position.y += Extents.y * 2.f;
        }
        if ((Corner & 4) != 0)
        {
            Position.z += Extents.z * 2.f;
        }
        Corners[Corner] = Vector3::Transform(Position, Pose);
    }
    const uint32_t Indices[] = {0,2,1,1,2,3,4,5,6,5,7,6,0,4,2,4,6,2,1,3,5,3,7,5,0,1,4,1,5,4,2,6,3,3,6,7};
    for (size_t Triangle = 0; Triangle < 36; Triangle += 3)
    {
        Vector3 Normal = (Corners[Indices[Triangle + 1]] - Corners[Indices[Triangle]])
            .Cross(Corners[Indices[Triangle + 2]] - Corners[Indices[Triangle]]);
        Normal.Normalize();
        for (size_t Corner = 0; Corner < 3; ++Corner)
        {
            StaticMeshVertex Vertex;
            Vertex.Position = Corners[Indices[Triangle + Corner]];
            Vertex.Normal = Normal;
            Vertex.bHasNormal = true;
            Mesh.Bounds.Minimum = Vector3::Min(Mesh.Bounds.Minimum, Vertex.Position);
            Mesh.Bounds.Maximum = Vector3::Max(Mesh.Bounds.Maximum, Vertex.Position);
            Mesh.Indices.push_back(static_cast<uint32_t>(Mesh.Vertices.size()));
            Mesh.Vertices.push_back(Vertex);
        }
    }
}

std::shared_ptr<const StaticMeshResource> BuildStandardCharacterPose(const std::string& Pose, float Time)
{
    auto Mesh = std::make_shared<StaticMeshResource>();
    float LegSwing = 0.f;
    float ArmLift = 0.f;
    float HeadTilt = 0.f;
    float HeightOffset = 0.f;
    if (Pose == "walk")
    {
        LegSwing = std::sin(Time * 7.f) * 0.5f;
    }
    if (Pose == "sit")
    {
        LegSwing = 1.4f;
        HeightOffset = -0.4f;
    }
    if (Pose == "wave")
    {
        ArmLift = 2.4f + std::sin(Time * 8.f) * 0.3f;
    }
    if (Pose == "dance")
    {
        ArmLift = 1.9f + std::sin(Time * 5.f) * 0.4f;
        LegSwing = std::sin(Time * 5.f) * 0.3f;
        HeightOffset = std::abs(std::sin(Time * 5.f)) * 0.1f;
    }
    if (Pose == "smile")
    {
        HeadTilt = -0.15f;
        ArmLift = 0.6f;
    }
    if (Pose == "think")
    {
        HeadTilt = 0.2f;
        ArmLift = 2.6f;
    }
    if (Pose == "sad")
    {
        HeadTilt = 0.3f;
    }
    const auto Root = Matrix::CreateTranslation(0.f, HeightOffset, 0.f);
    AppendCharacterBox(*Mesh, {0.f, 1.15f, 0.f}, {0.22f, 0.3f, 0.13f}, Root);
    AppendCharacterBox(*Mesh, Vector3::Zero, {0.16f, 0.19f, 0.15f},
        Matrix::CreateRotationZ(HeadTilt) * Matrix::CreateTranslation(0.f, 1.67f, 0.f) * Root);
    for (int Side : {-1, 1})
    {
        float LegAngle = LegSwing * static_cast<float>(Side);
        float ArmAngle = ArmLift;
        if (Pose == "sit")
        {
            LegAngle = LegSwing;
        }
        if (Side < 0)
        {
            ArmAngle = 0.f;
        }
        AppendCharacterBox(*Mesh, {0.f, -0.4f, 0.f}, {0.09f, 0.4f, 0.1f},
            Matrix::CreateRotationX(LegAngle) * Matrix::CreateTranslation(Side * 0.12f, 0.85f, 0.f) * Root);
        AppendCharacterBox(*Mesh, {0.f, -0.28f, 0.f}, {0.07f, 0.28f, 0.07f},
            Matrix::CreateRotationX(-LegAngle) * Matrix::CreateRotationZ(ArmAngle)
            * Matrix::CreateTranslation(Side * 0.31f, 1.4f, 0.f) * Root);
    }
    PrepareSurfaceVertices(*Mesh);
    return Mesh;
}

std::shared_ptr<const StaticMeshResource> BuildSkinnedCharacterPose(const SkeletalMeshResource& Mesh,
    const SkeletonResource& Skeleton, const AnimationClipResource* Animation, float Ratio, bool bInPlace, std::string& Error)
{
    CpuProfileScope CpuMeasurement("Character.Skin");
    if (!Mesh.Binding || !Skeleton.Skeleton)
    {
        Error = "Character requires a skeleton and skin binding";
        return nullptr;
    }
    const auto& Rig = *Skeleton.Skeleton;
    const auto& Binding = *Mesh.Binding;
    std::vector<ozz::math::SoaTransform> Locals(Rig.joint_rest_poses().begin(), Rig.joint_rest_poses().end());
    if (Animation)
    {
        if (Animation->JointNames.size() != static_cast<size_t>(Rig.num_joints()))
        {
            Error = "Animation skeleton does not match character skeleton";
            return nullptr;
        }
        for (int Joint = 0; Joint < Rig.num_joints(); ++Joint)
        {
            if (Animation->JointNames[Joint] != Rig.joint_names()[Joint])
            {
                Error = "Animation joint order does not match character skeleton";
                return nullptr;
            }
        }
        ozz::animation::SamplingJob::Context Context(Rig.num_joints());
        ozz::animation::SamplingJob Sampling;
        Sampling.animation = Animation->Animation.get();
        Sampling.context = &Context;
        Sampling.ratio = std::clamp(Ratio, 0.f, 1.f);
        Sampling.output = ozz::make_span(Locals);
        if (!Sampling.Run())
        {
            Error = "Character animation sampling failed";
            return nullptr;
        }
        if (bInPlace)
        {
            for (int Joint = 0; Joint < Rig.num_joints(); ++Joint)
            {
                std::string Name = Rig.joint_names()[Joint];
                std::transform(Name.begin(), Name.end(), Name.begin(), [](unsigned char Value)
                { return static_cast<char>(std::tolower(Value)); });
                if (Rig.joint_parents()[Joint] < 0 || Name.find("hips") != std::string::npos || Name.find("pelvis") != std::string::npos)
                {
                    const int Group = Joint / 4;
                    const int Lane = Joint % 4;
                    float RestHorizontal[4];
                    ozz::math::StorePtrU(Rig.joint_rest_poses()[Group].translation.x, RestHorizontal);
                    Locals[Group].translation.x = ozz::math::SetI(Locals[Group].translation.x, ozz::math::simd_float4::Load1(RestHorizontal[Lane]), Lane);
                    ozz::math::StorePtrU(Rig.joint_rest_poses()[Group].translation.z, RestHorizontal);
                    Locals[Group].translation.z = ozz::math::SetI(Locals[Group].translation.z, ozz::math::simd_float4::Load1(RestHorizontal[Lane]), Lane);
                }
            }
        }
    }
    std::vector<ozz::math::Float4x4> Models(Rig.num_joints());
    ozz::animation::LocalToModelJob Conversion;
    Conversion.skeleton = &Rig;
    Conversion.input = ozz::make_span(Locals);
    Conversion.output = ozz::make_span(Models);
    if (!Conversion.Run() || Binding.JointIndices.size() != Mesh.Mesh.Vertices.size() * 4
        || Binding.JointWeights.size() != Binding.JointIndices.size()
        || Binding.InverseBindMatrices.size() != Binding.JointNames.size())
    {
        Error = "Character skin binding does not match mesh vertices";
        return nullptr;
    }
    std::vector<Matrix> Matrices;
    std::vector<Matrix> NormalMatrices;
    for (size_t SkinJoint = 0; SkinJoint < Binding.JointNames.size(); ++SkinJoint)
    {
        int Joint = 0;
        while (Joint < Rig.num_joints() && Binding.JointNames[SkinJoint] != Rig.joint_names()[Joint])
        {
            ++Joint;
        }
        if (Joint == Rig.num_joints())
        {
            Error = "Skin joint is absent from character skeleton";
            return nullptr;
        }
        Matrix Model;
        for (int Row = 0; Row < 4; ++Row)
        {
            ozz::math::StorePtrU(Models[Joint].cols[Row], Model.m[Row]);
        }
        Matrices.push_back(Binding.InverseBindMatrices[SkinJoint] * Model);
        NormalMatrices.push_back(Matrices.back().Invert().Transpose());
    }
    auto Result = std::make_shared<StaticMeshResource>(Mesh.Mesh);
    Result->Bounds.Minimum = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    Result->Bounds.Maximum = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    const auto AccumulateDirection = [](Vector3& Destination, const Vector3& Source, const Matrix& Transform, float Weight)
    {
        Destination.x += (Source.x * Transform._11 + Source.y * Transform._21 + Source.z * Transform._31) * Weight;
        Destination.y += (Source.x * Transform._12 + Source.y * Transform._22 + Source.z * Transform._32) * Weight;
        Destination.z += (Source.x * Transform._13 + Source.y * Transform._23 + Source.z * Transform._33) * Weight;
    };
    for (size_t VertexIndex = 0; VertexIndex < Result->Vertices.size(); ++VertexIndex)
    {
        auto& Vertex = Result->Vertices[VertexIndex];
        Vector3 Position = Vector3::Zero;
        Vector3 Normal = Vector3::Zero;
        Vector3 Tangent = Vector3::Zero;
        Vector3 Bitangent = Vector3::Zero;
        const Vector3 SourceTangent(Vertex.Tangent.x, Vertex.Tangent.y, Vertex.Tangent.z);
        const Vector3 SourceBitangent = Vertex.Normal.Cross(SourceTangent) * Vertex.Tangent.w;
        float TotalWeight = 0.f;
        for (size_t Influence = 0; Influence < 4; ++Influence)
        {
            const auto Offset = VertexIndex * 4 + Influence;
            const auto Joint = Binding.JointIndices[Offset];
            const float Weight = Binding.JointWeights[Offset];
            if (!std::isfinite(Weight) || Weight < 0.f || (Weight > 0.f && Joint >= Matrices.size()))
            {
                Error = "Invalid skin joint or weight";
                return nullptr;
            }
            if (Weight > 0.f)
            {
                const auto& JointTransform = Matrices[Joint];
                AccumulateDirection(Position, Vertex.Position, JointTransform, Weight);
                Position.x += JointTransform._41 * Weight;
                Position.y += JointTransform._42 * Weight;
                Position.z += JointTransform._43 * Weight;
                AccumulateDirection(Normal, Vertex.Normal, NormalMatrices[Joint], Weight);
                if (Vertex.bHasTangent)
                {
                    AccumulateDirection(Tangent, SourceTangent, JointTransform, Weight);
                    AccumulateDirection(Bitangent, SourceBitangent, JointTransform, Weight);
                }
                TotalWeight += Weight;
            }
        }
        if (TotalWeight > 0.f)
        {
            Vertex.Position = Position / TotalWeight;
            Normal.Normalize();
            Vertex.Normal = Normal;
            if (Vertex.bHasTangent)
            {
                Tangent -= Normal * Normal.Dot(Tangent);
                if (Tangent.LengthSquared() > 0.00000001f)
                {
                    Tangent.Normalize();
                    float Handedness = 1.f;
                    if (Normal.Cross(Tangent).Dot(Bitangent) < 0.f)
                    {
                        Handedness = -1.f;
                    }
                    Vertex.Tangent = Vector4(Tangent.x, Tangent.y, Tangent.z, Handedness);
                }
                else
                {
                    Vertex.Tangent = Vector4::Zero;
                    Vertex.bHasTangent = false;
                }
            }
        }
        Result->Bounds.Minimum = Vector3::Min(Result->Bounds.Minimum, Vertex.Position);
        Result->Bounds.Maximum = Vector3::Max(Result->Bounds.Maximum, Vertex.Position);
    }
    return Result;
}
