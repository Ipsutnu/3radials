#include "PCH.h"
#include "Animation/Track/TrackMovement.h"
#include "Animation/Track/TrackGeometry.h"
#include "Animation/Track/TrackLayout.h"

#include <algorithm>
#include <cmath>

namespace
{
    struct FrameCircuitCache
    {
        int frame{-1};
        ImVec2 center{};
        float radius{};
        bool leftSide{};
        RadialShape::Style shape{ RadialShape::Style::ClassicOrbit };
        std::vector<ImVec2> points;
        std::vector<float> cumulativeLengths;
        float length{};
    };

    FrameCircuitCache g_frameCircuitCache;

    const std::vector<ImVec2>& FrameCircuit(const ImVec2& center,
        float radius, bool leftSide, RadialShape::Style shape, float& length)
    {
        auto& cache = g_frameCircuitCache;
        const int frame = ImGui::GetFrameCount();
        if (cache.frame != frame || cache.center.x != center.x ||
            cache.center.y != center.y || cache.radius != radius ||
            cache.leftSide != leftSide || cache.shape != shape)
        {
            cache.frame = frame;
            cache.center = center;
            cache.radius = radius;
            cache.leftSide = leftSide;
            cache.shape = shape;
            cache.points = Track::Circuit(center, radius, leftSide, shape);
            cache.cumulativeLengths.clear();
            cache.cumulativeLengths.reserve(cache.points.size());
            cache.cumulativeLengths.push_back(0.0f);
            cache.length = 0.0f;
            for (std::size_t i = 1; i < cache.points.size(); ++i)
            {
                cache.length += Track::Distance(cache.points[i - 1], cache.points[i]);
                cache.cumulativeLengths.push_back(cache.length);
            }
        }
        length = cache.length;
        return cache.points;
    }

    ImVec2 SampleFrameCircuit(float distance)
    {
        const auto& cache = g_frameCircuitCache;
        if (cache.points.empty()) return {};
        if (cache.points.size() == 1 || cache.length <= 0.001f)
            return cache.points.front();

        distance = std::fmod(distance, cache.length);
        if (distance < 0.0f) distance += cache.length;
        const auto upper = std::upper_bound(cache.cumulativeLengths.begin(),
            cache.cumulativeLengths.end(), distance);
        const std::size_t index = std::clamp<std::size_t>(
            static_cast<std::size_t>(std::distance(
                cache.cumulativeLengths.begin(), upper)),
            1, cache.points.size() - 1);
        const float startDistance = cache.cumulativeLengths[index - 1];
        const float segmentLength = std::max(
            cache.cumulativeLengths[index] - startDistance, 0.001f);
        const float t = std::clamp(
            (distance - startDistance) / segmentLength, 0.0f, 1.0f);
        const ImVec2& a = cache.points[index - 1];
        const ImVec2& b = cache.points[index];
        return ImVec2(a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t);
    }

    float ClosestDistance(const std::vector<ImVec2>& path, const ImVec2& point)
    {
        if (path.size() < 2) return 0.0f;
        float along = 0.0f;
        float bestAlong = 0.0f;
        float bestSq = FLT_MAX;
        for (std::size_t i = 1; i < path.size(); ++i)
        {
            const ImVec2 a = path[i - 1];
            const ImVec2 b = path[i];
            const float x = b.x - a.x;
            const float y = b.y - a.y;
            const float lengthSq = x * x + y * y;
            const float length = std::sqrt(lengthSq);
            const float t = lengthSq > 0.001f ? std::clamp(
                ((point.x - a.x) * x + (point.y - a.y) * y) / lengthSq, 0.0f, 1.0f) : 0.0f;
            const ImVec2 candidate(a.x + x * t, a.y + y * t);
            const float dx = point.x - candidate.x;
            const float dy = point.y - candidate.y;
            const float distanceSq = dx * dx + dy * dy;
            if (distanceSq < bestSq)
            {
                bestSq = distanceSq;
                bestAlong = along + length * t;
            }
            along += length;
        }
        return bestAlong;
    }

    float MoveCyclic(float current, float target, float length, float amount)
    {
        if (length <= 0.001f) return target;
        float delta = target - current;
        while (delta > length * 0.5f) delta -= length;
        while (delta < -length * 0.5f) delta += length;
        if (std::abs(delta) <= amount) return target;
        current += std::copysign(amount, delta);
        while (current < 0.0f) current += length;
        while (current >= length) current -= length;
        return current;
    }

    float MoveDirected(float current, float target, float length, float amount, int direction)
    {
        if (length <= 0.001f) return target;
        direction = direction < 0 ? -1 : 1;
        float forward = (target - current) * static_cast<float>(direction);
        while (forward < 0.0f) forward += length;
        while (forward >= length) forward -= length;
        if (forward <= amount || length - forward < 1.0f) return target;
        current += amount * static_cast<float>(direction);
        while (current < 0.0f) current += length;
        while (current >= length) current -= length;
        return current;
    }

    float SpringDirected(float current, float target, float length, float dt,
        int direction, float& velocity)
    {
        if (length <= 0.001f) return target;
        direction = direction < 0 ? -1 : 1;
        float delta = target - current;
        if (std::abs(delta) < 0.01f)
        {
            velocity = 0.0f;
            return target;
        }
        if (direction > 0)
            while (delta < 0.0f) delta += length;
        else
            while (delta > 0.0f) delta -= length;

        constexpr float stiffness = 235.0f;
        constexpr float damping = 27.0f;
        velocity += (delta * stiffness - velocity * damping) * dt;
        velocity = direction > 0 ? std::max(0.0f, velocity) : std::min(0.0f, velocity);
        const float step = velocity * dt;
        if (std::abs(step) >= std::abs(delta))
        {
            velocity = 0.0f;
            return target;
        }
        current += step;
        while (current < 0.0f) current += length;
        while (current >= length) current -= length;
        return current;
    }

    float SpringDirectedStyled(float current, float target, float length, float dt,
        int direction, float& velocity, float stiffness, float damping)
    {
        if (length <= 0.001f) return target;
        direction = direction < 0 ? -1 : 1;
        float delta = target - current;
        if (direction > 0)
            while (delta < 0.0f) delta += length;
        else
            while (delta > 0.0f) delta -= length;

        if (std::abs(delta) < 0.01f)
        {
            velocity = 0.0f;
            return target;
        }

        velocity += (delta * stiffness - velocity * damping) * dt;
        velocity = direction > 0
            ? std::max(0.0f, velocity)
            : std::min(0.0f, velocity);
        const float step = velocity * dt;
        if (std::abs(step) >= std::abs(delta))
        {
            velocity = 0.0f;
            return target;
        }

        current += step;
        while (current < 0.0f) current += length;
        while (current >= length) current -= length;
        return current;
    }

    std::pair<float, float> ClosestGates(const std::vector<ImVec2>& mainPath,
        const std::vector<ImVec2>& overflowPath)
    {
        float mainAlong = 0.0f, bestMain = 0.0f, bestOverflow = 0.0f;
        float bestSq = FLT_MAX;
        for (std::size_t i = 0; i + 1 < mainPath.size(); ++i)
        {
            float overflowAlong = 0.0f;
            for (std::size_t j = 0; j + 1 < overflowPath.size(); ++j)
            {
                const float dx = mainPath[i].x - overflowPath[j].x;
                const float dy = mainPath[i].y - overflowPath[j].y;
                const float sq = dx * dx + dy * dy;
                if (sq < bestSq)
                {
                    bestSq = sq;
                    bestMain = mainAlong;
                    bestOverflow = overflowAlong;
                }
                overflowAlong += Track::Distance(overflowPath[j], overflowPath[j + 1]);
            }
            mainAlong += Track::Distance(mainPath[i], mainPath[i + 1]);
        }
        return { bestMain, bestOverflow };
    }

    ImVec2 SmoothCurve(const ImVec2& start, const ImVec2& end,
        const ImVec2& center, float t)
    {
        (void)center;
        t = t * t * (3.0f - 2.0f * t);
        return ImVec2(start.x + (end.x - start.x) * t,
            start.y + (end.y - start.y) * t);
    }
}

namespace TrackMovement
{
    ImVec2 UpdateCircuit(State& state, const ImVec2& current,
        const ImVec2& target, const ImVec2& center, float radius,
        bool leftSide, int rotationDirection, float deltaTime,
        RadialAnimation::Style style, RadialShape::Style radialShape,
        float targetCircuitT)
    {
        float length = 0.0f;
        const auto& circuit = FrameCircuit(
            center, radius, leftSide, radialShape, length);
        if (circuit.size() < 2 || length <= 0.001f)
            return target;

        const float targetDistance = targetCircuitT >= 0.0f
            ? std::clamp(targetCircuitT, 0.0f, 1.0f) * length
            : ClosestDistance(circuit, target);
        if (!state.initialized)
        {
            state.distance = ClosestDistance(circuit, current);
            state.position = SampleFrameCircuit(state.distance);
            state.distanceVelocity = 0.0f;
            state.targetDistance = targetDistance;
            state.progress = 1.0f;
            state.initialized = true;
        }

        float targetChange = targetDistance - state.targetDistance;
        while (targetChange > length * 0.5f) targetChange -= length;
        while (targetChange < -length * 0.5f) targetChange += length;
        if (std::abs(targetChange) > 0.5f)
        {
            const int nextDirection = rotationDirection > 0 ? -1 :
                rotationDirection < 0 ? 1 : (targetChange < 0.0f ? -1 : 1);
            if (state.travelDirection != 0 && state.travelDirection != nextDirection)
                state.distanceVelocity *= 0.20f;
            state.travelDirection = nextDirection;
            state.targetDistance = targetDistance;
            state.styleTransition = 0.0f;
            // Marca uma transição real. Quando ela terminar, o item permanece
            // encaixado no slot até outro passo de scroll alterar o alvo.
            state.progress = 0.0f;
        }

        float stiffness = 315.0f;
        float damping = 25.0f;
        switch (style)
        {
        case RadialAnimation::Style::SimpleRadial:
            stiffness = 315.0f; damping = 25.0f; break;
        case RadialAnimation::Style::MagneticDetent:
        {
            float remaining = state.targetDistance - state.distance;
            if (state.travelDirection > 0)
                while (remaining < 0.0f) remaining += length;
            else
                while (remaining > 0.0f) remaining -= length;
            if (std::abs(remaining) < length * 0.025f)
            {
                stiffness = 360.0f;
                damping = 30.0f;
            }
            else
            {
                stiffness = 135.0f;
                damping = 20.0f;
            }
            break;
        }
        case RadialAnimation::Style::CycloidalGear:
            stiffness = 165.0f; damping = 24.0f; break;
        case RadialAnimation::Style::OrbitalTransfer:
            stiffness = 120.0f; damping = 22.0f; break;
        case RadialAnimation::Style::FluidVortex:
            stiffness = 145.0f; damping = 22.0f; break;
        default: break;
        }
        const float dt = std::clamp(deltaTime, 0.0f, 1.0f / 30.0f);
        const int direction = state.travelDirection != 0
            ? state.travelDirection
            : (rotationDirection > 0 ? -1 : 1);
        if (state.progress >= 1.0f)
        {
            state.distance = state.targetDistance;
            state.distanceVelocity = 0.0f;
        }
        else
        {
            float shortestRemaining = state.targetDistance - state.distance;
            while (shortestRemaining > length * 0.5f) shortestRemaining -= length;
            while (shortestRemaining < -length * 0.5f) shortestRemaining += length;

            // A mola dirigida não pode interpretar um pequeno excesso depois
            // do alvo como uma nova volta inteira. A margem dinâmica cobre o
            // passo que seria percorrido neste frame e encaixa no mesmo ponto
            // em que a animação cartesiana do Legacy já estaria visualmente
            // concluída.
            const float settleDistance = std::clamp(
                std::abs(state.distanceVelocity) * dt * 1.15f,
                1.25f, 6.0f);
            if (std::abs(shortestRemaining) <= settleDistance)
            {
                state.distance = state.targetDistance;
                state.distanceVelocity = 0.0f;
                state.progress = 1.0f;
            }
            else
            {
                state.distance = SpringDirectedStyled(state.distance,
                    state.targetDistance, length, dt, direction,
                    state.distanceVelocity, stiffness, damping);

                float remainingAfter = state.targetDistance - state.distance;
                while (remainingAfter > length * 0.5f) remainingAfter -= length;
                while (remainingAfter < -length * 0.5f) remainingAfter += length;
                if (std::abs(remainingAfter) <= 1.25f)
                {
                    state.distance = state.targetDistance;
                    state.distanceVelocity = 0.0f;
                    state.progress = 1.0f;
                }
            }
        }
        while (state.distance < 0.0f) state.distance += length;
        while (state.distance >= length) state.distance -= length;
        const ImVec2 circuitPosition = SampleFrameCircuit(state.distance);
        const float x = circuitPosition.x - center.x;
        const float y = circuitPosition.y - center.y;
        const float circuitAngle = std::atan2(y, x);
        const float shapeAngle = leftSide
            ? circuitAngle - Track::RadialRotation()
            : 3.141592654f - circuitAngle - Track::RadialRotation();
        const ImVec2 shapedPoint = RadialShape::PositionAtAngles(
            radialShape, center, radius, shapeAngle, circuitAngle);
        const float radialDifference = Track::Distance(circuitPosition, shapedPoint);
        // A aparência muda somente ao cruzar efetivamente entre o arco
        // principal e o trilho externo. A interpolação visual já é feita por
        // radialSizeT, como no Legacy; interpolar também pela distância fazia
        // o ícone crescer durante todo o percurso.
        state.mainBlend = radialDifference <= 3.5f ? 1.0f : 0.0f;
        state.styleTransition = std::min(
            1.0f, state.styleTransition + dt * 3.8f);
        const float wave = std::sin(state.styleTransition * 3.141592654f);
        float radialOffset = 0.0f;
        float tangentOffset = 0.0f;
        switch (style)
        {
        case RadialAnimation::Style::CycloidalGear:
            radialOffset = (state.mainBlend > 0.5f ? 10.0f : 18.0f) * wave;
            tangentOffset = (leftSide ? -10.0f : 10.0f) * wave;
            break;
        case RadialAnimation::Style::OrbitalTransfer:
            radialOffset = (state.mainBlend > 0.5f ? 24.0f : 54.0f) * wave;
            break;
        case RadialAnimation::Style::FluidVortex:
            radialOffset = -(state.mainBlend > 0.5f ? 34.0f : 22.0f) * wave;
            tangentOffset = (leftSide ? -16.0f : 16.0f) * wave;
            break;
        default:
            break;
        }
        const float radialLength = std::max(std::sqrt(x * x + y * y), 0.001f);
        const ImVec2 radial(x / radialLength, y / radialLength);
        const ImVec2 tangent(-radial.y, radial.x);
        state.position = ImVec2(
            circuitPosition.x + radial.x * radialOffset + tangent.x * tangentOffset,
            circuitPosition.y + radial.y * radialOffset + tangent.y * tangentOffset);
        return state.position;
    }

    ImVec2 MainTarget(const ImVec2& center, float radius, bool leftSide,
        RadialShape::Style radialShape, int index, int count)
    {
        const auto circuit = Track::Circuit(center, radius, leftSide, radialShape);
        std::vector<ImVec2> mainRail;
        mainRail.reserve(circuit.size());
        for (const ImVec2& point : circuit)
        {
            const float dx = point.x - center.x;
            const float dy = point.y - center.y;
            const float angle = std::atan2(dy, dx);
            const float shapeAngle = leftSide
                ? angle - Track::RadialRotation()
                : 3.141592654f - angle - Track::RadialRotation();
            if (Track::Distance(point, RadialShape::PositionAtAngles(
                radialShape, center, radius, shapeAngle, angle)) <= 0.75f)
                mainRail.push_back(point);
        }
        if (!mainRail.empty())
        {
            const float t = count > 1
                ? static_cast<float>(std::clamp(index, 0, count - 1)) /
                    static_cast<float>(count - 1)
                : 0.5f;
            const std::size_t sample = static_cast<std::size_t>(std::lround(
                t * static_cast<float>(mainRail.size() - 1)));
            return mainRail[std::min(sample, mainRail.size() - 1)];
        }
        const ImVec2 entry = Track::GatePositions(
            Track::GateKind::Entry, 0, center, radius, leftSide, radialShape).first;
        const ImVec2 exit = Track::GatePositions(
            Track::GateKind::Exit, 0, center, radius, leftSide, radialShape).first;
        const float entryAngle = std::atan2(
            entry.y - center.y, entry.x - center.x);
        const float exitAngle = std::atan2(
            exit.y - center.y, exit.x - center.x);

        // This is deliberately the same arc convention used by the editor
        // preview: start at the exit and distribute towards the entry in the
        // saved flow direction. Reconstructing it from outline point order
        // could choose the complementary (short) arc and bunch every item at
        // one gate. Mirroring the right wheel reverses angular orientation.
        const int direction = Track::FlowDirection() * (leftSide ? 1 : -1);
        float delta = exitAngle - entryAngle;
        if (direction > 0)
            while (delta < 0.0f) delta += 6.283185307f;
        else
            while (delta > 0.0f) delta -= 6.283185307f;
        const float t = count > 1
            ? static_cast<float>(std::clamp(index, 0, count - 1)) /
                static_cast<float>(count - 1)
            : 0.5f;
        const float angle = exitAngle - delta * t;
        const float shapeAngle = leftSide
            ? angle - Track::RadialRotation()
            : 3.141592654f - angle - Track::RadialRotation();
        return RadialShape::PositionAtAngles(
            radialShape, center, radius, shapeAngle, angle);
    }

    ImVec2 Update(State& state, const ImVec2& current, const ImVec2& target,
        const ImVec2& center, float radius, bool leftSide,
        RadialShape::Style radialShape, OverflowMechanism::Layer layer,
        int routeIndex, int rotationDirection, float deltaTime)
    {
        auto paths = Track::GuidePaths(center, radius, leftSide);
        if (paths.empty() || paths.front().size() < 2) return target;
        const auto& path = paths[static_cast<std::size_t>(
            std::abs(routeIndex) % static_cast<int>(paths.size()))];
        const float length = Track::PolylineLength(path);
        auto mainPath = RadialShape::Outline(radialShape, center, radius, leftSide, 128);
        if (mainPath.empty()) return target;
        mainPath.push_back(mainPath.front());
        const float mainLength = Track::PolylineLength(mainPath);
        const float dt = std::clamp(deltaTime, 0.0f, 1.0f / 30.0f);
        const float targetDistance = ClosestDistance(path, target);
        const float mainTargetDistance = ClosestDistance(mainPath, target);
        const auto entryPositions = Track::GatePositions(
            Track::GateKind::Entry, routeIndex, center, radius, leftSide, radialShape);
        const auto exitPositions = Track::GatePositions(
            Track::GateKind::Exit, routeIndex, center, radius, leftSide, radialShape);
        // O caminho externo substitui um trecho do radial: os itens deixam o
        // círculo pela saída vermelha e retornam pela entrada verde.
        const int logicalDirection = rotationDirection < 0 ? -1 : 1;
        const bool useExit = (layer == OverflowMechanism::Layer::Overflow) ==
            (logicalDirection > 0);
        const ImVec2 mainGatePoint = useExit
            ? exitPositions.first : entryPositions.first;
        const ImVec2 overflowGatePoint = useExit
            ? exitPositions.second : entryPositions.second;
        const float mainGate = ClosestDistance(mainPath, mainGatePoint);
        const float overflowGate = ClosestDistance(path, overflowGatePoint);
        const int flowDirection = Track::FlowDirection() * logicalDirection;
        const int trackDirection = logicalDirection;
        const float mainSpeed = std::max(120.0f, mainLength * 1.15f);
        const float trackSpeed = std::max(120.0f, length * 1.15f);

        if (!state.initialized)
        {
            state.layer = layer;
            state.distance = layer == OverflowMechanism::Layer::Overflow
                ? targetDistance : mainTargetDistance;
            state.position = layer == OverflowMechanism::Layer::Overflow
                ? Track::SamplePolyline(path, state.distance).position
                : Track::SamplePolyline(mainPath, state.distance).position;
            state.gateDistance = overflowGate;
            state.mainGateDistance = mainGate;
            state.distanceVelocity = 0.0f;
            state.phase = layer == OverflowMechanism::Layer::Overflow ? Phase::Track : Phase::Main;
            state.initialized = true;
            return state.position;
        }

        if (state.layer != layer)
        {
            state.layer = layer;
            state.start = state.position;
            state.gateDistance = overflowGate;
            state.mainGateDistance = mainGate;
            state.progress = 0.0f;
            state.distanceVelocity = 0.0f;
            if (layer == OverflowMechanism::Layer::Overflow)
            {
                state.distance = ClosestDistance(mainPath, state.position);
                state.phase = Phase::EnteringMainTrack;
            }
            else
            {
                state.distance = ClosestDistance(path, state.position);
                state.phase = Phase::LeavingTrack;
            }
        }

        if (state.phase == Phase::EnteringMainTrack)
        {
            state.distance = MoveDirected(state.distance, state.mainGateDistance,
                mainLength, mainSpeed * dt,
                flowDirection);
            state.position = Track::SamplePolyline(mainPath, state.distance).position;
            float delta = std::abs(state.distance - state.mainGateDistance);
            delta = std::min(delta, mainLength - delta);
            if (delta < 2.0f)
            {
                state.start = state.position;
                state.endpoint = Track::SamplePolyline(path, state.gateDistance).position;
                state.progress = 0.0f;
                state.phase = Phase::Entering;
            }
            return state.position;
        }

        if (state.phase == Phase::Entering)
        {
            state.progress = std::min(1.0f, state.progress + dt * 3.25f);
            state.position = SmoothCurve(state.start, state.endpoint, center, state.progress);
            if (state.progress >= 1.0f)
            {
                state.distance = state.gateDistance;
                state.phase = Phase::Track;
            }
            return state.position;
        }

        if (state.phase == Phase::Track)
        {
            state.distance = SpringDirected(state.distance, targetDistance, length,
                dt, trackDirection, state.distanceVelocity);
            state.position = Track::SamplePolyline(path, state.distance).position;
            return state.position;
        }

        if (state.phase == Phase::LeavingTrack)
        {
            state.distance = MoveDirected(state.distance, state.gateDistance, length,
                trackSpeed * dt, trackDirection);
            state.position = Track::SamplePolyline(path, state.distance).position;
            float delta = std::abs(state.distance - state.gateDistance);
            delta = std::min(delta, length - delta);
            if (delta < 2.0f)
            {
                state.start = state.position;
                state.endpoint = Track::SamplePolyline(mainPath,
                    state.mainGateDistance).position;
                state.progress = 0.0f;
                state.phase = Phase::Leaving;
            }
            return state.position;
        }

        if (state.phase == Phase::Leaving)
        {
            state.progress = std::min(1.0f, state.progress + dt * 3.25f);
            state.position = SmoothCurve(state.start, state.endpoint, center, state.progress);
            if (state.progress >= 1.0f)
            {
                state.distance = state.mainGateDistance;
                state.phase = Phase::Main;
            }
            return state.position;
        }

        state.distance = SpringDirected(state.distance, mainTargetDistance, mainLength,
            dt, flowDirection, state.distanceVelocity);
        state.position = Track::SamplePolyline(mainPath, state.distance).position;
        return state.position;
    }
}
