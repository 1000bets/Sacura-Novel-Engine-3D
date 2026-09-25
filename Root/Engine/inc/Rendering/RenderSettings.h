#pragma once

struct RenderSettings
{
    float Exposure = 1.f;
    float EnvironmentIntensity = 1.f;
    float BloomIntensity = 0.05f;
    float BloomThreshold = 1.f;
    float ShadowDistance = 80.f;
    bool bAntialiasing = true;
};

struct RenderStatistics
{
    unsigned long long FrameIndex = 0;
    unsigned int SubmittedObjects = 0;
    unsigned int VisibleObjects = 0;
    unsigned int CulledObjects = 0;
    unsigned int DrawCalls = 0;
    unsigned int Triangles = 0;
    unsigned int LightCount = 0;
    unsigned int DroppedLights = 0;
    unsigned long long GpuFrameIndex = 0;
    unsigned long long ResidentBytes = 0;
    unsigned int ResidentMeshes = 0;
    unsigned int ResidentTextures = 0;
    unsigned int RetiredResources = 0;
    unsigned int ShadowViews = 0;
    unsigned int DroppedShadowLights = 0;
    double CpuMilliseconds = 0.0;
    double ShadowMilliseconds = 0.0;
    double OpaqueMilliseconds = 0.0;
    double TransparencyMilliseconds = 0.0;
    double PostprocessMilliseconds = 0.0;
    bool bGpuTimingsAvailable = false;
};
