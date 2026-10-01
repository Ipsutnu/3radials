#include "Resolution.h"

#include <algorithm>
#include <imgui_internal.h>

namespace Resolution
{
    namespace
    {
        ImVec2 g_realSize = kVirtualSize;
        float g_scale = 1.0f;
        ImVec2 g_offset{ 0.0f, 0.0f };
        bool g_viewportChanged = false;
        bool g_frameActive = false;

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
    }

    void PrepareFrame()
    {
        ImGuiIO& io = ImGui::GetIO();
        UpdateTransform(io.DisplaySize);
        io.DisplaySize = kVirtualSize;
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
        ImGui::SetPixelDensity(std::max(1.0f, 1.0f / g_scale));
        g_frameActive = true;
    }

    void TransformDrawData(ImDrawData* a_drawData)
    {
        if (!a_drawData || !g_frameActive)
            return;

        // This ImGui context is created and owned by WheelWheel. Transforming
        // its draw data therefore cannot affect another plugin's context.
        for (int listIndex = 0; listIndex < a_drawData->CmdListsCount; ++listIndex)
        {
            ImDrawList* list = a_drawData->CmdLists[listIndex];
            for (ImDrawVert& vertex : list->VtxBuffer)
                vertex.pos = ToReal(vertex.pos);

            for (ImDrawCmd& command : list->CmdBuffer)
            {
                // ImGui creates every root draw list with a 1920x1080 clip
                // because WheelWheel renders in virtual coordinates. Geometry
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
                    command.ClipRect = ImVec4(0.0f, 0.0f, g_realSize.x, g_realSize.y);
                }
                else
                {
                    const ImVec2 min = ToReal(ImVec2(command.ClipRect.x, command.ClipRect.y));
                    const ImVec2 max = ToReal(ImVec2(command.ClipRect.z, command.ClipRect.w));
                    command.ClipRect = ImVec4(min.x, min.y, max.x, max.y);
                }
            }
        }

        a_drawData->DisplayPos = ImVec2(0.0f, 0.0f);
        a_drawData->DisplaySize = g_realSize;
        a_drawData->FramebufferScale = ImVec2(1.0f, 1.0f);
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
    ImVec2 GetRealCenter() { return ImVec2(g_realSize.x * 0.5f, g_realSize.y * 0.5f); }
    float GetScale() { return g_scale; }
    bool DidViewportChange() { return g_viewportChanged; }

    ImVec2 ToReal(const ImVec2& a_virtualPosition)
    {
        return ImVec2(a_virtualPosition.x * g_scale + g_offset.x,
            a_virtualPosition.y * g_scale + g_offset.y);
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
