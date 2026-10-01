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
        bool leftSide, Layer layer, float deltaTime, Style style)
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
            state.radius = targetRadius;
            state.initialized = true;
            return target;
        }

        const float targetMoveX = target.x - state.lastTarget.x;
        const float targetMoveY = target.y - state.lastTarget.y;
        if (targetMoveX * targetMoveX + targetMoveY * targetMoveY > 1.0f)
            state.transition = 0.0f;
        state.lastTarget = target;
        state.transition = std::min(1.0f, state.transition + dt * 3.8f);

        if (style == Style::ShortestPath)
        {
            constexpr float stiffness = 315.0f;
            constexpr float damping = 25.0f;
            state.velocity.x += ((target.x - state.position.x) * stiffness - state.velocity.x * damping) * dt;
            state.velocity.y += ((target.y - state.position.y) * stiffness - state.velocity.y * damping) * dt;
            state.position.x += state.velocity.x * dt;
            state.position.y += state.velocity.y * dt;
            return state.position;
        }

        targetAngle = ShortestAngle(state.angle, targetAngle);
        float desiredRadius = targetRadius;
        float angleStiffness = 190.0f;
        float angleDamping = 24.0f;
        float radiusStiffness = 170.0f;
        float radiusDamping = 22.0f;
        const float wave = std::sin(state.transition * kPi);
        const float direction = leftSide ? -1.0f : 1.0f;

        switch (style)
        {
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
