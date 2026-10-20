#include "Navigation/NavigationCrowd.h"
#include "Navigation/Recast/NavigationMeshInternal.h"
#include "Core/Threading/ThreadContext.h"

#include <DetourCrowd.h>
#include <DetourNavMeshQuery.h>
#include <algorithm>
#include <cmath>
#include <optional>

struct NavigationCrowd::Implementation
{
    std::shared_ptr<const NavigationMesh> Mesh;
    std::unique_ptr<dtCrowd, decltype(&dtFreeCrowd)> Crowd{nullptr, dtFreeCrowd};
    std::array<std::optional<NavigationQueryFilter>, DT_CROWD_MAX_QUERY_FILTER_TYPE> Filters;
    dtNavMeshQuery PositionQuery;

    int GetFilterIndex(const NavigationQueryFilter& Filter, int ExistingAgent)
    {
        for (float Cost : Filter.AreaCosts)
        {
            if (!std::isfinite(Cost) || Cost < 1.f)
            {
                return -1;
            }
        }
        int AvailableIndex = -1;
        for (size_t FilterIndex = 0; FilterIndex < Filters.size(); ++FilterIndex)
        {
            if (Filters[FilterIndex] && (Filters[FilterIndex]->IncludedAreas == Filter.IncludedAreas
                && Filters[FilterIndex]->ExcludedAreas == Filter.ExcludedAreas && Filters[FilterIndex]->AreaCosts == Filter.AreaCosts))
            {
                return static_cast<int>(FilterIndex);
            }
            bool bUsed = false;
            for (int AgentIndex = 0; AgentIndex < Crowd->getAgentCount(); ++AgentIndex)
            {
                const dtCrowdAgent* Agent = Crowd->getAgent(AgentIndex);
                if (AgentIndex != ExistingAgent && Agent->active && Agent->params.queryFilterType == FilterIndex)
                {
                    bUsed = true;
                    break;
                }
            }
            if (!bUsed && AvailableIndex < 0)
            {
                AvailableIndex = static_cast<int>(FilterIndex);
            }
        }
        if (AvailableIndex < 0)
        {
            return -1;
        }
        Filters[AvailableIndex] = Filter;
        dtQueryFilter* NativeFilter = Crowd->getEditableFilter(AvailableIndex);
        NativeFilter->setIncludeFlags(Filter.IncludedAreas);
        NativeFilter->setExcludeFlags(Filter.ExcludedAreas);
        for (size_t AreaIndex = 0; AreaIndex < Filter.AreaCosts.size(); ++AreaIndex)
        {
            NativeFilter->setAreaCost(static_cast<int>(AreaIndex + 1), Filter.AreaCosts[AreaIndex]);
        }
        return AvailableIndex;
    }

    bool MakeParameters(const NavigationCrowdSettings& Settings, dtCrowdAgentParams& Parameters, int ExistingAgent = -1)
    {
        if (!std::isfinite(Settings.Radius) || Settings.Radius <= 0.f
            || Settings.Radius > Mesh->GetSettings().AgentRadius
            || !std::isfinite(Settings.Height) || Settings.Height <= 0.f
            || !std::isfinite(Settings.MaximumSpeed) || Settings.MaximumSpeed < 0.f
            || !std::isfinite(Settings.MaximumAcceleration) || Settings.MaximumAcceleration <= 0.f
            || !std::isfinite(Settings.AvoidanceRadius) || Settings.AvoidanceRadius <= 0.f)
        {
            return false;
        }
        const int FilterIndex = GetFilterIndex(Settings.QueryFilter, ExistingAgent);
        if (FilterIndex < 0)
        {
            return false;
        }
        Parameters = {};
        Parameters.radius = Settings.Radius;
        Parameters.height = Settings.Height;
        Parameters.maxAcceleration = Settings.MaximumAcceleration;
        Parameters.maxSpeed = Settings.MaximumSpeed;
        Parameters.collisionQueryRange = std::max(Settings.AvoidanceRadius, Settings.Radius * 4.f);
        Parameters.pathOptimizationRange = Settings.Radius * 30.f;
        Parameters.separationWeight = 2.f;
        Parameters.queryFilterType = static_cast<unsigned char>(FilterIndex);
        Parameters.updateFlags = DT_CROWD_ANTICIPATE_TURNS | DT_CROWD_OPTIMIZE_VIS | DT_CROWD_OPTIMIZE_TOPO;
        if (Settings.bAvoidOtherAgents)
        {
            Parameters.updateFlags |= DT_CROWD_OBSTACLE_AVOIDANCE | DT_CROWD_SEPARATION;
        }
        return true;
    }
};

NavigationCrowd::NavigationCrowd(std::shared_ptr<const NavigationMesh> Mesh) : State(std::make_unique<Implementation>())
{
    State->Mesh = std::move(Mesh);
}

NavigationCrowd::~NavigationCrowd() = default;

bool NavigationCrowd::Initialize(int MaximumAgents)
{
    AssertGameThread();
    if (!State->Mesh || !State->Mesh->State->Mesh || MaximumAgents < 1 || MaximumAgents > 2048)
    {
        return false;
    }
    State->Crowd.reset(dtAllocCrowd());
    if (!State->Crowd || !State->Crowd->init(MaximumAgents, State->Mesh->GetSettings().AgentRadius, State->Mesh->State->Mesh.get())
        || dtStatusFailed(State->PositionQuery.init(State->Mesh->State->Mesh.get(), 4096)))
    {
        return false;
    }
    dtObstacleAvoidanceParams Avoidance = *State->Crowd->getObstacleAvoidanceParams(0);
    Avoidance.velBias = 0.5f;
    Avoidance.adaptiveDivs = 7;
    Avoidance.adaptiveRings = 2;
    Avoidance.adaptiveDepth = 3;
    State->Crowd->setObstacleAvoidanceParams(0, &Avoidance);
    return true;
}

int NavigationCrowd::AddAgent(const Vector3& Position, const NavigationCrowdSettings& Settings)
{
    AssertGameThread();
    dtCrowdAgentParams Parameters{};
    if (!State->Crowd || !State->MakeParameters(Settings, Parameters))
    {
        return -1;
    }
    return State->Crowd->addAgent(&Position.x, &Parameters);
}

void NavigationCrowd::RemoveAgent(int AgentIndex)
{
    AssertGameThread();
    if (State->Crowd && AgentIndex >= 0 && AgentIndex < State->Crowd->getAgentCount())
    {
        State->Crowd->removeAgent(AgentIndex);
    }
}

bool NavigationCrowd::UpdateAgent(int AgentIndex, const Vector3& Position, const Vector3& Velocity,
    const NavigationCrowdSettings& Settings)
{
    AssertGameThread();
    dtCrowdAgentParams Parameters{};
    if (!State->Crowd || AgentIndex < 0 || AgentIndex >= State->Crowd->getAgentCount()
        || !State->Crowd->getAgent(AgentIndex)->active || !State->MakeParameters(Settings, Parameters, AgentIndex))
    {
        return false;
    }
    State->Crowd->updateAgentParameters(AgentIndex, &Parameters);
    dtCrowdAgent* Agent = State->Crowd->getEditableAgent(AgentIndex);
    dtNavMeshQuery* Query = &State->PositionQuery;
    const dtQueryFilter* Filter = State->Crowd->getFilter(Parameters.queryFilterType);
    if ((Position - Vector3(Agent->npos)).LengthSquared() > 1.f || Agent->state == DT_CROWDAGENT_STATE_INVALID)
    {
        dtPolyRef Polygon = 0;
        float Projected[3]{};
        Query->findNearestPoly(&Position.x, State->Crowd->getQueryHalfExtents(), Filter, &Polygon, Projected);
        if (Polygon == 0)
        {
            return false;
        }
        Agent->corridor.reset(Polygon, Projected);
        Agent->state = DT_CROWDAGENT_STATE_WALKING;
    }
    else
    {
        Agent->corridor.movePosition(&Position.x, Query, Filter);
    }
    for (int AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        Agent->npos[AxisIndex] = Agent->corridor.getPos()[AxisIndex];
    }
    Agent->vel[0] = Velocity.x;
    Agent->vel[1] = 0.f;
    Agent->vel[2] = Velocity.z;
    return true;
}

bool NavigationCrowd::SetTarget(int AgentIndex, const Vector3& Target)
{
    AssertGameThread();
    if (!State->Crowd || AgentIndex < 0 || AgentIndex >= State->Crowd->getAgentCount())
    {
        return false;
    }
    const dtCrowdAgent* Agent = State->Crowd->getAgent(AgentIndex);
    dtPolyRef Polygon = 0;
    float Projected[3]{};
    const dtStatus Status = State->Crowd->getNavMeshQuery()->findNearestPoly(&Target.x,
        State->Crowd->getQueryHalfExtents(), State->Crowd->getFilter(Agent->params.queryFilterType), &Polygon, Projected);
    if (dtStatusFailed(Status) || Polygon == 0)
    {
        return false;
    }
    return State->Crowd->requestMoveTarget(AgentIndex, Polygon, Projected);
}

void NavigationCrowd::StopAgent(int AgentIndex)
{
    AssertGameThread();
    if (State->Crowd && AgentIndex >= 0 && AgentIndex < State->Crowd->getAgentCount())
    {
        const float Velocity[3] = {0.f, 0.f, 0.f};
        State->Crowd->requestMoveVelocity(AgentIndex, Velocity);
    }
}

void NavigationCrowd::Tick(float DeltaTime)
{
    AssertGameThread();
    if (State->Crowd && std::isfinite(DeltaTime) && DeltaTime > 0.f)
    {
        State->Crowd->update(std::min(DeltaTime, 0.1f), nullptr);
    }
}

Vector3 NavigationCrowd::GetVelocity(int AgentIndex) const
{
    AssertGameThread();
    if (State->Crowd && AgentIndex >= 0 && AgentIndex < State->Crowd->getAgentCount())
    {
        const dtCrowdAgent* Agent = State->Crowd->getAgent(AgentIndex);
        if (Agent->active)
        {
            return Vector3(Agent->vel);
        }
    }
    return Vector3::Zero;
}
