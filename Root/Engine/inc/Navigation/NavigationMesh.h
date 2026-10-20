#pragma once

#include "Navigation/NavigationTypes.h"
#include <nlohmann/json_fwd.hpp>

class NavigationMesh
{
public:
    NavigationMesh();
    ~NavigationMesh();
    NavigationMesh(const NavigationMesh&) = delete;
    NavigationMesh& operator=(const NavigationMesh&) = delete;
    static std::shared_ptr<NavigationMesh> Build(const NavigationBuildInput& Input, std::string& Error);
    NavigationPath FindPath(const Vector3& Start, const Vector3& Goal,
        const NavigationQueryFilter& Filter = {}, const Vector3& ProjectionExtent = Vector3(1.f, 2.f, 1.f)) const;
    bool ProjectPoint(const Vector3& Position, Vector3& Projected,
        const Vector3& Extent = Vector3(1.f, 2.f, 1.f), const NavigationQueryFilter& Filter = {}) const;
    bool Raycast(const Vector3& Start, const Vector3& End, Vector3& Hit,
        const NavigationQueryFilter& Filter = {}) const;
    bool MoveAlongSurface(const Vector3& Start, const Vector3& Destination, Vector3& Result,
        const NavigationQueryFilter& Filter = {}) const;
    bool FindRandomPoint(Vector3& Point, uint32_t& RandomState, const NavigationQueryFilter& Filter = {}) const;
    std::vector<NavigationDebugTriangle> GetDebugTriangles() const;
    const NavigationBuildSettings& GetSettings() const;
    const NavigationStatistics& GetStatistics() const;
    uint64_t GetFingerprint() const;
    bool HasLink(uint32_t Identity) const;
    void Serialize(nlohmann::json& Document) const;
    static std::shared_ptr<NavigationMesh> Deserialize(const nlohmann::json& Document, std::string& Error);

private:
    friend class NavigationCrowd;
    struct Implementation;
    std::unique_ptr<Implementation> State;
};
