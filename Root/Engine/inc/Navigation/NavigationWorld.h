#pragma once

#include "Navigation/NavigationMesh.h"
#include "Navigation/NavigationCrowd.h"
#include "Core/Threading/JobSystem.h"

#include <map>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <nlohmann/json_fwd.hpp>

class Scene;
class NavigationAgentComponent;

class NavigationWorld
{
public:
    explicit NavigationWorld(Scene& World);
    ~NavigationWorld();
    void SetJobSystem(JobSystem* System) { Jobs = System; }
    void Update();
    void RequestBuild();
    bool PrepareForSimulation();
    void UpdateCrowds(float DeltaTime);
    void ResetCrowds();
    bool GetCrowdVelocity(const NavigationAgentComponent& Agent, Vector3& Velocity) const;
    bool BuildSynchronously();
    void CancelBuilds();
    bool IsReady(const std::string& Profile = "Default") const;
    NavigationBuildState GetBuildState(const std::string& Profile = "Default") const;
    NavigationPath FindPath(const Vector3& Start, const Vector3& Goal,
        const std::string& Profile = "Default", const NavigationQueryFilter& Filter = {}) const;
    bool ProjectPoint(const Vector3& Position, Vector3& Projected, const std::string& Profile = "Default",
        const Vector3& Extent = Vector3(1.f, 2.f, 1.f), const NavigationQueryFilter& Filter = {}) const;
    const NavigationMesh* GetMesh(const std::string& Profile = "Default") const;
    uint64_t GetGeneration(const std::string& Profile = "Default") const;
    std::vector<NavigationDebugTriangle> GetDebugTriangles() const;
    std::vector<std::string> GetProfiles() const;
    const std::string& GetDiagnostic(const std::string& Profile = {}) const;
    bool HasRejectedInputs() const { return bInputRejected; }
    bool HasPreparationFailed() const;
    void Serialize(nlohmann::json& Document) const;
    bool Deserialize(const nlohmann::json& Document, std::string& Error);
    void CopyMeshesFrom(const NavigationWorld& Source);

private:
    struct BuildCompletion
    {
        std::mutex Mutex;
        std::shared_ptr<NavigationMesh> Mesh;
        std::shared_ptr<NavigationBuildInput> Input;
        std::string Error;
        std::atomic<bool> bDone{false};
        std::shared_ptr<std::atomic<bool>> Cancellation = std::make_shared<std::atomic<bool>>(false);
    };

    struct ProfileState
    {
        struct CrowdAgent
        {
            ObjectHandle Component;
            int Index = -1;
            uint64_t SteeringRevision = 0;
            Vector3 Target = Vector3::Zero;
            bool bMoving = false;
        };

        std::shared_ptr<NavigationMesh> Mesh;
        std::unique_ptr<NavigationCrowd> Crowd;
        std::unordered_map<uint64_t, CrowdAgent> CrowdAgents;
        uint64_t CrowdGeneration = 0;
        std::shared_ptr<BuildCompletion> Pending;
        std::shared_ptr<NavigationBuildInput> Input;
        std::shared_ptr<NavigationBuildInput> BaseInput;
        std::shared_ptr<NavigationBuildInput> PublishedInput;
        uint64_t WantedFingerprint = 0;
        uint64_t Generation = 0;
        std::string Error;
        NavigationBuildState State = NavigationBuildState::Empty;
        NavigationGenerationMode Mode = NavigationGenerationMode::Static;
        bool bAutoBuild = true;
        bool bForceFullBuild = false;
    };

    bool CollectInputs(std::map<std::string, NavigationBuildInput>& Inputs);
    bool ComputeSourceFingerprint(uint64_t& Fingerprint);
    void PublishCompleted();
    void Schedule(ProfileState& Profile);
    void RejectBuildInputs();

    Scene& World;
    JobSystem* Jobs = nullptr;
    std::map<std::string, ProfileState> Profiles;
    std::string Diagnostic;
    uint64_t NextGeneration = 1;
    uint64_t SourceFingerprint = 0;
    bool bBuildRequested = false;
    bool bWaitingForAssets = false;
    bool bInputRejected = false;
};
