#include "PCH.h"
#include "Animation/Track/TrackGeometry.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;
}

namespace Track
{
    std::vector<ImVec2> SamplePieceLocal(PieceKind kind, int subdivisions)
    {
        subdivisions = std::max(subdivisions, 4);
        std::vector<ImVec2> result;
        result.reserve(static_cast<std::size_t>(subdivisions + 1));
        for (int i = 0; i <= subdivisions; ++i)
        {
            const float t = static_cast<float>(i) / subdivisions;
            if (kind == PieceKind::CircleLoop)
            {
                constexpr float terminalGap = 0.14f;
                const float angle = -kPi * 0.5f + terminalGap +
                    (kPi * 2.0f - terminalGap * 2.0f) * t;
                result.emplace_back(std::cos(angle) * 0.62f,
                    std::sin(angle) * 0.62f);
                continue;
            }
            const float x = -0.70f + 1.40f * t;
            float shapedX = x;
            float y = 0.0f;
            switch (kind)
            {
            case PieceKind::ArcUp:
                y = -0.52f * std::sin(kPi * t);
                break;
            case PieceKind::ArcDown:
                y = 0.52f * std::sin(kPi * t);
                break;
            case PieceKind::SCurve:
                y = 0.32f * std::sin(kPi * 2.0f * t);
                break;
            case PieceKind::WideArcUp:
                y = -0.82f * std::sin(kPi * t);
                break;
            case PieceKind::WideArcDown:
                y = 0.82f * std::sin(kPi * t);
                break;
            case PieceKind::Wave:
                y = 0.24f * std::sin(kPi * 4.0f * t);
                break;
            case PieceKind::SoftZigzag:
                y = 0.34f * std::sin(kPi * 3.0f * t);
                break;
            case PieceKind::HookUp:
                y = -0.55f * t * std::sin(kPi * t);
                break;
            case PieceKind::HookDown:
                y = 0.55f * t * std::sin(kPi * t);
                break;
            case PieceKind::Bulge:
                y = 0.42f * std::sin(kPi * t) * std::sin(kPi * 2.0f * t);
                break;
            case PieceKind::HalfCircle:
            {
                const float angle = -kPi * 0.5f + kPi * t;
                shapedX = 0.68f * std::cos(angle) - 0.18f;
                y = 0.68f * std::sin(angle);
                break;
            }
            case PieceKind::Spiral:
            {
                const float angle = -kPi + kPi * 4.5f * t;
                const float spiralRadius = 0.74f * (1.0f - t) + 0.06f;
                shapedX = spiralRadius * std::cos(angle);
                y = spiralRadius * std::sin(angle);
                break;
            }
            case PieceKind::RightAngle:
                if (t < 0.58f)
                {
                    shapedX = -0.70f + 1.25f * (t / 0.58f);
                    y = -0.52f;
                }
                else
                {
                    shapedX = 0.55f;
                    y = -0.52f + 1.12f * ((t - 0.58f) / 0.42f);
                }
                break;
            case PieceKind::Valley:
                y = -0.24f + 0.82f * std::sin(kPi * t);
                break;
            case PieceKind::ZPath:
                if (t < 0.25f)
                {
                    shapedX = -0.70f + 1.40f * (t / 0.25f);
                    y = -0.52f;
                }
                else if (t < 0.75f)
                {
                    const float u = (t - 0.25f) / 0.50f;
                    shapedX = 0.70f - 1.40f * u;
                    y = -0.52f + 1.04f * u;
                }
                else
                {
                    shapedX = -0.70f + 1.40f * ((t - 0.75f) / 0.25f);
                    y = 0.52f;
                }
                break;
            case PieceKind::Hairpin:
                if (t < 0.22f)
                {
                    shapedX = 0.70f - 0.92f * (t / 0.22f);
                    y = -0.48f;
                }
                else if (t < 0.78f)
                {
                    const float u = (t - 0.22f) / 0.56f;
                    const float angle = -kPi * 0.5f - kPi * u;
                    shapedX = -0.22f + 0.48f * std::cos(angle);
                    y = 0.48f * std::sin(angle);
                }
                else
                {
                    shapedX = -0.22f + 0.92f * ((t - 0.78f) / 0.22f);
                    y = 0.48f;
                }
                break;
            default:
                break;
            }
            result.emplace_back(shapedX, y);
        }
        return result;
    }

    ImVec2 TransformPoint(const Piece& piece, const ImVec2& local,
        const ImVec2& center, float radius)
    {
        const float c = std::cos(piece.rotation);
        const float s = std::sin(piece.rotation);
        const float x = local.x * piece.scale;
        const float y = local.y * piece.scale * (piece.mirrored ? -1.0f : 1.0f);
        return ImVec2(center.x + (piece.position.x + x * c - y * s) * radius,
            center.y + (piece.position.y + x * s + y * c) * radius);
    }

    std::vector<ImVec2> SamplePiece(const Piece& piece,
        const ImVec2& center, float radius, int subdivisions)
    {
        auto points = piece.kind == PieceKind::FreeDraw && piece.customPoints.size() >= 2
            ? piece.customPoints : SamplePieceLocal(piece.kind, subdivisions);
        for (auto& point : points)
            point = TransformPoint(piece, point, center, radius);
        return points;
    }

    Port GetPort(const Piece& piece, int portIndex,
        const ImVec2& center, float radius)
    {
        const bool sampledEndpoints = piece.kind == PieceKind::HalfCircle ||
            piece.kind == PieceKind::Spiral || piece.kind == PieceKind::RightAngle ||
            piece.kind == PieceKind::Valley || piece.kind == PieceKind::ZPath ||
            piece.kind == PieceKind::Hairpin;
        const ImVec2 local = piece.kind == PieceKind::FreeDraw && piece.customPoints.size() >= 2
            ? (portIndex == 0 ? piece.customPoints.front() : piece.customPoints.back())
            : sampledEndpoints
                ? [&] {
                    const auto sampled = SamplePieceLocal(piece.kind, 64);
                    return portIndex == 0 ? sampled.front() : sampled.back();
                }()
            : piece.kind == PieceKind::CircleLoop
                ? [&] {
                    constexpr float terminalGap = 0.14f;
                    const float angle = portIndex == 0
                        ? -kPi * 0.5f + terminalGap
                        : -kPi * 0.5f + kPi * 2.0f - terminalGap;
                    return ImVec2(std::cos(angle) * 0.62f,
                        std::sin(angle) * 0.62f);
                }()
            : (portIndex == 0 ? ImVec2(-0.70f, 0.0f) : ImVec2(0.70f, 0.0f));
        return Port{ piece.id, portIndex, TransformPoint(piece, local, center, radius) };
    }

    float Distance(const ImVec2& a, const ImVec2& b)
    {
        const float x = a.x - b.x;
        const float y = a.y - b.y;
        return std::sqrt(x * x + y * y);
    }

    float PolylineLength(std::span<const ImVec2> points)
    {
        float length = 0.0f;
        for (std::size_t i = 1; i < points.size(); ++i)
            length += Distance(points[i - 1], points[i]);
        return length;
    }

    Sample SamplePolyline(std::span<const ImVec2> points, float distance)
    {
        if (points.empty()) return {};
        if (points.size() == 1) return { points.front(), ImVec2(1.0f, 0.0f) };
        const float length = PolylineLength(points);
        if (length <= 0.001f) return { points.front(), ImVec2(1.0f, 0.0f) };
        distance = std::fmod(distance, length);
        if (distance < 0.0f) distance += length;
        for (std::size_t i = 1; i < points.size(); ++i)
        {
            const ImVec2 a = points[i - 1];
            const ImVec2 b = points[i];
            const float segment = Distance(a, b);
            if (distance <= segment || i + 1 == points.size())
            {
                const float t = segment > 0.001f ? std::clamp(distance / segment, 0.0f, 1.0f) : 0.0f;
                const ImVec2 tangent((b.x - a.x) / std::max(segment, 0.001f),
                    (b.y - a.y) / std::max(segment, 0.001f));
                return { ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t), tangent };
            }
            distance -= segment;
        }
        return { points.back(), ImVec2(1.0f, 0.0f) };
    }
}
