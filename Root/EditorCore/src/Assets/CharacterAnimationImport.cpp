#include "Assets/CharacterAnimationImport.h"
#include "Assets/Loaders/SkeletalLoader.h"
#include "Assets/Resources/SkeletonResource.h"
#include "Characters/CharacterAnimationIO.h"
#include "Core/IO/AtomicFileWriter.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/skeleton_utils.h"
#include "ufbx.h"
#include <SimpleMath.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>

using namespace DirectX::SimpleMath;

static std::string CharacterBoneRole(std::string Name)
{
    const auto Separator = Name.find_last_of("|:");
    if (Separator != std::string::npos)
    {
        Name = Name.substr(Separator + 1);
    }
    std::string Normalized;
    for (unsigned char Character : Name)
    {
        if (std::isalnum(Character))
        {
            Normalized.push_back(static_cast<char>(std::tolower(Character)));
        }
    }
    for (const std::string Prefix : {"mixamorig", "armature"})
    {
        if (Normalized.rfind(Prefix, 0) == 0)
        {
            Normalized.erase(0, Prefix.size());
            while (!Normalized.empty() && std::isdigit(static_cast<unsigned char>(Normalized.front())))
            {
                Normalized.erase(0, 1);
            }
        }
    }
    static const std::map<std::string, std::string> Aliases{
        {"pelvis", "hips"}, {"chest", "spine1"}, {"upperchest", "spine2"},
        {"leftupperarm", "leftarm"}, {"rightupperarm", "rightarm"},
        {"upperarml", "leftarm"}, {"upperarmr", "rightarm"},
        {"leftlowerarm", "leftforearm"}, {"rightlowerarm", "rightforearm"},
        {"forearml", "leftforearm"}, {"forearmr", "rightforearm"},
        {"leftthigh", "leftupleg"}, {"rightthigh", "rightupleg"},
        {"thighl", "leftupleg"}, {"thighr", "rightupleg"},
        {"leftcalf", "leftleg"}, {"rightcalf", "rightleg"},
        {"shinl", "leftleg"}, {"shinr", "rightleg"},
        {"handl", "lefthand"}, {"handr", "righthand"},
        {"footl", "leftfoot"}, {"footr", "rightfoot"}};
    const auto Alias = Aliases.find(Normalized);
    if (Alias != Aliases.end())
    {
        return Alias->second;
    }
    return Normalized;
}

static Matrix CharacterFbxMatrix(const ufbx_matrix& Value)
{
    return Matrix(static_cast<float>(Value.m00), static_cast<float>(Value.m10), static_cast<float>(Value.m20), 0.f,
        static_cast<float>(Value.m01), static_cast<float>(Value.m11), static_cast<float>(Value.m21), 0.f,
        static_cast<float>(Value.m02), static_cast<float>(Value.m12), static_cast<float>(Value.m22), 0.f,
        static_cast<float>(Value.m03), static_cast<float>(Value.m13), static_cast<float>(Value.m23), 1.f);
}

static Matrix CharacterRotation(const Matrix& Value)
{
    Vector3 Scale;
    Quaternion Rotation;
    Vector3 Position;
    Matrix Decomposed = Value;
    Decomposed.Decompose(Scale, Rotation, Position);
    return Matrix::CreateFromQuaternion(Rotation);
}

bool ImportCharacterFbxAnimations(AssetRegistry& Registry, const std::filesystem::path& Source,
    const CharacterDefinition& Character, std::vector<CharacterAnimation>& Animations, std::string& Error)
{
    std::error_code FileError;
    if (std::filesystem::file_size(Source, FileError) > 100 * 1024 * 1024 || FileError)
    {
        Error = "FBX file is missing or exceeds 100 MB";
        return false;
    }
    AssetLoadContext Context;
    const SubAssetRecord* SubAsset = nullptr;
    if (!Registry.TryResolveKey(Character.Skeleton, Context.Entry, &SubAsset))
    {
        Error = "Load a character model before importing FBX animations";
        return false;
    }
    Context.Key = Character.Skeleton;
    BindLoadContextSubAsset(Context, SubAsset);
    ModelLoader Model;
    SkeletalLoader Loader(Model);
    AssetDiagnostic Diagnostic;
    auto Skeleton = std::static_pointer_cast<const SkeletonResource>(Loader.Load(Context, Diagnostic));
    if (!Skeleton)
    {
        Error = Diagnostic.Message;
        return false;
    }
    ufbx_load_opts Options{};
    Options.ignore_geometry = true;
    Options.target_axes = ufbx_axes_right_handed_y_up;
    Options.target_unit_meters = 1.0;
    ufbx_error ImportError{};
    std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> Scene(
        ufbx_load_file(Source.u8string().c_str(), &Options, &ImportError), &ufbx_free_scene);
    if (!Scene || Scene->anim_stacks.count == 0)
    {
        Error = "FBX contains no readable animation stacks";
        return false;
    }
    std::map<std::string, const ufbx_node*> SourceBones;
    for (const auto* Node : Scene->nodes)
    {
        if (Node->attrib_type == UFBX_ELEMENT_BONE)
        {
            const auto Role = CharacterBoneRole(std::string(Node->name.data, Node->name.length));
            if (!SourceBones.emplace(Role, Node).second)
            {
                Error = "FBX contains ambiguous humanoid bone names";
                return false;
            }
        }
    }
    const auto& Rig = *Skeleton->Skeleton;
    std::map<std::string, int> TargetBones;
    std::vector<Matrix> TargetWorld;
    for (int Joint = 0; Joint < Rig.num_joints(); ++Joint)
    {
        const auto Rest = ozz::animation::GetJointLocalRestPose(Rig, Joint);
        Matrix World = Matrix::CreateScale(Rest.scale.x, Rest.scale.y, Rest.scale.z)
            * Matrix::CreateFromQuaternion({Rest.rotation.x, Rest.rotation.y, Rest.rotation.z, Rest.rotation.w})
            * Matrix::CreateTranslation(Rest.translation.x, Rest.translation.y, Rest.translation.z);
        if (Rig.joint_parents()[Joint] >= 0)
        {
            World *= TargetWorld[Rig.joint_parents()[Joint]];
        }
        TargetWorld.push_back(World);
        const auto Role = CharacterBoneRole(Rig.joint_names()[Joint]);
        if (!TargetBones.emplace(Role, Joint).second)
        {
            Error = "Character contains ambiguous humanoid bone names";
            return false;
        }
    }
    for (const std::string Required : {"hips", "spine", "head", "leftarm", "leftforearm", "lefthand",
        "rightarm", "rightforearm", "righthand", "leftupleg", "leftleg", "leftfoot", "rightupleg", "rightleg", "rightfoot"})
    {
        if (SourceBones.count(Required) == 0 || TargetBones.count(Required) == 0)
        {
            Error = "Humanoid bone is missing: " + Required;
            return false;
        }
    }
    const auto SourcePosition = [&](const char* Role)
    { return CharacterFbxMatrix(SourceBones.at(Role)->node_to_world).Translation(); };
    const auto TargetPosition = [&](const char* Role)
    { return TargetWorld[TargetBones.at(Role)].Translation(); };
    const float SourceLength = Vector3::Distance(SourcePosition("leftupleg"), SourcePosition("leftleg"))
        + Vector3::Distance(SourcePosition("leftleg"), SourcePosition("leftfoot"));
    const float TargetLength = Vector3::Distance(TargetPosition("leftupleg"), TargetPosition("leftleg"))
        + Vector3::Distance(TargetPosition("leftleg"), TargetPosition("leftfoot"));
    if (SourceLength < 0.000001f || TargetLength < 0.000001f)
    {
        Error = "Humanoid leg length is zero";
        return false;
    }
    std::vector<std::pair<std::filesystem::path, nlohmann::json>> Outputs;
    for (size_t StackIndex = 0; StackIndex < Scene->anim_stacks.count; ++StackIndex)
    {
        const auto* Stack = Scene->anim_stacks[StackIndex];
        const float Duration = static_cast<float>(Stack->time_end - Stack->time_begin);
        if (!std::isfinite(Duration) || Duration <= 0.f || Duration > 600.f)
        {
            Error = "FBX animation duration must be between zero and 600 seconds";
            return false;
        }
        nlohmann::json Data{{"format", "sakura.animation"}, {"version", 1}, {"duration", Duration},
            {"name", std::string(Stack->name.data, Stack->name.length)}, {"tracks", nlohmann::json::array()}};
        const int Samples = static_cast<int>(std::ceil(Duration * 30.f));
        for (int Joint = 0; Joint < Rig.num_joints(); ++Joint)
        {
            const auto Rest = ozz::animation::GetJointLocalRestPose(Rig, Joint);
            const auto Role = CharacterBoneRole(Rig.joint_names()[Joint]);
            nlohmann::json Track{{"joint", Rig.joint_names()[Joint]}, {"translation", nlohmann::json::array()},
                {"rotation", nlohmann::json::array()}, {"scale", {{0.f, Rest.scale.x, Rest.scale.y, Rest.scale.z}}}};
            const auto Matched = SourceBones.find(Role);
            for (int Sample = 0; Sample <= Samples; ++Sample)
            {
                const float Time = Duration * static_cast<float>(Sample) / static_cast<float>(Samples);
                Quaternion Rotation{Rest.rotation.x, Rest.rotation.y, Rest.rotation.z, Rest.rotation.w};
                Vector3 Position{Rest.translation.x, Rest.translation.y, Rest.translation.z};
                if (Matched != SourceBones.end())
                {
                    const auto* Bone = Matched->second;
                    const auto Evaluated = ufbx_evaluate_transform(Stack->anim, Bone, Stack->time_begin + Time);
                    Matrix SourceParent = Matrix::Identity;
                    Matrix TargetParent = Matrix::Identity;
                    if (Bone->parent)
                    {
                        SourceParent = CharacterFbxMatrix(Bone->parent->node_to_world);
                    }
                    if (Rig.joint_parents()[Joint] >= 0)
                    {
                        TargetParent = TargetWorld[Rig.joint_parents()[Joint]];
                    }
                    const Quaternion Animated{static_cast<float>(Evaluated.rotation.x), static_cast<float>(Evaluated.rotation.y),
                        static_cast<float>(Evaluated.rotation.z), static_cast<float>(Evaluated.rotation.w)};
                    const Matrix Retargeted = CharacterRotation(TargetWorld[Joint])
                        * CharacterRotation(CharacterFbxMatrix(Bone->node_to_world)).Invert()
                        * Matrix::CreateFromQuaternion(Animated) * CharacterRotation(SourceParent) * CharacterRotation(TargetParent).Invert();
                    Rotation = Quaternion::CreateFromRotationMatrix(Retargeted);
                    Rotation.Normalize();
                    if (Role == "hips")
                    {
                        Vector3 Displacement{static_cast<float>(Evaluated.translation.x - Bone->local_transform.translation.x),
                            static_cast<float>(Evaluated.translation.y - Bone->local_transform.translation.y),
                            static_cast<float>(Evaluated.translation.z - Bone->local_transform.translation.z)};
                        Displacement = Vector3::TransformNormal(Displacement, SourceParent) * (TargetLength / SourceLength);
                        Position += Vector3::TransformNormal(Displacement, TargetParent.Invert());
                    }
                }
                Track["translation"].push_back({Time, Position.x, Position.y, Position.z});
                Track["rotation"].push_back({Time, Rotation.x, Rotation.y, Rotation.z, Rotation.w});
            }
            Data["tracks"].push_back(std::move(Track));
        }
        const auto Filename = Source.stem().u8string() + "_" + std::to_string(StackIndex) + ".animation";
        const auto Path = Registry.GetGameContentRoot() / "Characters" / "Animations" / std::filesystem::u8path(Filename);
        if (Registry.LeafNameExists(Filename) || std::filesystem::exists(Path))
        {
            Error = "Animation asset already exists: " + Filename;
            return false;
        }
        Outputs.emplace_back(Path, std::move(Data));
    }
    std::vector<std::filesystem::path> Published;
    const auto Rollback = [&]()
    {
        for (const auto& Path : Published)
        {
            std::filesystem::remove(Path, FileError);
            std::filesystem::remove(Path.string() + ".meta", FileError);
        }
        Registry.ScanContent();
    };
    for (const auto& Output : Outputs)
    {
        if (!AtomicFileWriter::WriteText(Output.first, Output.second.dump(), Error))
        {
            Rollback();
            return false;
        }
        Published.push_back(Output.first);
        if (!ReadCharacterAnimation(Output.first, Error))
        {
            Rollback();
            return false;
        }
    }
    Registry.ScanContent();
    for (const auto& Output : Outputs)
    {
        AssetRegistryEntry Entry;
        if (!Registry.TryGetByPath("Characters/Animations/" + Output.first.filename().u8string(), Entry))
        {
            Error = "Imported animation registration failed";
            Rollback();
            return false;
        }
        CharacterAnimation Animation;
        Animation.Identifier = Guid::Generate().ToString();
        Animation.Name = Output.second.at("name").get<std::string>();
        if (Animation.Name.empty())
        {
            Animation.Name = Output.first.stem().u8string();
        }
        Animation.Clip.Asset = Entry.Metadata.Guid;
        Animations.push_back(std::move(Animation));
    }
    return true;
}
