#include "PCH.h"
#include "Animation/OverflowMechanism.h"

namespace OverflowMechanism
{
    const char* Name(Style) { return "Concentric Rings"; }
    Style FromName(std::string_view) { return Style::ConcentricRings; }

    ImVec2 OverflowPosition(Style, RadialShape::Style radialShape,
        const ImVec2& center, float radius, int index, int, bool leftSide)
    {
        return RadialShape::OverflowPosition(
            radialShape, center, radius, index, leftSide);
    }

    ImVec2 RouteTransition(State& state, const ImVec2&,
        const ImVec2& target, const ImVec2&, float,
        bool, Layer layer, int routeIndex, int routeCount,
        float, Style)
    {
        state.style = Style::ConcentricRings;
        state.layer = layer;
        state.position = target;
        state.lastTarget = target;
        state.routeIndex = routeIndex;
        state.routeCount = routeCount;
        state.progress = 1.0f;
        state.initialized = true;
        return target;
    }

    std::vector<std::vector<ImVec2>> GuidePaths(Style,
        const ImVec2&, float, bool, int)
    {
        return {};
    }
}
