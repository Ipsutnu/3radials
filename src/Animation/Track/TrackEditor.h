#pragma once

#include <imgui.h>
#include <cstdint>
#include <vector>

namespace TrackEditor
{
    struct PreviewIcon
    {
        ImTextureID texture{};
        ImU32 color{ IM_COL32_WHITE };
    };

    void Open();
    void Cancel();
    bool IsOpen();
    void Draw(float alpha, const ImVec2& mouse, const ImVec2& screen,
        const std::vector<PreviewIcon>& icons, int mainVisibleCount);
    bool MouseDown(const ImVec2& mouse, int button);
    bool MouseUp(const ImVec2& mouse, int button);
    bool Scroll(int direction);
    void SetPanModifier(bool held);
    void SetZoomModifier(bool held);
    bool IsOverflowEraserSelected();
    void SetOverflowEraserSelected(bool selected);
    bool KeyboardInput(std::uint32_t scanCode, bool pressed);
}
