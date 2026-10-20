#pragma once

#include "Navigation/NavigationMesh.h"

struct NavigationCrowdSettings
{
    float Radius = 0.35f;
    float Height = 1.9f;
    float MaximumSpeed = 3.f;
    float MaximumAcceleration = 12.f;
    float AvoidanceRadius = 1.2f;
    bool bAvoidOtherAgents = true;
    NavigationQueryFilter QueryFilter;
};

class NavigationCrowd
{
public:
    explicit NavigationCrowd(std::shared_ptr<const NavigationMesh> Mesh);
    ~NavigationCrowd();
    bool Initialize(int MaximumAgents = 256);
    int AddAgent(const Vector3& Position, const NavigationCrowdSettings& Settings);
    void RemoveAgent(int AgentIndex);
    bool UpdateAgent(int AgentIndex, const Vector3& Position, const Vector3& Velocity, const NavigationCrowdSettings& Settings);
    bool SetTarget(int AgentIndex, const Vector3& Target);
    void StopAgent(int AgentIndex);
    void Tick(float DeltaTime);
    Vector3 GetVelocity(int AgentIndex) const;

private:
    struct Implementation;
    std::unique_ptr<Implementation> State;
};
