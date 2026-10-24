#include "World/Components/NavigationAgentComponent.h"
#include "World/Components/PhysicsBodyComponent.h"
#include "World/GameObject.h"
#include "World/Scene.h"
#include "Navigation/NavigationWorld.h"
#include "Core/Threading/ThreadContext.h"

#include <algorithm>
#include <cmath>

uint64_t NavigationAgentComponent::MoveTo(const Vector3& NewDestination)
{
    AssertGameThread();
    StopMovement();
    ++RequestIdentity;
    Destination = NewDestination;
    bHasDestination = true;
    BlockedTime = 0.f;
    Path = {};
    PreviousPosition = GetGameObject()->GetWorldPosition();
    MovementState = NavigationAgentState::WaitingForNavigation;
    Diagnostic.clear();
    return RequestIdentity;
}

void NavigationAgentComponent::StopMovement()
{
    Velocity = Vector3::Zero;
    if (GetGameObject() == nullptr || GetGameObject()->GetScene() == nullptr)
    {
        return;
    }
    if (PhysicsBodyComponent* Controller = GetCharacterController())
    {
        Controller->SetCharacterVelocity(Vector3::Zero);
    }
}

PhysicsBodyComponent* NavigationAgentComponent::GetCharacterController() const
{
    for (PhysicsBodyComponent* Body : GetGameObject()->GetComponents<PhysicsBodyComponent>())
    {
        if (Body->MotionType == PhysicsMotionType::Character)
        {
            return Body;
        }
    }
    return nullptr;
}

void NavigationAgentComponent::SetEnabled(bool bEnabled)
{
    if (!bEnabled)
    {
        StopMovement();
    }
    Component::SetEnabled(bEnabled);
}

void NavigationAgentComponent::OnDestroy()
{
    StopMovement();
}

void NavigationAgentComponent::Stop()
{
    AssertGameThread();
    StopMovement();
    bHasDestination = false;
    MovementState = NavigationAgentState::Cancelled;
}

void NavigationAgentComponent::EndPlay()
{
    Stop();
    Path = {};
}

void NavigationAgentComponent::Fail(const std::string& Error)
{
    StopMovement();
    bHasDestination = false;
    MovementState = NavigationAgentState::Failed;
    Diagnostic = Error;
}

bool NavigationAgentComponent::SetAreaCost(int Area, float Cost)
{
    if (Area < 1 || Area > 16 || !std::isfinite(Cost) || Cost < 1.f)
    {
        return false;
    }
    QueryFilter.AreaCosts[Area - 1] = Cost;
    Path.Generation = 0;
    return true;
}

bool NavigationAgentComponent::PlanPath()
{
    NavigationWorld& Navigation = GetGameObject()->GetScene()->GetNavigation();
    if (!Navigation.IsReady(AgentProfile))
    {
        StopMovement();
        MovementState = NavigationAgentState::WaitingForNavigation;
        if (Navigation.GetBuildState(AgentProfile) == NavigationBuildState::Failed)
        {
            Fail(Navigation.GetDiagnostic(AgentProfile));
        }
        else if (Navigation.GetBuildState(AgentProfile) == NavigationBuildState::Empty)
        {
            Fail("Navigation agent profile has no navigation bounds");
        }
        return false;
    }
    const PhysicsBodyComponent* Controller = GetCharacterController();
    if (Controller == nullptr || Controller->MotionType != PhysicsMotionType::Character
        || !Controller->IsEnabled() || Controller->IsPendingRemoval())
    {
        Fail("Navigation agent requires an enabled character controller on the same GameObject");
        return false;
    }
    const NavigationBuildSettings& Settings = Navigation.GetMesh(AgentProfile)->GetSettings();
    PhysicsBodyDefinition Definition;
    std::string Error;
    if (!Controller->BuildDefinition(Definition, Error)
        || Definition.Radius > Settings.AgentRadius + 0.001f
        || 2.f * (Definition.Radius + Definition.CapsuleHalfHeight) > Settings.AgentHeight + 0.001f)
    {
        Fail("Character dimensions exceed the navigation agent profile");
        return false;
    }
    QueryFilter.bAllowPartialPath = bAllowPartialPath;
    QueryFilter.ExcludedAreas = static_cast<uint16_t>(ExcludedAreas);
    Path = Navigation.FindPath(GetGameObject()->GetWorldPosition(), Destination, AgentProfile, QueryFilter);
    if (!Path.IsUsable() || Path.Points.empty())
    {
        Fail("Navigation path failed with status " + std::to_string(static_cast<int>(Path.Status)));
        return false;
    }
    CornerIndex = 0;
    ++SteeringRevision;
    BlockedTime = 0.f;
    MovementState = NavigationAgentState::Moving;
    return true;
}

bool NavigationAgentComponent::GetSteeringTarget(Vector3& Target) const
{
    if (MovementState == NavigationAgentState::Moving && CornerIndex < Path.Points.size())
    {
        Target = Path.Points[CornerIndex].Position;
        return true;
    }
    return false;
}

Vector3 NavigationAgentComponent::GetPendingLinkEnd() const
{
    if (MovementState == NavigationAgentState::WaitingForLink && CornerIndex < Path.Points.size())
    {
        return Path.Points[CornerIndex].LinkEnd;
    }
    return GetGameObject()->GetWorldPosition();
}

bool NavigationAgentComponent::CompleteLinkTraversal(bool bSuccess)
{
    if (MovementState != NavigationAgentState::WaitingForLink)
    {
        return false;
    }
    if (!bSuccess)
    {
        Fail("Navigation link traversal failed");
        return true;
    }
    if ((GetGameObject()->GetWorldPosition() - GetPendingLinkEnd()).Length() > std::max(0.25f, AcceptanceRadius * 2.f))
    {
        Diagnostic = "Navigation link completion requires arriving at its end";
        return false;
    }
    Path.Generation = 0;
    MovementState = NavigationAgentState::Moving;
    return true;
}

void NavigationAgentComponent::Tick(float DeltaTime)
{
    if (!GetGameObject()->GetScene()->IsPlaying() || !bHasDestination || DeltaTime <= 0.f)
    {
        return;
    }
    if (GetGameObject()->IsStoryControllingTransform())
    {
        Stop();
        Diagnostic = "Navigation movement cancelled by story transform control";
        return;
    }
    if (!std::isfinite(MaximumSpeed) || MaximumSpeed < 0.f || !std::isfinite(Acceleration) || Acceleration <= 0.f
        || !std::isfinite(AcceptanceRadius) || AcceptanceRadius <= 0.f || !std::isfinite(BlockedTimeout) || BlockedTimeout <= 0.f
        || (bAvoidOtherAgents && (!std::isfinite(AvoidanceRadius) || AvoidanceRadius <= 0.f)))
    {
        Fail("Navigation movement settings are invalid");
        return;
    }
    NavigationWorld& Navigation = GetGameObject()->GetScene()->GetNavigation();
    if (QueryFilter.ExcludedAreas != static_cast<uint16_t>(ExcludedAreas) || QueryFilter.bAllowPartialPath != bAllowPartialPath)
    {
        Path.Generation = 0;
    }
    if (MovementState == NavigationAgentState::WaitingForLink)
    {
        StopMovement();
        const NavigationMesh* Mesh = Navigation.GetMesh(AgentProfile);
        if (Mesh == nullptr || CornerIndex >= Path.Points.size() || !Mesh->HasLink(Path.Points[CornerIndex].LinkIdentity))
        {
            Fail("Navigation link is no longer available");
        }
        return;
    }
    if (MovementState == NavigationAgentState::WaitingForNavigation
        || Path.Generation != Navigation.GetGeneration(AgentProfile))
    {
        if (!PlanPath())
        {
            return;
        }
    }
    const Vector3 Position = GetGameObject()->GetWorldPosition();
    float VerticalTolerance = 0.2f;
    if (const NavigationMesh* Mesh = Navigation.GetMesh(AgentProfile))
    {
        VerticalTolerance = std::max(VerticalTolerance, Mesh->GetSettings().CellHeight * 2.f);
    }
    while (CornerIndex < Path.Points.size())
    {
        Vector3 Difference = Path.Points[CornerIndex].Position - Position;
        const float VerticalDifference = std::abs(Difference.y);
        Difference.y = 0.f;
        if (Difference.Length() > AcceptanceRadius || VerticalDifference > VerticalTolerance)
        {
            break;
        }
        if (Path.Points[CornerIndex].bOffMeshConnection)
        {
            StopMovement();
            MovementState = NavigationAgentState::WaitingForLink;
            return;
        }
        ++CornerIndex;
    }
    if (CornerIndex >= Path.Points.size())
    {
        StopMovement();
        bHasDestination = false;
        MovementState = NavigationAgentState::Reached;
        if (Path.Status == NavigationPathStatus::Partial)
        {
            MovementState = NavigationAgentState::PartialReached;
        }
        return;
    }
    Vector3 Direction = Path.Points[CornerIndex].Position - Position;
    Direction.y = 0.f;
    const float Distance = Direction.Length();
    Direction.Normalize();
    Vector3 DesiredVelocity = Direction * std::min(MaximumSpeed, Distance / DeltaTime);
    if (bAvoidOtherAgents && AvoidanceRadius > 0.f)
    {
        Navigation.GetCrowdVelocity(*this, DesiredVelocity);
    }
    if (DesiredVelocity.Length() > MaximumSpeed)
    {
        DesiredVelocity.Normalize();
        DesiredVelocity *= MaximumSpeed;
    }
    const float VelocityDifference = (DesiredVelocity - Velocity).Length();
    if (VelocityDifference > 0.000001f)
    {
        Velocity = Vector3::Lerp(Velocity, DesiredVelocity, std::min(1.f, Acceleration * DeltaTime / VelocityDifference));
    }
    Vector3 Boundary;
    if (const NavigationMesh* Mesh = Navigation.GetMesh(AgentProfile))
    {
        if (Mesh->MoveAlongSurface(Position, Position + Velocity * DeltaTime, Boundary, QueryFilter))
        {
            Velocity = (Boundary - Position) / DeltaTime;
            Velocity.y = 0.f;
        }
        else
        {
            Velocity = Vector3::Zero;
            Path.Generation = 0;
        }
    }
    PhysicsBodyComponent* Controller = GetCharacterController();
    if (Controller == nullptr || !Controller->IsEnabled() || Controller->IsPendingRemoval())
    {
        Fail("Navigation character controller is no longer available");
        return;
    }
    Controller->SetCharacterVelocity(Velocity);
    if ((Position - PreviousPosition).LengthSquared() < 0.000001f && MaximumSpeed > 0.f)
    {
        BlockedTime += DeltaTime;
    }
    else
    {
        BlockedTime = 0.f;
    }
    PreviousPosition = Position;
    if (BlockedTime >= BlockedTimeout)
    {
        StopMovement();
        bHasDestination = false;
        MovementState = NavigationAgentState::Blocked;
        Diagnostic = "Navigation movement is blocked";
    }
}
