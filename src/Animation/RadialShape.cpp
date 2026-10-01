#include "PCH.h"
#include "Animation/RadialShape.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kOverflowAngleStep = kPi / 15.0f;
    constexpr float kOverflowBaseOffset = 65.0f;
    constexpr float kOverflowRingStep = 42.0f;

    float ShapeRadius(RadialShape::Style style, float radius, float angle)
    {
        switch (style)
        {
        case RadialShape::Style::HarmonicFlower:
            return radius * (1.0f + 0.085f * std::cos(angle * 4.0f));
        case RadialShape::Style::DualOrbit:
            return radius * (1.0f + 0.105f * std::cos(angle * 2.0f));
        case RadialShape::Style::Turbine:
            return radius * (1.0f + 0.075f * std::sin(angle * 3.0f));
        case RadialShape::Style::SpiralGalaxy:
        {
            float phase = std::fmod(angle + kPi * 0.5f + kPi * 2.0f, kPi * 2.0f);
            if (phase < 0.0f) phase += kPi * 2.0f;
            return radius * (0.90f + 0.20f * phase / (kPi * 2.0f));
        }
        case RadialShape::Style::PulsarCrown:
            return radius * (1.0f + 0.13f * std::cos(angle * 7.0f));
        case RadialShape::Style::LiquidDiamond:
        {
            const float c = std::abs(std::cos(angle));
            const float s = std::abs(std::sin(angle));
            const float superellipse = std::pow(
                std::pow(c, 4.0f) + std::pow(s, 4.0f), -0.25f);
            return radius * (0.91f + 0.12f * superellipse);
        }
        case RadialShape::Style::CometTail:
            return radius * (1.0f + 0.12f * std::cos(angle) +
                0.045f * std::sin(angle * 3.0f));
        case RadialShape::Style::RoseEngine:
            return radius * (1.0f + 0.14f * std::sin(angle * 5.0f) +
                0.035f * std::cos(angle * 10.0f));
        case RadialShape::Style::QuantumRipple:
            return radius * (1.0f + 0.075f * std::sin(angle * 6.0f) +
                0.055f * std::cos(angle * 11.0f));
        case RadialShape::Style::Star:
        {
            float phase = std::fmod(angle + kPi * 0.5f + kPi * 2.0f, kPi * 2.0f);
            if (phase < 0.0f) phase += kPi * 2.0f;
            const float vertex = phase / (kPi / 5.0f);
            const int segment = static_cast<int>(std::floor(vertex));
            const float local = vertex - static_cast<float>(segment);
            const float from = (segment % 2 == 0) ? 1.0f : 0.56f;
            const float to = (segment % 2 == 0) ? 0.56f : 1.0f;
            return radius * (from + (to - from) * local);
        }
        default:
            return radius;
        }
    }

    ImVec2 PositionAt(RadialShape::Style style, const ImVec2& center,
        float radius, float angle)
    {
        const float shapedRadius = ShapeRadius(style, radius, angle);
        return ImVec2(center.x + std::cos(angle) * shapedRadius,
            center.y + std::sin(angle) * shapedRadius);
    }
}

namespace RadialShape
{
    ImVec2 PositionAtAngle(Style style, const ImVec2& center,
        float radius, float angle)
    {
        return PositionAt(style, center, radius, angle);
    }

    ImVec2 PositionAtAngles(Style style, const ImVec2& center,
        float radius, float shapeAngle, float directionAngle)
    {
        const float shapedRadius = ShapeRadius(style, radius, shapeAngle);
        return ImVec2(center.x + std::cos(directionAngle) * shapedRadius,
            center.y + std::sin(directionAngle) * shapedRadius);
    }

    const char* Name(Style style)
    {
        switch (style)
        {
        case Style::HarmonicFlower: return "Harmonic Flower";
        case Style::DualOrbit: return "Dual Orbit";
        case Style::Turbine: return "Turbine";
        case Style::SpiralGalaxy: return "Spiral Galaxy";
        case Style::PulsarCrown: return "Pulsar Crown";
        case Style::LiquidDiamond: return "Liquid Diamond";
        case Style::CometTail: return "Comet Tail";
        case Style::RoseEngine: return "Rose Engine";
        case Style::QuantumRipple: return "Quantum Ripple";
        case Style::Star: return "Star";
        default: return "Classic Orbit";
        }
    }

    Style FromName(std::string_view name)
    {
        for (int i = 0; i < Count(); ++i)
        {
            const auto style = static_cast<Style>(i);
            if (name == Name(style)) return style;
        }
        return Style::ClassicOrbit;
    }

    ImVec2 MainPosition(Style style, const ImVec2& center, float radius,
        int index, int count, bool leftSide)
    {
        const int safeCount = std::max(count, 1);
        float angle = -kPi * 0.5f +
            kPi * 2.0f * static_cast<float>(index) / static_cast<float>(safeCount);
        if (leftSide) angle = kPi - angle;
        return PositionAt(style, center, radius, angle);
    }

    ImVec2 OverflowPosition(Style style, const ImVec2& center, float radius,
        int overflowIndex, bool leftSide)
    {
        overflowIndex = std::max(overflowIndex, 0);
        float angleStep = kOverflowAngleStep;
        float radialAdvance = 0.0f;
        switch (style)
        {
        case Style::HarmonicFlower:
            angleStep = kPi / 16.0f;
            radialAdvance = 1.25f;
            break;
        case Style::DualOrbit:
            angleStep = kPi / 12.0f;
            radialAdvance = (overflowIndex % 2 == 0) ? -8.0f : 8.0f;
            break;
        case Style::Turbine:
            angleStep = kPi / 13.0f;
            radialAdvance = 0.9f * static_cast<float>(overflowIndex % 13);
            break;
        case Style::SpiralGalaxy:
            angleStep = kPi / 10.0f;
            radialAdvance = 3.25f * static_cast<float>(overflowIndex % 20);
            break;
        case Style::PulsarCrown:
            angleStep = kPi / 14.0f;
            radialAdvance = 1.8f * static_cast<float>(overflowIndex % 7);
            break;
        case Style::LiquidDiamond:
            angleStep = kPi / 16.0f;
            radialAdvance = (overflowIndex % 4) * 3.0f;
            break;
        case Style::CometTail:
            angleStep = kPi / 11.0f;
            radialAdvance = 2.4f * static_cast<float>(overflowIndex % 11);
            break;
        case Style::RoseEngine:
            angleStep = kPi / 15.0f;
            radialAdvance = (overflowIndex % 5) * 4.0f;
            break;
        case Style::QuantumRipple:
            angleStep = kPi / 18.0f;
            radialAdvance = 6.0f * std::sin(static_cast<float>(overflowIndex) * 1.7f);
            break;
        case Style::Star:
            angleStep = kPi / 15.0f;
            radialAdvance = (overflowIndex % 2 == 0) ? 2.0f : 10.0f;
            break;
        default:
            break;
        }
        const int itemsPerRing = std::max(1,
            static_cast<int>(std::floor(kPi * 2.0f / angleStep)));
        const int ring = overflowIndex / itemsPerRing;
        const int indexInRing = overflowIndex % itemsPerRing;
        const float overflowRadius = radius + kOverflowBaseOffset + radialAdvance +
            static_cast<float>(ring) * kOverflowRingStep;
        const float direction = leftSide ? -1.0f : 1.0f;
        const float angle = -kPi * 0.5f +
            direction * static_cast<float>(indexInRing) * angleStep;
        return PositionAt(style, center, overflowRadius, angle);
    }

    std::vector<ImVec2> Outline(Style style, const ImVec2& center,
        float radius, bool leftSide, int samples)
    {
        samples = std::max(samples, 12);
        std::vector<ImVec2> points;
        points.reserve(static_cast<std::size_t>(samples));
        for (int i = 0; i < samples; ++i)
        {
            float angle = -kPi * 0.5f +
                kPi * 2.0f * static_cast<float>(i) / static_cast<float>(samples);
            if (leftSide) angle = kPi - angle;
            points.push_back(PositionAt(style, center, radius, angle));
        }
        return points;
    }
}
