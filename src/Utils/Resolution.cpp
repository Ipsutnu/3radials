#include "Resolution.h"

#include <algorithm>
#include <imgui_internal.h>

namespace Resolution
{
    namespace
    {
        // "Real" remains the physical game window. It is the coordinate
        // space delivered by Win32 input and used by the rest of the UI.
        ImVec2 g_realSize = kVirtualSize;
        float g_scale = 1.0f;
        ImVec2 g_offset{ 0.0f, 0.0f };

        // Display Tweaks can render the game into a smaller buffer and scale
        // it afterwards. Keep that destination independent from the window.
        ImVec2 g_renderSize = kVirtualSize;
        float g_renderScale = 1.0f;
        ImVec2 g_renderOffset{ 0.0f, 0.0f };
        bool g_viewportChanged = false;
        bool g_frameActive = false;
        bool g_drawDataTransformed = false;

        void UpdateTransform(const ImVec2& a_realSize)
        {
            if (a_realSize.x <= 0.0f || a_realSize.y <= 0.0f)
                return;

            g_viewportChanged = a_realSize.x != g_realSize.x ||
                a_realSize.y != g_realSize.y;
            g_realSize = a_realSize;
            g_scale = std::min(
                g_realSize.x / kVirtualSize.x,
                g_realSize.y / kVirtualSize.y);
            g_offset = ImVec2(
                (g_realSize.x - kVirtualSize.x * g_scale) * 0.5f,
                (g_realSize.y - kVirtualSize.y * g_scale) * 0.5f);
        }

        void UpdateRenderTransform(const ImVec2& a_renderSize)
        {
            if (a_renderSize.x <= 0.0f || a_renderSize.y <= 0.0f)
                return;

            g_renderSize = a_renderSize;
            g_renderScale = std::min(
                g_renderSize.x / kVirtualSize.x,
                g_renderSize.y / kVirtualSize.y);
            g_renderOffset = ImVec2(
                (g_renderSize.x - kVirtualSize.x * g_renderScale) * 0.5f,
                (g_renderSize.y - kVirtualSize.y * g_renderScale) * 0.5f);
        }
    }

    void PrepareFrame()
    {
        ImGuiIO& io = ImGui::GetIO();
        UpdateTransform(io.DisplaySize);
        io.DisplaySize = kVirtualSize;
        g_drawDataTransformed = false;
    }

    void BeginFrame()
    {
        ImGuiIO& io = ImGui::GetIO();
        // ImGui::NewFrame has consumed the backend's physical mouse sample.
        // Convert it in place; queuing a second event here would make ImGui
        // consume real and virtual positions on alternating frames.
        io.MousePos = ToVirtual(io.MousePos);

        // Geometry is reduced afterwards for low resolutions. Bake scalable
        // glyphs at a higher density first so that reduction stays sharp.
        // ImGui 1.92 renomeou SetFontRasterizerDensity() para
        // SetPixelDensity(); o comportamento continua sendo o mesmo aqui.
        ImGui::SetPixelDensity(std::max(1.0f, 1.0f / g_renderScale));
        g_frameActive = true;
    }

    void SetRenderTargetSize(const ImVec2& a_renderTargetSize)
    {
        UpdateRenderTransform(a_renderTargetSize);
    }

    void TransformDrawData(ImDrawData* a_drawData,
        const ImVec2& a_renderTargetSize)
    {
        if (!a_drawData || g_drawDataTransformed)
            return;

        SetRenderTargetSize(a_renderTargetSize);

        // This ImGui context is created and owned by p-radials. Transforming
        // its draw data therefore cannot affect another plugin's context.
        for (int listIndex = 0; listIndex < a_drawData->CmdListsCount; ++listIndex)
        {
            ImDrawList* list = a_drawData->CmdLists[listIndex];
            for (ImDrawVert& vertex : list->VtxBuffer)
                vertex.pos = ToRender(vertex.pos);

            for (ImDrawCmd& command : list->CmdBuffer)
            {
                // ImGui creates every root draw list with a 1920x1080 clip
                // because p-radials renders in virtual coordinates. Geometry
                // intentionally positioned in the extra area of ultrawide or
                // taller viewports must not remain clipped to that reference
                // rectangle. Explicit smaller clips (descriptor, scrolling,
                // etc.) are still transformed normally.
                const bool isRootViewportClip =
                    command.ClipRect.x <= 0.5f &&
                    command.ClipRect.y <= 0.5f &&
                    command.ClipRect.z >= kVirtualSize.x - 0.5f &&
                    command.ClipRect.w >= kVirtualSize.y - 0.5f;

                if (isRootViewportClip)
                {
                    command.ClipRect = ImVec4(0.0f, 0.0f,
                        g_renderSize.x, g_renderSize.y);
                }
                else
                {
                    const ImVec2 min = ToRender(ImVec2(command.ClipRect.x, command.ClipRect.y));
                    const ImVec2 max = ToRender(ImVec2(command.ClipRect.z, command.ClipRect.w));
                    command.ClipRect = ImVec4(min.x, min.y, max.x, max.y);
                }
            }
        }

        a_drawData->DisplayPos = ImVec2(0.0f, 0.0f);
        a_drawData->DisplaySize = g_renderSize;
        a_drawData->FramebufferScale = ImVec2(1.0f, 1.0f);
        g_drawDataTransformed = true;
    }

    void EndFrame()
    {
        if (!g_frameActive)
            return;
        ImGuiIO& io = ImGui::GetIO();
        // Code executed by the SKSE input sink runs between frames. Return its
        // observable cursor state to physical pixels so opening a radial never
        // applies the virtual conversion twice.
        io.MousePos = ToReal(io.MousePos);
        io.DisplaySize = g_realSize;
        g_frameActive = false;
    }

    ImVec2 GetRealSize() { return g_realSize; }
    ImVec2 GetRenderSize() { return g_renderSize; }
    ImVec2 GetRealCenter() { return ImVec2(g_realSize.x * 0.5f, g_realSize.y * 0.5f); }
    float GetScale() { return g_scale; }
    bool DidViewportChange() { return g_viewportChanged; }

    ImVec2 ToReal(const ImVec2& a_virtualPosition)
    {
        return ImVec2(a_virtualPosition.x * g_scale + g_offset.x,
            a_virtualPosition.y * g_scale + g_offset.y);
    }
    ImVec2 ToRender(const ImVec2& a_virtualPosition)
    {
        return ImVec2(a_virtualPosition.x * g_renderScale + g_renderOffset.x,
            a_virtualPosition.y * g_renderScale + g_renderOffset.y);
    }
    ImVec2 ToVirtual(const ImVec2& a_realPosition)
    {
        if (g_scale <= 0.0f)
            return a_realPosition;
        return ImVec2((a_realPosition.x - g_offset.x) / g_scale,
            (a_realPosition.y - g_offset.y) / g_scale);
    }
    ImVec2 ToRealDelta(const ImVec2& a_virtualDelta)
    {
        return ImVec2(a_virtualDelta.x * g_scale, a_virtualDelta.y * g_scale);
    }
    ImVec2 ToVirtualDelta(const ImVec2& a_realDelta)
    {
        if (g_scale <= 0.0f)
            return a_realDelta;
        return ImVec2(a_realDelta.x / g_scale, a_realDelta.y / g_scale);
    }
    float ToRealSize(float a_virtualSize) { return a_virtualSize * g_scale; }
    float ToVirtualSize(float a_realSize) { return g_scale > 0.0f ? a_realSize / g_scale : a_realSize; }
    ImVec2 ToRealFromVirtualCenter(const ImVec2& a_virtualOffset)
    {
        const ImVec2 center = GetVirtualCenter();
        return ToReal(ImVec2(center.x + a_virtualOffset.x, center.y + a_virtualOffset.y));
    }
    ImVec2 ToVirtualFromRealCenter(const ImVec2& a_realOffset)
    {
        const ImVec2 center = GetRealCenter();
        return ToVirtual(ImVec2(center.x + a_realOffset.x, center.y + a_realOffset.y));
    }
}
