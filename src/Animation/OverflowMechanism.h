#pragma once

#include "Animation/RadialShape.h"

#include <imgui.h>

#include <string_view>
#include <vector>

namespace OverflowMechanism
{
    enum class Style : int
    {
        ConcentricRings = 0,
        Count
    };

    enum class Layer { Main, Overflow };

    struct State
    {
        Style style{ Style::ConcentricRings };
        Layer layer{ Layer::Main };
        ImVec2 start{};
        ImVec2 gate{};
        ImVec2 gate2{};
        ImVec2 position{};
        ImVec2 velocity{};
        ImVec2 lastTarget{};
        float progress{ 1.0f };
        int routeIndex{ 0 };
        int routeCount{ 1 };
        bool initialized{ false };
    };

    constexpr int Count() { return static_cast<int>(Style::Count); }
    const char* Name(Style style);
    Style FromName(std::string_view name);

    ImVec2 OverflowPosition(Style style, RadialShape::Style radialShape,
        const ImVec2& center, float radius, int index, int count, bool leftSide);
    ImVec2 RouteTransition(State& state, const ImVec2& current,
        const ImVec2& target, const ImVec2& center, float radius,
        bool leftSide, Layer layer, int routeIndex, int routeCount,
        float deltaTime, Style style);
    std::vector<std::vector<ImVec2>> GuidePaths(Style style,
        const ImVec2& center, float radius, bool leftSide, int samples = 48);
}
