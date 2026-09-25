#include "Rendering/RHI/Renderer.h"
#include "Core/Threading/ThreadContext.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "ImGuiImplDiligent.hpp"
#include "ImGuiDiligentRenderer.hpp"
#include "imgui.h"

#include <cstring>
#include <vector>

void Renderer::CreateImGuiOverlay()
{
    AssertRenderThread();
    DestroyImGuiOverlay();
    OverlayImGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(OverlayImGuiContext);
    ImGui::GetIO().IniFilename = nullptr;
    Diligent::ImGuiDiligentCreateInfo OverlayCreateInfo;
    OverlayCreateInfo.pDevice = Device.GetDevice();
    OverlayCreateInfo.BackBufferFmt = Device.GetSwapChain()->GetDesc().ColorBufferFormat;
    OverlayCreateInfo.DepthBufferFmt = Diligent::TEX_FORMAT_UNKNOWN;
    OverlayImGuiRenderer = new Diligent::ImGuiDiligentRenderer(OverlayCreateInfo);
}

void Renderer::DestroyImGuiOverlay()
{
    AssertRenderThread();
    delete OverlayImGuiRenderer;
    OverlayImGuiRenderer = nullptr;
    if (OverlayImGuiContext != nullptr)
    {
        ImGui::DestroyContext(OverlayImGuiContext);
        OverlayImGuiContext = nullptr;
    }
}

void Renderer::RenderImGuiOverlay(
    Diligent::IDeviceContext* Context,
    Diligent::ISwapChain* SwapChain,
    const ImGuiOverlaySnapshot& Overlay)
{
    AssertRenderThread();
    if (Context == nullptr
        || SwapChain == nullptr
        || OverlayImGuiRenderer == nullptr
        || OverlayImGuiContext == nullptr
        || !Overlay.bValid
        || Overlay.Lists.empty()
        || Overlay.DisplayWidth <= 0.0f
        || Overlay.DisplayHeight <= 0.0f)
    {
        return;
    }

    static_assert(sizeof(ImGuiOverlayVertex) == sizeof(ImDrawVert), "ImGui overlay vertex layout must match ImDrawVert");
    static_assert(sizeof(uint16_t) == sizeof(ImDrawIdx), "ImGui overlay index type must match ImDrawIdx");

    ImGuiContext* PreviousContext = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(OverlayImGuiContext);

    const Diligent::SwapChainDesc& Description = SwapChain->GetDesc();
    OverlayImGuiRenderer->NewFrame(Description.Width, Description.Height, Diligent::SURFACE_TRANSFORM_IDENTITY);

    const ImTextureID FontTexture = ImGui::GetIO().Fonts->TexID;
    std::vector<ImDrawList> OwnedLists;
    std::vector<ImDrawList*> ListPointers;
    OwnedLists.reserve(Overlay.Lists.size());
    ListPointers.reserve(Overlay.Lists.size());

    ImDrawData DrawData;
    DrawData.Valid = true;
    DrawData.DisplayPos = ImVec2(0.0f, 0.0f);
    DrawData.DisplaySize = ImVec2(Overlay.DisplayWidth, Overlay.DisplayHeight);
    DrawData.FramebufferScale = ImVec2(Overlay.FramebufferScaleX, Overlay.FramebufferScaleY);

    for (const ImGuiOverlayDrawList& SourceList : Overlay.Lists)
    {
        OwnedLists.emplace_back(ImGui::GetDrawListSharedData());
        ImDrawList& DrawList = OwnedLists.back();
        DrawList._ResetForNewFrame();
        DrawList.VtxBuffer.resize(static_cast<int>(SourceList.Vertices.size()));
        if (!SourceList.Vertices.empty())
        {
            std::memcpy(DrawList.VtxBuffer.Data, SourceList.Vertices.data(), SourceList.Vertices.size() * sizeof(ImDrawVert));
        }
        DrawList.IdxBuffer.resize(static_cast<int>(SourceList.Indices.size()));
        if (!SourceList.Indices.empty())
        {
            std::memcpy(DrawList.IdxBuffer.Data, SourceList.Indices.data(), SourceList.Indices.size() * sizeof(ImDrawIdx));
        }
        DrawList.CmdBuffer.reserve(static_cast<int>(SourceList.Commands.size()));
        for (const ImGuiOverlayDrawCommand& SourceCommand : SourceList.Commands)
        {
            ImDrawCmd Command;
            Command.ClipRect = ImVec4(
                SourceCommand.ClipMinX,
                SourceCommand.ClipMinY,
                SourceCommand.ClipMaxX,
                SourceCommand.ClipMaxY);
            Command.TextureId = FontTexture;
            Command.VtxOffset = SourceCommand.VertexOffset;
            Command.IdxOffset = SourceCommand.IndexOffset;
            Command.ElemCount = SourceCommand.IndexCount;
            DrawList.CmdBuffer.push_back(Command);
        }
        DrawData.TotalVtxCount += DrawList.VtxBuffer.Size;
        DrawData.TotalIdxCount += DrawList.IdxBuffer.Size;
        ListPointers.push_back(&DrawList);
    }

    DrawData.CmdLists = ListPointers.data();
    DrawData.CmdListsCount = static_cast<int>(ListPointers.size());

    Diligent::ITextureView* BackBuffer = SwapChain->GetCurrentBackBufferRTV();
    Context->SetRenderTargets(1, &BackBuffer, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    OverlayImGuiRenderer->RenderDrawData(Context, &DrawData);
    OverlayImGuiRenderer->EndFrame();

    ImGui::SetCurrentContext(PreviousContext);
}
