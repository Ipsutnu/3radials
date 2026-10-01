#include "PCH.h"
#include "Animation/Track/TrackLayout.h"
#include "Animation/Track/TrackGeometry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <unordered_map>
#include <ranges>

namespace
{
    Track::Layout g_saved;
    Track::Layout g_editing;
    std::uint64_t g_savedRevision{ 1 };
    constexpr float kSnapNormalized = 0.13f;

    Track::Layout BuiltInPreset(std::string_view name)
    {
        (void)name;
        constexpr float pi = 3.14159265358979323846f;
        Track::Layout legacy;
        Track::Piece ring;
        ring.id = legacy.nextID++;
        ring.kind = Track::PieceKind::CircleLoop;
        ring.scale = 2.18f;
        legacy.pieces.push_back(ring);
        constexpr float top = -pi * 0.5f;
        legacy.gates.push_back(Track::Gate{ legacy.nextGateID++,
            Track::GateKind::Exit, ring.id, 0, top - 0.14f });
        legacy.gates.push_back(Track::Gate{ legacy.nextGateID++,
            Track::GateKind::Entry, ring.id, 1, top + 0.14f });
        legacy.flowDirection = 1;
        legacy.lineOpacity = 100.0f;
        legacy.radialLineOpacity = 100.0f;
        return legacy;
    }

    struct Link { int piece{-1}; int port{-1}; };

    std::vector<std::array<Link, 2>> BuildLinks(const Track::Layout& layout)
    {
        std::vector<std::array<Link, 2>> links(layout.pieces.size(),
            std::array<Link, 2>{ Link{}, Link{} });
        const auto pieceIndex = [&](int id) {
            const auto found = std::ranges::find_if(layout.pieces,
                [id](const Track::Piece& piece) { return piece.id == id; });
            return found == layout.pieces.end() ? -1 :
                static_cast<int>(std::distance(layout.pieces.begin(), found));
        };
        // Conexões desenhadas pelo usuário têm prioridade sobre o encaixe
        // automático por proximidade.
        for (const Track::Connection& connection : layout.connections)
        {
            const int a = pieceIndex(connection.pieceA);
            const int b = pieceIndex(connection.pieceB);
            if (a < 0 || b < 0 || connection.portA < 0 || connection.portA > 1 ||
                connection.portB < 0 || connection.portB > 1 || a == b)
                continue;
            if (links[a][connection.portA].piece >= 0 ||
                links[b][connection.portB].piece >= 0)
                continue;
            links[a][connection.portA] = Link{ b, connection.portB };
            links[b][connection.portB] = Link{ a, connection.portA };
        }
        const ImVec2 origin(0.0f, 0.0f);
        constexpr float radius = 1.0f;
        for (std::size_t a = 0; a < layout.pieces.size(); ++a)
        {
            for (int pa = 0; pa < 2; ++pa)
            {
                if (links[a][pa].piece >= 0) continue;
                const auto portA = Track::GetPort(layout.pieces[a], pa, origin, radius);
                float best = kSnapNormalized;
                Link match{};
                for (std::size_t b = 0; b < layout.pieces.size(); ++b)
                {
                    for (int pb = 0; pb < 2; ++pb)
                    {
                        if (a == b && pa == pb) continue;
                        if (links[b][pb].piece >= 0) continue;
                        const auto portB = Track::GetPort(layout.pieces[b], pb, origin, radius);
                        const float distance = Track::Distance(portA.position, portB.position);
                        if (distance < best)
                        {
                            best = distance;
                            match = Link{ static_cast<int>(b), pb };
                        }
                    }
                }
                if (match.piece >= 0)
                {
                    links[a][pa] = match;
                    links[match.piece][match.port] = Link{
                        static_cast<int>(a), pa };
                }
            }
        }
        return links;
    }

    float ClosestPathT(const std::vector<ImVec2>& path, const ImVec2& point)
    {
        const float total = Track::PolylineLength(path);
        if (total <= 0.001f) return 0.0f;
        float along = 0.0f, bestAlong = 0.0f, bestSq = FLT_MAX;
        for (std::size_t i = 1; i < path.size(); ++i)
        {
            const ImVec2 a = path[i - 1], b = path[i];
            const float x = b.x - a.x, y = b.y - a.y;
            const float lengthSq = x * x + y * y;
            const float length = std::sqrt(lengthSq);
            const float t = lengthSq > 0.001f ? std::clamp(
                ((point.x - a.x) * x + (point.y - a.y) * y) / lengthSq, 0.0f, 1.0f) : 0.0f;
            const float px = a.x + x * t, py = a.y + y * t;
            const float dx = point.x - px, dy = point.y - py;
            const float sq = dx * dx + dy * dy;
            if (sq < bestSq) { bestSq = sq; bestAlong = along + length * t; }
            along += length;
        }
        return std::clamp(bestAlong / total, 0.0f, 1.0f);
    }

    const Track::Gate* GateAt(const Track::Layout& layout, int pieceID, int port)
    {
        const auto it = std::ranges::find_if(layout.gates, [&](const Track::Gate& gate) {
            return gate.trackT < 0.0f && gate.pieceID == pieceID && gate.portIndex == port;
        });
        return it == layout.gates.end() ? nullptr : &*it;
    }

    std::vector<ImVec2> BuildRoute(const Track::Layout& layout,
        const Track::Gate& startGate, const ImVec2& center, float radius,
        std::vector<bool>* visitedOut = nullptr,
        const Track::Gate** endGateOut = nullptr)
    {
        std::vector<ImVec2> path;
        const auto links = BuildLinks(layout);
        auto pieceIt = std::ranges::find_if(layout.pieces,
            [&](const Track::Piece& piece) { return piece.id == startGate.pieceID; });
        if (pieceIt == layout.pieces.end() || startGate.portIndex < 0 || startGate.portIndex > 1)
            return path;
        int pieceIndex = static_cast<int>(std::distance(layout.pieces.begin(), pieceIt));
        int incomingPort = startGate.portIndex;
        const Track::GateKind endKind = startGate.kind == Track::GateKind::Exit
            ? Track::GateKind::Entry : Track::GateKind::Exit;
        std::vector<bool> visited(layout.pieces.size(), false);
        for (std::size_t step = 0; step < layout.pieces.size(); ++step)
        {
            if (pieceIndex < 0 || visited[pieceIndex]) return {};
            visited[pieceIndex] = true;
            auto points = Track::SamplePiece(layout.pieces[pieceIndex], center, radius, 36);
            if (incomingPort == 1) std::reverse(points.begin(), points.end());
            if (!path.empty() && !points.empty()) points.erase(points.begin());
            path.insert(path.end(), points.begin(), points.end());
            const int outgoingPort = 1 - incomingPort;
            const auto* terminal = GateAt(layout, layout.pieces[pieceIndex].id, outgoingPort);
            if (terminal)
            {
                if (terminal->kind != endKind) return {};
                if (endGateOut) *endGateOut = terminal;
                if (visitedOut)
                    for (std::size_t i = 0; i < visited.size(); ++i)
                        (*visitedOut)[i] = (*visitedOut)[i] || visited[i];
                return path;
            }
            const Link next = links[pieceIndex][outgoingPort];
            if (next.piece < 0 || next.port < 0 ||
                GateAt(layout, layout.pieces[next.piece].id, next.port)) return {};
            pieceIndex = next.piece;
            incomingPort = next.port;
        }
        return {};
    }

    void AppendPoint(std::vector<ImVec2>& path, const ImVec2& point)
    {
        if (path.empty() || Track::Distance(path.back(), point) > 0.5f)
            path.push_back(point);
    }

    void AppendMainArc(std::vector<ImVec2>& path, const ImVec2& center,
        float radius, float from, float to, int direction,
        RadialShape::Style shape, float rotation, bool leftSide)
    {
        direction = direction < 0 ? -1 : 1;
        float delta = to - from;
        if (direction > 0)
            while (delta < 0.0f) delta += 6.283185307f;
        else
            while (delta > 0.0f) delta -= 6.283185307f;
        const int samples = std::max(2, static_cast<int>(
            std::ceil(std::abs(delta) * radius / 14.0f)));
        for (int i = 0; i <= samples; ++i)
        {
            const float angle = from + delta * static_cast<float>(i) / samples;
            const float shapeAngle = leftSide
                ? angle - rotation
                : 3.141592654f - angle - rotation;
            AppendPoint(path, RadialShape::PositionAtAngles(
                shape, center, radius, shapeAngle, angle));
        }
    }

    bool WriteLayoutFile(const std::filesystem::path& path,
        const Track::Layout& layout)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream file(path, std::ios::trunc);
        if (!file) return false;
        file << "WHEEL_TRACK_V8\n" << layout.nextID << ' '
            << layout.nextGateID << ' ' << layout.nextConnectionID << ' '
            << layout.flowDirection << ' '
            << layout.lineOpacity << ' ' << layout.radialLineOpacity << ' '
            << layout.radialRotation << '\n';
        for (const Track::Piece& piece : layout.pieces)
        {
            file << "P " << piece.id << ' ' << static_cast<int>(piece.kind) << ' '
                << piece.position.x << ' ' << piece.position.y << ' '
                << piece.rotation << ' ' << piece.scale << ' '
                << (piece.mirrored ? 1 : 0) << ' ' << piece.customPoints.size();
            for (const ImVec2& point : piece.customPoints)
                file << ' ' << point.x << ' ' << point.y;
            file << '\n';
        }
        for (const Track::Gate& gate : layout.gates)
            file << "G " << gate.id << ' ' << static_cast<int>(gate.kind) << ' '
                << gate.pieceID << ' ' << gate.portIndex << ' ' << gate.mainAngle
                << ' ' << gate.trackT << '\n';
        for (const Track::Connection& connection : layout.connections)
            file << "C " << connection.id << ' '
                << static_cast<int>(connection.sourceKind) << ' '
                << connection.pieceA << ' ' << connection.portA << ' '
                << connection.pieceB << ' ' << connection.portB << '\n';
        for (const Track::RadialErase& erase : layout.radialErases)
            file << "E " << erase.angle << ' ' << erase.size << ' '
                << erase.hardness << '\n';
        for (const Track::OverflowErase& erase : layout.overflowErases)
            file << "O " << erase.position.x << ' ' << erase.position.y << ' '
                << erase.size << ' ' << erase.hardness << '\n';
        return true;
    }

    bool ReadLayoutFile(const std::filesystem::path& path, Track::Layout& loaded)
    {
        std::ifstream file(path);
        std::string header;
        if (!(file >> header) || (header != "WHEEL_TRACK_V1" &&
            header != "WHEEL_TRACK_V2" && header != "WHEEL_TRACK_V3" &&
            header != "WHEEL_TRACK_V4" && header != "WHEEL_TRACK_V5" &&
            header != "WHEEL_TRACK_V6" && header != "WHEEL_TRACK_V7" &&
            header != "WHEEL_TRACK_V8")) return false;
        if (!(file >> loaded.nextID)) return false;
        if (header != "WHEEL_TRACK_V1")
        {
            if (!(file >> loaded.nextGateID)) return false;
            if ((header == "WHEEL_TRACK_V7" || header == "WHEEL_TRACK_V8") &&
                !(file >> loaded.nextConnectionID)) return false;
            if (!(file >> loaded.flowDirection)) return false;
            if ((header == "WHEEL_TRACK_V5" || header == "WHEEL_TRACK_V6" ||
                header == "WHEEL_TRACK_V7" || header == "WHEEL_TRACK_V8") &&
                !(file >> loaded.lineOpacity)) return false;
            if ((header == "WHEEL_TRACK_V6" || header == "WHEEL_TRACK_V7" ||
                header == "WHEEL_TRACK_V8") &&
                !(file >> loaded.radialLineOpacity >> loaded.radialRotation)) return false;
        }
        int kind{};
        if (header == "WHEEL_TRACK_V1")
        {
            Track::Piece piece;
            while (file >> piece.id >> kind >> piece.position.x >> piece.position.y >>
                piece.rotation >> piece.scale)
            {
                piece.kind = static_cast<Track::PieceKind>(std::clamp(kind, 0, 3));
                loaded.pieces.push_back(piece);
            }
            if (!loaded.pieces.empty())
            {
                loaded.gates.push_back(Track::Gate{ loaded.nextGateID++, Track::GateKind::Entry,
                    loaded.pieces.front().id, 0, -1.5707963268f });
                loaded.gates.push_back(Track::Gate{ loaded.nextGateID++, Track::GateKind::Exit,
                    loaded.pieces.back().id, 1, 1.5707963268f });
            }
        }
        else
        {
            char record{};
            while (file >> record)
            {
                if (record == 'P')
                {
                    std::size_t pointCount{};
                    Track::Piece value;
                    if (!(file >> value.id >> kind >> value.position.x >> value.position.y >>
                        value.rotation >> value.scale)) return false;
                    if (header == "WHEEL_TRACK_V4" || header == "WHEEL_TRACK_V5" ||
                        header == "WHEEL_TRACK_V6" || header == "WHEEL_TRACK_V7" ||
                        header == "WHEEL_TRACK_V8")
                    {
                        int mirrored{};
                        if (!(file >> mirrored)) return false;
                        value.mirrored = mirrored != 0;
                    }
                    if (!(file >> pointCount)) return false;
                    value.kind = static_cast<Track::PieceKind>(std::clamp(kind, 0, 18));
                    for (std::size_t i = 0; i < pointCount; ++i)
                    {
                        ImVec2 point;
                        if (!(file >> point.x >> point.y)) return false;
                        value.customPoints.push_back(point);
                    }
                    loaded.pieces.push_back(std::move(value));
                }
                else if (record == 'G')
                {
                    Track::Gate gate;
                    int gateKind{};
                    if (!(file >> gate.id >> gateKind >> gate.pieceID >> gate.portIndex >> gate.mainAngle))
                        return false;
                    if ((header == "WHEEL_TRACK_V3" || header == "WHEEL_TRACK_V4" ||
                        header == "WHEEL_TRACK_V5" || header == "WHEEL_TRACK_V6" ||
                        header == "WHEEL_TRACK_V7" || header == "WHEEL_TRACK_V8") &&
                        !(file >> gate.trackT)) return false;
                    gate.kind = gateKind == 0 ? Track::GateKind::Entry : Track::GateKind::Exit;
                    loaded.gates.push_back(gate);
                }
                else if (record == 'C')
                {
                    Track::Connection connection;
                    int sourceKind{};
                    if (!(file >> connection.id >> sourceKind >>
                        connection.pieceA >> connection.portA >>
                        connection.pieceB >> connection.portB)) return false;
                    connection.sourceKind = sourceKind == 0
                        ? Track::GateKind::Entry : Track::GateKind::Exit;
                    loaded.connections.push_back(connection);
                }
                else if (record == 'E')
                {
                    Track::RadialErase erase;
                    if (!(file >> erase.angle >> erase.size >> erase.hardness)) return false;
                    loaded.radialErases.push_back(erase);
                }
                else if (record == 'O')
                {
                    Track::OverflowErase erase;
                    if (!(file >> erase.position.x >> erase.position.y >>
                        erase.size >> erase.hardness)) return false;
                    loaded.overflowErases.push_back(erase);
                }
            }
        }
        if (header != "WHEEL_TRACK_V3" && header != "WHEEL_TRACK_V4" &&
            header != "WHEEL_TRACK_V5" && header != "WHEEL_TRACK_V6" &&
            header != "WHEEL_TRACK_V7" && header != "WHEEL_TRACK_V8")
        {
            const auto legacyPath = Track::BuildClosedPath(loaded, ImVec2{}, 1.0f);
            for (auto& gate : loaded.gates)
            {
                const auto piece = std::ranges::find_if(loaded.pieces,
                    [&](const Track::Piece& value) { return value.id == gate.pieceID; });
                if (piece != loaded.pieces.end() && !legacyPath.empty())
                    gate.trackT = ClosestPathT(legacyPath,
                        Track::GetPort(*piece, gate.portIndex, ImVec2{}, 1.0f).position);
            }
        }
        return true;
    }
}

namespace Track
{
    Layout& CustomLayout() { return g_editing; }
    const Layout& SavedLayout() { return g_saved; }
    std::filesystem::path Path()
    {
        return std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" /
            "3radials" / "radials" / "current.txt";
    }

    void BeginEdit()
    {
        g_editing = g_saved;
        // Primeiro uso: oferece um circuito pequeno já válido para que o
        // editor possa ser testado imediatamente e desmontado pelo usuário.
        if (g_editing.pieces.empty())
        {
            Piece upper;
            upper.id = g_editing.nextID++;
            upper.kind = PieceKind::ArcUp;
            upper.position = ImVec2(0.0f, -1.35f);
            g_editing.pieces = { upper };
            g_editing.gates.push_back(Gate{ g_editing.nextGateID++,
                GateKind::Entry, upper.id, 0, -1.5707963268f });
            g_editing.gates.push_back(Gate{ g_editing.nextGateID++,
                GateKind::Exit, upper.id, 1, -1.5707963268f });
        }
    }
    void CancelEdit() { g_editing = g_saved; }

    bool IsValid(const Layout& layout, std::string* reason)
    {
        if (layout.pieces.empty())
        {
            if (reason) *reason = "Add at least one piece";
            return false;
        }
        const auto links = BuildLinks(layout);
        std::vector<std::pair<int, int>> connectedPorts;
        connectedPorts.reserve(layout.connections.size() * 2);
        for (const Connection& connection : layout.connections)
        {
            const bool piecesExist = connection.pieceA != connection.pieceB &&
                std::ranges::any_of(layout.pieces, [&](const Piece& piece) {
                    return piece.id == connection.pieceA;
                }) &&
                std::ranges::any_of(layout.pieces, [&](const Piece& piece) {
                    return piece.id == connection.pieceB;
                });
            if (!piecesExist || connection.portA < 0 || connection.portA > 1 ||
                connection.portB < 0 || connection.portB > 1)
            {
                if (reason) *reason = "A radial terminal is not attached to a piece end";
                return false;
            }
            for (const auto endpoint : {
                std::pair{ connection.pieceA, connection.portA },
                std::pair{ connection.pieceB, connection.portB } })
            {
                if (std::ranges::find(connectedPorts, endpoint) != connectedPorts.end())
                {
                    if (reason) *reason = "A piece cannot have two equal terminals";
                    return false;
                }
                connectedPorts.push_back(endpoint);
            }
        }
        for (const Gate& gate : layout.gates)
        {
            if (gate.trackT >= 0.0f || gate.portIndex < 0 || gate.portIndex > 1 ||
                std::ranges::none_of(layout.pieces,
                    [&](const Piece& piece) { return piece.id == gate.pieceID; }))
            {
                if (reason) *reason = "A radial terminal is not attached to a piece end";
                return false;
            }
            if (std::ranges::find(connectedPorts,
                std::pair{ gate.pieceID, gate.portIndex }) != connectedPorts.end())
            {
                if (reason) *reason = "A piece cannot have two equal terminals";
                return false;
            }
        }
        for (const Piece& piece : layout.pieces)
        {
            for (const GateKind kind : { GateKind::Entry, GateKind::Exit })
            {
                const int count = static_cast<int>(std::ranges::count_if(layout.gates,
                    [&](const Gate& gate) { return gate.pieceID == piece.id && gate.kind == kind; }));
                if (count > 1)
                {
                    if (reason) *reason = "A piece cannot have two equal terminals";
                    return false;
                }
            }
        }
        for (std::size_t pieceIndex = 0; pieceIndex < layout.pieces.size(); ++pieceIndex)
        {
            for (int port = 0; port < 2; ++port)
            {
                const bool terminal = GateAt(layout, layout.pieces[pieceIndex].id, port) != nullptr;
                const Link link = links[pieceIndex][port];
                const bool connected = link.piece >= 0 && link.port >= 0 &&
                    links[link.piece][link.port].piece == static_cast<int>(pieceIndex) &&
                    links[link.piece][link.port].port == port;
                if (!terminal && !connected)
                {
                    if (reason) *reason = "Every free piece end needs a radial terminal";
                    return false;
                }
            }
        }
        const bool hasEntry = std::ranges::any_of(layout.gates,
            [](const Gate& gate) { return gate.kind == GateKind::Entry; });
        const bool hasExit = std::ranges::any_of(layout.gates,
            [](const Gate& gate) { return gate.kind == GateKind::Exit; });
        if (!hasEntry || !hasExit)
        {
            if (reason) *reason = "Add at least one entry and one exit";
            return false;
        }
        std::vector<bool> visited(layout.pieces.size(), false);
        for (const Gate& gate : layout.gates)
        {
            if (gate.kind == GateKind::Exit &&
                BuildRoute(layout, gate, ImVec2{}, 1.0f, &visited).empty())
            {
                if (reason) *reason = "Each exit must reach an entry through one continuous path";
                return false;
            }
        }
        if (!std::all_of(visited.begin(), visited.end(), [](bool value) { return value; }))
        {
            if (reason) *reason = "Every piece must belong to a radial entry/exit path";
            return false;
        }
        if (reason) reason->clear();
        return true;
    }

    std::vector<ImVec2> BuildClosedPath(const Layout& layout,
        const ImVec2& center, float radius)
    {
        auto paths = BuildPaths(layout, center, radius);
        return paths.empty() ? std::vector<ImVec2>{} : std::move(paths.front());
    }

    std::vector<std::vector<ImVec2>> BuildPaths(const Layout& layout,
        const ImVec2& center, float radius)
    {
        std::vector<std::vector<ImVec2>> paths;
        for (const Gate& gate : layout.gates)
        {
            if (gate.kind != GateKind::Exit) continue;
            auto path = BuildRoute(layout, gate, center, radius);
            if (path.size() > 1)
                paths.push_back(std::move(path));
        }
        return paths;
    }

    std::vector<ImVec2> BuildCircuit(const Layout& layout,
        const ImVec2& center, float radius, bool leftSide,
        RadialShape::Style shape)
    {
        struct Route
        {
            const Gate* exit{};
            const Gate* entry{};
            std::vector<ImVec2> points;
            bool used{};
        };
        std::vector<Route> routes;
        for (const Gate& gate : layout.gates)
        {
            if (gate.kind != GateKind::Exit) continue;
            const Gate* entry = nullptr;
            auto points = BuildRoute(layout, gate, center, radius, nullptr, &entry);
            if (entry && points.size() > 1)
                routes.push_back(Route{ &gate, entry, std::move(points), false });
        }
        if (routes.empty()) return {};

        const int direction = (layout.flowDirection < 0 ? -1 : 1) *
            (leftSide ? 1 : -1);
        const auto angleOf = [&](const Gate& gate) {
            float angle = gate.mainAngle + layout.radialRotation;
            if (!leftSide) angle = 3.141592654f - angle;
            return angle;
        };
        const auto mainPoint = [&](const Gate& gate) {
            return RadialShape::PositionAtAngles(shape, center, radius,
                gate.mainAngle, angleOf(gate));
        };
        if (!leftSide)
            for (auto& route : routes)
                for (auto& point : route.points)
                    point.x = center.x - (point.x - center.x);

        std::vector<ImVec2> circuit;
        std::size_t current = 0;
        for (std::size_t step = 0; step < routes.size(); ++step)
        {
            Route& route = routes[current];
            route.used = true;
            AppendPoint(circuit, mainPoint(*route.exit));
            for (const ImVec2& point : route.points) AppendPoint(circuit, point);
            AppendPoint(circuit, mainPoint(*route.entry));

            if (step + 1 == routes.size())
            {
                AppendMainArc(circuit, center, radius, angleOf(*route.entry),
                    angleOf(*routes.front().exit), direction, shape,
                    layout.radialRotation, leftSide);
                break;
            }

            std::size_t next = routes.size();
            float best = FLT_MAX;
            const float from = angleOf(*route.entry);
            for (std::size_t i = 0; i < routes.size(); ++i)
            {
                if (routes[i].used) continue;
                float delta = angleOf(*routes[i].exit) - from;
                if (direction > 0)
                    while (delta < 0.0f) delta += 6.283185307f;
                else
                    while (delta > 0.0f) delta -= 6.283185307f;
                const float distance = std::abs(delta);
                if (distance < best) { best = distance; next = i; }
            }
            if (next == routes.size()) break;
            AppendMainArc(circuit, center, radius, from,
                angleOf(*routes[next].exit), direction, shape,
                layout.radialRotation, leftSide);
            current = next;
        }
        if (circuit.size() > 1) AppendPoint(circuit, circuit.front());
        return circuit;
    }

    std::vector<ImVec2> Circuit(
        const ImVec2& center, float radius, bool leftSide,
        RadialShape::Style shape)
    {
        struct Cache
        {
            std::uint64_t revision{};
            ImVec2 center{};
            float radius{};
            RadialShape::Style shape{ RadialShape::Style::ClassicOrbit };
            std::vector<ImVec2> value;
        };
        static std::array<Cache, 2> caches;
        auto& cache = caches[leftSide ? 1u : 0u];
        if (cache.revision != g_savedRevision ||
            cache.center.x != center.x || cache.center.y != center.y ||
            cache.radius != radius || cache.shape != shape)
        {
            cache.revision = g_savedRevision;
            cache.center = center;
            cache.radius = radius;
            cache.shape = shape;
            cache.value = BuildCircuit(g_saved, center, radius, leftSide, shape);
        }
        return cache.value;
    }

    ImVec2 ProjectToCircuit(const ImVec2& point,
        const ImVec2& center, float radius, bool leftSide,
        RadialShape::Style shape)
    {
        const auto circuit = Circuit(center, radius, leftSide, shape);
        if (circuit.size() < 2) return point;
        const float distance = ClosestPathT(circuit, point) * PolylineLength(circuit);
        return SamplePolyline(circuit, distance).position;
    }

    std::vector<CircuitSlot> CircuitSlots(const Layout& layout,
        const ImVec2& center, float radius, bool leftSide,
        int mainCount, int overflowCount, RadialShape::Style shape)
    {
        struct PendingSlot
        {
            CircuitSlot slot;
            float distance{};
            bool terminal{};
        };
        struct OverflowCandidate
        {
            ImVec2 position{};
        };
        std::vector<PendingSlot> pending;
        mainCount = std::max(0, mainCount);
        overflowCount = std::max(0, overflowCount);
        pending.reserve(static_cast<std::size_t>(mainCount + overflowCount));

        // Sem excedentes não há motivo para abrir o circuito principal nos
        // terminais. Distribui todos os slots pelo contorno integral, já com
        // formato, rotação e espelhamento corretos.
        if (overflowCount == 0)
        {
            std::vector<CircuitSlot> radialOnly;
            radialOnly.reserve(static_cast<std::size_t>(mainCount));
            constexpr float twoPi = 6.283185307f;
            for (int i = 0; i < mainCount; ++i)
            {
                const float shapeAngle = mainCount > 0
                    ? twoPi * static_cast<float>(i) / static_cast<float>(mainCount)
                    : 0.0f;
                const float directionAngle = leftSide
                    ? shapeAngle + layout.radialRotation
                    : 3.141592654f - (shapeAngle + layout.radialRotation);
                radialOnly.push_back(CircuitSlot{
                    RadialShape::PositionAtAngles(shape, center, radius,
                        shapeAngle, directionAngle), true, i });
            }
            return radialOnly;
        }

        const auto circuit = BuildCircuit(layout, center, radius, leftSide, shape);
        const float circuitLength = PolylineLength(circuit);
        if (circuit.size() < 2 || circuitLength <= 0.001f) return {};

        // O trilho principal pode ser circular, floral, estrela etc. Um ponto
        // pertence a ele quando coincide com o contorno do formato no mesmo
        // ângulo; comparar apenas com o raio quebrava todos os formatos Custom.
        std::vector<ImVec2> circuitPoints = circuit;
        if (circuitPoints.size() > 1 &&
            Distance(circuitPoints.front(), circuitPoints.back()) <= 0.75f)
            circuitPoints.pop_back();

        std::vector<bool> isMainPoint;
        isMainPoint.reserve(circuitPoints.size());
        for (const ImVec2& point : circuitPoints)
        {
            const float x = point.x - center.x;
            const float y = point.y - center.y;
            const float angle = std::atan2(y, x);
            const float shapeAngle = leftSide
                ? angle - layout.radialRotation
                : 3.141592654f - angle - layout.radialRotation;
            const ImVec2 expected = RadialShape::PositionAtAngles(
                shape, center, radius, shapeAngle, angle);
            isMainPoint.push_back(Distance(point, expected) <= 0.75f);
        }

        // Separa cada arco contínuo do radial entre uma entrada e uma saída.
        // Distribuir cada arco pelo seu comprimento mantém os terminais exatos
        // sem substituir um slot já espaçado (a substituição era a origem dos
        // vãos grandes perto de algumas entradas).
        std::vector<std::vector<ImVec2>> mainSegments;
        const std::size_t pointCount = circuitPoints.size();
        if (pointCount > 0 && std::ranges::all_of(isMainPoint,
            [](bool value) { return value; }))
        {
            mainSegments.push_back(circuitPoints);
        }
        else if (pointCount > 0)
        {
            for (std::size_t start = 0; start < pointCount; ++start)
            {
                const std::size_t previous = (start + pointCount - 1) % pointCount;
                if (!isMainPoint[start] || isMainPoint[previous]) continue;
                std::vector<ImVec2> segment;
                std::size_t index = start;
                do
                {
                    if (!isMainPoint[index]) break;
                    AppendPoint(segment, circuitPoints[index]);
                    index = (index + 1) % pointCount;
                } while (index != start);
                if (!segment.empty()) mainSegments.push_back(std::move(segment));
            }
        }

        std::vector<ImVec2> mainRail;
        for (const auto& segment : mainSegments)
            for (const ImVec2& point : segment)
                AppendPoint(mainRail, point);

        std::vector<ImVec2> mainPositions;
        std::vector<bool> mainTerminals;
        mainPositions.reserve(static_cast<std::size_t>(mainCount));
        mainTerminals.reserve(static_cast<std::size_t>(mainCount));
        const int segmentCount = static_cast<int>(mainSegments.size());
        if (segmentCount > 0 && mainCount >= segmentCount * 2)
        {
            // Um segmento com N intervalos contém N+1 slots. Reservamos um
            // intervalo para cada arco e dividimos os restantes pela extensão
            // real de cada um, usando os maiores restos para fechar exatamente
            // mainCount slots.
            const int totalIntervals = mainCount - segmentCount;
            std::vector<int> intervals(static_cast<std::size_t>(segmentCount), 1);
            std::vector<float> lengths(static_cast<std::size_t>(segmentCount), 0.0f);
            std::vector<float> remainders(static_cast<std::size_t>(segmentCount), 0.0f);
            float totalLength = 0.0f;
            for (int i = 0; i < segmentCount; ++i)
            {
                lengths[static_cast<std::size_t>(i)] =
                    PolylineLength(mainSegments[static_cast<std::size_t>(i)]);
                totalLength += lengths[static_cast<std::size_t>(i)];
            }
            int remaining = totalIntervals - segmentCount;
            int assigned = 0;
            if (remaining > 0)
            {
                for (int i = 0; i < segmentCount; ++i)
                {
                    const float exact = totalLength > 0.001f
                        ? static_cast<float>(remaining) *
                            lengths[static_cast<std::size_t>(i)] / totalLength
                        : static_cast<float>(remaining) / segmentCount;
                    const int whole = static_cast<int>(std::floor(exact));
                    intervals[static_cast<std::size_t>(i)] += whole;
                    remainders[static_cast<std::size_t>(i)] = exact - whole;
                    assigned += whole;
                }
                for (int left = remaining - assigned; left > 0; --left)
                {
                    const auto best = static_cast<std::size_t>(std::distance(
                        remainders.begin(), std::max_element(
                            remainders.begin(), remainders.end())));
                    ++intervals[best];
                    remainders[best] = -1.0f;
                }
            }

            for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
            {
                const auto& segment = mainSegments[static_cast<std::size_t>(segmentIndex)];
                const float length = lengths[static_cast<std::size_t>(segmentIndex)];
                const int count = intervals[static_cast<std::size_t>(segmentIndex)];
                for (int slot = 0; slot <= count; ++slot)
                {
                    const float distance = count > 0
                        ? length * static_cast<float>(slot) / count : 0.0f;
                    // SamplePolyline trata a distância exatamente igual ao
                    // comprimento como volta ao início (comportamento correto
                    // para circuitos fechados). Este arco, porém, é aberto:
                    // seu último slot precisa ficar no segundo terminal.
                    mainPositions.push_back(slot == count
                        ? segment.back()
                        : SamplePolyline(segment, distance).position);
                    mainTerminals.push_back(slot == 0 || slot == count);
                }
            }
        }
        else
        {
            // Layouts extremos com mais pares de terminais que itens visíveis
            // mantêm a distribuição circular anterior como fallback seguro.
            mainTerminals.assign(static_cast<std::size_t>(mainCount), false);
            for (int i = 0; i < mainCount && !mainRail.empty(); ++i)
            {
                const float t = static_cast<float>(i) / mainCount;
                const std::size_t sample = static_cast<std::size_t>(std::floor(
                    t * static_cast<float>(mainRail.size())));
                mainPositions.push_back(
                    mainRail[std::min(sample, mainRail.size() - 1)]);
            }
        }
        for (std::size_t i = 0; i < mainPositions.size(); ++i)
        {
            pending.push_back({ CircuitSlot{ mainPositions[i], true, 0 },
                ClosestPathT(circuit, mainPositions[i]) * circuitLength,
                mainTerminals[i] });
        }

        auto paths = BuildPaths(layout, center, radius);
        if (!leftSide)
            for (auto& path : paths)
                for (auto& point : path)
                    point.x = center.x - (point.x - center.x);

        std::vector<OverflowCandidate> overflow;
        overflow.reserve(static_cast<std::size_t>(overflowCount));
        if (!paths.empty())
        {
            const int pathCount = static_cast<int>(paths.size());
            const int base = overflowCount / pathCount;
            const int remainder = overflowCount % pathCount;
            for (int pathIndex = 0; pathIndex < pathCount; ++pathIndex)
            {
                const int localCount = base + (pathIndex < remainder ? 1 : 0);
                const auto& path = paths[static_cast<std::size_t>(pathIndex)];
                const float length = PolylineLength(path);
                for (int localIndex = 0; localIndex < localCount; ++localIndex)
                {
                    // As duas pontas pertencem aos itens principais. Os
                    // excedentes usam somente o interior aberto da peça.
                    const float normalized = static_cast<float>(
                        localCount - localIndex) / static_cast<float>(localCount + 1);
                    const float distance = length * normalized;
                    overflow.push_back(OverflowCandidate{
                        SamplePolyline(path, distance).position });
                }
            }
        }

        for (const OverflowCandidate& item : overflow)
        {
            pending.push_back({ CircuitSlot{ item.position, false, 0 },
                ClosestPathT(circuit, item.position) * circuitLength, false });
        }
        std::ranges::sort(pending, [](const PendingSlot& a, const PendingSlot& b) {
            if (std::abs(a.distance - b.distance) > 0.01f)
                return a.distance < b.distance;
            return a.slot.main && !b.slot.main;
        });

        std::vector<CircuitSlot> result;
        result.reserve(pending.size());
        for (std::size_t i = 0; i < pending.size(); ++i)
        {
            pending[i].slot.ordinal = static_cast<int>(i);
            pending[i].slot.terminal = pending[i].terminal;
            pending[i].slot.circuitT = circuitLength > 0.001f
                ? pending[i].distance / circuitLength : 0.0f;
            result.push_back(pending[i].slot);
        }
        return result;
    }

    std::vector<CircuitSlot> CircuitSlots(const ImVec2& center, float radius,
        bool leftSide, int mainCount, int overflowCount,
        RadialShape::Style shape)
    {
        struct Cache
        {
            std::uint64_t revision{};
            ImVec2 center{};
            float radius{};
            int mainCount{};
            int overflowCount{};
            RadialShape::Style shape{ RadialShape::Style::ClassicOrbit };
            std::vector<CircuitSlot> value;
        };
        static std::array<Cache, 2> caches;
        auto& cache = caches[leftSide ? 1u : 0u];
        if (cache.revision != g_savedRevision ||
            cache.center.x != center.x || cache.center.y != center.y ||
            cache.radius != radius || cache.mainCount != mainCount ||
            cache.overflowCount != overflowCount || cache.shape != shape)
        {
            cache.revision = g_savedRevision;
            cache.center = center;
            cache.radius = radius;
            cache.mainCount = mainCount;
            cache.overflowCount = overflowCount;
            cache.shape = shape;
            cache.value = CircuitSlots(g_saved, center, radius, leftSide,
                mainCount, overflowCount, shape);
        }
        return cache.value;
    }

    ImVec2 OverflowPosition(const ImVec2& center, float radius,
        int index, int count, bool leftSide)
    {
        auto paths = BuildPaths(g_saved, center, radius);
        if (paths.empty()) return center;
        const int pathCount = static_cast<int>(paths.size());
        index = std::clamp(index, 0, std::max(0, count - 1));
        // Cada trilho recebe um bloco contíguo de itens. A distribuição
        // alternada antiga (A, B, A, B) fazia vizinhos lógicos saltarem entre
        // trajetórias distantes e permitia overflow -> overflow sem passar
        // pelo arco correspondente do radial principal.
        const int base = count / pathCount;
        const int remainder = count % pathCount;
        int pathIndex = 0;
        int blockStart = 0;
        for (; pathIndex < pathCount; ++pathIndex)
        {
            const int blockCount = base + (pathIndex < remainder ? 1 : 0);
            if (index < blockStart + blockCount) break;
            blockStart += blockCount;
        }
        pathIndex = std::min(pathIndex, pathCount - 1);
        auto& path = paths[static_cast<std::size_t>(pathIndex)];
        if (!leftSide)
            for (auto& point : path) point.x = center.x - (point.x - center.x);
        const float length = PolylineLength(path);
        // O caminho é armazenado no sentido saída -> entrada. A ordem visual
        // do overflow é inversa à lista: o primeiro excedente espera junto da
        // entrada (é o próximo a retornar ao radial) e o último fica junto da
        // saída (é o que acabou de deixar o radial).
        const int localIndex = index - blockStart;
        const int localCount = std::max(1,
            base + (pathIndex < remainder ? 1 : 0));
        const float normalized = localCount > 1
            ? static_cast<float>(std::clamp(localCount - 1 - localIndex, 0, localCount - 1)) /
                static_cast<float>(localCount - 1)
            : 0.5f;
        const float distance = normalized >= 1.0f
            ? std::nextafter(length, 0.0f) : length * normalized;
        return SamplePolyline(path, distance).position;
    }

    std::vector<std::vector<ImVec2>> GuidePaths(
        const ImVec2& center, float radius, bool leftSide)
    {
        struct Cache
        {
            std::uint64_t revision{};
            ImVec2 center{};
            float radius{};
            std::vector<std::vector<ImVec2>> value;
        };
        static std::array<Cache, 2> caches;
        auto& cache = caches[leftSide ? 1u : 0u];
        if (cache.revision != g_savedRevision ||
            cache.center.x != center.x || cache.center.y != center.y ||
            cache.radius != radius)
        {
            cache.revision = g_savedRevision;
            cache.center = center;
            cache.radius = radius;
            cache.value = BuildPaths(g_saved, center, radius);
            if (!leftSide)
                for (auto& path : cache.value)
                    for (auto& point : path)
                        point.x = center.x - (point.x - center.x);
        }
        return cache.value;
    }

    bool HasValidSavedLayout()
    {
        static std::uint64_t cachedRevision{};
        static bool cachedValue{};
        if (cachedRevision != g_savedRevision)
        {
            cachedValue = IsValid(g_saved);
            cachedRevision = g_savedRevision;
        }
        return cachedValue;
    }

    int FlowDirection() { return g_saved.flowDirection < 0 ? -1 : 1; }
    float LineOpacity() { return std::clamp(g_saved.lineOpacity, 0.0f, 100.0f); }
    float RadialLineOpacity() { return std::clamp(g_saved.radialLineOpacity, 0.0f, 100.0f); }
    void SetLineOpacity(float value)
    {
        value = std::clamp(value, 0.0f, 100.0f);
        g_saved.lineOpacity = value;
        g_editing.lineOpacity = value;
        ++g_savedRevision;
    }

    void SetRadialLineOpacity(float value)
    {
        value = std::clamp(value, 0.0f, 100.0f);
        g_saved.radialLineOpacity = value;
        g_editing.radialLineOpacity = value;
        ++g_savedRevision;
    }
    float RadialRotation() { return g_saved.radialRotation; }
    void SetRadialRotation(float value)
    {
        constexpr float pi = 3.141592654f;
        value = std::clamp(value, -pi, pi);
        const auto rotateLayout = [&](Layout& layout) {
            const float delta = value - layout.radialRotation;
            if (std::abs(delta) <= 0.000001f) return;
            const float c = std::cos(delta);
            const float s = std::sin(delta);
            for (auto& piece : layout.pieces)
            {
                const ImVec2 position = piece.position;
                piece.position = ImVec2(
                    position.x * c - position.y * s,
                    position.x * s + position.y * c);
                piece.rotation += delta;
            }
            layout.radialRotation = value;
        };
        rotateLayout(g_saved);
        rotateLayout(g_editing);
        ++g_savedRevision;
    }

    float RadialLineVisibility(float angle)
    {
        float visibility = 1.0f;
        for (const RadialErase& erase : g_saved.radialErases)
        {
            float delta = std::abs(angle - erase.angle);
            while (delta > 6.283185307f) delta -= 6.283185307f;
            delta = std::min(delta, 6.283185307f - delta);
            const float size = std::max(erase.size, 0.001f);
            if (delta >= size) continue;
            const float normalized = delta / size;
            const float hardness = std::clamp(erase.hardness, 0.0f, 0.98f);
            const float local = normalized <= hardness ? 0.0f :
                (normalized - hardness) / (1.0f - hardness);
            visibility = std::min(visibility, local);
        }
        return visibility;
    }

    float OverflowLineVisibility(const ImVec2& point,
        const ImVec2& center, float radius)
    {
        float visibility = 1.0f;
        const float safeRadius = std::max(radius, 0.001f);
        const ImVec2 normalized((point.x - center.x) / safeRadius,
            (point.y - center.y) / safeRadius);
        for (const OverflowErase& erase : g_saved.overflowErases)
        {
            const float distance = Distance(normalized, erase.position);
            const float size = std::max(erase.size, 0.001f);
            if (distance >= size) continue;
            const float localDistance = distance / size;
            const float hardness = std::clamp(erase.hardness, 0.0f, 0.98f);
            const float local = localDistance <= hardness ? 0.0f :
                (localDistance - hardness) / (1.0f - hardness);
            visibility = std::min(visibility, local);
        }
        return visibility;
    }

    int GateCount(GateKind kind)
    {
        return static_cast<int>(std::ranges::count_if(g_saved.gates,
            [kind](const Gate& gate) { return gate.kind == kind; }));
    }

    std::pair<ImVec2, ImVec2> GatePositions(GateKind kind, int ordinal,
        const ImVec2& center, float radius, bool leftSide,
        RadialShape::Style shape)
    {
        std::vector<const Gate*> gates;
        for (const Gate& gate : g_saved.gates)
            if (gate.kind == kind) gates.push_back(&gate);
        if (gates.empty()) return { center, center };
        const Gate& gate = *gates[static_cast<std::size_t>(
            std::abs(ordinal) % static_cast<int>(gates.size()))];
        ImVec2 track{};
        if (gate.trackT >= 0.0f)
        {
            const auto path = BuildClosedPath(g_saved, center, radius);
            track = SamplePolyline(path, PolylineLength(path) * gate.trackT).position;
        }
        else
        {
            const auto piece = std::ranges::find_if(g_saved.pieces,
                [&](const Piece& value) { return value.id == gate.pieceID; });
            if (piece == g_saved.pieces.end()) return { center, center };
            track = GetPort(*piece, gate.portIndex, center, radius).position;
        }
        float angle = gate.mainAngle + g_saved.radialRotation;
        const float directionAngle = leftSide ? angle : 3.141592654f - angle;
        ImVec2 main = RadialShape::PositionAtAngles(
            shape, center, radius, gate.mainAngle, directionAngle);
        if (!leftSide)
        {
            track.x = center.x - (track.x - center.x);
        }
        return { main, track };
    }

    void AutoDirection(Layout& layout)
    {
        if (layout.gates.empty()) { layout.flowDirection = 1; return; }
        const auto entry = std::ranges::find_if(layout.gates,
            [](const Gate& gate) { return gate.kind == GateKind::Entry; });
        const auto exit = std::ranges::find_if(layout.gates,
            [](const Gate& gate) { return gate.kind == GateKind::Exit; });
        if (entry == layout.gates.end() || exit == layout.gates.end()) return;
        const ImVec2 origin{};
        const auto path = BuildClosedPath(layout, origin, 1.0f);
        const auto entryPiece = std::ranges::find_if(layout.pieces,
            [&](const Piece& piece) { return piece.id == entry->pieceID; });
        const auto exitPiece = std::ranges::find_if(layout.pieces,
            [&](const Piece& piece) { return piece.id == exit->pieceID; });
        if (path.empty() || entryPiece == layout.pieces.end() || exitPiece == layout.pieces.end())
            return;
        const float length = PolylineLength(path);
        const float entryDistance = [&] {
            const ImVec2 point = GetPort(*entryPiece, entry->portIndex, origin, 1.0f).position;
            float along = 0.0f, best = 0.0f, bestDistance = FLT_MAX;
            for (std::size_t i = 1; i < path.size(); ++i)
            {
                const float distance = Distance(path[i - 1], point);
                if (distance < bestDistance) { bestDistance = distance; best = along; }
                along += Distance(path[i - 1], path[i]);
            }
            return best;
        }();
        const float exitDistance = [&] {
            const ImVec2 point = GetPort(*exitPiece, exit->portIndex, origin, 1.0f).position;
            float along = 0.0f, best = 0.0f, bestDistance = FLT_MAX;
            for (std::size_t i = 1; i < path.size(); ++i)
            {
                const float distance = Distance(path[i - 1], point);
                if (distance < bestDistance) { bestDistance = distance; best = along; }
                along += Distance(path[i - 1], path[i]);
            }
            return best;
        }();
        float forward = exitDistance - entryDistance;
        if (forward < 0.0f) forward += length;
        layout.flowDirection = forward <= length * 0.5f ? 1 : -1;
    }

    bool Save()
    {
        if (!IsValid(g_editing)) return false;
        g_saved = g_editing;
        ++g_savedRevision;
        return WriteLayoutFile(Path(), g_saved);
    }

    bool CommitEdit() { return Save(); }

    bool Load()
    {
        Layout loaded;
        if (!ReadLayoutFile(Path(), loaded)) return false;
        if (!IsValid(loaded)) return false;
        g_saved = std::move(loaded);
        ++g_savedRevision;
        g_editing = g_saved;
        return true;
    }

    std::filesystem::path PresetPath(std::string_view name)
    {
        std::string safe;
        for (const char value : name)
            if (std::isalnum(static_cast<unsigned char>(value)) || value == ' ' ||
                value == '-' || value == '_') safe.push_back(value);
        if (safe.empty()) safe = "Preset";
        std::string lower = safe;
        std::ranges::transform(lower, lower.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        if (lower == "current") safe += " Preset";
        return Path().parent_path() / (safe + ".txt");
    }

    bool SavePreset(std::string_view name)
    {
        return IsValid(g_editing) && WriteLayoutFile(PresetPath(name), g_editing);
    }

    bool LoadPreset(std::string_view name)
    {
        Layout loaded;
        if (!ReadLayoutFile(PresetPath(name), loaded) || !IsValid(loaded)) return false;
        g_editing = std::move(loaded);
        return true;
    }

    bool IsBuiltInPreset(std::string_view name)
    {
        (void)name;
        return false;
    }

    bool RenamePreset(std::string_view oldName, std::string_view newName)
    {
        if (newName.empty()) return false;
        const auto destination = PresetPath(newName);
        std::error_code ec;
        if (std::filesystem::exists(destination, ec))
            return false;

        Layout loaded;
        const auto source = PresetPath(oldName);
        if (!ReadLayoutFile(source, loaded) || !IsValid(loaded) ||
            !WriteLayoutFile(destination, loaded))
            return false;
        std::filesystem::remove(source, ec);
        return !ec;
    }

    bool DuplicatePreset(std::string_view name, std::string* createdName)
    {
        Layout source;
        if (!ReadLayoutFile(PresetPath(name), source) || !IsValid(source))
            return false;

        const std::string base(name);
        for (int suffix = 1; suffix < 10000; ++suffix)
        {
            const std::string candidate = base + std::to_string(suffix);
            std::error_code ec;
            if (std::filesystem::exists(PresetPath(candidate), ec))
                continue;
            if (!WriteLayoutFile(PresetPath(candidate), source)) return false;
            if (createdName) *createdName = candidate;
            return true;
        }
        return false;
    }

    bool DeletePreset(std::string_view name)
    {
        std::error_code ec;
        const bool removed = std::filesystem::remove(PresetPath(name), ec);
        return removed && !ec;
    }

    std::vector<std::string> Presets()
    {
        std::vector<std::string> result;
        const auto directory = Path().parent_path();
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);

        // Uma versão intermediária acrescentava "radials" ao diretório pai
        // de current.txt, embora esse pai já fosse Wheel/radials. Recupera os
        // presets que tenham caído em Wheel/radials/radials.
        const auto nestedDirectory = directory / "radials";
        if (std::filesystem::exists(nestedDirectory, ec))
        {
            for (const auto& item : std::filesystem::directory_iterator(nestedDirectory, ec))
            {
                if (!item.is_regular_file() || item.path().extension() != ".txt") continue;
                auto destination = directory / item.path().filename();
                if (destination.filename() == "current.txt")
                    destination = directory / "current preset.txt";
                for (int suffix = 1; std::filesystem::exists(destination, ec); ++suffix)
                    destination = directory /
                        (item.path().stem().string() + " migrated " + std::to_string(suffix) + ".txt");
                std::filesystem::rename(item.path(), destination, ec);
                ec.clear();
            }
            std::filesystem::remove(nestedDirectory / ".defaults_initialized", ec);
            ec.clear();
            std::filesystem::remove(nestedDirectory, ec); // Só remove se estiver vazia.
            ec.clear();
        }
        const auto legacyDirectory = directory.parent_path() / "presets";
        if (std::filesystem::exists(legacyDirectory, ec))
        {
            for (const auto& item : std::filesystem::directory_iterator(legacyDirectory, ec))
            {
                if (!item.is_regular_file() || item.path().extension() != ".txt") continue;
                auto destination = directory / item.path().filename();
                for (int suffix = 1; std::filesystem::exists(destination, ec); ++suffix)
                    destination = directory /
                        (item.path().stem().string() + " migrated " + std::to_string(suffix) + ".txt");
                std::filesystem::rename(item.path(), destination, ec);
                ec.clear();
            }
            std::filesystem::remove(legacyDirectory / ".defaults_initialized", ec);
            ec.clear();
            std::filesystem::remove(legacyDirectory, ec); // Só remove se estiver vazia.
            ec.clear();
        }
        // O marcador só servia para criar Legacy Orbit uma única vez. Presets
        // agora são totalmente administrados pelo usuário, sem arquivo oculto.
        std::filesystem::remove(directory / ".defaults_initialized", ec);
        std::filesystem::remove(legacyDirectory / ".defaults_initialized", ec);
        ec.clear();
        for (const auto& item : std::filesystem::directory_iterator(directory, ec))
            if (item.is_regular_file() && item.path().extension() == ".txt" &&
                item.path().filename() != "current.txt")
                result.push_back(item.path().stem().string());
        std::ranges::sort(result);
        return result;
    }

    void ResetEditingToLegacy()
    {
        g_editing = BuiltInPreset("Legacy Orbit");
    }
}
