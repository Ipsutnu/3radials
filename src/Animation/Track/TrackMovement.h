#pragma once

#include "Animation/OverflowMechanism.h"
#include "Animation/Animation.h"

namespace TrackMovement
{
    enum class Phase { Main, EnteringMainTrack, Entering, Track, LeavingTrack, Leaving };

    struct State
    {
        Phase phase{ Phase::Main };
        OverflowMechanism::Layer layer{ OverflowMechanism::Layer::Main };
        ImVec2 position{};
        ImVec2 velocity{};
        ImVec2 start{};
        ImVec2 endpoint{};
        float distance{};
        float distanceVelocity{};
        float gateDistance{};
        float mainGateDistance{};
        float progress{ 1.0f };
        float targetDistance{ -1.0f };
        float motionStartDistance{};
        float motionDelta{};
        float mainBlend{ 1.0f };
        float styleTransition{ 1.0f };
        int travelDirection{};
        bool initialized{ false };
    };

    ImVec2 Update(State& state, const ImVec2& current, const ImVec2& target,
        const ImVec2& center, float radius, bool leftSide,
        RadialShape::Style radialShape, OverflowMechanism::Layer layer,
        int routeIndex, int rotationDirection, float deltaTime);

    ImVec2 MainTarget(const ImVec2& center, float radius, bool leftSide,
        RadialShape::Style radialShape, int index, int count);

    ImVec2 UpdateCircuit(State& state, const ImVec2& current,
        const ImVec2& target, const ImVec2& center, float radius,
        bool leftSide, int rotationDirection, float deltaTime,
        RadialAnimation::Style style, RadialShape::Style radialShape,
        float targetCircuitT = -1.0f);
}
