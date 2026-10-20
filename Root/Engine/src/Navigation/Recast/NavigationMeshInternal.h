#pragma once

#include "Navigation/NavigationMesh.h"
#include <DetourNavMesh.h>

struct NavigationMesh::Implementation
{
    std::unique_ptr<dtNavMesh, decltype(&dtFreeNavMesh)> Mesh{nullptr, dtFreeNavMesh};
    NavigationBuildSettings Settings;
    NavigationStatistics Statistics;
    uint64_t Fingerprint = 0;
};
