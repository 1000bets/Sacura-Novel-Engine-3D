#pragma once

#include "World/Components/Component.h"
#include "Reflection/ReflectionMacros.h"
#include "Navigation/NavigationTypes.h"

enum class NavigationAgentState
{
    Idle,
    WaitingForNavigation,
    Moving,
    Reached,
    PartialReached,
    WaitingForLink,
    Blocked,
    Failed,
    Cancelled
};

class PhysicsBodyComponent;

class NavigationAgentComponent : public Component
{
    ENGINE_CLASS(NavigationAgentComponent, Component, "engine.NavigationAgentComponent", 1)
public:
    std::string AgentProfile = "Default";
    ENGINE_REFLECT_FIELD(NavigationAgentComponent, AgentProfile, "agent_profile", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation Agent"), RF_DISPLAY_NAME("Agent Profile"))
    float MaximumSpeed = 3.f;
    ENGINE_REFLECT_FIELD(NavigationAgentComponent, MaximumSpeed, "maximum_speed", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation Agent"), RF_DISPLAY_NAME("Maximum Speed (m/s)"), RF_UI_RANGE(0, 1000))
    float Acceleration = 12.f;
    ENGINE_REFLECT_FIELD(NavigationAgentComponent, Acceleration, "acceleration", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation Agent"), RF_DISPLAY_NAME("Acceleration (m/s2)"), RF_UI_RANGE(0.001, 10000))
    float AcceptanceRadius = 0.1f;
    ENGINE_REFLECT_FIELD(NavigationAgentComponent, AcceptanceRadius, "acceptance_radius", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation Agent"), RF_DISPLAY_NAME("Acceptance Radius"), RF_UI_RANGE(0.001, 1000))
    float BlockedTimeout = 2.f;
    ENGINE_REFLECT_FIELD(NavigationAgentComponent, BlockedTimeout, "blocked_timeout", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation Agent"), RF_DISPLAY_NAME("Blocked Timeout (seconds)"), RF_UI_RANGE(0.1, 1000))
    bool bAllowPartialPath = false;
    ENGINE_REFLECT_FIELD(NavigationAgentComponent, bAllowPartialPath, "allow_partial_path", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation Agent"), RF_DISPLAY_NAME("Allow Partial Path"))
    bool bAvoidOtherAgents = true;
    ENGINE_REFLECT_FIELD(NavigationAgentComponent, bAvoidOtherAgents, "avoid_other_agents", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation Agent"), RF_DISPLAY_NAME("Avoid Other Agents"))
    float AvoidanceRadius = 1.2f;
    ENGINE_REFLECT_FIELD(NavigationAgentComponent, AvoidanceRadius, "avoidance_radius", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation Agent"), RF_DISPLAY_NAME("Avoidance Radius"), RF_UI_RANGE(0.001, 1000))
    int64_t ExcludedAreas = 0;
    ENGINE_REFLECT_FIELD(NavigationAgentComponent, ExcludedAreas, "excluded_areas", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation Agent"), RF_DISPLAY_NAME("Excluded Area Mask"), RF_UI_RANGE(0, 65535))

    uint64_t MoveTo(const Vector3& Destination);
    void Stop();
    bool CompleteLinkTraversal(bool bSuccess);
    void Tick(float DeltaTime) override;
    void EndPlay() override;
    void SetEnabled(bool bEnabled) override;
    void OnDestroy() override;
    NavigationAgentState GetMovementState() const { return MovementState; }
    const NavigationPath& GetPath() const { return Path; }
    uint64_t GetRequestId() const { return RequestIdentity; }
    const Vector3& GetDestination() const { return Destination; }
    const std::string& GetDiagnostic() const { return Diagnostic; }
    const NavigationQueryFilter& GetQueryFilter() const { return QueryFilter; }
    uint64_t GetSteeringRevision() const { return SteeringRevision; }
    bool GetSteeringTarget(Vector3& Target) const;
    bool SetAreaCost(int Area, float Cost);
    Vector3 GetPendingLinkEnd() const;
    int64_t GetMovementStateValue() const { return static_cast<int64_t>(MovementState); }
    ENGINE_REFLECT_READONLY(NavigationAgentComponent, StateProperty, "movement_state", int64_t,
        GetMovementStateValue, RF_EDITOR_READONLY, RF_DISPLAY_NAME("Movement State"))
protected:
    NavigationAgentComponent() = default;
private:
    void Fail(const std::string& Error);
    void StopMovement();
    bool PlanPath();
    PhysicsBodyComponent* GetCharacterController() const;
    NavigationAgentState MovementState = NavigationAgentState::Idle;
    NavigationPath Path;
    NavigationQueryFilter QueryFilter;
    Vector3 Destination = Vector3::Zero;
    Vector3 Velocity = Vector3::Zero;
    Vector3 PreviousPosition = Vector3::Zero;
    size_t CornerIndex = 0;
    uint64_t RequestIdentity = 0;
    uint64_t SteeringRevision = 0;
    float BlockedTime = 0.f;
    bool bHasDestination = false;
    std::string Diagnostic;
};
ENGINE_CLASS_END(NavigationAgentComponent)
ENGINE_IMPLEMENT_FIELD(NavigationAgentComponent, AgentProfile)
ENGINE_IMPLEMENT_FIELD(NavigationAgentComponent, MaximumSpeed)
ENGINE_IMPLEMENT_FIELD(NavigationAgentComponent, Acceleration)
ENGINE_IMPLEMENT_FIELD(NavigationAgentComponent, AcceptanceRadius)
ENGINE_IMPLEMENT_FIELD(NavigationAgentComponent, BlockedTimeout)
ENGINE_IMPLEMENT_FIELD(NavigationAgentComponent, bAllowPartialPath)
ENGINE_IMPLEMENT_FIELD(NavigationAgentComponent, bAvoidOtherAgents)
ENGINE_IMPLEMENT_FIELD(NavigationAgentComponent, AvoidanceRadius)
ENGINE_IMPLEMENT_FIELD(NavigationAgentComponent, ExcludedAreas)
ENGINE_IMPLEMENT_READONLY(NavigationAgentComponent, StateProperty, int64_t, GetMovementStateValue)

