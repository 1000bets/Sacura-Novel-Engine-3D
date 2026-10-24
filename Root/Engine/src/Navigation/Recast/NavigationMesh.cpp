#include "Navigation/NavigationMesh.h"
#include "Assets/ContentHash.h"
#include "Navigation/Recast/NavigationMeshInternal.h"

#include <Recast.h>
#include <RecastDemo/Include/ChunkyTriMesh.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include "Core/IO/Base64.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

static bool IsFiniteNavigationVector(const Vector3& Value)
{
    return std::isfinite(Value.x) && std::isfinite(Value.y) && std::isfinite(Value.z)
        && std::abs(Value.x) <= 1000000.f && std::abs(Value.y) <= 1000000.f && std::abs(Value.z) <= 1000000.f;
}

bool ValidateNavigationSettings(const NavigationBuildSettings& Settings, std::string& Error)
{
    if (Settings.AgentProfile.empty() || !std::isfinite(Settings.AgentRadius) || Settings.AgentRadius <= 0.f
        || !std::isfinite(Settings.AgentHeight) || Settings.AgentHeight <= 0.f
        || !std::isfinite(Settings.MaximumStepHeight) || Settings.MaximumStepHeight < 0.f
        || Settings.MaximumStepHeight >= Settings.AgentHeight
        || !std::isfinite(Settings.MaximumSlopeDegrees) || Settings.MaximumSlopeDegrees < 0.f || Settings.MaximumSlopeDegrees >= 90.f
        || !std::isfinite(Settings.CellSize) || Settings.CellSize < 0.001f
        || !std::isfinite(Settings.CellHeight) || Settings.CellHeight < 0.001f
        || Settings.TileSize < 16 || Settings.TileSize > 256
        || Settings.MinimumRegionArea < 0 || Settings.MinimumRegionArea > 1024
        || Settings.MergeRegionArea < 0 || Settings.MergeRegionArea > 1024
        || !std::isfinite(Settings.MaximumSimplificationError) || Settings.MaximumSimplificationError < 0.f
        || Settings.AgentRadius / Settings.CellSize > 64.f
        || Settings.AgentHeight / Settings.CellHeight > 1024.f)
    {
        Error = "Navigation settings are invalid or exceed build limits";
        return false;
    }
    return true;
}

static bool MakeDetourFilter(const NavigationQueryFilter& Source, dtQueryFilter& Filter)
{
    if (Source.MaximumSearchNodes < 8 || Source.MaximumSearchNodes > 65535
        || Source.MaximumPathPolygons < 1 || Source.MaximumPathPolygons > 65535)
    {
        return false;
    }
    Filter.setIncludeFlags(Source.IncludedAreas);
    Filter.setExcludeFlags(Source.ExcludedAreas);
    for (size_t AreaIndex = 0; AreaIndex < Source.AreaCosts.size(); ++AreaIndex)
    {
        if (!std::isfinite(Source.AreaCosts[AreaIndex]) || Source.AreaCosts[AreaIndex] < 1.f)
        {
            return false;
        }
        Filter.setAreaCost(static_cast<int>(AreaIndex + 1), Source.AreaCosts[AreaIndex]);
    }
    return true;
}

static bool IsValidNavigationExtent(const Vector3& Extent, const NavigationBuildSettings& Settings)
{
    const float MaximumHorizontalExtent = Settings.CellSize * Settings.TileSize * 32.f;
    return IsFiniteNavigationVector(Extent) && Extent.x > 0.f && Extent.y > 0.f && Extent.z > 0.f
        && Extent.x <= MaximumHorizontalExtent && Extent.z <= MaximumHorizontalExtent && Extent.y <= 1000.f;
}

static bool ContainsNavigationPoint(const AxisAlignedBounds& Bounds, const Vector3& Point)
{
    return Point.x >= Bounds.Minimum.x && Point.x <= Bounds.Maximum.x
        && Point.y >= Bounds.Minimum.y && Point.y <= Bounds.Maximum.y
        && Point.z >= Bounds.Minimum.z && Point.z <= Bounds.Maximum.z;
}

static void ApplyNavigationVolumes(rcCompactHeightfield& Heightfield, const NavigationBuildInput& Input)
{
    std::vector<const AxisAlignedBounds*> RelevantBounds;
    for (const AxisAlignedBounds& Bounds : Input.Bounds)
    {
        if (Bounds.Minimum.x <= Heightfield.bmax[0] && Bounds.Maximum.x >= Heightfield.bmin[0]
            && Bounds.Minimum.z <= Heightfield.bmax[2] && Bounds.Maximum.z >= Heightfield.bmin[2])
        {
            RelevantBounds.push_back(&Bounds);
        }
    }
    for (int Depth = 0; Depth < Heightfield.height; ++Depth)
    {
        for (int Horizontal = 0; Horizontal < Heightfield.width; ++Horizontal)
        {
            const rcCompactCell& Cell = Heightfield.cells[Horizontal + Depth * Heightfield.width];
            for (unsigned int SpanIndex = Cell.index; SpanIndex < Cell.index + Cell.count; ++SpanIndex)
            {
                if (Heightfield.areas[SpanIndex] == RC_NULL_AREA)
                {
                    continue;
                }
                const Vector3 Position(Heightfield.bmin[0] + (Horizontal + 0.5f) * Heightfield.cs,
                    Heightfield.bmin[1] + Heightfield.spans[SpanIndex].y * Heightfield.ch,
                    Heightfield.bmin[2] + (Depth + 0.5f) * Heightfield.cs);
                bool bInside = false;
                for (const AxisAlignedBounds* Bounds : RelevantBounds)
                {
                    if (ContainsNavigationPoint(*Bounds, Position))
                    {
                        bInside = true;
                        break;
                    }
                }
                if (!bInside)
                {
                    Heightfield.areas[SpanIndex] = RC_NULL_AREA;
                }
            }
        }
    }
}

static void ApplyNavigationModifiers(rcCompactHeightfield& Heightfield, const NavigationBuildInput& Input)
{
    const std::vector<unsigned char> BaseAreas(Heightfield.areas, Heightfield.areas + Heightfield.spanCount);
    for (const NavigationModifier& Modifier : Input.Modifiers)
    {
        const AxisAlignedBounds Bounds = AxisAlignedBounds::FromCenterExtents(Vector3::Zero, Modifier.HalfExtents)
            .TransformedBy(Modifier.WorldToLocal.Invert());
        if (Bounds.Minimum.x > Heightfield.bmax[0] || Bounds.Maximum.x < Heightfield.bmin[0]
            || Bounds.Minimum.y > Heightfield.bmax[1] || Bounds.Maximum.y < Heightfield.bmin[1]
            || Bounds.Minimum.z > Heightfield.bmax[2] || Bounds.Maximum.z < Heightfield.bmin[2])
        {
            continue;
        }
        for (int Depth = 0; Depth < Heightfield.height; ++Depth)
        {
            for (int Horizontal = 0; Horizontal < Heightfield.width; ++Horizontal)
            {
                const rcCompactCell& Cell = Heightfield.cells[Horizontal + Depth * Heightfield.width];
                for (unsigned int SpanIndex = Cell.index; SpanIndex < Cell.index + Cell.count; ++SpanIndex)
                {
                    if (BaseAreas[SpanIndex] == RC_NULL_AREA)
                    {
                        continue;
                    }
                    const Vector3 Position(Heightfield.bmin[0] + (Horizontal + 0.5f) * Heightfield.cs,
                        Heightfield.bmin[1] + Heightfield.spans[SpanIndex].y * Heightfield.ch,
                        Heightfield.bmin[2] + (Depth + 0.5f) * Heightfield.cs);
                    const Vector3 Local = Vector3::Transform(Position, Modifier.WorldToLocal);
                    if (std::abs(Local.x) <= Modifier.HalfExtents.x && std::abs(Local.y) <= Modifier.HalfExtents.y
                        && std::abs(Local.z) <= Modifier.HalfExtents.z)
                    {
                        Heightfield.areas[SpanIndex] = Modifier.Area;
                    }
                }
            }
        }
    }
}

static bool BuildNavigationTile(const NavigationBuildInput& Input, const std::vector<float>& Vertices,
    const rcChunkyTriMesh& GeometryIndex, const AxisAlignedBounds& OverallBounds, int TileHorizontal, int TileDepth,
    dtNavMesh& Destination, NavigationStatistics& Statistics, std::string& Error)
{
    const NavigationBuildSettings& Settings = Input.Settings;
    rcConfig Configuration{};
    Configuration.cs = Settings.CellSize;
    Configuration.ch = Settings.CellHeight;
    Configuration.walkableSlopeAngle = std::max(0.1f, Settings.MaximumSlopeDegrees);
    Configuration.walkableHeight = static_cast<int>(std::ceil(Settings.AgentHeight / Settings.CellHeight));
    Configuration.walkableClimb = static_cast<int>(std::floor(Settings.MaximumStepHeight / Settings.CellHeight));
    Configuration.walkableRadius = static_cast<int>(std::ceil(Settings.AgentRadius / Settings.CellSize));
    Configuration.maxEdgeLen = static_cast<int>(12.f / Settings.CellSize);
    Configuration.maxSimplificationError = Settings.MaximumSimplificationError;
    Configuration.minRegionArea = Settings.MinimumRegionArea * Settings.MinimumRegionArea;
    Configuration.mergeRegionArea = Settings.MergeRegionArea * Settings.MergeRegionArea;
    Configuration.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
    Configuration.tileSize = Settings.TileSize;
    Configuration.borderSize = Configuration.walkableRadius + 3;
    Configuration.width = Configuration.tileSize + 2 * Configuration.borderSize;
    Configuration.height = Configuration.width;
    Configuration.detailSampleDist = Settings.CellSize * 6.f;
    Configuration.detailSampleMaxError = Settings.CellHeight;
    const float TileLength = Settings.CellSize * Settings.TileSize;
    Configuration.bmin[0] = OverallBounds.Minimum.x + TileHorizontal * TileLength - Configuration.borderSize * Settings.CellSize;
    Configuration.bmin[1] = OverallBounds.Minimum.y;
    Configuration.bmin[2] = OverallBounds.Minimum.z + TileDepth * TileLength - Configuration.borderSize * Settings.CellSize;
    Configuration.bmax[0] = OverallBounds.Minimum.x + (TileHorizontal + 1) * TileLength + Configuration.borderSize * Settings.CellSize;
    Configuration.bmax[1] = OverallBounds.Maximum.y;
    Configuration.bmax[2] = OverallBounds.Minimum.z + (TileDepth + 1) * TileLength + Configuration.borderSize * Settings.CellSize;

    float Minimum[2] = {Configuration.bmin[0], Configuration.bmin[2]};
    float Maximum[2] = {Configuration.bmax[0], Configuration.bmax[2]};
    std::vector<int> Chunks(GeometryIndex.nnodes);
    const int ChunkCount = rcGetChunksOverlappingRect(&GeometryIndex, Minimum, Maximum, Chunks.data(), static_cast<int>(Chunks.size()));
    if (ChunkCount == 0)
    {
        return true;
    }
    std::vector<int> Indices;
    for (int ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
    {
        const rcChunkyTriMeshNode& Node = GeometryIndex.nodes[Chunks[ChunkIndex]];
        Indices.insert(Indices.end(), GeometryIndex.tris + Node.i * 3, GeometryIndex.tris + (Node.i + Node.n) * 3);
    }

    rcContext Context;
    std::unique_ptr<rcHeightfield, decltype(&rcFreeHeightField)> Solid(rcAllocHeightfield(), rcFreeHeightField);
    std::unique_ptr<rcCompactHeightfield, decltype(&rcFreeCompactHeightfield)> Compact(rcAllocCompactHeightfield(), rcFreeCompactHeightfield);
    std::unique_ptr<rcContourSet, decltype(&rcFreeContourSet)> Contours(rcAllocContourSet(), rcFreeContourSet);
    std::unique_ptr<rcPolyMesh, decltype(&rcFreePolyMesh)> Polygons(rcAllocPolyMesh(), rcFreePolyMesh);
    std::unique_ptr<rcPolyMeshDetail, decltype(&rcFreePolyMeshDetail)> Detail(rcAllocPolyMeshDetail(), rcFreePolyMeshDetail);
    if (!Solid || !Compact || !Contours || !Polygons || !Detail
        || !rcCreateHeightfield(&Context, *Solid, Configuration.width, Configuration.height,
            Configuration.bmin, Configuration.bmax, Configuration.cs, Configuration.ch))
    {
        Error = "Navigation heightfield allocation failed";
        return false;
    }
    const int TriangleCount = static_cast<int>(Indices.size() / 3);
    std::vector<unsigned char> Areas(TriangleCount, RC_NULL_AREA);
    rcMarkWalkableTriangles(&Context, Configuration.walkableSlopeAngle, Vertices.data(),
        static_cast<int>(Vertices.size() / 3), Indices.data(), TriangleCount, Areas.data());
    if (!rcRasterizeTriangles(&Context, Vertices.data(), static_cast<int>(Vertices.size() / 3),
        Indices.data(), Areas.data(), TriangleCount, *Solid, Configuration.walkableClimb))
    {
        Error = "Navigation rasterization failed";
        return false;
    }
    rcFilterLowHangingWalkableObstacles(&Context, Configuration.walkableClimb, *Solid);
    rcFilterLedgeSpans(&Context, Configuration.walkableHeight, Configuration.walkableClimb, *Solid);
    rcFilterWalkableLowHeightSpans(&Context, Configuration.walkableHeight, *Solid);
    if (!rcBuildCompactHeightfield(&Context, Configuration.walkableHeight, Configuration.walkableClimb, *Solid, *Compact))
    {
        Error = "Navigation compact heightfield failed";
        return false;
    }
    Solid.reset();
    ApplyNavigationVolumes(*Compact, Input);
    ApplyNavigationModifiers(*Compact, Input);
    if (!rcErodeWalkableArea(&Context, Configuration.walkableRadius, *Compact)
        || !rcBuildDistanceField(&Context, *Compact)
        || !rcBuildRegions(&Context, *Compact, Configuration.borderSize, Configuration.minRegionArea, Configuration.mergeRegionArea)
        || !rcBuildContours(&Context, *Compact, Configuration.maxSimplificationError, Configuration.maxEdgeLen, *Contours)
        || !rcBuildPolyMesh(&Context, *Contours, Configuration.maxVertsPerPoly, *Polygons))
    {
        Error = "Navigation polygon generation failed";
        return false;
    }
    if (Polygons->npolys == 0)
    {
        return true;
    }
    if (Polygons->npolys >= 16384 || !rcBuildPolyMeshDetail(&Context, *Polygons, *Compact,
        Configuration.detailSampleDist, Configuration.detailSampleMaxError, *Detail))
    {
        Error = "Navigation detail mesh failed or tile polygon limit exceeded";
        return false;
    }
    for (int PolygonIndex = 0; PolygonIndex < Polygons->npolys; ++PolygonIndex)
    {
        if (Polygons->areas[PolygonIndex] == RC_WALKABLE_AREA)
        {
            Polygons->areas[PolygonIndex] = 1;
        }
        Polygons->flags[PolygonIndex] = static_cast<unsigned short>(1u << (Polygons->areas[PolygonIndex] - 1));
    }
    std::vector<float> LinkVertices;
    std::vector<float> LinkRadii;
    std::vector<unsigned char> LinkDirections;
    std::vector<unsigned char> LinkAreas;
    std::vector<unsigned short> LinkFlags;
    std::vector<unsigned int> LinkIdentities;
    for (const NavigationLink& Link : Input.Links)
    {
        LinkVertices.insert(LinkVertices.end(), {Link.Start.x, Link.Start.y, Link.Start.z, Link.End.x, Link.End.y, Link.End.z});
        LinkRadii.push_back(Link.Radius);
        LinkDirections.push_back(static_cast<unsigned char>(Link.bBidirectional));
        LinkAreas.push_back(Link.Area);
        LinkFlags.push_back(static_cast<unsigned short>(1u << (Link.Area - 1)));
        LinkIdentities.push_back(Link.Identity);
    }
    dtNavMeshCreateParams Parameters{};
    Parameters.verts = Polygons->verts;
    Parameters.vertCount = Polygons->nverts;
    Parameters.polys = Polygons->polys;
    Parameters.polyAreas = Polygons->areas;
    Parameters.polyFlags = Polygons->flags;
    Parameters.polyCount = Polygons->npolys;
    Parameters.nvp = Polygons->nvp;
    Parameters.detailMeshes = Detail->meshes;
    Parameters.detailVerts = Detail->verts;
    Parameters.detailVertsCount = Detail->nverts;
    Parameters.detailTris = Detail->tris;
    Parameters.detailTriCount = Detail->ntris;
    Parameters.offMeshConVerts = LinkVertices.data();
    Parameters.offMeshConRad = LinkRadii.data();
    Parameters.offMeshConDir = LinkDirections.data();
    Parameters.offMeshConAreas = LinkAreas.data();
    Parameters.offMeshConFlags = LinkFlags.data();
    Parameters.offMeshConUserID = LinkIdentities.data();
    Parameters.offMeshConCount = static_cast<int>(Input.Links.size());
    Parameters.walkableHeight = Settings.AgentHeight;
    Parameters.walkableRadius = Settings.AgentRadius;
    Parameters.walkableClimb = Settings.MaximumStepHeight;
    Parameters.tileX = TileHorizontal;
    Parameters.tileY = TileDepth;
    std::memcpy(Parameters.bmin, Polygons->bmin, sizeof(Parameters.bmin));
    std::memcpy(Parameters.bmax, Polygons->bmax, sizeof(Parameters.bmax));
    Parameters.cs = Settings.CellSize;
    Parameters.ch = Settings.CellHeight;
    Parameters.buildBvTree = true;
    unsigned char* TileData = nullptr;
    int DataSize = 0;
    if (!dtCreateNavMeshData(&Parameters, &TileData, &DataSize))
    {
        Error = "Detour tile creation failed";
        return false;
    }
    if (reinterpret_cast<const dtMeshHeader*>(TileData)->polyCount >= 16384)
    {
        dtFree(TileData);
        Error = "Navigation tile exceeds its polygon budget after adding links";
        return false;
    }
    if (dtStatusFailed(Destination.addTile(TileData, DataSize, DT_TILE_FREE_DATA, 0, nullptr)))
    {
        dtFree(TileData);
        Error = "Detour tile insertion failed";
        return false;
    }
    ++Statistics.TileCount;
    Statistics.PolygonCount += Polygons->npolys;
    Statistics.DataBytes += DataSize;
    return true;
}

static bool CopyNavigationTile(const dtMeshTile& Source, dtNavMesh& Destination,
    NavigationStatistics& Statistics, std::string& Error)
{
    unsigned char* Data = static_cast<unsigned char*>(dtAlloc(Source.dataSize, DT_ALLOC_PERM));
    if (Data == nullptr)
    {
        Error = "Navigation tile copy allocation failed";
        return false;
    }
    std::memcpy(Data, Source.data, Source.dataSize);
    if (dtStatusFailed(Destination.addTile(Data, Source.dataSize, DT_TILE_FREE_DATA, 0, nullptr)))
    {
        dtFree(Data);
        Error = "Navigation tile copy insertion failed";
        return false;
    }
    ++Statistics.TileCount;
    ++Statistics.ReusedTileCount;
    Statistics.PolygonCount += Source.header->offMeshBase;
    Statistics.DataBytes += Source.dataSize;
    return true;
}

static bool IsNavigationTileDirty(const NavigationBuildInput& Input, const AxisAlignedBounds& Tile)
{
    const float Padding = (std::ceil(Input.Settings.AgentRadius / Input.Settings.CellSize) + 4.f
        + Input.Settings.MaximumSimplificationError) * Input.Settings.CellSize;
    for (const AxisAlignedBounds& Bounds : Input.DirtyBounds)
    {
        if (Bounds.Minimum.x <= Tile.Maximum.x + Padding && Bounds.Maximum.x >= Tile.Minimum.x - Padding
            && Bounds.Minimum.z <= Tile.Maximum.z + Padding && Bounds.Maximum.z >= Tile.Minimum.z - Padding)
        {
            return true;
        }
    }
    return false;
}

NavigationMesh::NavigationMesh() : State(std::make_unique<Implementation>()) {}
NavigationMesh::~NavigationMesh() = default;

std::shared_ptr<NavigationMesh> NavigationMesh::Build(const NavigationBuildInput& Input, std::string& Error)
{
    Error.clear();
    const auto Started = std::chrono::steady_clock::now();
    if (Input.Cancellation && Input.Cancellation->load())
    {
        Error = "Navigation build cancelled";
        return {};
    }
    if (!ValidateNavigationSettings(Input.Settings, Error))
    {
        return {};
    }
    if (Input.Bounds.empty() || Input.Geometry.Vertices.empty() || Input.Geometry.Indices.empty()
        || Input.Geometry.Indices.size() % 3 != 0 || Input.Geometry.Indices.size() > 6000000
        || Input.Geometry.Vertices.size() > 2000000)
    {
        Error = "Navigation requires bounds and valid collision geometry within the build budget";
        return {};
    }
    AxisAlignedBounds OverallBounds = Input.Bounds.front();
    for (const AxisAlignedBounds& Bounds : Input.Bounds)
    {
        if (!IsFiniteNavigationVector(Bounds.Minimum) || !IsFiniteNavigationVector(Bounds.Maximum)
            || Bounds.Maximum.x <= Bounds.Minimum.x || Bounds.Maximum.y <= Bounds.Minimum.y
            || Bounds.Maximum.z <= Bounds.Minimum.z)
        {
            Error = "Navigation bounds must be finite with positive dimensions";
            return {};
        }
        OverallBounds.Minimum = Vector3::Min(OverallBounds.Minimum, Bounds.Minimum);
        OverallBounds.Maximum = Vector3::Max(OverallBounds.Maximum, Bounds.Maximum);
    }
    if ((OverallBounds.Maximum.y - OverallBounds.Minimum.y) / Input.Settings.CellHeight > 65500.f)
    {
        Error = "Navigation vertical range exceeds the voxel height limit";
        return {};
    }
    const float TileLength = Input.Settings.CellSize * Input.Settings.TileSize;
    const double HorizontalCount = std::ceil((OverallBounds.Maximum.x - OverallBounds.Minimum.x) / TileLength);
    const double DepthCount = std::ceil((OverallBounds.Maximum.z - OverallBounds.Minimum.z) / TileLength);
    if (HorizontalCount * DepthCount > 4096.0)
    {
        Error = "Navigation bounds exceed the 4096 tile budget";
        return {};
    }
    for (const NavigationModifier& Modifier : Input.Modifiers)
    {
        bool bFiniteMatrix = true;
        for (int RowIndex = 0; RowIndex < 4; ++RowIndex)
        {
            for (int ColumnIndex = 0; ColumnIndex < 4; ++ColumnIndex)
            {
                bFiniteMatrix = bFiniteMatrix && std::isfinite(Modifier.WorldToLocal.m[RowIndex][ColumnIndex]);
            }
        }
        if (Modifier.Area > 16 || !IsFiniteNavigationVector(Modifier.HalfExtents)
            || Modifier.HalfExtents.x <= 0.f || Modifier.HalfExtents.y <= 0.f || Modifier.HalfExtents.z <= 0.f
            || !bFiniteMatrix || std::abs(Modifier.WorldToLocal.Determinant()) < 0.00000001f)
        {
            Error = "Navigation modifier area or dimensions are invalid";
            return {};
        }
    }
    for (const NavigationLink& Link : Input.Links)
    {
        if (Link.Area < 1 || Link.Area > 16 || !IsFiniteNavigationVector(Link.Start) || !IsFiniteNavigationVector(Link.End)
            || !std::isfinite(Link.Radius) || Link.Radius <= 0.f)
        {
            Error = "Navigation link endpoints, radius or area are invalid";
            return {};
        }
    }
    std::vector<float> Vertices;
    Vertices.reserve(Input.Geometry.Vertices.size() * 3);
    for (const Vector3& Vertex : Input.Geometry.Vertices)
    {
        if (!IsFiniteNavigationVector(Vertex))
        {
            Error = "Navigation geometry contains non-finite vertices";
            return {};
        }
        Vertices.insert(Vertices.end(), {Vertex.x, Vertex.y, Vertex.z});
    }
    std::vector<int> Indices;
    Indices.reserve(Input.Geometry.Indices.size());
    for (uint32_t Index : Input.Geometry.Indices)
    {
        if (Index >= Input.Geometry.Vertices.size())
        {
            Error = "Navigation geometry index is out of range";
            return {};
        }
        Indices.push_back(static_cast<int>(Index));
    }
    rcChunkyTriMesh GeometryIndex;
    if (!rcCreateChunkyTriMesh(Vertices.data(), Indices.data(), static_cast<int>(Indices.size() / 3), 256, &GeometryIndex))
    {
        Error = "Navigation geometry spatial indexing failed";
        return {};
    }
    auto Result = std::make_shared<NavigationMesh>();
    Result->State->Mesh.reset(dtAllocNavMesh());
    Result->State->Settings = Input.Settings;
    Result->State->Fingerprint = Input.Fingerprint;
    dtNavMeshParams Parameters{};
    Parameters.orig[0] = OverallBounds.Minimum.x;
    Parameters.orig[1] = OverallBounds.Minimum.y;
    Parameters.orig[2] = OverallBounds.Minimum.z;
    Parameters.tileWidth = TileLength;
    Parameters.tileHeight = TileLength;
    Parameters.maxTiles = 1;
    while (Parameters.maxTiles < HorizontalCount * DepthCount)
    {
        Parameters.maxTiles *= 2;
    }
    Parameters.maxPolys = 16384;
    if (!Result->State->Mesh || dtStatusFailed(Result->State->Mesh->init(&Parameters)))
    {
        Error = "Detour navigation initialization failed";
        return {};
    }
    const dtNavMesh* Previous = nullptr;
    if (Input.PreviousMesh && Input.PreviousMesh->State->Mesh)
    {
        const dtNavMeshParams& Before = *Input.PreviousMesh->State->Mesh->getParams();
        if (std::memcmp(Before.orig, Parameters.orig, sizeof(Parameters.orig)) == 0
            && Before.tileWidth == Parameters.tileWidth && Before.tileHeight == Parameters.tileHeight
            && Before.maxTiles == Parameters.maxTiles && Before.maxPolys == Parameters.maxPolys)
        {
            Previous = Input.PreviousMesh->State->Mesh.get();
        }
    }
    for (int TileDepth = 0; TileDepth < static_cast<int>(DepthCount); ++TileDepth)
    {
        for (int TileHorizontal = 0; TileHorizontal < static_cast<int>(HorizontalCount); ++TileHorizontal)
        {
            if (Input.Cancellation && Input.Cancellation->load())
            {
                Error = "Navigation build cancelled";
                return {};
            }
            AxisAlignedBounds TileBounds;
            TileBounds.Minimum = Vector3(OverallBounds.Minimum.x + TileHorizontal * TileLength,
                OverallBounds.Minimum.y, OverallBounds.Minimum.z + TileDepth * TileLength);
            TileBounds.Maximum = TileBounds.Minimum + Vector3(TileLength, OverallBounds.Maximum.y - OverallBounds.Minimum.y, TileLength);
            if (Previous != nullptr && !IsNavigationTileDirty(Input, TileBounds))
            {
                const dtMeshTile* ExistingTile = Previous->getTileAt(TileHorizontal, TileDepth, 0);
                if (ExistingTile != nullptr && ExistingTile->header != nullptr
                    && !CopyNavigationTile(*ExistingTile, *Result->State->Mesh, Result->State->Statistics, Error))
                {
                    return {};
                }
                continue;
            }
            ++Result->State->Statistics.RebuiltTileCount;
            if (!BuildNavigationTile(Input, Vertices, GeometryIndex, OverallBounds, TileHorizontal, TileDepth,
                *Result->State->Mesh, Result->State->Statistics, Error))
            {
                return {};
            }
        }
    }
    if (Result->State->Statistics.PolygonCount == 0)
    {
        Error = "No walkable navigation surface was generated";
        return {};
    }
    Result->State->Statistics.BuildMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - Started).count();
    return Result;
}

NavigationPath NavigationMesh::FindPath(const Vector3& Start, const Vector3& Goal,
    const NavigationQueryFilter& SourceFilter, const Vector3& Extent) const
{
    NavigationPath Result;
    Result.RequestedGoal = Goal;
    dtQueryFilter Filter;
    if (!IsFiniteNavigationVector(Start) || !IsFiniteNavigationVector(Goal)
        || !IsValidNavigationExtent(Extent, State->Settings) || !MakeDetourFilter(SourceFilter, Filter))
    {
        Result.Status = NavigationPathStatus::InvalidInput;
        return Result;
    }
    dtNavMeshQuery Query;
    if (!State->Mesh || dtStatusFailed(Query.init(State->Mesh.get(), SourceFilter.MaximumSearchNodes)))
    {
        return Result;
    }
    dtPolyRef StartPolygon = 0;
    dtPolyRef GoalPolygon = 0;
    float ProjectedStart[3]{};
    float ProjectedGoal[3]{};
    Query.findNearestPoly(&Start.x, &Extent.x, &Filter, &StartPolygon, ProjectedStart);
    Query.findNearestPoly(&Goal.x, &Extent.x, &Filter, &GoalPolygon, ProjectedGoal);
    if (StartPolygon == 0)
    {
        Result.Status = NavigationPathStatus::StartOutsideNavigation;
        return Result;
    }
    if (GoalPolygon == 0)
    {
        Result.Status = NavigationPathStatus::GoalOutsideNavigation;
        return Result;
    }
    Result.ProjectedGoal = Vector3(ProjectedGoal);
    std::vector<dtPolyRef> Corridor(SourceFilter.MaximumPathPolygons);
    int PolygonCount = 0;
    const dtStatus Status = Query.findPath(StartPolygon, GoalPolygon, ProjectedStart, ProjectedGoal,
        &Filter, Corridor.data(), &PolygonCount, static_cast<int>(Corridor.size()));
    if (dtStatusFailed(Status) || PolygonCount == 0)
    {
        Result.Status = NavigationPathStatus::Unreachable;
        return Result;
    }
    if ((Status & (DT_OUT_OF_NODES | DT_BUFFER_TOO_SMALL)) != 0)
    {
        Result.Status = NavigationPathStatus::SearchLimitReached;
        Result.Diagnostic = "Navigation search node or path buffer budget exhausted";
        return Result;
    }
    const bool bPartial = Corridor[PolygonCount - 1] != GoalPolygon || (Status & DT_PARTIAL_RESULT) != 0;
    if (bPartial)
    {
        if (!SourceFilter.bAllowPartialPath)
        {
            Result.Status = NavigationPathStatus::Unreachable;
            return Result;
        }
        Query.closestPointOnPoly(Corridor[PolygonCount - 1], ProjectedGoal, ProjectedGoal, nullptr);
        Result.ProjectedGoal = Vector3(ProjectedGoal);
    }
    std::vector<float> Points(SourceFilter.MaximumPathPolygons * 3);
    std::vector<unsigned char> Flags(SourceFilter.MaximumPathPolygons);
    std::vector<dtPolyRef> Polygons(SourceFilter.MaximumPathPolygons);
    int PointCount = 0;
    const dtStatus StraightStatus = Query.findStraightPath(ProjectedStart, ProjectedGoal, Corridor.data(), PolygonCount,
        Points.data(), Flags.data(), Polygons.data(), &PointCount, SourceFilter.MaximumPathPolygons);
    if (dtStatusFailed(StraightStatus) || (StraightStatus & (DT_BUFFER_TOO_SMALL | DT_PARTIAL_RESULT)) != 0)
    {
        Result.Status = NavigationPathStatus::SearchLimitReached;
        return Result;
    }
    for (int PointIndex = 0; PointIndex < PointCount; ++PointIndex)
    {
        NavigationPathPoint Point;
        Point.Position = Vector3(Points.data() + PointIndex * 3);
        Point.bOffMeshConnection = (Flags[PointIndex] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) != 0;
        if (Point.bOffMeshConnection)
        {
            const dtOffMeshConnection* Connection = State->Mesh->getOffMeshConnectionByRef(Polygons[PointIndex]);
            if (Connection != nullptr)
            {
                Point.LinkIdentity = Connection->userId;
                Point.LinkEnd = Vector3(Connection->pos + 3);
                for (int CorridorIndex = 1; CorridorIndex < PolygonCount; ++CorridorIndex)
                {
                    if (Corridor[CorridorIndex] == Polygons[PointIndex])
                    {
                        float LinkStart[3]{};
                        float LinkEnd[3]{};
                        if (dtStatusSucceed(State->Mesh->getOffMeshConnectionPolyEndPoints(
                            Corridor[CorridorIndex - 1], Corridor[CorridorIndex], LinkStart, LinkEnd)))
                        {
                            Point.LinkEnd = Vector3(LinkEnd);
                        }
                        break;
                    }
                }
            }
        }
        Result.Points.push_back(Point);
    }
    Result.Status = NavigationPathStatus::Complete;
    if (bPartial)
    {
        Result.Status = NavigationPathStatus::Partial;
    }
    return Result;
}

bool NavigationMesh::ProjectPoint(const Vector3& Position, Vector3& Projected,
    const Vector3& Extent, const NavigationQueryFilter& SourceFilter) const
{
    dtQueryFilter Filter;
    if (!State->Mesh || !IsFiniteNavigationVector(Position)
        || !IsValidNavigationExtent(Extent, State->Settings) || !MakeDetourFilter(SourceFilter, Filter))
    {
        return false;
    }
    dtNavMeshQuery Query;
    if (dtStatusFailed(Query.init(State->Mesh.get(), SourceFilter.MaximumSearchNodes)))
    {
        return false;
    }
    dtPolyRef Polygon = 0;
    const dtStatus Status = Query.findNearestPoly(&Position.x, &Extent.x, &Filter, &Polygon, &Projected.x);
    return dtStatusSucceed(Status) && Polygon != 0;
}

bool NavigationMesh::Raycast(const Vector3& Start, const Vector3& End, Vector3& Hit, const NavigationQueryFilter& SourceFilter) const
{
    dtQueryFilter Filter;
    if (!State->Mesh || !IsFiniteNavigationVector(Start) || !IsFiniteNavigationVector(End)
        || !MakeDetourFilter(SourceFilter, Filter))
    {
        return false;
    }
    dtNavMeshQuery Query;
    if (dtStatusFailed(Query.init(State->Mesh.get(), SourceFilter.MaximumSearchNodes)))
    {
        return false;
    }
    const float Extent[3] = {1.f, 2.f, 1.f};
    float ProjectedStart[3]{};
    dtPolyRef Polygon = 0;
    Query.findNearestPoly(&Start.x, Extent, &Filter, &Polygon, ProjectedStart);
    if (Polygon == 0)
    {
        return false;
    }
    float Distance = FLT_MAX;
    float Normal[3]{};
    const dtStatus Status = Query.raycast(Polygon, ProjectedStart, &End.x, &Filter, &Distance, Normal, nullptr, nullptr, 0);
    Hit = End;
    if (dtStatusSucceed(Status) && Distance <= 1.f)
    {
        Hit = Vector3::Lerp(Vector3(ProjectedStart), End, Distance);
        return true;
    }
    return false;
}

static float NextNavigationRandom(uint32_t& State)
{
    State = State * 1664525u + 1013904223u;
    return static_cast<float>(State >> 8) / 16777216.f;
}

bool NavigationMesh::MoveAlongSurface(const Vector3& Start, const Vector3& Destination, Vector3& Result,
    const NavigationQueryFilter& SourceFilter) const
{
    dtQueryFilter Filter;
    if (!State->Mesh || !IsFiniteNavigationVector(Start) || !IsFiniteNavigationVector(Destination)
        || !MakeDetourFilter(SourceFilter, Filter))
    {
        return false;
    }
    dtNavMeshQuery Query;
    if (dtStatusFailed(Query.init(State->Mesh.get(), SourceFilter.MaximumSearchNodes)))
    {
        return false;
    }
    const float Extent[3] = {1.f, 2.f, 1.f};
    float ProjectedStart[3]{};
    dtPolyRef StartPolygon = 0;
    Query.findNearestPoly(&Start.x, Extent, &Filter, &StartPolygon, ProjectedStart);
    if (StartPolygon == 0)
    {
        return false;
    }
    dtPolyRef Visited[64]{};
    int VisitedCount = 0;
    const dtStatus Status = Query.moveAlongSurface(StartPolygon, ProjectedStart, &Destination.x, &Filter,
        &Result.x, Visited, &VisitedCount, 64);
    if (dtStatusFailed(Status) || VisitedCount == 0 || (Status & DT_BUFFER_TOO_SMALL) != 0)
    {
        return false;
    }
    Query.getPolyHeight(Visited[VisitedCount - 1], &Result.x, &Result.y);
    return true;
}

bool NavigationMesh::FindRandomPoint(Vector3& Point, uint32_t& RandomState, const NavigationQueryFilter& Filter) const
{
    dtQueryFilter Checked;
    if (!MakeDetourFilter(Filter, Checked))
    {
        return false;
    }
    NavigationDebugTriangle Selected;
    double TotalArea = 0.0;
    for (const NavigationDebugTriangle& Triangle : GetDebugTriangles())
    {
        const uint16_t AreaFlag = static_cast<uint16_t>(1u << (Triangle.Area - 1));
        if ((Filter.IncludedAreas & AreaFlag) == 0 || (Filter.ExcludedAreas & AreaFlag) != 0)
        {
            continue;
        }
        const double Area = (Triangle.Second - Triangle.First).Cross(Triangle.Third - Triangle.First).Length() * 0.5;
        TotalArea += Area;
        if (NextNavigationRandom(RandomState) * TotalArea < Area)
        {
            Selected = Triangle;
        }
    }
    if (TotalArea <= 0.0)
    {
        return false;
    }
    const float FirstWeight = std::sqrt(NextNavigationRandom(RandomState));
    const float SecondWeight = NextNavigationRandom(RandomState);
    Point = Selected.First * (1.f - FirstWeight) + Selected.Second * (FirstWeight * (1.f - SecondWeight))
        + Selected.Third * (FirstWeight * SecondWeight);
    return true;
}

std::vector<NavigationDebugTriangle> NavigationMesh::GetDebugTriangles() const
{
    std::vector<NavigationDebugTriangle> Result;
    if (!State->Mesh)
    {
        return Result;
    }
    const dtNavMesh& Mesh = *State->Mesh;
    for (int TileIndex = 0; TileIndex < Mesh.getMaxTiles(); ++TileIndex)
    {
        const dtMeshTile* Tile = Mesh.getTile(TileIndex);
        if (Tile->header == nullptr)
        {
            continue;
        }
        for (int PolygonIndex = 0; PolygonIndex < Tile->header->polyCount; ++PolygonIndex)
        {
            const dtPoly& Polygon = Tile->polys[PolygonIndex];
            if (Polygon.getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
            {
                continue;
            }
            const dtPolyDetail& Detail = Tile->detailMeshes[PolygonIndex];
            for (int TriangleIndex = 0; TriangleIndex < Detail.triCount; ++TriangleIndex)
            {
                const unsigned char* Indices = Tile->detailTris + (Detail.triBase + TriangleIndex) * 4;
                Vector3 Positions[3];
                for (int Corner = 0; Corner < 3; ++Corner)
                {
                    if (Indices[Corner] < Polygon.vertCount)
                    {
                        Positions[Corner] = Vector3(Tile->verts + Polygon.verts[Indices[Corner]] * 3);
                    }
                    else
                    {
                        Positions[Corner] = Vector3(Tile->detailVerts + (Detail.vertBase + Indices[Corner] - Polygon.vertCount) * 3);
                    }
                }
                Result.push_back({Positions[0], Positions[1], Positions[2], Polygon.getArea()});
            }
        }
    }
    return Result;
}

const NavigationBuildSettings& NavigationMesh::GetSettings() const { return State->Settings; }
const NavigationStatistics& NavigationMesh::GetStatistics() const { return State->Statistics; }
uint64_t NavigationMesh::GetFingerprint() const { return State->Fingerprint; }

bool NavigationMesh::HasLink(uint32_t Identity) const
{
    if (!State->Mesh)
    {
        return false;
    }
    const dtNavMesh& Mesh = *State->Mesh;
    for (int TileIndex = 0; TileIndex < Mesh.getMaxTiles(); ++TileIndex)
    {
        const dtMeshTile* Tile = Mesh.getTile(TileIndex);
        if (Tile->header == nullptr)
        {
            continue;
        }
        for (int ConnectionIndex = 0; ConnectionIndex < Tile->header->offMeshConCount; ++ConnectionIndex)
        {
            if (Tile->offMeshCons[ConnectionIndex].userId == Identity)
            {
                return true;
            }
        }
    }
    return false;
}

void NavigationMesh::Serialize(nlohmann::json& Document) const
{
    Document = nlohmann::json::object();
    if (!State->Mesh)
    {
        return;
    }
    const NavigationBuildSettings& Settings = State->Settings;
    Document["format"] = "sakura.navigation";
    Document["version"] = 1;
    Document["detourVersion"] = DT_NAVMESH_VERSION;
    Document["polygonIdentityBytes"] = sizeof(dtPolyRef);
    Document["fingerprint"] = State->Fingerprint;
    Document["settings"] = {{"profile", Settings.AgentProfile}, {"radius", Settings.AgentRadius}, {"height", Settings.AgentHeight},
        {"step", Settings.MaximumStepHeight}, {"slope", Settings.MaximumSlopeDegrees}, {"cellSize", Settings.CellSize},
        {"cellHeight", Settings.CellHeight}, {"tileSize", Settings.TileSize}, {"minimumRegion", Settings.MinimumRegionArea},
        {"mergeRegion", Settings.MergeRegionArea}, {"simplification", Settings.MaximumSimplificationError}};
    const dtNavMeshParams& Parameters = *State->Mesh->getParams();
    Document["origin"] = {Parameters.orig[0], Parameters.orig[1], Parameters.orig[2]};
    Document["tileWidth"] = Parameters.tileWidth;
    Document["tileHeight"] = Parameters.tileHeight;
    Document["maximumTiles"] = Parameters.maxTiles;
    Document["maximumPolygons"] = Parameters.maxPolys;
    Document["tiles"] = nlohmann::json::array();
    const dtNavMesh& Mesh = *State->Mesh;
    for (int TileIndex = 0; TileIndex < Mesh.getMaxTiles(); ++TileIndex)
    {
        const dtMeshTile* Tile = Mesh.getTile(TileIndex);
        if (Tile->header != nullptr && Tile->data != nullptr && Tile->dataSize > 0)
        {
            Document["tiles"].push_back({{"data", Base64::Encode(Tile->data, Tile->dataSize)},
                {"checksum", ContentHash::FromBytes(Tile->data, Tile->dataSize).Value}});
        }
    }
}

static size_t AlignedNavigationBytes(size_t Size)
{
    return (Size + 3) & ~size_t(3);
}

static bool ValidateNavigationTileBytes(const std::vector<uint8_t>& Bytes)
{
    if (Bytes.size() < sizeof(dtMeshHeader))
    {
        return false;
    }
    dtMeshHeader Header;
    std::memcpy(&Header, Bytes.data(), sizeof(Header));
    if (Header.magic != DT_NAVMESH_MAGIC || Header.version != DT_NAVMESH_VERSION
        || Header.polyCount < 1 || Header.polyCount >= 16384 || Header.vertCount < 3 || Header.vertCount > 65535
        || Header.maxLinkCount < 1 || Header.maxLinkCount > 1000000 || Header.detailMeshCount < 1
        || Header.detailMeshCount > Header.polyCount || Header.detailVertCount < 0 || Header.detailVertCount > 1000000
        || Header.detailTriCount < 0 || Header.detailTriCount > 1000000 || Header.bvNodeCount < 0
        || Header.bvNodeCount > Header.polyCount * 2 || Header.offMeshConCount < 0
        || Header.offMeshConCount > Header.polyCount || Header.offMeshBase != Header.detailMeshCount
        || Header.offMeshBase + Header.offMeshConCount != Header.polyCount)
    {
        return false;
    }
    const size_t Required = AlignedNavigationBytes(sizeof(dtMeshHeader))
        + AlignedNavigationBytes(sizeof(float) * 3 * Header.vertCount)
        + AlignedNavigationBytes(sizeof(dtPoly) * Header.polyCount)
        + AlignedNavigationBytes(sizeof(dtLink) * Header.maxLinkCount)
        + AlignedNavigationBytes(sizeof(dtPolyDetail) * Header.detailMeshCount)
        + AlignedNavigationBytes(sizeof(float) * 3 * Header.detailVertCount)
        + AlignedNavigationBytes(sizeof(unsigned char) * 4 * Header.detailTriCount)
        + AlignedNavigationBytes(sizeof(dtBVNode) * Header.bvNodeCount)
        + AlignedNavigationBytes(sizeof(dtOffMeshConnection) * Header.offMeshConCount);
    if (Required != static_cast<size_t>(Bytes.size()) || Header.x < 0 || Header.x >= 4096
        || Header.y < 0 || Header.y >= 4096 || Header.layer != 0
        || !IsFiniteNavigationVector(Vector3(Header.bmin)) || !IsFiniteNavigationVector(Vector3(Header.bmax))
        || Header.bmax[0] <= Header.bmin[0] || Header.bmax[1] <= Header.bmin[1] || Header.bmax[2] <= Header.bmin[2]
        || !std::isfinite(Header.bvQuantFactor) || Header.bvQuantFactor <= 0.f
        || !std::isfinite(Header.walkableRadius) || Header.walkableRadius <= 0.f
        || !std::isfinite(Header.walkableHeight) || Header.walkableHeight <= 0.f
        || !std::isfinite(Header.walkableClimb) || Header.walkableClimb < 0.f)
    {
        return false;
    }
    const char* Data = reinterpret_cast<const char*>(Bytes.data()) + AlignedNavigationBytes(sizeof(dtMeshHeader));
    const char* VertexData = Data;
    Data += AlignedNavigationBytes(sizeof(float) * 3 * Header.vertCount);
    const char* PolygonData = Data;
    Data += AlignedNavigationBytes(sizeof(dtPoly) * Header.polyCount);
    Data += AlignedNavigationBytes(sizeof(dtLink) * Header.maxLinkCount);
    const char* DetailData = Data;
    Data += AlignedNavigationBytes(sizeof(dtPolyDetail) * Header.detailMeshCount);
    const char* DetailVertexData = Data;
    Data += AlignedNavigationBytes(sizeof(float) * 3 * Header.detailVertCount);
    const char* DetailTriangleData = Data;
    Data += AlignedNavigationBytes(sizeof(unsigned char) * 4 * Header.detailTriCount);
    const char* BoundsData = Data;
    Data += AlignedNavigationBytes(sizeof(dtBVNode) * Header.bvNodeCount);
    const char* ConnectionData = Data;
    for (int VertexIndex = 0; VertexIndex < Header.vertCount; ++VertexIndex)
    {
        float Coordinates[3];
        std::memcpy(Coordinates, VertexData + sizeof(float) * 3 * VertexIndex, sizeof(Coordinates));
        if (!IsFiniteNavigationVector(Vector3(Coordinates)))
        {
            return false;
        }
    }
    for (int VertexIndex = 0; VertexIndex < Header.detailVertCount; ++VertexIndex)
    {
        float Coordinates[3];
        std::memcpy(Coordinates, DetailVertexData + sizeof(float) * 3 * VertexIndex, sizeof(Coordinates));
        if (!IsFiniteNavigationVector(Vector3(Coordinates)))
        {
            return false;
        }
    }
    for (int PolygonIndex = 0; PolygonIndex < Header.polyCount; ++PolygonIndex)
    {
        dtPoly Polygon;
        std::memcpy(&Polygon, PolygonData + sizeof(dtPoly) * PolygonIndex, sizeof(Polygon));
        if (Polygon.getArea() < 1 || Polygon.getArea() > 16 || Polygon.vertCount > DT_VERTS_PER_POLYGON)
        {
            return false;
        }
        if (PolygonIndex < Header.offMeshBase)
        {
            if (Polygon.getType() != DT_POLYTYPE_GROUND || Polygon.vertCount < 3)
            {
                return false;
            }
        }
        else if (Polygon.getType() != DT_POLYTYPE_OFFMESH_CONNECTION || Polygon.vertCount != 2)
        {
            return false;
        }
        for (unsigned char VertexIndex = 0; VertexIndex < Polygon.vertCount; ++VertexIndex)
        {
            if (Polygon.verts[VertexIndex] >= Header.vertCount)
            {
                return false;
            }
            const unsigned short Neighbor = Polygon.neis[VertexIndex];
            if (Neighbor != 0 && (Neighbor & DT_EXT_LINK) == 0 && Neighbor > Header.polyCount)
            {
                return false;
            }
            if ((Neighbor & DT_EXT_LINK) != 0 && (Neighbor & 0xff) > 7)
            {
                return false;
            }
        }
        if (PolygonIndex < Header.detailMeshCount)
        {
            dtPolyDetail Detail;
            std::memcpy(&Detail, DetailData + sizeof(dtPolyDetail) * PolygonIndex, sizeof(Detail));
            if (static_cast<uint64_t>(Detail.vertBase) + Detail.vertCount > static_cast<uint64_t>(Header.detailVertCount)
                || static_cast<uint64_t>(Detail.triBase) + Detail.triCount > static_cast<uint64_t>(Header.detailTriCount)
                || Detail.triCount == 0)
            {
                return false;
            }
            for (unsigned int TriangleIndex = 0; TriangleIndex < Detail.triCount; ++TriangleIndex)
            {
                const unsigned char* Corners = reinterpret_cast<const unsigned char*>(DetailTriangleData)
                    + (Detail.triBase + TriangleIndex) * 4;
                for (int Corner = 0; Corner < 3; ++Corner)
                {
                    if (Corners[Corner] >= Polygon.vertCount + Detail.vertCount)
                    {
                        return false;
                    }
                }
            }
        }
    }
    for (int NodeIndex = 0; NodeIndex < Header.bvNodeCount; ++NodeIndex)
    {
        dtBVNode Node;
        std::memcpy(&Node, BoundsData + sizeof(dtBVNode) * NodeIndex, sizeof(Node));
        if (Node.i >= Header.offMeshBase || (Node.i < 0 && static_cast<int64_t>(NodeIndex) - Node.i > Header.bvNodeCount))
        {
            return false;
        }
        for (int AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
        {
            if (Node.bmin[AxisIndex] > Node.bmax[AxisIndex])
            {
                return false;
            }
        }
    }
    for (int ConnectionIndex = 0; ConnectionIndex < Header.offMeshConCount; ++ConnectionIndex)
    {
        dtOffMeshConnection Connection;
        std::memcpy(&Connection, ConnectionData + sizeof(dtOffMeshConnection) * ConnectionIndex, sizeof(Connection));
        if (Connection.poly != Header.offMeshBase + ConnectionIndex || !std::isfinite(Connection.rad) || Connection.rad <= 0.f
            || !IsFiniteNavigationVector(Vector3(Connection.pos)) || !IsFiniteNavigationVector(Vector3(Connection.pos + 3))
            || (Connection.side != 0xff && Connection.side > 7))
        {
            return false;
        }
    }
    return true;
}

std::shared_ptr<NavigationMesh> NavigationMesh::Deserialize(const nlohmann::json& Document, std::string& Error)
{
    Error.clear();
    try
    {
        if (Document.at("format") != "sakura.navigation" || Document.at("version") != 1
            || Document.at("detourVersion") != DT_NAVMESH_VERSION || Document.at("polygonIdentityBytes") != sizeof(dtPolyRef))
        {
            Error = "Unsupported navigation data format";
            return {};
        }
        auto Result = std::make_shared<NavigationMesh>();
        NavigationBuildSettings& Settings = Result->State->Settings;
        const auto& Saved = Document.at("settings");
        Settings.AgentProfile = Saved.at("profile").get<std::string>();
        Settings.AgentRadius = Saved.at("radius").get<float>();
        Settings.AgentHeight = Saved.at("height").get<float>();
        Settings.MaximumStepHeight = Saved.at("step").get<float>();
        Settings.MaximumSlopeDegrees = Saved.at("slope").get<float>();
        Settings.CellSize = Saved.at("cellSize").get<float>();
        Settings.CellHeight = Saved.at("cellHeight").get<float>();
        Settings.TileSize = Saved.at("tileSize").get<int>();
        Settings.MinimumRegionArea = Saved.at("minimumRegion").get<int>();
        Settings.MergeRegionArea = Saved.at("mergeRegion").get<int>();
        Settings.MaximumSimplificationError = Saved.at("simplification").get<float>();
        if (!ValidateNavigationSettings(Settings, Error))
        {
            return {};
        }
        dtNavMeshParams Parameters{};
        const auto& Origin = Document.at("origin");
        for (size_t AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
        {
            Parameters.orig[AxisIndex] = Origin.at(AxisIndex).get<float>();
            if (!std::isfinite(Parameters.orig[AxisIndex]))
            {
                Error = "Navigation origin is not finite";
                return {};
            }
        }
        Parameters.tileWidth = Document.at("tileWidth").get<float>();
        Parameters.tileHeight = Document.at("tileHeight").get<float>();
        Parameters.maxTiles = Document.at("maximumTiles").get<int>();
        Parameters.maxPolys = Document.at("maximumPolygons").get<int>();
        if (!std::isfinite(Parameters.tileWidth) || Parameters.tileWidth <= 0.f
            || !std::isfinite(Parameters.tileHeight) || Parameters.tileHeight <= 0.f
            || Parameters.maxTiles < 1 || Parameters.maxTiles > 4096 || Parameters.maxPolys != 16384
            || !Document.at("tiles").is_array() || Document.at("tiles").size() > static_cast<size_t>(Parameters.maxTiles))
        {
            Error = "Navigation data exceeds storage limits";
            return {};
        }
        Result->State->Mesh.reset(dtAllocNavMesh());
        if (!Result->State->Mesh || dtStatusFailed(Result->State->Mesh->init(&Parameters)))
        {
            Error = "Saved navigation initialization failed";
            return {};
        }
        Result->State->Fingerprint = Document.at("fingerprint").get<uint64_t>();
        for (const auto& TileDocument : Document.at("tiles"))
        {
            const std::string Encoded = TileDocument.at("data").get<std::string>();
            if (Encoded.size() > 32 * 1024 * 1024 || Result->State->Statistics.DataBytes > 256 * 1024 * 1024)
            {
                Error = "Saved navigation tile exceeds storage limits";
                return {};
            }
            std::vector<uint8_t> Bytes;
            if (!Base64::Decode(Encoded, Bytes))
            {
                Error = "Saved navigation tile contains invalid Base64";
                return {};
            }
            if (Result->State->Statistics.DataBytes + Bytes.size() > 256 * 1024 * 1024)
            {
                Error = "Saved navigation exceeds the total storage budget";
                return {};
            }
            if (!ValidateNavigationTileBytes(Bytes)
                || ContentHash::FromBytes(Bytes.data(), Bytes.size()).Value != TileDocument.at("checksum").get<uint64_t>())
            {
                Error = "Saved navigation tile is corrupt";
                return {};
            }
            unsigned char* Data = static_cast<unsigned char*>(dtAlloc(Bytes.size(), DT_ALLOC_PERM));
            if (Data == nullptr)
            {
                Error = "Saved navigation allocation failed";
                return {};
            }
            std::memcpy(Data, Bytes.data(), Bytes.size());
            if (dtStatusFailed(Result->State->Mesh->addTile(Data, static_cast<int>(Bytes.size()), DT_TILE_FREE_DATA, 0, nullptr)))
            {
                dtFree(Data);
                Error = "Saved navigation tile insertion failed";
                return {};
            }
            ++Result->State->Statistics.TileCount;
            Result->State->Statistics.DataBytes += Bytes.size();
            Result->State->Statistics.PolygonCount += reinterpret_cast<const dtMeshHeader*>(Data)->offMeshBase;
        }
        return Result;
    }
    catch (const std::exception& Exception)
    {
        Error = std::string("Invalid saved navigation: ") + Exception.what();
        return {};
    }
}
