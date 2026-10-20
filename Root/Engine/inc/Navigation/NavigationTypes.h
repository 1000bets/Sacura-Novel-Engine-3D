#pragma once

#include "Physics/PhysicsTypes.h"

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

enum class NavigationGenerationMode
{
    Static,
    Dynamic,
    DynamicModifiersOnly
};

class NavigationMesh;

struct NavigationBuildSettings
{
    std::string AgentProfile = "Default";
    float AgentRadius = 0.35f;
    float AgentHeight = 1.9f;
    float MaximumStepHeight = 0.3f;
    float MaximumSlopeDegrees = 45.f;
    float CellSize = 0.15f;
    float CellHeight = 0.1f;
    int TileSize = 64;
    int MinimumRegionArea = 8;
    int MergeRegionArea = 20;
    float MaximumSimplificationError = 1.3f;
};

struct NavigationModifier
{
    Matrix WorldToLocal = Matrix::Identity;
    Vector3 HalfExtents = Vector3::One;
    uint8_t Area = 0;
    int Priority = 0;
};

struct NavigationLink
{
    Vector3 Start = Vector3::Zero;
    Vector3 End = Vector3::Zero;
    float Radius = 0.5f;
    uint32_t Identity = 0;
    uint8_t Area = 1;
    bool bBidirectional = true;
};

struct NavigationBuildInput
{
    NavigationBuildSettings Settings;
    PhysicsTriangleMesh Geometry;
    std::vector<AxisAlignedBounds> Bounds;
    std::vector<NavigationModifier> Modifiers;
    std::vector<NavigationLink> Links;
    uint64_t Fingerprint = 0;
    std::shared_ptr<std::atomic<bool>> Cancellation;
    std::shared_ptr<const NavigationMesh> PreviousMesh;
    std::vector<AxisAlignedBounds> DirtyBounds;
};

struct NavigationQueryFilter
{
    NavigationQueryFilter() { AreaCosts.fill(1.f); }
    std::array<float, 16> AreaCosts;
    uint16_t IncludedAreas = UINT16_MAX;
    uint16_t ExcludedAreas = 0;
    int MaximumSearchNodes = 4096;
    int MaximumPathPolygons = 2048;
    bool bAllowPartialPath = false;
};

enum class NavigationPathStatus
{
    Complete,
    Partial,
    NotReady,
    StartOutsideNavigation,
    GoalOutsideNavigation,
    Unreachable,
    SearchLimitReached,
    InvalidInput
};

struct NavigationPathPoint
{
    Vector3 Position = Vector3::Zero;
    bool bOffMeshConnection = false;
    uint32_t LinkIdentity = 0;
    Vector3 LinkEnd = Vector3::Zero;
};

struct NavigationPath
{
    NavigationPathStatus Status = NavigationPathStatus::NotReady;
    std::vector<NavigationPathPoint> Points;
    Vector3 RequestedGoal = Vector3::Zero;
    Vector3 ProjectedGoal = Vector3::Zero;
    uint64_t Generation = 0;
    std::string Diagnostic;
    bool IsUsable() const
    {
        return Status == NavigationPathStatus::Complete || Status == NavigationPathStatus::Partial;
    }
};

struct NavigationDebugTriangle
{
    Vector3 First = Vector3::Zero;
    Vector3 Second = Vector3::Zero;
    Vector3 Third = Vector3::Zero;
    uint8_t Area = 1;
};

struct NavigationStatistics
{
    uint32_t TileCount = 0;
    uint32_t PolygonCount = 0;
    uint32_t RebuiltTileCount = 0;
    uint32_t ReusedTileCount = 0;
    uint64_t DataBytes = 0;
    double BuildMilliseconds = 0.0;
};

enum class NavigationBuildState
{
    Empty,
    Building,
    Ready,
    Stale,
    Failed
};

bool ValidateNavigationSettings(const NavigationBuildSettings& Settings, std::string& Error);
