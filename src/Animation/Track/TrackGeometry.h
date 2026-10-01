#pragma once

#include "Animation/Track/TrackTypes.h"

#include <span>

namespace Track
{
    std::vector<ImVec2> SamplePieceLocal(PieceKind kind, int subdivisions = 28);
    ImVec2 TransformPoint(const Piece& piece, const ImVec2& local,
        const ImVec2& center, float radius);
    std::vector<ImVec2> SamplePiece(const Piece& piece,
        const ImVec2& center, float radius, int subdivisions = 28);
    Port GetPort(const Piece& piece, int portIndex,
        const ImVec2& center, float radius);
    float Distance(const ImVec2& a, const ImVec2& b);
    float PolylineLength(std::span<const ImVec2> points);
    Sample SamplePolyline(std::span<const ImVec2> points, float distance);
}
