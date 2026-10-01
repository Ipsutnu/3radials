#pragma once

#include <imgui.h>

#include <cstdint>
#include <vector>

namespace Track
{
    enum class PieceKind : std::uint8_t
    {
        Straight,
        ArcUp,
        ArcDown,
        SCurve,
        WideArcUp,
        WideArcDown,
        Wave,
        SoftZigzag,
        HookUp,
        HookDown,
        Bulge,
        CircleLoop,
        FreeDraw,
        HalfCircle,
        Spiral,
        RightAngle,
        Valley,
        ZPath,
        Hairpin
    };

    struct Piece
    {
        int id{ 0 };
        PieceKind kind{ PieceKind::Straight };
        ImVec2 position{};          // normalized around the radial center
        float rotation{};          // radians
        float scale{ 1.0f };
        bool mirrored{ false };
        std::vector<ImVec2> customPoints;
    };

    enum class GateKind : std::uint8_t { Entry, Exit };

    struct Gate
    {
        int id{};
        GateKind kind{ GateKind::Entry };
        int pieceID{};
        int portIndex{};
        float mainAngle{};
        float trackT{ -1.0f };
    };

    // Ligação explícita entre duas extremidades de peças.  Diferente de um
    // Gate, esta conexão não toca o radial principal: ela apenas prolonga o
    // trilho de uma peça até a extremidade de outra.
    struct Connection
    {
        int id{};
        GateKind sourceKind{ GateKind::Exit };
        int pieceA{};
        int portA{};
        int pieceB{};
        int portB{};
    };

    struct RadialErase
    {
        float angle{};
        float size{ 0.12f };
        float hardness{ 0.75f };
    };

    struct OverflowErase
    {
        ImVec2 position{}; // normalized against radial center/radius
        float size{ 0.12f };
        float hardness{ 0.75f };
    };

    struct Layout
    {
        std::vector<Piece> pieces;
        std::vector<Gate> gates;
        std::vector<Connection> connections;
        int nextID{ 1 };
        int nextGateID{ 1 };
        int nextConnectionID{ 1 };
        int flowDirection{ 1 };
        float lineOpacity{ 55.0f };
        float radialLineOpacity{ 100.0f };
        float radialRotation{};
        std::vector<RadialErase> radialErases;
        std::vector<OverflowErase> overflowErases;
    };

    struct Port
    {
        int pieceID{};
        int index{};
        ImVec2 position{};
    };

    struct Sample
    {
        ImVec2 position{};
        ImVec2 tangent{ 1.0f, 0.0f };
    };
}
