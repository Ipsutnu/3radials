#include "PCH.h"
#include "Animation/Animation.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    float ShortestAngle(float from, float to)
    {
        float delta = to - from;
        while (delta > kPi) delta -= kPi * 2.0f;
        while (delta < -kPi) delta += kPi * 2.0f;
        return from + delta;
    }

    float DirectedAngle(float from, float to, int direction)
    {
        float delta = to - from;
        if (direction > 0)
            while (delta < 0.0f) delta += kPi * 2.0f;
        else
            while (delta > 0.0f) delta -= kPi * 2.0f;
        return from + delta;
    }

    void Spring(float& value, float& velocity, float target,
        float stiffness, float damping, float dt)
    {
        velocity += ((target - value) * stiffness - velocity * damping) * dt;
        value += velocity * dt;
    }
}

namespace RadialAnimation
{
    const char* Name(Style style)
    {
        switch (style)
        {
        case Style::SimpleRadial: return "Simple Radial";
        case Style::MagneticDetent: return "Magnetic Detent";
        case Style::CycloidalGear: return "Cycloidal Gear";
        case Style::OrbitalTransfer: return "Orbital Transfer";
        case Style::FluidVortex: return "Fluid Vortex";
        default: return "Shortest Path";
        }
    }

    Style FromName(std::string_view name)
    {
        for (int i = 0; i < Count(); ++i)
        {
            const auto style = static_cast<Style>(i);
            if (name == Name(style)) return style;
        }
        return Style::ShortestPath;
    }

    ImVec2 Update(State& state, const ImVec2& target, const ImVec2& center,
        bool leftSide, Layer layer, float deltaTime, Style style,
        int directedRotation)
    {
        const float dt = std::clamp(deltaTime, 0.0f, 1.0f / 30.0f);
        const float targetDx = target.x - center.x;
        const float targetDy = target.y - center.y;
        float targetAngle = std::atan2(targetDy, targetDx);
        const float targetRadius = std::sqrt(targetDx * targetDx + targetDy * targetDy);

        if (!state.initialized)
        {
            state.position = target;
            state.lastTarget = target;
            state.angle = targetAngle;
            state.targetAngle = targetAngle;
            state.radius = targetRadius;
            state.initialized = true;
            return target;
        }

        const float targetMoveX = target.x - state.lastTarget.x;
        const float targetMoveY = target.y - state.lastTarget.y;
        const bool targetMoved =
            targetMoveX * targetMoveX + targetMoveY * targetMoveY > 1.0f;
        if (targetMoved)
            state.transition = 0.0f;
        state.lastTarget = target;
        state.transition = std::min(1.0f, state.transition + dt * 3.8f);

        const bool directed = directedRotation != 0;
        if (style == Style::ShortestPath && !directed)
        {
            constexpr float stiffness = 315.0f;
            constexpr float damping = 25.0f;
            state.velocity.x += ((target.x - state.position.x) * stiffness - state.velocity.x * damping) * dt;
            state.velocity.y += ((target.y - state.position.y) * stiffness - state.velocity.y * damping) * dt;
            state.position.x += state.velocity.x * dt;
            state.position.y += state.velocity.y * dt;
            return state.position;
        }

        // The legacy animation normally uses the shortest arc. During a
        // rapid wheel sequence, however, identical first/final slots may
        // represent a whole revolution. Preserve the endpoint unwrapped so
        // that it cannot turn around just because its screen point repeats.
        if (targetMoved)
        {
            if (directed)
            {
                // A ShortestPath state may have spent earlier frames in
                // Cartesian space. Synchronize its polar state once before
                // beginning a directed side-radial motion.
                if (style == Style::ShortestPath)
                {
                    const float x = state.position.x - center.x;
                    const float y = state.position.y - center.y;
                    state.angle = std::atan2(y, x);
                    state.radius = std::sqrt(x * x + y * y);
                    state.angularVelocity = 0.0f;
                    state.radialVelocity = 0.0f;
                    state.targetAngle = state.angle;
                }
                state.targetAngle = DirectedAngle(
                    state.targetAngle, targetAngle, directedRotation);
            }
            else
            {
                state.targetAngle = ShortestAngle(state.angle, targetAngle);
            }
        }
        targetAngle = state.targetAngle;
        float desiredRadius = targetRadius;
        float angleStiffness = 190.0f;
        float angleDamping = 24.0f;
        float radiusStiffness = 170.0f;
        float radiusDamping = 22.0f;
        const float wave = std::sin(state.transition * kPi);
        const float direction = leftSide ? -1.0f : 1.0f;

        switch (style)
        {
        case Style::ShortestPath:
            // Same response as the Cartesian legacy path, but in a directed
            // polar arc only while rapid scroll needs to preserve a full turn.
            angleStiffness = 315.0f;
            angleDamping = 25.0f;
            radiusStiffness = 315.0f;
            radiusDamping = 25.0f;
            break;
        case Style::SimpleRadial:
            // A mesma resposta firme do Shortest Path, porém em coordenadas
            // polares: alcança o destino pelo menor arco e nunca corta o
            // interior do radial.
            angleStiffness = 315.0f;
            angleDamping = 25.0f;
            radiusStiffness = 315.0f;
            radiusDamping = 25.0f;
            break;
        case Style::MagneticDetent:
        {
            const float remaining = std::abs(targetAngle - state.angle);
            angleStiffness = remaining < 0.12f ? 360.0f : 135.0f;
            angleDamping = remaining < 0.12f ? 30.0f : 20.0f;
            break;
        }
        case Style::CycloidalGear:
            desiredRadius += (layer == Layer::Overflow ? 18.0f : 10.0f) * wave;
            targetAngle += direction * 0.10f * wave;
            angleStiffness = 165.0f;
            break;
        case Style::OrbitalTransfer:
            desiredRadius += (layer == Layer::Overflow ? 54.0f : 24.0f) * wave;
            angleStiffness = 120.0f;
            radiusStiffness = 115.0f;
            break;
        case Style::FluidVortex:
            desiredRadius -= (layer == Layer::Overflow ? 22.0f : 34.0f) * wave;
            targetAngle += direction * 0.16f * wave;
            angleStiffness = 145.0f;
            radiusStiffness = 125.0f;
            break;
        default: break;
        }

        Spring(state.angle, state.angularVelocity, targetAngle,
            angleStiffness, angleDamping, dt);
        Spring(state.radius, state.radialVelocity, desiredRadius,
            radiusStiffness, radiusDamping, dt);
        state.position = ImVec2(
            center.x + std::cos(state.angle) * state.radius,
            center.y + std::sin(state.angle) * state.radius);
        return state.position;
    }
}
