#include "Navigation/NavigationWorld.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Components/PhysicsBodyComponent.h"
#include "World/Components/NavMeshBoundsComponent.h"
#include "World/Components/NavigationModifierComponent.h"
#include "World/Components/NavigationLinkComponent.h"
#include "World/Components/NavigationAgentComponent.h"
#include "Assets/ContentHash.h"
#include "Core/Threading/ThreadContext.h"
#include "Reflection/Json/ReflectionJson.h"

#include <algorithm>
#include <cfloat>
#include <unordered_set>
#include <nlohmann/json.hpp>

NavigationBuildSettings NavMeshBoundsComponent::GetBuildSettings() const
{
    NavigationBuildSettings Settings;
    Settings.AgentProfile = AgentProfile;
    Settings.AgentRadius = AgentRadius;
    Settings.AgentHeight = AgentHeight;
    Settings.MaximumStepHeight = MaximumStepHeight;
    Settings.MaximumSlopeDegrees = MaximumSlopeDegrees;
    Settings.CellSize = CellSize;
    Settings.CellHeight = CellHeight;
    Settings.TileSize = static_cast<int>(TileSize);
    Settings.MinimumRegionArea = static_cast<int>(MinimumRegionArea);
    Settings.MergeRegionArea = static_cast<int>(MergeRegionArea);
    Settings.MaximumSimplificationError = MaximumSimplificationError;
    return Settings;
}

AxisAlignedBounds NavMeshBoundsComponent::GetWorldBounds() const
{
    return AxisAlignedBounds::FromCenterExtents(Vector3::Zero, HalfExtents).TransformedBy(GetGameObject()->GetWorldMatrix());
}

static nlohmann::json MakeNavigationSettingsDocument(const NavigationBuildSettings& Settings)
{
    return {
        {"profile", Settings.AgentProfile}, {"radius", Settings.AgentRadius}, {"height", Settings.AgentHeight},
        {"step", Settings.MaximumStepHeight}, {"slope", Settings.MaximumSlopeDegrees}, {"cellSize", Settings.CellSize},
        {"cellHeight", Settings.CellHeight}, {"tileSize", Settings.TileSize}, {"minimumRegion", Settings.MinimumRegionArea},
        {"mergeRegion", Settings.MergeRegionArea}, {"simplification", Settings.MaximumSimplificationError}};
}

static uint64_t GetNavigationInputFingerprint(const NavigationBuildInput& Input)
{
    nlohmann::json Description = MakeNavigationSettingsDocument(Input.Settings);
    Description["vertices"] = ContentHash::FromBytes(Input.Geometry.Vertices.data(), Input.Geometry.Vertices.size() * sizeof(Vector3)).Value;
    Description["indices"] = ContentHash::FromBytes(Input.Geometry.Indices.data(), Input.Geometry.Indices.size() * sizeof(uint32_t)).Value;
    Description["bounds"] = nlohmann::json::array();
    for (const AxisAlignedBounds& Bounds : Input.Bounds)
    {
        Description["bounds"].push_back({Bounds.Minimum.x, Bounds.Minimum.y, Bounds.Minimum.z,
            Bounds.Maximum.x, Bounds.Maximum.y, Bounds.Maximum.z});
    }
    Description["modifiers"] = nlohmann::json::array();
    for (const NavigationModifier& Modifier : Input.Modifiers)
    {
        nlohmann::json MatrixValues = nlohmann::json::array();
        for (int Row = 0; Row < 4; ++Row)
        {
            for (int Column = 0; Column < 4; ++Column)
            {
                MatrixValues.push_back(Modifier.WorldToLocal.m[Row][Column]);
            }
        }
        Description["modifiers"].push_back({{"matrix", MatrixValues}, {"area", Modifier.Area}, {"priority", Modifier.Priority},
            {"extents", {Modifier.HalfExtents.x, Modifier.HalfExtents.y, Modifier.HalfExtents.z}}});
    }
    Description["links"] = nlohmann::json::array();
    for (const NavigationLink& Link : Input.Links)
    {
        Description["links"].push_back({{"start", {Link.Start.x, Link.Start.y, Link.Start.z}},
            {"end", {Link.End.x, Link.End.y, Link.End.z}}, {"radius", Link.Radius},
            {"area", Link.Area}, {"identity", Link.Identity}, {"bidirectional", Link.bBidirectional}});
    }
    const std::string Serialized = Description.dump();
    return ContentHash::FromBytes(Serialized.data(), Serialized.size()).Value;
}

static AxisAlignedBounds GetNavigationModifierBounds(const NavigationModifier& Modifier)
{
    return AxisAlignedBounds::FromCenterExtents(Vector3::Zero, Modifier.HalfExtents).TransformedBy(Modifier.WorldToLocal.Invert());
}

static AxisAlignedBounds GetNavigationLinkBounds(const NavigationLink& Link)
{
    AxisAlignedBounds Bounds;
    const Vector3 Padding(Link.Radius, Link.Radius, Link.Radius);
    Bounds.Minimum = Vector3::Min(Link.Start, Link.End) - Padding;
    Bounds.Maximum = Vector3::Max(Link.Start, Link.End) + Padding;
    return Bounds;
}

static bool HaveMatchingNavigationLayout(const NavigationBuildInput& Input, const NavigationBuildInput& Previous)
{
    if (MakeNavigationSettingsDocument(Input.Settings) != MakeNavigationSettingsDocument(Previous.Settings)
        || Input.Bounds.size() != Previous.Bounds.size())
    {
        return false;
    }
    for (size_t BoundsIndex = 0; BoundsIndex < Input.Bounds.size(); ++BoundsIndex)
    {
        if (Input.Bounds[BoundsIndex].Minimum != Previous.Bounds[BoundsIndex].Minimum
            || Input.Bounds[BoundsIndex].Maximum != Previous.Bounds[BoundsIndex].Maximum)
        {
            return false;
        }
    }
    return true;
}

static bool PrepareIncrementalNavigation(NavigationBuildInput& Input, const NavigationBuildInput& Previous)
{
    if (!HaveMatchingNavigationLayout(Input, Previous)
        || Input.Geometry.Vertices.size() != Previous.Geometry.Vertices.size() || Input.Geometry.Indices != Previous.Geometry.Indices)
    {
        return false;
    }
    AxisAlignedBounds ChangedGeometry;
    ChangedGeometry.Minimum = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    ChangedGeometry.Maximum = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    bool bGeometryChanged = false;
    for (size_t TriangleIndex = 0; TriangleIndex + 2 < Input.Geometry.Indices.size(); TriangleIndex += 3)
    {
        bool bTriangleChanged = false;
        for (size_t CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
        {
            const uint32_t VertexIndex = Input.Geometry.Indices[TriangleIndex + CornerIndex];
            bTriangleChanged = bTriangleChanged || Input.Geometry.Vertices[VertexIndex] != Previous.Geometry.Vertices[VertexIndex];
        }
        if (bTriangleChanged)
        {
            bGeometryChanged = true;
            for (size_t CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
            {
                const uint32_t VertexIndex = Input.Geometry.Indices[TriangleIndex + CornerIndex];
                const Vector3& CurrentVertex = Input.Geometry.Vertices[VertexIndex];
                const Vector3& PreviousVertex = Previous.Geometry.Vertices[VertexIndex];
                ChangedGeometry.Minimum = Vector3::Min(ChangedGeometry.Minimum, Vector3::Min(CurrentVertex, PreviousVertex));
                ChangedGeometry.Maximum = Vector3::Max(ChangedGeometry.Maximum, Vector3::Max(CurrentVertex, PreviousVertex));
            }
        }
    }
    if (bGeometryChanged)
    {
        Input.DirtyBounds.push_back(ChangedGeometry);
    }
    for (size_t ModifierIndex = 0; ModifierIndex < std::max(Input.Modifiers.size(), Previous.Modifiers.size()); ++ModifierIndex)
    {
        bool bChanged = true;
        if (ModifierIndex < Input.Modifiers.size() && ModifierIndex < Previous.Modifiers.size())
        {
            const NavigationModifier& Current = Input.Modifiers[ModifierIndex];
            const NavigationModifier& Before = Previous.Modifiers[ModifierIndex];
            bChanged = Current.WorldToLocal != Before.WorldToLocal || Current.HalfExtents != Before.HalfExtents
                || Current.Area != Before.Area || Current.Priority != Before.Priority;
        }
        if (bChanged)
        {
            if (ModifierIndex < Input.Modifiers.size())
            {
                Input.DirtyBounds.push_back(GetNavigationModifierBounds(Input.Modifiers[ModifierIndex]));
            }
            if (ModifierIndex < Previous.Modifiers.size())
            {
                Input.DirtyBounds.push_back(GetNavigationModifierBounds(Previous.Modifiers[ModifierIndex]));
            }
        }
    }
    for (size_t LinkIndex = 0; LinkIndex < std::max(Input.Links.size(), Previous.Links.size()); ++LinkIndex)
    {
        bool bChanged = true;
        if (LinkIndex < Input.Links.size() && LinkIndex < Previous.Links.size())
        {
            const NavigationLink& Current = Input.Links[LinkIndex];
            const NavigationLink& Before = Previous.Links[LinkIndex];
            bChanged = Current.Start != Before.Start || Current.End != Before.End || Current.Radius != Before.Radius
                || Current.Area != Before.Area || Current.bBidirectional != Before.bBidirectional || Current.Identity != Before.Identity;
        }
        if (bChanged)
        {
            if (LinkIndex < Input.Links.size())
            {
                Input.DirtyBounds.push_back(GetNavigationLinkBounds(Input.Links[LinkIndex]));
            }
            if (LinkIndex < Previous.Links.size())
            {
                Input.DirtyBounds.push_back(GetNavigationLinkBounds(Previous.Links[LinkIndex]));
            }
        }
    }
    return true;
}

NavigationWorld::NavigationWorld(Scene& InWorld) : World(InWorld) {}
NavigationWorld::~NavigationWorld() { CancelBuilds(); }

bool NavigationWorld::ComputeSourceFingerprint(uint64_t& Fingerprint)
{
    nlohmann::json Description = nlohmann::json::array();
    bool bHasBounds = false;
    bool bTrackGeometry = !World.IsPlaying() || bBuildRequested || Profiles.empty();
    for (const auto& Entry : Profiles)
    {
        bTrackGeometry = bTrackGeometry || Entry.second.Mode == NavigationGenerationMode::Dynamic;
    }
    Description.push_back(World.IsPlaying());
    for (GameObject* Owner : World.GetAllObjects())
    {
        if (!Owner->IsActiveInHierarchy())
        {
            continue;
        }
        const Matrix Placement = Owner->GetWorldMatrix();
        const uint64_t PlacementFingerprint = ContentHash::FromBytes(&Placement, sizeof(Placement)).Value;
        for (Component* ComponentInstance : Owner->GetAllComponents())
        {
            if (!ComponentInstance->IsEnabled() || ComponentInstance->IsPendingRemoval())
            {
                continue;
            }
            if (const auto* Bounds = dynamic_cast<const NavMeshBoundsComponent*>(ComponentInstance))
            {
                bHasBounds = true;
                Description.push_back({Bounds->GetID(), PlacementFingerprint, Bounds->HalfExtents.x, Bounds->HalfExtents.y,
                    Bounds->HalfExtents.z, Bounds->GenerationMode, Bounds->bAutoBuild,
                    MakeNavigationSettingsDocument(Bounds->GetBuildSettings())});
            }
            else if (const auto* Modifier = dynamic_cast<const NavigationModifierComponent*>(ComponentInstance))
            {
                Description.push_back({Modifier->GetID(), PlacementFingerprint, Modifier->HalfExtents.x,
                    Modifier->HalfExtents.y, Modifier->HalfExtents.z, Modifier->Area, Modifier->Priority, Modifier->AgentProfile});
            }
            else if (const auto* Link = dynamic_cast<const NavigationLinkComponent*>(ComponentInstance))
            {
                Description.push_back({Link->GetID(), PlacementFingerprint, Link->Start.x, Link->Start.y, Link->Start.z,
                    Link->End.x, Link->End.y, Link->End.z, Link->Radius, Link->bBidirectional, Link->Area, Link->AgentProfile});
            }
        }
    }
    if (bHasBounds && bTrackGeometry)
    {
        for (GameObject* Owner : World.GetAllObjects())
        {
            if (!Owner->IsActiveInHierarchy())
            {
                continue;
            }
            const Matrix Placement = Owner->GetWorldMatrix();
            const uint64_t PlacementFingerprint = ContentHash::FromBytes(&Placement, sizeof(Placement)).Value;
            for (PhysicsBodyComponent* Body : Owner->GetComponents<PhysicsBodyComponent>())
            {
                if (!Body->IsEnabled() || Body->IsPendingRemoval() || !Body->bAffectsNavigation || Body->bTrigger || !Body->bSimulationEnabled
                    || Body->MotionType == PhysicsMotionType::Character || Body->CollisionMask == 0)
                {
                    continue;
                }
                if (!Body->PrepareCollisionMesh(World.GetAssetManager()))
                {
                    Diagnostic = Body->GetDiagnostic();
                    bWaitingForAssets = Body->IsCollisionMeshPending();
                    return false;
                }
                Description.push_back({Body->GetID(), PlacementFingerprint, Body->GetConfigurationFingerprint()});
            }
        }
    }
    const std::string Serialized = Description.dump();
    Fingerprint = ContentHash::FromBytes(Serialized.data(), Serialized.size()).Value;
    return true;
}

bool NavigationWorld::CollectInputs(std::map<std::string, NavigationBuildInput>& Inputs)
{
    std::map<std::string, std::string> ProfileSettings;
    for (GameObject* Owner : World.GetAllObjects())
    {
        if (!Owner->IsActiveInHierarchy())
        {
            continue;
        }
        for (NavMeshBoundsComponent* Bounds : Owner->GetComponents<NavMeshBoundsComponent>())
        {
            if (!Bounds->IsEnabled() || Bounds->IsPendingRemoval())
            {
                continue;
            }
            if (Bounds->GenerationMode < NavigationGenerationMode::Static || Bounds->GenerationMode > NavigationGenerationMode::DynamicModifiersOnly || Bounds->TileSize < 16 || Bounds->TileSize > 256
                || Bounds->MinimumRegionArea < 0 || Bounds->MinimumRegionArea > 1024
                || Bounds->MergeRegionArea < 0 || Bounds->MergeRegionArea > 1024
                || Bounds->HalfExtents.x <= 0.f || Bounds->HalfExtents.y <= 0.f || Bounds->HalfExtents.z <= 0.f)
            {
                Diagnostic = "Navigation bounds settings are invalid";
                return false;
            }
            const NavigationBuildSettings Settings = Bounds->GetBuildSettings();
            if (!ValidateNavigationSettings(Settings, Diagnostic))
            {
                return false;
            }
            ReflectionJsonDocument Serialized;
            ReflectionJson::SerializeObject(Bounds, Serialized);
            Serialized.Properties.erase("half_extents");
            const std::string Identity = Serialized.Properties.dump();
            auto Existing = ProfileSettings.find(Settings.AgentProfile);
            if (Existing != ProfileSettings.end() && Existing->second != Identity)
            {
                Diagnostic = "Navigation bounds sharing an agent profile must use identical build settings";
                return false;
            }
            ProfileSettings[Settings.AgentProfile] = Identity;
            NavigationBuildInput& Input = Inputs[Settings.AgentProfile];
            if (Inputs.size() > 32)
            {
                Diagnostic = "Navigation supports at most 32 agent profiles per scene";
                return false;
            }
            Input.Settings = Settings;
            Input.Bounds.push_back(Bounds->GetWorldBounds());
            ProfileState& Profile = Profiles[Settings.AgentProfile];
            Profile.Mode = Bounds->GenerationMode;
            Profile.bAutoBuild = Bounds->bAutoBuild;
        }
    }
    if (Inputs.empty())
    {
        return true;
    }
    bool bNeedsCollisionGeometry = !World.IsPlaying() || bBuildRequested;
    for (const auto& Entry : Inputs)
    {
        const ProfileState& Profile = Profiles[Entry.first];
        bNeedsCollisionGeometry = bNeedsCollisionGeometry || Profile.Mode == NavigationGenerationMode::Dynamic || !Profile.BaseInput;
    }
    PhysicsTriangleMesh CollisionGeometry;
    for (GameObject* Owner : World.GetAllObjects())
    {
        if (!bNeedsCollisionGeometry || !Owner->IsActiveInHierarchy())
        {
            continue;
        }
        for (PhysicsBodyComponent* Body : Owner->GetComponents<PhysicsBodyComponent>())
        {
            if (!Body->IsEnabled() || Body->IsPendingRemoval() || !Body->bAffectsNavigation || Body->bTrigger || !Body->bSimulationEnabled
                || Body->MotionType == PhysicsMotionType::Character || Body->CollisionMask == 0)
            {
                continue;
            }
            if (!Body->PrepareCollisionMesh(World.GetAssetManager()))
            {
                Diagnostic = Body->GetDiagnostic();
                bWaitingForAssets = Body->IsCollisionMeshPending();
                return false;
            }
            PhysicsBodyDefinition Definition;
            if (!Body->BuildDefinition(Definition, Diagnostic))
            {
                return false;
            }
            const PhysicsTriangleMesh Geometry = MakePhysicsTriangleMesh(Definition);
            const uint32_t VertexOffset = static_cast<uint32_t>(CollisionGeometry.Vertices.size());
            CollisionGeometry.Vertices.insert(CollisionGeometry.Vertices.end(), Geometry.Vertices.begin(), Geometry.Vertices.end());
            for (uint32_t VertexIndex : Geometry.Indices)
            {
                CollisionGeometry.Indices.push_back(VertexIndex + VertexOffset);
            }
        }
    }
    for (auto& Entry : Inputs)
    {
        NavigationBuildInput& Input = Entry.second;
        Input.Geometry = CollisionGeometry;
        for (GameObject* Owner : World.GetAllObjects())
        {
            if (!Owner->IsActiveInHierarchy())
            {
                continue;
            }
            for (NavigationModifierComponent* ComponentInstance : Owner->GetComponents<NavigationModifierComponent>())
            {
                if (!ComponentInstance->IsEnabled() || ComponentInstance->IsPendingRemoval()
                    || (!ComponentInstance->AgentProfile.empty() && ComponentInstance->AgentProfile != Entry.first))
                {
                    continue;
                }
                const Matrix WorldMatrix = Owner->GetWorldMatrix();
                if (ComponentInstance->Area < 0 || ComponentInstance->Area > 16 || std::abs(WorldMatrix.Determinant()) < 0.000001f)
                {
                    Diagnostic = "Navigation modifier has an invalid area or singular transform";
                    return false;
                }
                NavigationModifier Modifier;
                Modifier.WorldToLocal = WorldMatrix.Invert();
                Modifier.HalfExtents = ComponentInstance->HalfExtents;
                Modifier.Area = static_cast<uint8_t>(ComponentInstance->Area);
                Modifier.Priority = static_cast<int>(ComponentInstance->Priority);
                Input.Modifiers.push_back(Modifier);
            }
            for (NavigationLinkComponent* ComponentInstance : Owner->GetComponents<NavigationLinkComponent>())
            {
                if (!ComponentInstance->IsEnabled() || ComponentInstance->IsPendingRemoval()
                    || (!ComponentInstance->AgentProfile.empty() && ComponentInstance->AgentProfile != Entry.first))
                {
                    continue;
                }
                if (ComponentInstance->Area < 1 || ComponentInstance->Area > 16)
                {
                    Diagnostic = "Navigation link area is out of range";
                    return false;
                }
                NavigationLink Link;
                Link.Start = Vector3::Transform(ComponentInstance->Start, Owner->GetWorldMatrix());
                Link.End = Vector3::Transform(ComponentInstance->End, Owner->GetWorldMatrix());
                Link.Radius = ComponentInstance->Radius;
                Link.Area = static_cast<uint8_t>(ComponentInstance->Area);
                Link.bBidirectional = ComponentInstance->bBidirectional;
                const std::string& PersistentIdentity = ComponentInstance->GetPersistentId();
                Link.Identity = static_cast<uint32_t>(ContentHash::FromBytes(PersistentIdentity.data(), PersistentIdentity.size()).Value);
                Input.Links.push_back(Link);
            }
        }
        std::stable_sort(Input.Modifiers.begin(), Input.Modifiers.end(), [](const NavigationModifier& First, const NavigationModifier& Second)
        {
            return First.Priority < Second.Priority;
        });
        const ProfileState& Profile = Profiles[Entry.first];
        if (World.IsPlaying() && Profile.Mode != NavigationGenerationMode::Dynamic && Profile.BaseInput && !bBuildRequested)
        {
            Input.Geometry = Profile.BaseInput->Geometry;
        }
        Input.Fingerprint = GetNavigationInputFingerprint(Input);
    }
    return true;
}

void NavigationWorld::PublishCompleted()
{
    for (auto& Entry : Profiles)
    {
        ProfileState& Profile = Entry.second;
        if (!Profile.Pending || !Profile.Pending->bDone.load(std::memory_order_acquire))
        {
            continue;
        }
        std::shared_ptr<BuildCompletion> Completion = std::move(Profile.Pending);
        std::lock_guard<std::mutex> Lock(Completion->Mutex);
        const bool bMatchesWanted = Completion->Mesh && Completion->Mesh->GetFingerprint() == Profile.WantedFingerprint;
        const bool bDynamicContinuation = World.IsPlaying() && Profile.Mode != NavigationGenerationMode::Static
            && Profile.Input && Completion->Input && HaveMatchingNavigationLayout(*Profile.Input, *Completion->Input);
        if (Completion->Mesh && !Completion->Cancellation->load() && (bMatchesWanted || bDynamicContinuation))
        {
            Profile.Mesh = std::move(Completion->Mesh);
            Profile.PublishedInput = Completion->Input;
            Profile.Generation = NextGeneration++;
            Profile.State = NavigationBuildState::Ready;
            Profile.Error.clear();
            if (!bMatchesWanted)
            {
                Profile.State = NavigationBuildState::Stale;
            }
        }
        else if (Completion->Mesh || Completion->Cancellation->load())
        {
            Profile.State = NavigationBuildState::Stale;
        }
        else
        {
            Profile.State = NavigationBuildState::Failed;
            Profile.Error = Completion->Error;
            PrintString("Navigation build failed: " + Profile.Error);
        }
    }
    Diagnostic.clear();
    for (const auto& Entry : Profiles)
    {
        if (Entry.second.State == NavigationBuildState::Failed)
        {
            Diagnostic = Entry.second.Error;
            break;
        }
    }
}

void NavigationWorld::Schedule(ProfileState& Profile)
{
    const auto Input = std::make_shared<NavigationBuildInput>(*Profile.Input);
    if (!Profile.bForceFullBuild && Profile.Mesh && Profile.PublishedInput
        && PrepareIncrementalNavigation(*Input, *Profile.PublishedInput))
    {
        Input->PreviousMesh = Profile.Mesh;
    }
    auto Completion = std::make_shared<BuildCompletion>();
    Completion->Input = Input;
    Input->Cancellation = Completion->Cancellation;
    Profile.Pending = Completion;
    Profile.State = NavigationBuildState::Building;
    auto Build = [Input, Completion]()
    {
        std::string Error;
        std::shared_ptr<NavigationMesh> Mesh;
        try
        {
            Mesh = NavigationMesh::Build(*Input, Error);
        }
        catch (const std::exception& Exception)
        {
            Error = std::string("Navigation build exception: ") + Exception.what();
        }
        {
            std::lock_guard<std::mutex> Lock(Completion->Mutex);
            Completion->Mesh = std::move(Mesh);
            Completion->Error = std::move(Error);
        }
        Completion->bDone.store(true, std::memory_order_release);
    };
    if (Jobs != nullptr && Jobs->IsInitialized())
    {
        Jobs->Schedule(std::move(Build));
    }
    else
    {
        Build();
    }
}

void NavigationWorld::Update()
{
    AssertGameThread();
    bWaitingForAssets = false;
    bInputRejected = false;
    uint64_t CurrentSourceFingerprint = 0;
    if (!ComputeSourceFingerprint(CurrentSourceFingerprint))
    {
        RejectBuildInputs();
        return;
    }
    if (CurrentSourceFingerprint == SourceFingerprint && !bBuildRequested)
    {
        PublishCompleted();
        for (auto& Entry : Profiles)
        {
            ProfileState& Profile = Entry.second;
            if (!Profile.Pending && Profile.Input && Profile.State == NavigationBuildState::Stale
                && (World.IsPlaying() || Profile.bAutoBuild))
            {
                Schedule(Profile);
            }
        }
        PublishCompleted();
        return;
    }
    std::map<std::string, NavigationBuildInput> Inputs;
    if (!CollectInputs(Inputs))
    {
        RejectBuildInputs();
        return;
    }
    SourceFingerprint = CurrentSourceFingerprint;
    for (auto Iterator = Profiles.begin(); Iterator != Profiles.end();)
    {
        if (Inputs.count(Iterator->first) == 0)
        {
            if (Iterator->second.Pending)
            {
                Iterator->second.Pending->Cancellation->store(true);
            }
            Iterator = Profiles.erase(Iterator);
        }
        else
        {
            ++Iterator;
        }
    }
    for (auto& Entry : Inputs)
    {
        ProfileState& Profile = Profiles[Entry.first];
        if (World.IsPlaying() && Profile.Mode == NavigationGenerationMode::Static && Profile.Mesh && !bBuildRequested)
        {
            continue;
        }
        if (Profile.WantedFingerprint != Entry.second.Fingerprint || !Profile.Input || bBuildRequested)
        {
            const bool bExistingMeshMatches = Profile.Mesh && Profile.Mesh->GetFingerprint() == Entry.second.Fingerprint && !bBuildRequested;
            Profile.WantedFingerprint = Entry.second.Fingerprint;
            Profile.Input = std::make_shared<NavigationBuildInput>(std::move(Entry.second));
            if (!Profile.BaseInput || bBuildRequested || !World.IsPlaying())
            {
                Profile.BaseInput = Profile.Input;
            }
            Profile.State = NavigationBuildState::Stale;
            if (bExistingMeshMatches)
            {
                Profile.State = NavigationBuildState::Ready;
                Profile.PublishedInput = Profile.Input;
            }
            Profile.bForceFullBuild = bBuildRequested;
            if (Profile.Pending && (bBuildRequested || !World.IsPlaying() || Profile.Mode == NavigationGenerationMode::Static
                || !HaveMatchingNavigationLayout(*Profile.Input, *Profile.Pending->Input)))
            {
                Profile.Pending->Cancellation->store(true);
            }
        }
    }
    PublishCompleted();
    for (auto& Entry : Profiles)
    {
        ProfileState& Profile = Entry.second;
        if (!Profile.Pending && Profile.Input && Profile.State != NavigationBuildState::Ready
            && Profile.State != NavigationBuildState::Failed && (bBuildRequested || World.IsPlaying() || Profile.bAutoBuild))
        {
            Schedule(Profile);
        }
    }
    bBuildRequested = false;
    PublishCompleted();
}

void NavigationWorld::RejectBuildInputs()
{
    bInputRejected = true;
    for (auto& Entry : Profiles)
    {
        Entry.second.State = NavigationBuildState::Failed;
        Entry.second.Error = Diagnostic;
        if (bWaitingForAssets)
        {
            Entry.second.State = NavigationBuildState::Stale;
        }
        if (Entry.second.Pending)
        {
            Entry.second.Pending->Cancellation->store(true);
        }
    }
    SourceFingerprint = 0;
}

void NavigationWorld::RequestBuild()
{
    bBuildRequested = true;
    Update();
}

bool NavigationWorld::PrepareForSimulation()
{
    Update();
    if (bInputRejected)
    {
        return false;
    }
    bool bNeedsBuild = false;
    for (const auto& Entry : Profiles)
    {
        bNeedsBuild = bNeedsBuild || (!Entry.second.Pending && Entry.second.Input
            && Entry.second.State == NavigationBuildState::Stale);
    }
    if (bNeedsBuild)
    {
        RequestBuild();
    }
    for (const auto& Entry : Profiles)
    {
        if (!IsReady(Entry.first))
        {
            return false;
        }
    }
    return true;
}

void NavigationWorld::UpdateCrowds(float DeltaTime)
{
    AssertGameThread();
    if (!World.IsPlaying() || !std::isfinite(DeltaTime) || DeltaTime <= 0.f)
    {
        return;
    }
    for (auto& ProfileEntry : Profiles)
    {
        ProfileState& Profile = ProfileEntry.second;
        if (!IsReady(ProfileEntry.first))
        {
            Profile.Crowd.reset();
            Profile.CrowdAgents.clear();
            continue;
        }
        if (!Profile.Crowd || Profile.CrowdGeneration != Profile.Generation)
        {
            Profile.Crowd = std::make_unique<NavigationCrowd>(Profile.Mesh);
            Profile.CrowdAgents.clear();
            if (!Profile.Crowd->Initialize())
            {
                Profile.Crowd.reset();
                continue;
            }
            Profile.CrowdGeneration = Profile.Generation;
        }
        std::unordered_set<uint64_t> ActiveAgents;
        for (GameObject* Owner : World.GetAllObjects())
        {
            if (!Owner->IsActiveInHierarchy() || Owner->IsStoryControllingTransform())
            {
                continue;
            }
            NavigationAgentComponent* Agent = Owner->GetComponent<NavigationAgentComponent>();
            if (Agent == nullptr || !Agent->IsEnabled() || Agent->IsPendingRemoval() || Agent->AgentProfile != ProfileEntry.first
                || Agent->GetMovementState() == NavigationAgentState::WaitingForLink)
            {
                continue;
            }
            PhysicsBodyComponent* Controller = nullptr;
            for (PhysicsBodyComponent* Body : Owner->GetComponents<PhysicsBodyComponent>())
            {
                if (Body->MotionType == PhysicsMotionType::Character && Body->IsEnabled() && !Body->IsPendingRemoval())
                {
                    Controller = Body;
                    break;
                }
            }
            PhysicsBodyDefinition Definition;
            std::string Error;
            if (Controller == nullptr || !Controller->BuildDefinition(Definition, Error)
                || Definition.Radius > Profile.Mesh->GetSettings().AgentRadius)
            {
                continue;
            }
            Vector3 Target;
            const bool bMoving = Agent->GetSteeringTarget(Target) && Agent->GetPath().Generation == Profile.Generation;
            NavigationCrowdSettings Settings;
            Settings.Radius = std::min(Definition.Radius + std::min(0.05f, Definition.Radius * 0.1f), Profile.Mesh->GetSettings().AgentRadius);
            Settings.Height = 2.f * (Definition.Radius + Definition.CapsuleHalfHeight);
            Settings.MaximumSpeed = 0.f;
            if (bMoving)
            {
                Settings.MaximumSpeed = Agent->MaximumSpeed;
            }
            Settings.MaximumAcceleration = Agent->Acceleration;
            Settings.AvoidanceRadius = Agent->AvoidanceRadius;
            Settings.bAvoidOtherAgents = Agent->bAvoidOtherAgents;
            Settings.QueryFilter = Agent->GetQueryFilter();
            auto Found = Profile.CrowdAgents.find(Agent->GetID());
            if (Found == Profile.CrowdAgents.end())
            {
                ProfileState::CrowdAgent Added;
                Added.Component = Agent->GetObjectHandle();
                Added.Index = Profile.Crowd->AddAgent(Owner->GetWorldPosition(), Settings);
                if (Added.Index < 0)
                {
                    continue;
                }
                Found = Profile.CrowdAgents.emplace(Agent->GetID(), Added).first;
            }
            ProfileState::CrowdAgent& Registered = Found->second;
            if (!Profile.Crowd->UpdateAgent(Registered.Index, Owner->GetWorldPosition(), Controller->GetVelocity(), Settings))
            {
                continue;
            }
            ActiveAgents.insert(Agent->GetID());
            if (bMoving)
            {
                if (!Registered.bMoving || Registered.Target != Target || Registered.SteeringRevision != Agent->GetSteeringRevision())
                {
                    if (!Profile.Crowd->SetTarget(Registered.Index, Target))
                    {
                        Registered.bMoving = false;
                        continue;
                    }
                }
                Registered.Target = Target;
                Registered.SteeringRevision = Agent->GetSteeringRevision();
            }
            else
            {
                Profile.Crowd->StopAgent(Registered.Index);
            }
            Registered.bMoving = bMoving;
        }
        for (auto Iterator = Profile.CrowdAgents.begin(); Iterator != Profile.CrowdAgents.end();)
        {
            if (ActiveAgents.count(Iterator->first) == 0)
            {
                Profile.Crowd->RemoveAgent(Iterator->second.Index);
                Iterator = Profile.CrowdAgents.erase(Iterator);
            }
            else
            {
                ++Iterator;
            }
        }
        Profile.Crowd->Tick(DeltaTime);
    }
}

void NavigationWorld::ResetCrowds()
{
    for (auto& Entry : Profiles)
    {
        Entry.second.Crowd.reset();
        Entry.second.CrowdAgents.clear();
        Entry.second.CrowdGeneration = 0;
    }
}

bool NavigationWorld::GetCrowdVelocity(const NavigationAgentComponent& Agent, Vector3& Velocity) const
{
    auto Profile = Profiles.find(Agent.AgentProfile);
    if (Profile == Profiles.end() || !Profile->second.Crowd)
    {
        return false;
    }
    const auto Found = Profile->second.CrowdAgents.find(Agent.GetID());
    Vector3 Target;
    if (Found == Profile->second.CrowdAgents.end() || Found->second.Component != Agent.GetObjectHandle()
        || !Found->second.bMoving || Found->second.SteeringRevision != Agent.GetSteeringRevision()
        || !Agent.GetSteeringTarget(Target) || Target != Found->second.Target)
    {
        return false;
    }
    Velocity = Profile->second.Crowd->GetVelocity(Found->second.Index);
    return true;
}

bool NavigationWorld::BuildSynchronously()
{
    AssertGameThread();
    CancelBuilds();
    Diagnostic.clear();
    bWaitingForAssets = false;
    bInputRejected = false;
    bBuildRequested = true;
    std::map<std::string, NavigationBuildInput> Inputs;
    if (!CollectInputs(Inputs))
    {
        RejectBuildInputs();
        return false;
    }
    for (auto Iterator = Profiles.begin(); Iterator != Profiles.end();)
    {
        if (Inputs.count(Iterator->first) == 0)
        {
            Iterator = Profiles.erase(Iterator);
        }
        else
        {
            ++Iterator;
        }
    }
    bBuildRequested = false;
    bInputRejected = false;
    SourceFingerprint = 0;
    bool bSuccess = true;
    for (auto& Entry : Inputs)
    {
        ProfileState& Profile = Profiles[Entry.first];
        Profile.Input = std::make_shared<NavigationBuildInput>(std::move(Entry.second));
        Profile.BaseInput = Profile.Input;
        Profile.WantedFingerprint = Profile.Input->Fingerprint;
        std::shared_ptr<NavigationMesh> Built = NavigationMesh::Build(*Profile.Input, Profile.Error);
        if (Built)
        {
            Profile.Mesh = std::move(Built);
            Profile.PublishedInput = Profile.Input;
            Profile.Generation = NextGeneration++;
            Profile.State = NavigationBuildState::Ready;
        }
        else
        {
            Profile.State = NavigationBuildState::Failed;
            bSuccess = false;
            Diagnostic = Profile.Error;
        }
    }
    return bSuccess;
}

bool NavigationWorld::HasPreparationFailed() const
{
    if (bInputRejected && !bWaitingForAssets)
    {
        return true;
    }
    for (const auto& Entry : Profiles)
    {
        if (Entry.second.State == NavigationBuildState::Failed)
        {
            return true;
        }
    }
    return false;
}

const std::string& NavigationWorld::GetDiagnostic(const std::string& ProfileName) const
{
    const auto Found = Profiles.find(ProfileName);
    if (Found != Profiles.end() && !Found->second.Error.empty())
    {
        return Found->second.Error;
    }
    return Diagnostic;
}

void NavigationWorld::CancelBuilds()
{
    for (auto& Entry : Profiles)
    {
        if (Entry.second.Pending)
        {
            Entry.second.Pending->Cancellation->store(true);
            Entry.second.Pending.reset();
            Entry.second.State = NavigationBuildState::Stale;
        }
    }
}

bool NavigationWorld::IsReady(const std::string& ProfileName) const
{
    auto Found = Profiles.find(ProfileName);
    if (Found == Profiles.end() || !Found->second.Mesh)
    {
        return false;
    }
    return Found->second.State == NavigationBuildState::Ready
        || (World.IsPlaying() && Found->second.Mode != NavigationGenerationMode::Static
            && (Found->second.State == NavigationBuildState::Building || Found->second.State == NavigationBuildState::Stale));
}

NavigationBuildState NavigationWorld::GetBuildState(const std::string& ProfileName) const
{
    auto Found = Profiles.find(ProfileName);
    if (Found != Profiles.end())
    {
        return Found->second.State;
    }
    return NavigationBuildState::Empty;
}

const NavigationMesh* NavigationWorld::GetMesh(const std::string& ProfileName) const
{
    auto Found = Profiles.find(ProfileName);
    if (Found == Profiles.end())
    {
        return nullptr;
    }
    return Found->second.Mesh.get();
}

uint64_t NavigationWorld::GetGeneration(const std::string& ProfileName) const
{
    auto Found = Profiles.find(ProfileName);
    if (Found == Profiles.end())
    {
        return 0;
    }
    return Found->second.Generation;
}

NavigationPath NavigationWorld::FindPath(const Vector3& Start, const Vector3& Goal,
    const std::string& ProfileName, const NavigationQueryFilter& Filter) const
{
    if (!IsReady(ProfileName))
    {
        return {};
    }
    NavigationPath Result = GetMesh(ProfileName)->FindPath(Start, Goal, Filter);
    Result.Generation = GetGeneration(ProfileName);
    return Result;
}

bool NavigationWorld::ProjectPoint(const Vector3& Position, Vector3& Projected, const std::string& ProfileName,
    const Vector3& Extent, const NavigationQueryFilter& Filter) const
{
    return IsReady(ProfileName) && GetMesh(ProfileName)->ProjectPoint(Position, Projected, Extent, Filter);
}

std::vector<NavigationDebugTriangle> NavigationWorld::GetDebugTriangles() const
{
    std::vector<NavigationDebugTriangle> Result;
    for (const auto& Entry : Profiles)
    {
        if (Entry.second.Mesh)
        {
            const auto Triangles = Entry.second.Mesh->GetDebugTriangles();
            Result.insert(Result.end(), Triangles.begin(), Triangles.end());
        }
    }
    return Result;
}

std::vector<std::string> NavigationWorld::GetProfiles() const
{
    std::vector<std::string> Result;
    for (const auto& Entry : Profiles)
    {
        Result.push_back(Entry.first);
    }
    return Result;
}

void NavigationWorld::Serialize(nlohmann::json& Document) const
{
    Document = nlohmann::json::array();
    for (const auto& Entry : Profiles)
    {
        if (Entry.second.State == NavigationBuildState::Ready && Entry.second.Mesh)
        {
            nlohmann::json Saved;
            Entry.second.Mesh->Serialize(Saved);
            Document.push_back(std::move(Saved));
        }
    }
}

bool NavigationWorld::Deserialize(const nlohmann::json& Document, std::string& Error)
{
    if (!Document.is_array() || Document.size() > 32)
    {
        Error = "Saved navigation must be an array of at most 32 agent profiles";
        return false;
    }
    std::map<std::string, ProfileState> Loaded;
    for (const auto& Entry : Document)
    {
        std::shared_ptr<NavigationMesh> Mesh = NavigationMesh::Deserialize(Entry, Error);
        if (!Mesh || Loaded.count(Mesh->GetSettings().AgentProfile) != 0)
        {
            return false;
        }
        ProfileState& Profile = Loaded[Mesh->GetSettings().AgentProfile];
        Profile.WantedFingerprint = Mesh->GetFingerprint();
        Profile.Mesh = std::move(Mesh);
        Profile.Generation = NextGeneration++;
        Profile.State = NavigationBuildState::Ready;
    }
    CancelBuilds();
    SourceFingerprint = 0;
    Profiles = std::move(Loaded);
    return true;
}

void NavigationWorld::CopyMeshesFrom(const NavigationWorld& Source)
{
    CancelBuilds();
    SourceFingerprint = 0;
    Profiles.clear();
    for (const auto& Entry : Source.Profiles)
    {
        if (!Entry.second.Mesh || Entry.second.State != NavigationBuildState::Ready)
        {
            continue;
        }
        ProfileState& Profile = Profiles[Entry.first];
        Profile.Mesh = Entry.second.Mesh;
        Profile.Input = Entry.second.Input;
        Profile.BaseInput = Entry.second.BaseInput;
        Profile.PublishedInput = Entry.second.PublishedInput;
        Profile.WantedFingerprint = Entry.second.WantedFingerprint;
        Profile.Generation = NextGeneration++;
        Profile.Mode = Entry.second.Mode;
        Profile.State = NavigationBuildState::Ready;
    }
}
