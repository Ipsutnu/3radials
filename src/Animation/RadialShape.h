#pragma once

#include <imgui.h>

#include <string_view>
#include <vector>

namespace RadialShape
{
    enum class Style : int
    {
        ClassicOrbit = 0,
        HarmonicFlower,
        DualOrbit,
        Turbine,
        SpiralGalaxy,
        PulsarCrown,
        LiquidDiamond,
        CometTail,
        RoseEngine,
        QuantumRipple,
        Star,
        Count
    };

    constexpr int Count() { return static_cast<int>(Style::Count); }
    const char* Name(Style style);
    Style FromName(std::string_view name);

    ImVec2 PositionAtAngle(Style style, const ImVec2& center,
        float radius, float angle);
    ImVec2 PositionAtAngles(Style style, const ImVec2& center,
        float radius, float shapeAngle, float directionAngle);

    ImVec2 MainPosition(Style style, const ImVec2& center, float radius,
        int index, int count, bool leftSide);
    ImVec2 OverflowPosition(Style style, const ImVec2& center, float radius,
        int overflowIndex, bool leftSide);
    std::vector<ImVec2> Outline(Style style, const ImVec2& center,
        float radius, bool leftSide, int samples = 64);
}
