#pragma once

#include <cstdint>
#include <vector>

struct ImGuiOverlayVertex
{
    float PositionX = 0.0f;
    float PositionY = 0.0f;
    float TextureU = 0.0f;
    float TextureV = 0.0f;
    uint32_t Color = 0;
};

struct ImGuiOverlayDrawCommand
{
    uint32_t IndexOffset = 0;
    uint32_t IndexCount = 0;
    uint32_t VertexOffset = 0;
    float ClipMinX = 0.0f;
    float ClipMinY = 0.0f;
    float ClipMaxX = 0.0f;
    float ClipMaxY = 0.0f;
};

struct ImGuiOverlayDrawList
{
    std::vector<ImGuiOverlayVertex> Vertices;
    std::vector<uint16_t> Indices;
    std::vector<ImGuiOverlayDrawCommand> Commands;
};

struct ImGuiOverlaySnapshot
{
    float DisplayWidth = 0.0f;
    float DisplayHeight = 0.0f;
    float FramebufferScaleX = 1.0f;
    float FramebufferScaleY = 1.0f;
    std::vector<ImGuiOverlayDrawList> Lists;
    bool bValid = false;
};
