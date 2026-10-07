#pragma once

#include <imgui.h>

#include <array>
#include <string_view>

namespace RadialAnimation
{
    enum class Style : int
    {
        ShortestPath = 0,
        SimpleRadial,
        MagneticDetent,
        CycloidalGear,
        OrbitalTransfer,
        FluidVortex,
        Count
    };

    enum class Layer
    {
        Main,
        Overflow
    };

    struct State
    {
        ImVec2 position{};
        ImVec2 velocity{};
        ImVec2 lastTarget{};
        bool initialized{ false };
        float transition{ 1.0f };
        float angle{ 0.0f };
        float targetAngle{ 0.0f };
        float angularVelocity{ 0.0f };
        float radius{ 0.0f };
        float radialVelocity{ 0.0f };
    };

    constexpr int Count() { return static_cast<int>(Style::Count); }
    const char* Name(Style style);
    Style FromName(std::string_view name);

    // directedRotation is used by rapid side-radial scroll. Unlike a normal
    // target change, a complete turn must keep its input direction instead of
    // resolving the final coincident slot through the shortest path.
    ImVec2 Update(State& state, const ImVec2& target, const ImVec2& center,
        bool leftSide, Layer layer, float deltaTime, Style style,
        int directedRotation = 0);
}
