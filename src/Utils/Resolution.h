#pragma once

#include <imgui.h>

// WheelWheel is authored in a fixed, virtual 1920x1080 coordinate space.
// This module is the sole bridge between that space and the real backbuffer.
namespace Resolution
{
    inline constexpr ImVec2 kVirtualSize{ 1920.0f, 1080.0f };

    // Called after platform backends and before ImGui::NewFrame. It snapshots
    // the real viewport and makes ImGui create its draw lists in virtual space.
    void PrepareFrame();

    // Called immediately after ImGui::NewFrame to convert the mouse sample
    // consumed by ImGui from real pixels to WheelWheel virtual coordinates.
    void BeginFrame();

    // Converts WheelWheel draw data to the real viewport. Call after ImGui::Render()
    // and immediately before the DX11 backend consumes the draw data.
    void TransformDrawData(ImDrawData* a_drawData);

    // Restores ImGui's public viewport after rendering.
    void EndFrame();

    [[nodiscard]] ImVec2 GetRealSize();
    [[nodiscard]] constexpr ImVec2 GetVirtualSize() { return kVirtualSize; }
    [[nodiscard]] ImVec2 GetRealCenter();
    [[nodiscard]] constexpr ImVec2 GetVirtualCenter()
    {
        return ImVec2(kVirtualSize.x * 0.5f, kVirtualSize.y * 0.5f);
    }
    [[nodiscard]] float GetScale();
    [[nodiscard]] bool DidViewportChange();

    [[nodiscard]] ImVec2 ToReal(const ImVec2& a_virtualPosition);
    [[nodiscard]] ImVec2 ToVirtual(const ImVec2& a_realPosition);
    [[nodiscard]] ImVec2 ToRealDelta(const ImVec2& a_virtualDelta);
    [[nodiscard]] ImVec2 ToVirtualDelta(const ImVec2& a_realDelta);
    [[nodiscard]] float ToRealSize(float a_virtualSize);
    [[nodiscard]] float ToVirtualSize(float a_realSize);

    // Center-relative variants are explicit to avoid accidental scaling of an
    // absolute coordinate as if it were an offset.
    [[nodiscard]] ImVec2 ToRealFromVirtualCenter(const ImVec2& a_virtualOffset);
    [[nodiscard]] ImVec2 ToVirtualFromRealCenter(const ImVec2& a_realOffset);
}
