#include "PCH.h"
#include "Animation/Track/TrackEditor.h"
#include "Animation/Track/TrackGeometry.h"
#include "Animation/Track/TrackLayout.h"
#include "Animation/RadialShape.h"
#include "Config.h"
#include "Language.h"
#include "Resolution.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <string>
#include <ranges>

namespace
{
    bool g_open = false;
    int g_selectedID = -1;
    int g_selectedPort = -1;
    bool g_dragging = false;
    bool g_panning = false;
    bool g_panTool = false;
    bool g_selectTool = true;
    bool g_freeDrawTool = false;
    bool g_geometryDrawTool = false;
    bool g_smoothDraw = false;
    float g_drawSmoothing = 55.0f;
    bool g_drawing = false;
    std::vector<ImVec2> g_drawPoints;
    bool g_gateToolActive = false;
    bool g_gateDragging = false;
    Track::GateKind g_gateToolKind = Track::GateKind::Entry;
    float g_gateStartAngle = 0.0f;
    ImVec2 g_gateStart{};
    ImVec2 g_gateMouse{};
    int g_gatePieceID = -1;
    int g_gatePort = -1;
    int g_liveGateID = -1;
    ImVec2 g_gatePressMouse{};
    ImVec2 g_dragOffset{};
    ImVec2 g_pan{};
    ImVec2 g_lastMouse{};
    float g_zoom = 1.0f;
    float g_panelScroll = 0.0f;
    float g_panelLeft = 0.0f;
    float g_panelMaxScroll = 0.0f;
    float g_scrollTrackTop = 0.0f;
    float g_scrollTrackBottom = 0.0f;
    float g_panelClipTop = 138.0f;
    float g_panelClipBottom = 0.0f;
    float g_scrollGrabOffset = 0.0f;
    bool g_scrollDragging = false;
    enum class EditorSlider {
        None, RadialQuantity, RadialStretch, GhostItems, RadialOpacity,
        RadialShape, RadialRotation, BrushSize, BrushHardness,
        OverflowOpacity, DrawSmoothing
    };
    EditorSlider g_activeSlider = EditorSlider::None;
    int g_previewGhostItems = 0;
    bool g_spaceHeld = false;
    bool g_ctrlHeld = false;
    bool g_rotating = false;
    bool g_scaling = false;
    int g_scaleDirection = 1;
    float g_scaleStartY = 0.0f;
    float g_scaleStartValue = 1.0f;
    bool g_eraseTool = false;
    bool g_eraseOverflowMode = false;
    bool g_erasing = false;
    float g_brushSize = 0.14f;
    float g_brushHardness = 0.72f;
    float g_rotateOffset = 0.0f;
    float g_previewPhase = 0.0f;
    float g_previewTargetPhase = 0.0f;
    std::vector<float> g_lastPreviewSlotT;
    std::vector<float> g_frozenPreviewSlotT;
    std::vector<ImVec2> g_lastPreviewMainPositions;
    std::vector<ImVec2> g_frozenPreviewMainPositions;
    bool g_previewSlotsFrozen = false;
    std::vector<Track::Layout> g_undoHistory;
    std::vector<Track::Layout> g_redoHistory;
    Track::Layout g_historyActionStart;
    bool g_historyActionPending = false;
    float g_resetRadialFlash = 0.0f;
    int g_gizmoOwner = -1;
    ImVec2 g_gizmoDirection{ 1.0f, 0.0f };
    ImVec2 g_gizmoAnimatedCenter{};
    bool g_gizmoCenterInitialized = false;
    bool g_presetNameOpen = false;
    bool g_presetListOpen = false;
    enum class PresetNameAction { SaveCurrent, RenamePreset };
    PresetNameAction g_presetNameAction = PresetNameAction::SaveCurrent;
    std::string g_presetSource;
    std::string g_presetName{ "My Radial" };
    ImVec2 g_canvasCenter{};
    ImVec2 g_viewportMin{};
    ImVec2 g_viewportMax{ 1920.0f, 1080.0f };
    float g_canvasRadius = 190.0f;

    struct Rect { ImVec2 min{}, max{}; };
    struct PortControl
    {
        int pieceID{-1};
        int port{-1};
        ImVec2 portPosition{};
        ImVec2 center{};
    };
    std::vector<PortControl> g_portControls;
    int g_stickyPortPiece = -1;
    int g_stickyPortIndex = -1;
    struct PresetRow
    {
        Rect load{}, rename{}, duplicate{}, remove{};
        std::string name;
        bool builtIn{};
    };
    std::vector<PresetRow> g_presetRows;
    Rect g_zoomOut, g_zoomIn, g_reset, g_selectButton, g_panButton,
        g_deleteButton, g_drawPathButton, g_scrollThumb, g_lineOpacitySlider,
        g_smoothDrawButton, g_drawSmoothingSlider, g_geometryDrawButton,
        g_duplicateSelectedButton,
        g_radialQuantitySlider, g_radialStretchSlider, g_ghostItemsSlider,
        g_radialOpacitySlider, g_radialShapeSlider, g_radialRotationSlider, g_brushSizeSlider,
        g_brushHardnessSlider, g_eraseRadialButton, g_centerReverseFlow,
        g_resetRadialDrawing, g_eraserRadialMode, g_eraserOverflowMode,
        g_gizmoPanel, g_gizmoMove, g_gizmoRotate, g_gizmoDelete, g_gizmoMirror,
        g_gizmoScaleUp, g_gizmoScaleDown,
        g_save, g_cancel, g_presetSave, g_presetLoad, g_presetReset,
        g_presetOK, g_presetCancel;
    Rect g_addEntry, g_addExit, g_autoFlow, g_reverseFlow, g_invertPiece, g_flipPoles;
    constexpr std::array<Track::PieceKind, 18> kPieceKinds{
        Track::PieceKind::Straight, Track::PieceKind::ArcUp,
        Track::PieceKind::ArcDown, Track::PieceKind::SCurve,
        Track::PieceKind::WideArcUp, Track::PieceKind::WideArcDown,
        Track::PieceKind::Wave, Track::PieceKind::SoftZigzag,
        Track::PieceKind::HookUp, Track::PieceKind::HookDown,
        Track::PieceKind::Bulge, Track::PieceKind::CircleLoop,
        Track::PieceKind::HalfCircle, Track::PieceKind::Spiral,
        Track::PieceKind::RightAngle, Track::PieceKind::Valley,
        Track::PieceKind::ZPath, Track::PieceKind::Hairpin };
    std::array<Rect, kPieceKinds.size()> g_pieceButtons{};

    void HashValue(std::uint64_t& hash, std::uint64_t value)
    {
        hash ^= value;
        hash *= 1099511628211ull;
    }

    void HashFloat(std::uint64_t& hash, float value)
    {
        HashValue(hash, std::bit_cast<std::uint32_t>(value));
    }

    std::uint64_t LayoutHash(const Track::Layout& layout)
    {
        std::uint64_t hash = 1469598103934665603ull;
        HashValue(hash, layout.pieces.size());
        for (const auto& piece : layout.pieces)
        {
            HashValue(hash, piece.id); HashValue(hash, static_cast<int>(piece.kind));
            HashFloat(hash, piece.position.x); HashFloat(hash, piece.position.y);
            HashFloat(hash, piece.rotation); HashFloat(hash, piece.scale);
            HashValue(hash, piece.mirrored); HashValue(hash, piece.customPoints.size());
            for (const auto& point : piece.customPoints)
            { HashFloat(hash, point.x); HashFloat(hash, point.y); }
        }
        for (const auto& gate : layout.gates)
        {
            HashValue(hash, gate.id); HashValue(hash, static_cast<int>(gate.kind));
            HashValue(hash, gate.pieceID); HashValue(hash, gate.portIndex);
            HashFloat(hash, gate.mainAngle); HashFloat(hash, gate.trackT);
        }
        for (const auto& connection : layout.connections)
        {
            HashValue(hash, connection.id);
            HashValue(hash, static_cast<int>(connection.sourceKind));
            HashValue(hash, connection.pieceA); HashValue(hash, connection.portA);
            HashValue(hash, connection.pieceB); HashValue(hash, connection.portB);
        }
        HashValue(hash, layout.nextID); HashValue(hash, layout.nextGateID);
        HashValue(hash, layout.nextConnectionID); HashValue(hash, layout.flowDirection);
        HashFloat(hash, layout.lineOpacity); HashFloat(hash, layout.radialLineOpacity);
        HashFloat(hash, layout.radialRotation);
        for (const auto& erase : layout.radialErases)
        { HashFloat(hash, erase.angle); HashFloat(hash, erase.size); HashFloat(hash, erase.hardness); }
        for (const auto& erase : layout.overflowErases)
        {
            HashFloat(hash, erase.position.x); HashFloat(hash, erase.position.y);
            HashFloat(hash, erase.size); HashFloat(hash, erase.hardness);
        }
        return hash;
    }

    void BeginHistoryAction()
    {
        if (g_historyActionPending) return;
        g_historyActionStart = Track::CustomLayout();
        g_historyActionPending = true;
    }

    void CommitHistoryAction()
    {
        if (!g_historyActionPending) return;
        if (LayoutHash(g_historyActionStart) != LayoutHash(Track::CustomLayout()))
        {
            g_undoHistory.push_back(std::move(g_historyActionStart));
            if (g_undoHistory.size() > 100) g_undoHistory.erase(g_undoHistory.begin());
            g_redoHistory.clear();
        }
        g_historyActionPending = false;
    }

    void Undo()
    {
        CommitHistoryAction();
        if (g_undoHistory.empty()) return;
        g_redoHistory.push_back(Track::CustomLayout());
        Track::CustomLayout() = std::move(g_undoHistory.back());
        g_undoHistory.pop_back();
        if (std::ranges::none_of(Track::CustomLayout().pieces,
            [](const Track::Piece& piece) { return piece.id == g_selectedID; }))
            g_selectedID = -1;
    }

    void Redo()
    {
        CommitHistoryAction();
        if (g_redoHistory.empty()) return;
        g_undoHistory.push_back(Track::CustomLayout());
        Track::CustomLayout() = std::move(g_redoHistory.back());
        g_redoHistory.pop_back();
        if (std::ranges::none_of(Track::CustomLayout().pieces,
            [](const Track::Piece& piece) { return piece.id == g_selectedID; }))
            g_selectedID = -1;
    }

    void DuplicateSelected()
    {
        const auto selected = std::ranges::find_if(Track::CustomLayout().pieces,
            [](const Track::Piece& piece) { return piece.id == g_selectedID; });
        if (selected == Track::CustomLayout().pieces.end()) return;
        BeginHistoryAction();
        auto& layout = Track::CustomLayout();
        Track::Piece duplicate = *selected;
        duplicate.id = layout.nextID++;
        duplicate.position.x += 0.12f;
        duplicate.position.y += 0.12f;
        layout.pieces.push_back(std::move(duplicate));
        g_selectedID = layout.pieces.back().id;
        g_gizmoOwner = -1;
        g_gizmoCenterInitialized = false;
        CommitHistoryAction();
    }

    bool Inside(const ImVec2& p, const Rect& r)
    {
        return p.x >= r.min.x && p.x <= r.max.x && p.y >= r.min.y && p.y <= r.max.y;
    }

    bool InsideCircle(const ImVec2& p, const Rect& r)
    {
        const ImVec2 center((r.min.x + r.max.x) * 0.5f,
            (r.min.y + r.max.y) * 0.5f);
        const float radius = std::min(r.max.x - r.min.x, r.max.y - r.min.y) * 0.48f;
        return Track::Distance(p, center) <= radius;
    }

    const char* Tr(std::string_view key)
    {
        return Language::Get(key).c_str();
    }

    std::string FitText(std::string text, float maximumWidth)
    {
        if (ImGui::CalcTextSize(text.c_str()).x <= maximumWidth) return text;
        constexpr std::string_view suffix = "...";
        while (!text.empty() && ImGui::CalcTextSize(
            (text + std::string(suffix)).c_str()).x > maximumWidth)
            text.pop_back();
        return text + std::string(suffix);
    }

    const char* ValidationReason(const std::string& reason)
    {
        if (reason == "Add at least one piece") return Tr("track_need_piece");
        if (reason == "A radial terminal is not attached to a piece end")
            return Tr("track_terminal_detached");
        if (reason == "A piece cannot have two equal terminals")
            return Tr("track_duplicate_terminal");
        if (reason == "Every free piece end needs a radial terminal")
            return Tr("track_free_end");
        if (reason == "Add at least one entry and one exit")
            return Tr("track_need_entry_exit");
        if (reason == "Each exit must reach an entry through one continuous path")
            return Tr("track_disconnected_exit");
        if (reason == "Every piece must belong to a radial entry/exit path")
            return Tr("track_unused_piece");
        return reason.c_str();
    }

    const char* ShapeDisplayName(RadialShape::Style style)
    {
        switch (style)
        {
        case RadialShape::Style::HarmonicFlower: return Tr("shape_harmonic_flower");
        case RadialShape::Style::DualOrbit: return Tr("shape_dual_orbit");
        case RadialShape::Style::Turbine: return Tr("shape_turbine");
        case RadialShape::Style::SpiralGalaxy: return Tr("shape_spiral_galaxy");
        case RadialShape::Style::PulsarCrown: return Tr("shape_pulsar_crown");
        case RadialShape::Style::LiquidDiamond: return Tr("shape_liquid_diamond");
        case RadialShape::Style::CometTail: return Tr("shape_comet_tail");
        case RadialShape::Style::RoseEngine: return Tr("shape_rose_engine");
        case RadialShape::Style::QuantumRipple: return Tr("shape_quantum_ripple");
        case RadialShape::Style::Star: return Tr("shape_star");
        default: return Tr("shape_classic_orbit");
        }
    }

    Track::Piece* Selected()
    {
        auto& pieces = Track::CustomLayout().pieces;
        const auto it = std::find_if(pieces.begin(), pieces.end(), [](const Track::Piece& piece) {
            return piece.id == g_selectedID;
        });
        return it == pieces.end() ? nullptr : &*it;
    }

    const char* PieceName(Track::PieceKind kind)
    {
        switch (kind)
        {
        case Track::PieceKind::ArcUp: return "Arc Up";
        case Track::PieceKind::ArcDown: return "Arc Down";
        case Track::PieceKind::SCurve: return "S Curve";
        case Track::PieceKind::WideArcUp: return "Wide Arc Up";
        case Track::PieceKind::WideArcDown: return "Wide Arc Down";
        case Track::PieceKind::Wave: return "Wave";
        case Track::PieceKind::SoftZigzag: return "Soft Zigzag";
        case Track::PieceKind::HookUp: return "Hook Up";
        case Track::PieceKind::HookDown: return "Hook Down";
        case Track::PieceKind::Bulge: return "Bulge";
        case Track::PieceKind::HalfCircle: return "Half Circle";
        case Track::PieceKind::Spiral: return "Spiral";
        case Track::PieceKind::RightAngle: return "Right Angle";
        case Track::PieceKind::Valley: return "Valley";
        case Track::PieceKind::ZPath: return "Z Path";
        case Track::PieceKind::Hairpin: return "Hairpin";
        case Track::PieceKind::FreeDraw: return ">> DRAW PATH <<";
        default: return "Straight";
        }
    }

    ImVec2 WorldCenter()
    {
        return ImVec2(g_canvasCenter.x + g_pan.x, g_canvasCenter.y + g_pan.y);
    }

    float WorldRadius() { return g_canvasRadius * g_zoom; }

    RadialShape::Style CurrentRadialShape()
    {
        return static_cast<RadialShape::Style>(std::clamp(
            Config::g_radialShape, 0, RadialShape::Count() - 1));
    }

    ImVec2 EditingRadialPoint(const ImVec2& center, float radius, float angle)
    {
        return RadialShape::PositionAtAngles(CurrentRadialShape(), center,
            radius, angle - Track::CustomLayout().radialRotation, angle);
    }

    bool NearestOtherPort(const ImVec2& mouse, int sourcePiece,
        ImVec2& position, int& pieceID, int& portIndex, float maximumDistance = 38.0f)
    {
        bool found = false;
        float nearest = maximumDistance;
        for (const auto& piece : Track::CustomLayout().pieces)
        {
            if (piece.id == sourcePiece) continue;
            for (int port = 0; port < 2; ++port)
            {
                const ImVec2 candidate = Track::GetPort(
                    piece, port, WorldCenter(), WorldRadius()).position;
                const float distance = Track::Distance(candidate, mouse);
                if (distance >= nearest) continue;
                nearest = distance;
                position = candidate;
                pieceID = piece.id;
                portIndex = port;
                found = true;
            }
        }
        return found;
    }

    void ClearEndpoint(Track::Layout& layout, int pieceID, int portIndex)
    {
        std::erase_if(layout.gates, [&](const Track::Gate& gate) {
            return gate.pieceID == pieceID && gate.portIndex == portIndex;
        });
        std::erase_if(layout.connections, [&](const Track::Connection& connection) {
            return (connection.pieceA == pieceID && connection.portA == portIndex) ||
                (connection.pieceB == pieceID && connection.portB == portIndex);
        });
    }

    void ClearLiveGate(Track::Layout& layout)
    {
        if (g_liveGateID < 0) return;
        std::erase_if(layout.gates, [](const Track::Gate& gate) {
            return gate.id == g_liveGateID;
        });
        g_liveGateID = -1;
    }

    void UpdateLiveRadialGate(const ImVec2& mouse)
    {
        if (!g_gateDragging) return;
        auto& layout = Track::CustomLayout();
        ImVec2 otherPort{};
        int targetPiece = -1;
        int targetPort = -1;
        const bool pieceTarget = NearestOtherPort(mouse, g_gatePieceID,
            otherPort, targetPiece, targetPort);
        const ImVec2 center = WorldCenter();
        const float radius = WorldRadius();
        const float screenAngle = std::atan2(mouse.y - center.y,
            mouse.x - center.x);
        const ImVec2 radialPoint = EditingRadialPoint(center, radius, screenAngle);
        const bool radialTarget = !pieceTarget &&
            Track::Distance(mouse, radialPoint) <= 46.0f;
        if (!radialTarget)
        {
            ClearLiveGate(layout);
            return;
        }

        const float angle = screenAngle - layout.radialRotation;
        if (g_liveGateID >= 0)
        {
            const auto live = std::ranges::find_if(layout.gates,
                [](const Track::Gate& gate) { return gate.id == g_liveGateID; });
            if (live != layout.gates.end())
            {
                live->mainAngle = angle;
                return;
            }
            g_liveGateID = -1;
        }

        // O novo encaixe já participa do circuito enquanto o mouse continua
        // pressionado. Assim o preview troca de posição no instante do snap.
        std::erase_if(layout.gates, [&](const Track::Gate& gate) {
            return gate.pieceID == g_gatePieceID &&
                gate.kind == g_gateToolKind;
        });
        g_liveGateID = layout.nextGateID++;
        layout.gates.push_back(Track::Gate{ g_liveGateID, g_gateToolKind,
            g_gatePieceID, g_gatePort, angle, -1.0f });
    }

    void RemovePieceConnections(Track::Layout& layout, int pieceID)
    {
        std::erase_if(layout.connections, [&](const Track::Connection& connection) {
            return connection.pieceA == pieceID || connection.pieceB == pieceID;
        });
    }

    bool UpdateGizmoRects()
    {
        auto* selected = Selected();
        if (!selected)
        {
            g_gizmoCenterInitialized = false;
            g_gizmoPanel = {};
            g_gizmoMove = {};
            g_gizmoRotate = {};
            g_gizmoDelete = {};
            g_gizmoMirror = {};
            return false;
        }

        const ImVec2 anchor = Track::TransformPoint(
            *selected, ImVec2{}, WorldCenter(), WorldRadius());
        float bestClearance = -1.0f;
        ImVec2 bestDirection = g_gizmoDirection;
        constexpr std::array<ImVec2, 8> directions{
            ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f),
            ImVec2(-1.0f, 0.0f), ImVec2(0.0f, -1.0f),
            ImVec2(0.7071f, 0.7071f), ImVec2(-0.7071f, 0.7071f),
            ImVec2(0.7071f, -0.7071f), ImVec2(-0.7071f, -0.7071f) };
        for (const ImVec2& direction : directions)
        {
            // Durante o gesto a caixa acompanha a peça, mas não troca de
            // quadrante a cada pequeno giro. Isso preserva os hitboxes e
            // elimina o teleporte/clipping observado durante a rotação.
            if ((g_rotating || g_scaling) &&
                direction.x * g_gizmoDirection.x +
                    direction.y * g_gizmoDirection.y < 0.99f)
                continue;
            const ImVec2 candidate(anchor.x + direction.x * 108.0f,
                anchor.y + direction.y * 108.0f);
            if (candidate.x - 52.0f < g_viewportMin.x ||
                candidate.x + 52.0f >= g_panelLeft ||
                candidate.y - 52.0f < g_viewportMin.y ||
                candidate.y + 52.0f > g_viewportMax.y)
                continue;
            float clearance = FLT_MAX;
            for (const auto& piece : Track::CustomLayout().pieces)
            {
                const auto sampled = Track::SamplePiece(
                    piece, WorldCenter(), WorldRadius(), 48);
                for (const ImVec2& point : sampled)
                    clearance = std::min(clearance, Track::Distance(candidate, point));
                for (int port = 0; port < 2; ++port)
                {
                    const ImVec2 portPosition = Track::GetPort(
                        piece, port, WorldCenter(), WorldRadius()).position;
                    clearance = std::min(clearance,
                        Track::Distance(candidate, portPosition) - 30.0f);
                }
            }
            for (const auto& gate : Track::CustomLayout().gates)
            {
                ImVec2 rail{};
                if (gate.trackT >= 0.0f)
                {
                    const auto path = Track::BuildClosedPath(
                        Track::CustomLayout(), WorldCenter(), WorldRadius());
                    if (path.empty()) continue;
                    rail = Track::SamplePolyline(path,
                        Track::PolylineLength(path) * gate.trackT).position;
                }
                else
                {
                    const auto piece = std::ranges::find_if(
                        Track::CustomLayout().pieces,
                        [&](const Track::Piece& value) {
                            return value.id == gate.pieceID;
                        });
                    if (piece == Track::CustomLayout().pieces.end()) continue;
                    rail = Track::GetPort(*piece, gate.portIndex,
                        WorldCenter(), WorldRadius()).position;
                }
                const float angle = gate.mainAngle +
                    Track::CustomLayout().radialRotation;
                const ImVec2 main = EditingRadialPoint(
                    WorldCenter(), WorldRadius(), angle);
                for (int sample = 0; sample <= 8; ++sample)
                {
                    const float t = static_cast<float>(sample) / 8.0f;
                    const ImVec2 point(main.x + (rail.x - main.x) * t,
                        main.y + (rail.y - main.y) * t);
                    clearance = std::min(clearance,
                        Track::Distance(candidate, point));
                }
            }
            const float candidateAngle = std::atan2(
                candidate.y - WorldCenter().y,
                candidate.x - WorldCenter().x);
            const float radialDistance = Track::Distance(candidate,
                EditingRadialPoint(WorldCenter(), WorldRadius(), candidateAngle));
            clearance = std::min(clearance, radialDistance);
            // Mantém a posição habitual enquanto ela estiver livre. Só troca
            // de lado quando alguma linha/terminal realmente invade a área.
            const float sameDirection = direction.x * g_gizmoDirection.x +
                direction.y * g_gizmoDirection.y;
            if (sameDirection > 0.99f && clearance >= 58.0f)
                clearance += 10000.0f;
            if (clearance > bestClearance)
            {
                bestClearance = clearance;
                bestDirection = direction;
            }
        }
        g_gizmoDirection = bestDirection;
        const ImVec2 target(anchor.x + g_gizmoDirection.x * 108.0f,
            anchor.y + g_gizmoDirection.y * 108.0f);
        if (g_gizmoOwner != selected->id || !g_gizmoCenterInitialized)
        {
            g_gizmoOwner = selected->id;
            g_gizmoAnimatedCenter = target;
            g_gizmoCenterInitialized = true;
        }
        else
        {
            const float factor = 1.0f - std::exp(
                -10.0f * std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f));
            g_gizmoAnimatedCenter.x += (target.x - g_gizmoAnimatedCenter.x) * factor;
            g_gizmoAnimatedCenter.y += (target.y - g_gizmoAnimatedCenter.y) * factor;
        }
        const ImVec2 gizmo = g_gizmoAnimatedCenter;

        g_gizmoPanel = { ImVec2(gizmo.x - 48.0f, gizmo.y - 51.5f),
            ImVec2(gizmo.x + 48.0f, gizmo.y + 44.5f) };
        g_gizmoMove = { ImVec2(gizmo.x - 16.0f, gizmo.y - 16.0f),
            ImVec2(gizmo.x + 16.0f, gizmo.y + 16.0f) };
        g_gizmoRotate = { ImVec2(gizmo.x + 12.0f, gizmo.y - 16.0f),
            ImVec2(gizmo.x + 44.0f, gizmo.y + 16.0f) };
        g_gizmoDelete = { ImVec2(gizmo.x - 44.0f, gizmo.y - 16.0f),
            ImVec2(gizmo.x - 12.0f, gizmo.y + 16.0f) };
        g_gizmoMirror = { ImVec2(gizmo.x - 16.0f, gizmo.y + 12.0f),
            ImVec2(gizmo.x + 16.0f, gizmo.y + 45.0f) };
        g_gizmoScaleDown = {};
        g_gizmoScaleUp = { ImVec2(gizmo.x - 14.0f, gizmo.y - 45.0f),
            ImVec2(gizmo.x + 14.0f, gizmo.y - 17.0f) };
        return true;
    }

    ImVec2 GizmoCenter()
    {
        return ImVec2((g_gizmoMove.min.x + g_gizmoMove.max.x) * 0.5f,
            (g_gizmoMove.min.y + g_gizmoMove.max.y) * 0.5f);
    }

    void DrawGizmoControls(ImDrawList* draw)
    {
        if (!Selected() || !g_gizmoCenterInitialized) return;
        const ImVec2 gizmo = GizmoCenter();
        const ImU32 tool = IM_COL32(235, 220, 180, 255);
        // Fundo local opaco: o controle é a última camada do editor e linhas,
        // ícones ou terminais nunca atravessam seus botões.
        const ImVec2 backgroundCenter(gizmo.x, gizmo.y - 3.5f);
        draw->AddCircleFilled(backgroundCenter, 49.0f, IM_COL32(14, 16, 21, 245), 48);
        draw->AddCircle(backgroundCenter, 49.0f, IM_COL32(115, 105, 85, 210), 48, 1.0f);
        draw->AddCircle(ImVec2(gizmo.x, gizmo.y - 31.0f),
            11.0f, tool, 24, 1.5f);
        draw->AddLine(ImVec2(gizmo.x - 6.0f, gizmo.y - 31.0f),
            ImVec2(gizmo.x + 6.0f, gizmo.y - 31.0f), tool, 2.0f);
        draw->AddLine(ImVec2(gizmo.x, gizmo.y - 37.0f),
            ImVec2(gizmo.x, gizmo.y - 25.0f), tool, 2.0f);
        draw->AddLine(ImVec2(gizmo.x - 9, gizmo.y), ImVec2(gizmo.x + 9, gizmo.y), tool, 2.0f);
        draw->AddLine(ImVec2(gizmo.x, gizmo.y - 9), ImVec2(gizmo.x, gizmo.y + 9), tool, 2.0f);
        draw->AddTriangleFilled(ImVec2(gizmo.x - 12, gizmo.y), ImVec2(gizmo.x - 7, gizmo.y - 3), ImVec2(gizmo.x - 7, gizmo.y + 3), tool);
        draw->AddTriangleFilled(ImVec2(gizmo.x + 12, gizmo.y), ImVec2(gizmo.x + 7, gizmo.y - 3), ImVec2(gizmo.x + 7, gizmo.y + 3), tool);
        draw->AddTriangleFilled(ImVec2(gizmo.x, gizmo.y - 12), ImVec2(gizmo.x - 3, gizmo.y - 7), ImVec2(gizmo.x + 3, gizmo.y - 7), tool);
        draw->AddTriangleFilled(ImVec2(gizmo.x, gizmo.y + 12), ImVec2(gizmo.x - 3, gizmo.y + 7), ImVec2(gizmo.x + 3, gizmo.y + 7), tool);
        const ImVec2 rotateCenter(gizmo.x + 27.0f, gizmo.y);
        draw->PathArcTo(rotateCenter, 8.0f, -1.35f, 2.65f, 24);
        draw->PathStroke(tool, 0, 2.0f);
        draw->AddTriangleFilled(
            ImVec2(rotateCenter.x - 9.0f, rotateCenter.y + 4.0f),
            ImVec2(rotateCenter.x - 4.0f, rotateCenter.y - 1.0f),
            ImVec2(rotateCenter.x - 2.0f, rotateCenter.y + 6.0f), tool);
        draw->AddRect(ImVec2(gizmo.x - 33, gizmo.y - 6), ImVec2(gizmo.x - 21, gizmo.y + 7), tool, 1.0f, 0, 1.8f);
        draw->AddLine(ImVec2(gizmo.x - 35, gizmo.y - 8), ImVec2(gizmo.x - 19, gizmo.y - 8), tool, 2.0f);
        const ImVec2 mirrorCenter(gizmo.x, gizmo.y + 27.0f);
        draw->AddCircle(mirrorCenter, 8.0f, tool, 28, 1.8f);
        draw->PathLineTo(ImVec2(mirrorCenter.x, mirrorCenter.y + 8.0f));
        draw->PathArcTo(mirrorCenter, 8.0f, 1.5707963f, 4.7123890f, 16);
        draw->PathLineTo(ImVec2(mirrorCenter.x, mirrorCenter.y + 8.0f));
        draw->PathFillConvex(tool);
    }

    float DistanceToSegment(const ImVec2& p, const ImVec2& a, const ImVec2& b)
    {
        const float x = b.x - a.x;
        const float y = b.y - a.y;
        const float lengthSq = x * x + y * y;
        const float t = lengthSq > 0.001f ? std::clamp(
            ((p.x - a.x) * x + (p.y - a.y) * y) / lengthSq, 0.0f, 1.0f) : 0.0f;
        return Track::Distance(p, ImVec2(a.x + x * t, a.y + y * t));
    }

    std::pair<float, float> ClosestTrackT(const std::vector<ImVec2>& path, const ImVec2& point)
    {
        const float total = Track::PolylineLength(path);
        float along = 0.0f, bestAlong = 0.0f, best = FLT_MAX;
        for (std::size_t i = 1; i < path.size(); ++i)
        {
            const ImVec2 a = path[i - 1], b = path[i];
            const float segment = Track::Distance(a, b);
            const float x = b.x - a.x, y = b.y - a.y;
            const float lengthSq = x * x + y * y;
            const float t = lengthSq > 0.001f ? std::clamp(
                ((point.x - a.x) * x + (point.y - a.y) * y) / lengthSq, 0.0f, 1.0f) : 0.0f;
            const ImVec2 candidate(a.x + x * t, a.y + y * t);
            const float distance = Track::Distance(candidate, point);
            if (distance < best) { best = distance; bestAlong = along + segment * t; }
            along += segment;
        }
        return { total > 0.001f ? bestAlong / total : 0.0f, best };
    }

    int HitPiece(const ImVec2& mouse)
    {
        int hit = -1;
        float best = 18.0f;
        for (const auto& piece : Track::CustomLayout().pieces)
        {
            const auto points = Track::SamplePiece(piece, WorldCenter(), WorldRadius(), 28);
            for (std::size_t i = 1; i < points.size(); ++i)
            {
                const float distance = DistanceToSegment(mouse, points[i - 1], points[i]);
                if (distance < best) { best = distance; hit = piece.id; }
            }
        }
        return hit;
    }

    float EditingRadialVisibility(float angle)
    {
        float visibility = 1.0f;
        for (const Track::RadialErase& erase : Track::CustomLayout().radialErases)
        {
            float delta = std::abs(angle - erase.angle);
            while (delta > 6.283185307f) delta -= 6.283185307f;
            delta = std::min(delta, 6.283185307f - delta);
            if (delta >= erase.size) continue;
            const float normalized = delta / std::max(erase.size, 0.001f);
            const float hardness = std::clamp(erase.hardness, 0.0f, 0.98f);
            const float local = normalized <= hardness ? 0.0f :
                (normalized - hardness) / (1.0f - hardness);
            visibility = std::min(visibility, local);
        }
        return visibility;
    }

    void DrawEditingRadial(ImDrawList* draw, const ImVec2& center, float radius,
        ImU32 rgb, float opacity, float thickness)
    {
        constexpr int segments = 192;
        const float rotation = Track::CustomLayout().radialRotation;
        for (int i = 0; i < segments; ++i)
        {
            const float a0 = 6.283185307f * static_cast<float>(i) / segments;
            const float a1 = 6.283185307f * static_cast<float>(i + 1) / segments;
            const float visibility = std::min(
                EditingRadialVisibility(a0), EditingRadialVisibility(a1));
            const int alpha = static_cast<int>(std::clamp(
                opacity * visibility, 0.0f, 255.0f));
            if (alpha <= 0) continue;
            const ImU32 color = (rgb & 0x00FFFFFFu) |
                (static_cast<ImU32>(alpha) << 24u);
            draw->AddLine(
                EditingRadialPoint(center, radius, a0 + rotation),
                EditingRadialPoint(center, radius, a1 + rotation),
                color, thickness);
        }
    }

    void EraseRadialAt(const ImVec2& mouse)
    {
        const ImVec2 center = WorldCenter();
        const float radius = WorldRadius();
        const float dx = mouse.x - center.x;
        const float dy = mouse.y - center.y;
        const float mouseAngle = std::atan2(dy, dx);
        if (Track::Distance(mouse,
            EditingRadialPoint(center, radius, mouseAngle)) > 30.0f)
            return;
        float angle = mouseAngle - Track::CustomLayout().radialRotation;
        while (angle < 0.0f) angle += 6.283185307f;
        while (angle >= 6.283185307f) angle -= 6.283185307f;
        auto& erases = Track::CustomLayout().radialErases;
        if (!erases.empty())
        {
            float delta = std::abs(erases.back().angle - angle);
            delta = std::min(delta, 6.283185307f - delta);
            if (delta < g_brushSize * 0.2f) return;
        }
        erases.push_back(Track::RadialErase{ angle, g_brushSize, g_brushHardness });
    }

    float EditingOverflowVisibility(const ImVec2& point)
    {
        const ImVec2 center = WorldCenter();
        const float radius = std::max(WorldRadius(), 0.001f);
        const ImVec2 normalized((point.x - center.x) / radius,
            (point.y - center.y) / radius);
        float visibility = 1.0f;
        for (const auto& erase : Track::CustomLayout().overflowErases)
        {
            const float distance = Track::Distance(normalized, erase.position);
            const float size = std::max(erase.size, 0.001f);
            if (distance >= size) continue;
            const float t = distance / size;
            const float hardness = std::clamp(erase.hardness, 0.0f, 0.98f);
            const float local = t <= hardness ? 0.0f :
                (t - hardness) / (1.0f - hardness);
            visibility = std::min(visibility, local);
        }
        return visibility;
    }

    void EraseOverflowAt(const ImVec2& mouse)
    {
        const ImVec2 center = WorldCenter();
        const float radius = std::max(WorldRadius(), 0.001f);
        const float brushPixels = std::max(8.0f, g_brushSize * radius);
        bool touchesTrack = false;
        const auto paths = Track::BuildPaths(
            Track::CustomLayout(), center, radius);
        for (const auto& points : paths)
        {
            for (std::size_t i = 1; i < points.size(); ++i)
            {
                if (DistanceToSegment(mouse, points[i - 1], points[i]) <= brushPixels)
                { touchesTrack = true; break; }
            }
            if (touchesTrack) break;
        }
        // Enquanto o circuito ainda está inválido, as peças continuam
        // apagáveis individualmente para não prender o usuário à validação.
        if (!touchesTrack)
            for (const auto& piece : Track::CustomLayout().pieces)
            {
                const auto points = Track::SamplePiece(piece, center, radius, 48);
                for (std::size_t i = 1; i < points.size(); ++i)
                {
                    if (DistanceToSegment(mouse, points[i - 1], points[i]) <= brushPixels)
                    { touchesTrack = true; break; }
                }
                if (touchesTrack) break;
            }
        if (!touchesTrack) return;

        const ImVec2 normalized((mouse.x - center.x) / radius,
            (mouse.y - center.y) / radius);
        auto& erases = Track::CustomLayout().overflowErases;
        if (!erases.empty() && Track::Distance(
            erases.back().position, normalized) < g_brushSize * 0.2f)
            return;
        erases.push_back(Track::OverflowErase{
            normalized, g_brushSize, g_brushHardness });
    }

    void SnapSelected()
    {
        auto* selected = Selected();
        if (!selected) return;
        float best = 28.0f;
        ImVec2 correction{};
        for (int ownPort = 0; ownPort < 2; ++ownPort)
        {
            const auto a = Track::GetPort(*selected, ownPort, WorldCenter(), WorldRadius());
            for (const auto& other : Track::CustomLayout().pieces)
            {
                if (other.id == selected->id) continue;
                for (int otherPort = 0; otherPort < 2; ++otherPort)
                {
                    const auto b = Track::GetPort(other, otherPort, WorldCenter(), WorldRadius());
                    const float distance = Track::Distance(a.position, b.position);
                    if (distance < best)
                    {
                        best = distance;
                        correction = ImVec2((b.position.x - a.position.x) / WorldRadius(),
                            (b.position.y - a.position.y) / WorldRadius());
                    }
                }
            }
        }
        if (best < 28.0f)
        {
            selected->position.x += correction.x;
            selected->position.y += correction.y;
        }
    }

    void Button(ImDrawList* draw, const Rect& rect, const char* text,
        bool active = false, bool darkActiveText = false)
    {
        const ImU32 fill = active ? IM_COL32(205, 190, 150, 220) : IM_COL32(30, 31, 37, 235);
        draw->AddRectFilled(rect.min, rect.max, fill, 5.0f);
        draw->AddRect(rect.min, rect.max, IM_COL32(210, 200, 175, 180), 5.0f, 0, 1.2f);
        const ImVec2 size = ImGui::CalcTextSize(text);
        draw->AddText(ImVec2((rect.min.x + rect.max.x - size.x) * 0.5f,
            (rect.min.y + rect.max.y - size.y) * 0.5f),
            active && darkActiveText ? IM_COL32(20, 21, 25, 255) : IM_COL32(245, 242, 230, 255), text);
    }

    void TransparentButton(ImDrawList* draw, const Rect& rect, const char* text,
        bool active = false)
    {
        draw->AddRect(rect.min, rect.max,
            active ? IM_COL32(225, 210, 170, 220) : IM_COL32(170, 165, 155, 150),
            5.0f, 0, active ? 1.8f : 1.2f);
        const ImVec2 size = ImGui::CalcTextSize(text);
        draw->AddText(ImVec2((rect.min.x + rect.max.x - size.x) * 0.5f,
            (rect.min.y + rect.max.y - size.y) * 0.5f),
            IM_COL32(245, 242, 230, 255), text);
    }

    void DisabledButton(ImDrawList* draw, const Rect& rect, const char* text)
    {
        draw->AddRectFilled(rect.min, rect.max, IM_COL32(24, 25, 29, 115), 5.0f);
        draw->AddRect(rect.min, rect.max, IM_COL32(75, 75, 80, 115), 5.0f);
        const ImVec2 size = ImGui::CalcTextSize(text);
        draw->AddText(ImVec2((rect.min.x + rect.max.x - size.x) * 0.5f,
            (rect.min.y + rect.max.y - size.y) * 0.5f),
            IM_COL32(125, 125, 130, 155), text);
    }

    void DisabledValueSlider(ImDrawList* draw, const Rect& rect, const char* label)
    {
        draw->AddText(ImVec2(rect.min.x, rect.min.y - 24.0f),
            IM_COL32(120, 120, 125, 180), label);
        draw->AddRectFilled(rect.min, rect.max, IM_COL32(24, 25, 29, 120), 5.0f);
        draw->AddRect(rect.min, rect.max, IM_COL32(75, 75, 80, 120), 5.0f);
    }

    void ValueSlider(ImDrawList* draw, const Rect& rect, const char* label,
        float value, float minimum, float maximum, bool integer)
    {
        draw->AddText(ImVec2(rect.min.x, rect.min.y - 24.0f),
            IM_COL32(225, 207, 165, 255), label);
        draw->AddRectFilled(rect.min, rect.max, IM_COL32(28, 29, 35, 210), 5.0f);
        const float t = std::clamp((value - minimum) /
            std::max(maximum - minimum, 0.001f), 0.0f, 1.0f);
        draw->AddRectFilled(rect.min,
            ImVec2(rect.min.x + (rect.max.x - rect.min.x) * t, rect.max.y),
            IM_COL32(210, 195, 155, 220), 5.0f);
        draw->AddRect(rect.min, rect.max, IM_COL32(220, 210, 185, 190), 5.0f);
        const std::string text = integer
            ? std::to_string(static_cast<int>(std::lround(value)))
            : std::format("{:.0f}%", value);
        const ImVec2 size = ImGui::CalcTextSize(text.c_str());
        draw->AddText(ImVec2(rect.max.x - size.x, rect.min.y - 24.0f),
            IM_COL32(235, 232, 220, 255), text.c_str());
    }

    void PieceButton(ImDrawList* draw, const Rect& rect, Track::PieceKind kind)
    {
        const ImVec2 center((rect.min.x + rect.max.x) * 0.5f,
            (rect.min.y + rect.max.y) * 0.5f);
        const float buttonRadius = std::min(rect.max.x - rect.min.x,
            rect.max.y - rect.min.y) * 0.46f;
        draw->AddCircleFilled(center, buttonRadius, IM_COL32(30, 31, 37, 170), 48);
        draw->AddCircle(center, buttonRadius, IM_COL32(210, 200, 175, 180), 48, 1.4f);
        const auto points = Track::SamplePieceLocal(kind, 28);
        std::vector<ImVec2> preview;
        preview.reserve(points.size());
        const float scale = std::min(rect.max.x - rect.min.x,
            rect.max.y - rect.min.y) * 0.38f;
        for (const ImVec2& point : points)
            preview.emplace_back(center.x + point.x * scale,
                center.y + point.y * scale);
        if (preview.size() > 1)
            draw->AddPolyline(preview.data(), static_cast<int>(preview.size()),
                IM_COL32(235, 225, 200, 235), 0, 2.2f);
    }

    void FinishFreeDraw()
    {
        if (g_drawPoints.size() < 3) { g_drawPoints.clear(); return; }
        std::vector<ImVec2> normalized;
        normalized.reserve(g_drawPoints.size());
        const ImVec2 center = WorldCenter();
        const float radius = WorldRadius();
        for (const ImVec2& point : g_drawPoints)
            normalized.emplace_back((point.x - center.x) / radius, (point.y - center.y) / radius);
        for (int pass = 0; pass < 2; ++pass)
        {
            std::vector<ImVec2> smooth;
            smooth.push_back(normalized.front());
            for (std::size_t i = 1; i < normalized.size(); ++i)
            {
                const ImVec2 a = normalized[i - 1], b = normalized[i];
                smooth.emplace_back(a.x * 0.75f + b.x * 0.25f,
                    a.y * 0.75f + b.y * 0.25f);
                smooth.emplace_back(a.x * 0.25f + b.x * 0.75f,
                    a.y * 0.25f + b.y * 0.75f);
            }
            smooth.push_back(normalized.back());
            normalized = std::move(smooth);
        }
        const float length = Track::PolylineLength(normalized);
        std::vector<ImVec2> uniform;
        constexpr int samples = 40;
        for (int i = 0; i <= samples; ++i)
            uniform.push_back(i == samples ? normalized.back() :
                Track::SamplePolyline(normalized,
                    length * static_cast<float>(i) / samples).position);

        const float smoothing = g_geometryDrawTool ? 1.0f :
            (g_smoothDraw ? std::clamp(g_drawSmoothing, 0.0f, 100.0f) * 0.01f : 0.0f);
        if (smoothing > 0.001f && uniform.size() > 2)
        {
            ImVec2 minimum = uniform.front();
            ImVec2 maximum = uniform.front();
            for (const ImVec2& point : uniform)
            {
                minimum.x = std::min(minimum.x, point.x);
                minimum.y = std::min(minimum.y, point.y);
                maximum.x = std::max(maximum.x, point.x);
                maximum.y = std::max(maximum.y, point.y);
            }
            const float diagonal = std::max(Track::Distance(minimum, maximum), 0.001f);
            const bool circleGesture = Track::Distance(
                uniform.front(), uniform.back()) <= diagonal * 0.32f;
            std::vector<ImVec2> ideal(uniform.size());
            if (circleGesture)
            {
                ImVec2 centroid{};
                for (const ImVec2& point : uniform)
                {
                    centroid.x += point.x;
                    centroid.y += point.y;
                }
                centroid.x /= static_cast<float>(uniform.size());
                centroid.y /= static_cast<float>(uniform.size());
                float fittedRadius = 0.0f;
                for (const ImVec2& point : uniform)
                    fittedRadius += Track::Distance(point, centroid);
                fittedRadius /= static_cast<float>(uniform.size());
                float signedArea = 0.0f;
                for (std::size_t i = 1; i < uniform.size(); ++i)
                    signedArea += (uniform[i - 1].x - centroid.x) *
                        (uniform[i].y - centroid.y) -
                        (uniform[i - 1].y - centroid.y) *
                        (uniform[i].x - centroid.x);
                const float direction = signedArea < 0.0f ? -1.0f : 1.0f;
                const float start = std::atan2(uniform.front().y - centroid.y,
                    uniform.front().x - centroid.x);
                constexpr float terminalGap = 0.14f;
                for (int i = 0; i <= samples; ++i)
                {
                    const float t = static_cast<float>(i) / samples;
                    const float angle = start + direction *
                        (6.283185307f - terminalGap) * t;
                    ideal[static_cast<std::size_t>(i)] = ImVec2(
                        centroid.x + std::cos(angle) * fittedRadius,
                        centroid.y + std::sin(angle) * fittedRadius);
                }
            }
            else
            {
                const ImVec2 start = uniform.front();
                const ImVec2 end = uniform.back();
                std::size_t bendIndex = uniform.size() / 2;
                float largestBend = -1.0f;
                for (std::size_t i = 1; i + 1 < uniform.size(); ++i)
                {
                    const float bend = DistanceToSegment(
                        uniform[i], start, end);
                    if (bend > largestBend)
                    {
                        largestBend = bend;
                        bendIndex = i;
                    }
                }
                const bool nearlyStraight = largestBend <= diagonal * 0.12f;
                ImVec2 control((start.x + end.x) * 0.5f,
                    (start.y + end.y) * 0.5f);
                if (!nearlyStraight)
                {
                    const float t = std::clamp(static_cast<float>(bendIndex) /
                        static_cast<float>(uniform.size() - 1), 0.08f, 0.92f);
                    const float denominator = 2.0f * (1.0f - t) * t;
                    control.x = (uniform[bendIndex].x -
                        (1.0f - t) * (1.0f - t) * start.x - t * t * end.x) /
                        denominator;
                    control.y = (uniform[bendIndex].y -
                        (1.0f - t) * (1.0f - t) * start.y - t * t * end.y) /
                        denominator;
                }
                for (int i = 0; i <= samples; ++i)
                {
                    const float t = static_cast<float>(i) / samples;
                    const float oneMinusT = 1.0f - t;
                    ideal[static_cast<std::size_t>(i)] = ImVec2(
                        oneMinusT * oneMinusT * start.x +
                            2.0f * oneMinusT * t * control.x + t * t * end.x,
                        oneMinusT * oneMinusT * start.y +
                            2.0f * oneMinusT * t * control.y + t * t * end.y);
                }
            }
            for (std::size_t i = 0; i < uniform.size(); ++i)
            {
                uniform[i].x += (ideal[i].x - uniform[i].x) * smoothing;
                uniform[i].y += (ideal[i].y - uniform[i].y) * smoothing;
            }
        }
        auto& layout = Track::CustomLayout();
        Track::Piece piece;
        piece.id = layout.nextID++;
        piece.kind = Track::PieceKind::FreeDraw;
        piece.customPoints = std::move(uniform);
        layout.pieces.push_back(std::move(piece));
        g_selectedID = layout.pieces.back().id;
        g_drawPoints.clear();
    }
}

namespace TrackEditor
{
    void Open()
    {
        Track::BeginEdit();
        g_open = true;
        g_selectedID = -1;
        g_selectedPort = -1;
        g_dragging = false;
        g_panning = false;
        g_panTool = false;
        g_selectTool = true;
        g_freeDrawTool = false;
        g_geometryDrawTool = false;
        g_smoothDraw = false;
        g_drawing = false;
        g_gateToolActive = false;
        g_gateDragging = false;
        g_liveGateID = -1;
        g_panelScroll = 0.0f;
        g_scrollDragging = false;
        g_activeSlider = EditorSlider::None;
        g_previewGhostItems = 0;
        g_spaceHeld = false;
        g_ctrlHeld = false;
        g_rotating = false;
        g_scaling = false;
        g_eraseTool = false;
        g_erasing = false;
        g_previewPhase = 0.0f;
        g_previewTargetPhase = 0.0f;
        g_lastPreviewSlotT.clear();
        g_frozenPreviewSlotT.clear();
        g_lastPreviewMainPositions.clear();
        g_frozenPreviewMainPositions.clear();
        g_previewSlotsFrozen = false;
        g_undoHistory.clear();
        g_redoHistory.clear();
        g_historyActionPending = false;
        g_resetRadialFlash = 0.0f;
        g_gizmoOwner = -1;
        g_gizmoCenterInitialized = false;
        g_portControls.clear();
        g_presetNameOpen = false;
        g_presetListOpen = false;
        g_pan = ImVec2(0.0f, 0.0f);
        g_zoom = 1.0f;
    }

    void Cancel()
    {
        if (!g_open)
            return;

        Track::CancelEdit();
        g_open = false;
        g_dragging = false;
        g_panning = false;
        g_drawing = false;
        g_gateDragging = false;
        g_liveGateID = -1;
        g_scrollDragging = false;
        g_rotating = false;
        g_scaling = false;
        g_erasing = false;
        g_activeSlider = EditorSlider::None;
        g_spaceHeld = false;
        g_ctrlHeld = false;
        g_drawPoints.clear();
        g_lastPreviewSlotT.clear();
        g_frozenPreviewSlotT.clear();
        g_lastPreviewMainPositions.clear();
        g_frozenPreviewMainPositions.clear();
        g_previewSlotsFrozen = false;
        g_historyActionPending = false;
    }

    bool IsOpen() { return g_open; }

    void Draw(float, const ImVec2& mouse, const ImVec2&,
        const std::vector<PreviewIcon>& icons, int mainVisibleCount)
    {
        auto* draw = ImGui::GetForegroundDrawList();
        const ImVec2 viewportMin = Resolution::ToVirtual(ImVec2(0.0f, 0.0f));
        const ImVec2 viewportMax = Resolution::ToVirtual(Resolution::GetRealSize());
        g_viewportMin = viewportMin;
        g_viewportMax = viewportMax;
        // O draw list raiz nasce limitado ao espaço virtual 1920x1080.
        // Substituímos esse clip pelo viewport físico convertido antes de
        // emitir textos; caso contrário, os glifos são cortados pela CPU e
        // não podem ser recuperados na transformação final de resolução.
        draw->PushClipRect(viewportMin, viewportMax, false);
        const float viewportHeight = viewportMax.y - viewportMin.y;
        const float panelWidth = 390.0f;
        const float panelX = viewportMax.x - panelWidth;
        g_panelLeft = panelX;
        const float ox = panelX + 20.0f;
        const float top = viewportMin.y;
        draw->AddText(ImVec2(ox, top + 20.0f),
            IM_COL32(225, 207, 165, 255), Tr("custom_radial_editor"));

        g_selectButton = {};
        // Linha 1: gerenciamento de presets.
        g_presetSave = { ImVec2(ox, top + 54), ImVec2(ox + 110, top + 88) };
        g_presetLoad = { ImVec2(ox + 120, top + 54), ImVec2(ox + 230, top + 88) };
        g_presetReset = { ImVec2(ox + 240, top + 54), ImVec2(ox + 350, top + 88) };
        const bool presetCanSave = Track::IsValid(Track::CustomLayout());
        if (presetCanSave) Button(draw, g_presetSave, Tr("save"));
        else
        {
            draw->AddRectFilled(g_presetSave.min, g_presetSave.max, IM_COL32(25, 26, 31, 120), 5.0f);
            draw->AddRect(g_presetSave.min, g_presetSave.max, IM_COL32(100, 100, 100, 100), 5.0f);
            draw->AddText(ImVec2(g_presetSave.min.x + 34, g_presetSave.min.y + 9),
                IM_COL32(135, 135, 135, 150), Tr("save"));
        }
        Button(draw, g_presetLoad, Tr("load"), g_presetListOpen);
        Button(draw, g_presetReset, Tr("reset"));

        // Linha 2: os dois zooms compartilham toda a largura do painel.
        g_zoomOut = { ImVec2(ox, top + 96), ImVec2(ox + 170, top + 130) };
        g_zoomIn = { ImVec2(ox + 180, top + 96), ImVec2(ox + 350, top + 130) };
        Button(draw, g_zoomOut, Tr("zoom_out"));
        Button(draw, g_zoomIn, Tr("zoom_in"));

        // Linha 3: restaurar a visão ocupa toda a linha.
        g_reset = { ImVec2(ox, top + 138), ImVec2(ox + 350, top + 172) };
        Button(draw, g_reset, Tr("reset_view"));

        const float contentHeight = 1940.0f;
        const float visibleHeight = std::max(1.0f, viewportHeight - 280.0f);
        const float maxScroll = std::max(0.0f, contentHeight - visibleHeight);
        g_panelMaxScroll = maxScroll;
        g_panelScroll = std::clamp(g_panelScroll, 0.0f, maxScroll);
        const float sy = -g_panelScroll;
        g_panelClipTop = top + 190.0f;
        g_panelClipBottom = viewportMax.y - 142.0f;
        draw->PushClipRect(ImVec2(panelX, g_panelClipTop),
            ImVec2(viewportMax.x, g_panelClipBottom), true);
        draw->AddText(ImVec2(ox, top + 202 + sy), IM_COL32(225, 207, 165, 255), Tr("pieces"));
        draw->AddLine(ImVec2(ox + 72, top + 212 + sy), ImVec2(ox + 350, top + 212 + sy),
            IM_COL32(190, 150, 70, 210), 1.2f);
        g_drawPathButton = { ImVec2(ox + 127, top + 244 + sy), ImVec2(ox + 223, top + 340 + sy) };
        const ImVec2 drawPathCenter(ox + 175.0f, top + 292.0f + sy);
        draw->AddCircleFilled(drawPathCenter, 44.0f,
            g_freeDrawTool ? IM_COL32(120, 108, 78, 235) : IM_COL32(30, 31, 37, 185), 48);
        draw->AddCircle(drawPathCenter, 45.0f, IM_COL32(225, 207, 165, 230), 48, 1.7f);
        draw->PathLineTo(ImVec2(drawPathCenter.x - 18, drawPathCenter.y + 8));
        draw->PathBezierCubicCurveTo(ImVec2(drawPathCenter.x - 6, drawPathCenter.y - 18),
            ImVec2(drawPathCenter.x + 7, drawPathCenter.y + 18),
            ImVec2(drawPathCenter.x + 19, drawPathCenter.y - 8), 20);
        draw->PathStroke(IM_COL32(245, 238, 218, 245), 0, 2.5f);
        draw->AddText(ImVec2(ox + 235, top + 282 + sy),
            IM_COL32(225, 207, 165, 255), Tr("draw_path"));
        g_smoothDrawButton = { ImVec2(ox + 12, top + 268 + sy),
            ImVec2(ox + 112, top + 316 + sy) };
        Button(draw, g_smoothDrawButton, Tr("smooth_line"), g_smoothDraw, true);
        g_drawSmoothingSlider = { ImVec2(ox, top + 365 + sy),
            ImVec2(ox + 350, top + 387 + sy) };
        if (g_activeSlider == EditorSlider::DrawSmoothing && g_smoothDraw)
        {
            g_drawSmoothing = std::clamp(
                (mouse.x - g_drawSmoothingSlider.min.x) /
                    (g_drawSmoothingSlider.max.x - g_drawSmoothingSlider.min.x) * 100.0f,
                0.0f, 100.0f);
        }
        if (g_smoothDraw)
            ValueSlider(draw, g_drawSmoothingSlider, Tr("smoothing_amount"),
                g_drawSmoothing, 0.0f, 100.0f, false);
        else
        {
            draw->AddText(ImVec2(g_drawSmoothingSlider.min.x,
                g_drawSmoothingSlider.min.y - 24.0f),
                IM_COL32(120, 120, 125, 180), Tr("smoothing_amount"));
            draw->AddRectFilled(g_drawSmoothingSlider.min,
                g_drawSmoothingSlider.max, IM_COL32(24, 25, 29, 120), 5.0f);
            draw->AddRect(g_drawSmoothingSlider.min, g_drawSmoothingSlider.max,
                IM_COL32(75, 75, 80, 120), 5.0f);
        }
        for (std::size_t i = 0; i < kPieceKinds.size(); ++i)
        {
            // Preserva as duas colunas originais (seis peças em cada uma) e
            // dedica exclusivamente a terceira coluna aos seis formatos novos.
            const int column = i < 12 ? static_cast<int>(i % 2) : 2;
            const int row = i < 12 ? static_cast<int>(i / 2) : static_cast<int>(i - 12);
            const float bx = ox + column * 120.0f;
            const float by = top + 410.0f + row * 103.0f + sy;
            g_pieceButtons[i] = { ImVec2(bx, by), ImVec2(bx + 110.0f, by + 96.0f) };
            PieceButton(draw, g_pieceButtons[i], kPieceKinds[i]);
        }
        g_reverseFlow = {};
        g_geometryDrawButton = {
            ImVec2(ox + 35, top + 1040 + sy), ImVec2(ox + 315, top + 1082 + sy) };
        Button(draw, g_geometryDrawButton, Tr("curve_circle_tool"),
            g_geometryDrawTool, true);
        g_duplicateSelectedButton = {
            ImVec2(ox + 35, top + 1092 + sy), ImVec2(ox + 315, top + 1134 + sy) };
        if (Selected())
            Button(draw, g_duplicateSelectedButton, Tr("duplicate_selected"));
        else
            DisabledButton(draw, g_duplicateSelectedButton, Tr("duplicate_selected"));

        // Mantém o mesmo tamanho do botão de duplicar e fica logo abaixo dele.
        g_flipPoles = {
            ImVec2(ox + 35, top + 1142 + sy), ImVec2(ox + 315, top + 1184 + sy) };
        Button(draw, g_flipPoles, Tr("clear_terminals"));

        draw->AddText(ImVec2(ox, top + 1194 + sy),
            IM_COL32(225, 207, 165, 255), Tr("radial_editor_section"));
        draw->AddLine(ImVec2(ox + 72, top + 1204 + sy),
            ImVec2(ox + 350, top + 1204 + sy), IM_COL32(190, 150, 70, 210), 1.2f);
        g_radialShapeSlider = {
            ImVec2(ox, top + 1242 + sy), ImVec2(ox + 350, top + 1264 + sy) };
        g_radialRotationSlider = {
            ImVec2(ox, top + 1324 + sy), ImVec2(ox + 350, top + 1346 + sy) };

        g_resetRadialDrawing = {
            ImVec2(ox, top + 1385 + sy), ImVec2(ox + 32, top + 1417 + sy) };
        g_eraseRadialButton = {
            ImVec2(ox + 42, top + 1380 + sy), ImVec2(ox + 225, top + 1422 + sy) };
        g_eraserRadialMode = {
            ImVec2(ox + 234, top + 1377 + sy), ImVec2(ox + 350, top + 1399 + sy) };
        g_eraserOverflowMode = {
            ImVec2(ox + 234, top + 1401 + sy), ImVec2(ox + 350, top + 1423 + sy) };
        Button(draw, g_eraseRadialButton, Tr("editor_eraser"), g_eraseTool, true);
        g_resetRadialFlash = std::max(0.0f,
            g_resetRadialFlash - ImGui::GetIO().DeltaTime);
        const bool resetLit = g_resetRadialFlash > 0.0f;
        draw->AddCircleFilled(ImVec2(ox + 16, top + 1401 + sy), 13.0f,
            resetLit ? IM_COL32(242, 242, 238, 245) : IM_COL32(48, 49, 55, 245), 28);
        draw->AddCircle(ImVec2(ox + 16, top + 1401 + sy), 14.0f,
            IM_COL32(120, 115, 105, 230), 28, 1.4f);
        if (Inside(mouse, g_resetRadialDrawing))
            draw->AddText(ImVec2(ox, top + 1358 + sy),
                IM_COL32(240, 230, 205, 255), Tr("reset"));
        const auto drawEraserMode = [&](const Rect& rect, bool selected,
            const char* label) {
            const ImVec2 circle(rect.min.x + 10.0f,
                (rect.min.y + rect.max.y) * 0.5f);
            const ImU32 enabled = g_eraseTool
                ? IM_COL32(230, 220, 195, 245) : IM_COL32(90, 91, 96, 150);
            draw->AddCircle(circle, 7.0f, enabled, 24, 1.4f);
            if (selected && g_eraseTool)
                draw->AddCircleFilled(circle, 4.0f,
                    IM_COL32(225, 200, 135, 255), 20);
            draw->AddText(ImVec2(rect.min.x + 22.0f, rect.min.y + 3.0f),
                enabled, label);
        };
        drawEraserMode(g_eraserRadialMode, !g_eraseOverflowMode,
            Tr("eraser_radial_mode"));
        drawEraserMode(g_eraserOverflowMode, g_eraseOverflowMode,
            Tr("eraser_overflow_mode"));
        g_brushSizeSlider = {
            ImVec2(ox, top + 1462 + sy), ImVec2(ox + 350, top + 1484 + sy) };
        g_brushHardnessSlider = {
            ImVec2(ox, top + 1524 + sy), ImVec2(ox + 350, top + 1546 + sy) };
        g_radialOpacitySlider = {
            ImVec2(ox, top + 1586 + sy), ImVec2(ox + 350, top + 1608 + sy) };
        g_lineOpacitySlider = {
            ImVec2(ox, top + 1648 + sy), ImVec2(ox + 350, top + 1670 + sy) };
        g_radialQuantitySlider = {
            ImVec2(ox, top + 1710 + sy), ImVec2(ox + 350, top + 1732 + sy) };
        g_radialStretchSlider = {
            ImVec2(ox, top + 1772 + sy), ImVec2(ox + 350, top + 1794 + sy) };
        g_ghostItemsSlider = {
            ImVec2(ox, top + 1834 + sy), ImVec2(ox + 350, top + 1856 + sy) };
        const auto sliderValue = [&](const Rect& rect, float minimum, float maximum) {
            return minimum + std::clamp(
                (mouse.x - rect.min.x) / (rect.max.x - rect.min.x), 0.0f, 1.0f) *
                (maximum - minimum);
        };
        if (g_activeSlider == EditorSlider::RadialShape)
        {
            const float normalized = std::clamp(
                (mouse.x - g_radialShapeSlider.min.x) /
                    (g_radialShapeSlider.max.x - g_radialShapeSlider.min.x),
                0.0f, 1.0f);
            Config::g_radialShape = std::clamp(static_cast<int>(
                normalized * RadialShape::Count()), 0, RadialShape::Count() - 1);
        }
        else if (g_activeSlider == EditorSlider::RadialQuantity)
        {
            Config::g_radialQuantity = std::round(sliderValue(
                g_radialQuantitySlider, 3.0f, 50.0f));
            Config::g_radialStretch = std::max(Config::g_radialStretch,
                std::max(0.0f, (Config::g_radialQuantity - 25.0f) * (80.0f / 25.0f)));
        }
        else if (g_activeSlider == EditorSlider::RadialStretch)
        {
            Config::g_radialStretch = sliderValue(g_radialStretchSlider, 0.0f, 100.0f);
            const float maximumQuantity = 25.0f +
                std::clamp(Config::g_radialStretch, 0.0f, 80.0f) * (25.0f / 80.0f);
            Config::g_radialQuantity = std::min(
                Config::g_radialQuantity, std::floor(maximumQuantity));
        }
        else if (g_activeSlider == EditorSlider::GhostItems)
        {
            g_previewGhostItems = static_cast<int>(std::lround(
                sliderValue(g_ghostItemsSlider, 0.0f, 50.0f)));
        }
        else if (g_activeSlider == EditorSlider::RadialOpacity)
            Track::CustomLayout().radialLineOpacity =
                sliderValue(g_radialOpacitySlider, 0.0f, 100.0f);
        else if (g_activeSlider == EditorSlider::RadialRotation)
        {
            auto& layout = Track::CustomLayout();
            const float nextRotation = sliderValue(
                g_radialRotationSlider, -180.0f, 180.0f) * 0.01745329252f;
            const float delta = nextRotation - layout.radialRotation;
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
            layout.radialRotation = nextRotation;
        }
        else if (g_activeSlider == EditorSlider::BrushSize && g_eraseTool)
            g_brushSize = sliderValue(g_brushSizeSlider, 2.0f, 35.0f) * 0.01745329252f;
        else if (g_activeSlider == EditorSlider::BrushHardness && g_eraseTool)
            g_brushHardness = sliderValue(g_brushHardnessSlider, 0.0f, 100.0f) * 0.01f;
        if (g_eraseTool)
        {
            ValueSlider(draw, g_brushSizeSlider, Tr("eraser_size"),
                g_brushSize * 57.2957795f, 2.0f, 35.0f, true);
            ValueSlider(draw, g_brushHardnessSlider, Tr("eraser_hardness"),
                g_brushHardness * 100.0f, 0.0f, 100.0f, false);
        }
        else
        {
            DisabledValueSlider(draw, g_brushSizeSlider, Tr("eraser_size"));
            DisabledValueSlider(draw, g_brushHardnessSlider, Tr("eraser_hardness"));
        }

        draw->AddText(ImVec2(g_radialShapeSlider.min.x,
            g_radialShapeSlider.min.y - 24.0f), IM_COL32(225, 207, 165, 255),
            Language::Get("radial_shape").c_str());
        draw->AddRectFilled(g_radialShapeSlider.min, g_radialShapeSlider.max,
            IM_COL32(28, 29, 35, 220), 5.0f);
        const int shapeCount = RadialShape::Count();
        const int selectedShape = std::clamp(
            Config::g_radialShape, 0, shapeCount - 1);
        const float shapeCell = (g_radialShapeSlider.max.x -
            g_radialShapeSlider.min.x) / static_cast<float>(shapeCount);
        draw->AddRectFilled(
            ImVec2(g_radialShapeSlider.min.x + shapeCell * selectedShape + 1.0f,
                g_radialShapeSlider.min.y + 2.0f),
            ImVec2(g_radialShapeSlider.min.x + shapeCell * (selectedShape + 1) - 1.0f,
                g_radialShapeSlider.max.y - 2.0f),
            IM_COL32(220, 210, 185, 235), 2.0f);
        for (int i = 0; i <= shapeCount; ++i)
        {
            const float x = g_radialShapeSlider.min.x + shapeCell * i;
            draw->AddLine(ImVec2(x, g_radialShapeSlider.min.y + 5.0f),
                ImVec2(x, g_radialShapeSlider.max.y - 5.0f),
                IM_COL32(160, 160, 165, 180), 1.0f);
        }
        const char* shapeName = ShapeDisplayName(
            static_cast<RadialShape::Style>(selectedShape));
        const ImVec2 shapeNameSize = ImGui::CalcTextSize(shapeName);
        draw->AddText(ImVec2(ox + (350.0f - shapeNameSize.x) * 0.5f,
            g_radialShapeSlider.max.y + 7.0f), IM_COL32(225, 220, 205, 255), shapeName);
        if (g_activeSlider == EditorSlider::OverflowOpacity)
            Track::CustomLayout().lineOpacity = std::clamp(
                (mouse.x - g_lineOpacitySlider.min.x) /
                (g_lineOpacitySlider.max.x - g_lineOpacitySlider.min.x) * 100.0f,
                0.0f, 100.0f);
        draw->AddRectFilled(g_lineOpacitySlider.min, g_lineOpacitySlider.max,
            IM_COL32(28, 29, 35, 210), 5.0f);
        const float opacityX = g_lineOpacitySlider.min.x +
            (g_lineOpacitySlider.max.x - g_lineOpacitySlider.min.x) *
            Track::CustomLayout().lineOpacity * 0.01f;
        draw->AddRectFilled(g_lineOpacitySlider.min,
            ImVec2(opacityX, g_lineOpacitySlider.max.y), IM_COL32(210, 195, 155, 220), 5.0f);
        draw->AddRect(g_lineOpacitySlider.min, g_lineOpacitySlider.max,
            IM_COL32(220, 210, 185, 190), 5.0f);
        draw->AddText(ImVec2(g_lineOpacitySlider.min.x,
            g_lineOpacitySlider.min.y - 24.0f), IM_COL32(225, 207, 165, 255),
            Tr("overflow_line_opacity"));
        ValueSlider(draw, g_radialRotationSlider, Tr("radial_rotation"),
            Track::CustomLayout().radialRotation * 57.2957795f, -180.0f, 180.0f, true);
        ValueSlider(draw, g_radialOpacitySlider, Tr("radial_line_opacity"),
            Track::CustomLayout().radialLineOpacity, 0.0f, 100.0f, false);
        ValueSlider(draw, g_radialQuantitySlider, Tr("main_radial_items"),
            Config::g_radialQuantity, 3.0f, 50.0f, true);
        ValueSlider(draw, g_radialStretchSlider, Tr("main_radial_stretch"),
            Config::g_radialStretch, 0.0f, 100.0f, false);
        ValueSlider(draw, g_ghostItemsSlider, Tr("editor_ghost_items"),
            static_cast<float>(g_previewGhostItems), 0.0f, 50.0f, true);
        draw->AddText(ImVec2(ox, top + 1896 + sy), IM_COL32(180, 180, 185, 255),
            Tr("editor_instructions"));
        draw->PopClipRect();

        if (maxScroll > 0.0f)
        {
            const float trackTop = top + 197.0f;
            const float trackBottom = viewportMax.y - 150.0f;
            g_scrollTrackTop = trackTop;
            g_scrollTrackBottom = trackBottom;
            const float thumbHeight = std::max(28.0f,
                (trackBottom - trackTop) * visibleHeight / contentHeight);
            if (g_scrollDragging)
            {
                const float travel = std::max(1.0f,
                    trackBottom - trackTop - thumbHeight);
                const float thumbTop = std::clamp(mouse.y - g_scrollGrabOffset,
                    trackTop, trackTop + travel);
                g_panelScroll = (thumbTop - trackTop) / travel * maxScroll;
            }
            const float thumbY = trackTop + (trackBottom - trackTop - thumbHeight) *
                (g_panelScroll / maxScroll);
            g_scrollThumb = { ImVec2(viewportMax.x - 12.0f, thumbY - 3.0f),
                ImVec2(viewportMax.x, thumbY + thumbHeight + 3.0f) };
            draw->AddRectFilled(ImVec2(viewportMax.x - 7.0f, trackTop),
                ImVec2(viewportMax.x - 3.0f, trackBottom), IM_COL32(35, 36, 42, 220), 3.0f);
            draw->AddRectFilled(ImVec2(viewportMax.x - 8.0f, thumbY),
                ImVec2(viewportMax.x - 2.0f, thumbY + thumbHeight),
                IM_COL32(190, 184, 170, 220), 3.0f);
        }

        std::string reason;
        const bool valid = Track::IsValid(Track::CustomLayout(), &reason);
        g_save = { ImVec2(ox, viewportMax.y - 92), ImVec2(ox + 170, viewportMax.y - 50) };
        g_cancel = { ImVec2(ox + 180, viewportMax.y - 92), ImVec2(ox + 350, viewportMax.y - 50) };
        TransparentButton(draw, g_save, valid ? Tr("save") : Tr("invalid"), valid);
        Button(draw, g_cancel, Tr("cancel"));
        draw->AddText(ImVec2(ox, viewportMax.y - 126), valid ? IM_COL32(145, 220, 155, 255) :
            IM_COL32(230, 130, 120, 255), valid ? Tr("circuit_ready") : ValidationReason(reason));

        g_canvasCenter = ImVec2((viewportMin.x + panelX) * 0.5f,
            (viewportMin.y + viewportMax.y) * 0.5f);
        const float maximumCanvasRadius = std::max(175.0f,
            std::min((panelX - viewportMin.x) * 0.5f, viewportHeight * 0.5f) - 44.0f);
        g_canvasRadius = 175.0f + (maximumCanvasRadius - 175.0f) *
            std::clamp(Config::g_radialStretch, 0.0f, 100.0f) * 0.01f;
        const ImVec2 center = WorldCenter();
        const float radius = WorldRadius();
        UpdateLiveRadialGate(mouse);
        draw->AddCircleFilled(center, radius + 10.0f, IM_COL32(18, 20, 25, 65), 128);
        const float radialOpacity = std::clamp(
            Track::CustomLayout().radialLineOpacity, 0.0f, 100.0f) * 0.01f;
        DrawEditingRadial(draw, center, radius + 10.0f, IM_COL32_WHITE,
            80.0f * radialOpacity, 1.5f);
        DrawEditingRadial(draw, center, radius, IM_COL32_WHITE,
            125.0f * radialOpacity, 2.5f);
        g_centerReverseFlow = { ImVec2(center.x - 18.0f, center.y - 18.0f),
            ImVec2(center.x + 18.0f, center.y + 18.0f) };
        if (g_activeSlider == EditorSlider::BrushSize ||
            g_activeSlider == EditorSlider::BrushHardness)
        {
            const float brushPreviewRadius = std::max(8.0f, g_brushSize * radius);
            const float hardRadius = brushPreviewRadius * g_brushHardness;
            draw->AddCircleFilled(center, hardRadius, IM_COL32(255, 255, 255, 105), 64);
            constexpr int fadeRings = 12;
            for (int ring = fadeRings; ring >= 1; --ring)
            {
                const float t = static_cast<float>(ring) / fadeRings;
                const float ringRadius = hardRadius +
                    (brushPreviewRadius - hardRadius) * t;
                const int opacity = static_cast<int>(105.0f * (1.0f - t));
                draw->AddCircleFilled(center, ringRadius,
                    IM_COL32(255, 255, 255, opacity), 64);
            }
            draw->AddCircle(center, brushPreviewRadius,
                IM_COL32(255, 255, 255, 100), 64, 1.0f);
        }
        const bool clockwise = Track::CustomLayout().flowDirection > 0;
        const auto centerHalf = [&](float from, float to, ImU32 color) {
            draw->PathLineTo(center);
            draw->PathArcTo(center, 16.0f, from, to, 20);
            draw->PathLineTo(center);
            draw->PathFillConvex(color);
        };
        if (clockwise)
        {
            centerHalf(-1.5707963f, 1.5707963f, IM_COL32(245, 245, 240, 245));
            centerHalf(1.5707963f, 4.7123890f, IM_COL32(22, 24, 29, 245));
        }
        else
        {
            centerHalf(-1.5707963f, 1.5707963f, IM_COL32(22, 24, 29, 245));
            centerHalf(1.5707963f, 4.7123890f, IM_COL32(245, 245, 240, 245));
        }
        draw->AddCircle(center, 17.0f, IM_COL32(225, 205, 155, 220), 32, 1.5f);
        if (g_dragging)
        {
            if (auto* selected = Selected())
            {
                selected->position = ImVec2((mouse.x - center.x) / radius - g_dragOffset.x,
                    (mouse.y - center.y) / radius - g_dragOffset.y);
            }
        }
        if (g_panning)
        {
            g_pan.x += mouse.x - g_lastMouse.x;
            g_pan.y += mouse.y - g_lastMouse.y;
            g_lastMouse = mouse;
        }
        if (g_rotating)
        {
            if (auto* selected = Selected())
            {
                const ImVec2 pieceCenter = Track::TransformPoint(
                    *selected, ImVec2{}, center, radius);
                selected->rotation = std::atan2(mouse.y - pieceCenter.y,
                    mouse.x - pieceCenter.x) + g_rotateOffset;
            }
        }
        if (g_scaling)
        {
            if (auto* selected = Selected())
            {
                const float movement = g_scaleDirection > 0
                    ? g_scaleStartY - mouse.y : mouse.y - g_scaleStartY;
                selected->scale = std::clamp(
                    g_scaleStartValue + movement * 0.006f, 0.25f, 3.0f);
            }
        }
        if (g_erasing)
        {
            if (g_eraseOverflowMode) EraseOverflowAt(mouse);
            else EraseRadialAt(mouse);
        }

        for (const auto& piece : Track::CustomLayout().pieces)
        {
            const auto points = Track::SamplePiece(piece, center, radius, 36);
            const bool selected = piece.id == g_selectedID;
            const float whitePreviewOpacity =
                g_activeSlider == EditorSlider::OverflowOpacity
                ? std::clamp(Track::CustomLayout().lineOpacity, 0.0f, 100.0f) * 0.01f
                : 1.0f;
            if (points.size() > 1)
            {
                const ImU32 rgb = selected
                    ? IM_COL32(245, 215, 135, 255)
                    : IM_COL32(220, 224, 235, 255);
                const float baseAlpha = (selected ? 255.0f : 210.0f) *
                    whitePreviewOpacity;
                for (std::size_t i = 1; i < points.size(); ++i)
                {
                    const ImVec2 middle((points[i - 1].x + points[i].x) * 0.5f,
                        (points[i - 1].y + points[i].y) * 0.5f);
                    const int segmentAlpha = static_cast<int>(baseAlpha *
                        EditingOverflowVisibility(middle));
                    if (segmentAlpha <= 0) continue;
                    draw->AddLine(points[i - 1], points[i],
                        (rgb & 0x00FFFFFFu) |
                            (static_cast<ImU32>(segmentAlpha) << 24u),
                        selected ? 4.0f : 2.5f);
                }
            }
            for (int port = 0; port < 2; ++port)
            {
                const auto p = Track::GetPort(piece, port, center, radius).position;
                const auto gate = std::ranges::find_if(Track::CustomLayout().gates,
                    [&](const Track::Gate& value) { return value.trackT < 0.0f &&
                        value.pieceID == piece.id && value.portIndex == port; });
                const bool hovered = Track::Distance(p, mouse) <= 27.0f;
                const ImU32 fill = gate == Track::CustomLayout().gates.end()
                    ? IM_COL32(35, 38, 48, 255)
                    : gate->kind == Track::GateKind::Entry
                        ? IM_COL32(90, 210, 135, 245) : IM_COL32(225, 95, 85, 245);
                draw->AddCircleFilled(p, hovered ? 10.0f : 8.0f, fill, 24);
                draw->AddCircle(p, hovered ? 11.0f : 9.0f,
                    IM_COL32(225, 195, 120, 230), 24, 2.0f);
            }
        }
        // Pontes explícitas entre extremidades permanecem parte do trilho e
        // são desenhadas antes dos terminais/pizzas para não cobrir controles.
        for (const auto& connection : Track::CustomLayout().connections)
        {
            const auto a = std::ranges::find_if(Track::CustomLayout().pieces,
                [&](const Track::Piece& piece) { return piece.id == connection.pieceA; });
            const auto b = std::ranges::find_if(Track::CustomLayout().pieces,
                [&](const Track::Piece& piece) { return piece.id == connection.pieceB; });
            if (a == Track::CustomLayout().pieces.end() ||
                b == Track::CustomLayout().pieces.end()) continue;
            const ImVec2 from = Track::GetPort(*a, connection.portA, center, radius).position;
            const ImVec2 to = Track::GetPort(*b, connection.portB, center, radius).position;
            const ImU32 fromColor = connection.sourceKind == Track::GateKind::Exit
                ? IM_COL32(225, 95, 85, 235) : IM_COL32(90, 210, 135, 235);
            const ImU32 toColor = connection.sourceKind == Track::GateKind::Exit
                ? IM_COL32(90, 210, 135, 235) : IM_COL32(225, 95, 85, 235);
            const ImVec2 middle((from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f);
            draw->AddLine(from, middle, fromColor, 3.0f);
            draw->AddLine(middle, to, toColor, 3.0f);
        }
        UpdateGizmoRects();
        for (const auto& gate : Track::CustomLayout().gates)
        {
            ImVec2 rail{};
            if (gate.trackT >= 0.0f)
            {
                const auto path = Track::BuildClosedPath(Track::CustomLayout(), center, radius);
                if (path.empty()) continue;
                rail = Track::SamplePolyline(path,
                    Track::PolylineLength(path) * gate.trackT).position;
            }
            else
            {
                const auto piece = std::ranges::find_if(Track::CustomLayout().pieces,
                    [&](const Track::Piece& value) { return value.id == gate.pieceID; });
                if (piece == Track::CustomLayout().pieces.end()) continue;
                rail = Track::GetPort(*piece, gate.portIndex, center, radius).position;
            }
            const float mainAngle = gate.mainAngle + Track::CustomLayout().radialRotation;
            const ImVec2 main = EditingRadialPoint(center, radius, mainAngle);
            const ImU32 color = gate.kind == Track::GateKind::Entry
                ? IM_COL32(105, 210, 145, 235) : IM_COL32(225, 135, 105, 235);
            draw->AddLine(main, rail, color, 3.0f);
            draw->AddCircleFilled(main, 7.0f, color, 24);
            const ImVec2 from = gate.kind == Track::GateKind::Entry ? main : rail;
            const ImVec2 to = gate.kind == Track::GateKind::Entry ? rail : main;
            const ImVec2 middle((from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f);
            const float angle = std::atan2(to.y - from.y, to.x - from.x);
            draw->AddTriangleFilled(
                ImVec2(middle.x + std::cos(angle) * 9.0f, middle.y + std::sin(angle) * 9.0f),
                ImVec2(middle.x + std::cos(angle + 2.45f) * 7.0f, middle.y + std::sin(angle + 2.45f) * 7.0f),
                ImVec2(middle.x + std::cos(angle - 2.45f) * 7.0f, middle.y + std::sin(angle - 2.45f) * 7.0f), color);
        }
        if (valid)
        {
            const auto paths = Track::BuildPaths(Track::CustomLayout(), center, radius);
            for (const auto& path : paths)
            {
                if (path.size() <= 1) continue;
                const float pathLength = Track::PolylineLength(path);
                // O trilho externo sempre flui da saída vermelha para a
                // entrada verde. O seletor central controla somente o giro
                // do radial principal.
                constexpr int direction = 1;
                for (float distance = 35.0f; distance < pathLength; distance += 90.0f)
                {
                    const auto sample = Track::SamplePolyline(path,
                        direction > 0 ? distance : pathLength - distance);
                    const float angle = std::atan2(sample.tangent.y * direction,
                        sample.tangent.x * direction);
                    draw->AddTriangleFilled(
                        ImVec2(sample.position.x + std::cos(angle) * 11.0f,
                            sample.position.y + std::sin(angle) * 11.0f),
                        ImVec2(sample.position.x + std::cos(angle + 2.5f) * 8.0f,
                            sample.position.y + std::sin(angle + 2.5f) * 8.0f),
                        ImVec2(sample.position.x + std::cos(angle - 2.5f) * 8.0f,
                            sample.position.y + std::sin(angle - 2.5f) * 8.0f),
                        IM_COL32(245, 210, 115, 245));
                }
            }

            // A seleção é uma camada de destaque, não parte da linha-base.
            // Redesenhá-la depois do circuito verde impede que a validação
            // esconda o amarelo da peça selecionada.
            if (const auto* selected = Selected())
            {
                const auto selectedPoints = Track::SamplePiece(
                    *selected, center, radius, 36);
                if (selectedPoints.size() > 1)
                    for (std::size_t i = 1; i < selectedPoints.size(); ++i)
                    {
                        const ImVec2 middle(
                            (selectedPoints[i - 1].x + selectedPoints[i].x) * 0.5f,
                            (selectedPoints[i - 1].y + selectedPoints[i].y) * 0.5f);
                        const int opacity = static_cast<int>(255.0f *
                            EditingOverflowVisibility(middle));
                        if (opacity <= 0) continue;
                        draw->AddLine(selectedPoints[i - 1], selectedPoints[i],
                            IM_COL32(245, 215, 135, opacity), 4.0f);
                    }
            }

            std::vector<PreviewIcon> editorIcons = icons;
            editorIcons.reserve(icons.size() + static_cast<std::size_t>(g_previewGhostItems));
            for (int i = 0; i < g_previewGhostItems; ++i)
            {
                PreviewIcon ghost{};
                if (!icons.empty())
                    ghost = icons[static_cast<std::size_t>(i) % icons.size()];
                ghost.color = (ghost.color & 0x00FFFFFFu) | 0xA0000000u;
                editorIcons.push_back(ghost);
            }

            if (!editorIcons.empty() && !paths.empty())
            {
                const auto& layout = Track::CustomLayout();
                const int iconCount = static_cast<int>(editorIcons.size());
                // O menu já fornece a quantidade efetivamente visível no
                // radial principal. Recalculá-la aqui a partir da configuração
                // global pode divergir do radial real (especialmente depois de
                // carregar/trocar um layout) e classificar todos os ícones como
                // principais, esvaziando o preview do circuito excedente.
                // Os fantasmas entram no preview antes da divisão entre
                // radial principal e excedente. Recalcular aqui permite que
                // preencham primeiro as casas principais ainda vazias.
                const float stretchCapacity = 25.0f +
                    std::clamp(Config::g_radialStretch, 0.0f, 80.0f) *
                        (25.0f / 80.0f);
                const int maximumMainSlots = std::clamp(
                    static_cast<int>(std::floor(stretchCapacity)), 25, 50);
                const int previewMainCapacity = std::clamp(
                    static_cast<int>(std::lround(Config::g_radialQuantity)),
                    3, maximumMainSlots);
                mainVisibleCount = std::min(iconCount, previewMainCapacity);
                const int overflowCount = iconCount - mainVisibleCount;
                const auto circuit = Track::BuildCircuit(
                    layout, center, radius, true, CurrentRadialShape());
                const float circuitLength = Track::PolylineLength(circuit);
                const bool transformingPiece =
                    g_dragging || g_rotating || g_scaling;
                const auto previewSlots = Track::CircuitSlots(layout,
                    center, radius, true, mainVisibleCount, overflowCount,
                    CurrentRadialShape());
                std::vector<ImVec2> slotPositions;
                slotPositions.reserve(previewSlots.size());
                for (const auto& slot : previewSlots)
                    slotPositions.push_back(slot.position);
                if (transformingPiece && g_previewSlotsFrozen)
                {
                    // Só os itens internos do radial principal conservam a
                    // posição anterior. Terminais e todos os excedentes
                    // continuam livres para acompanhar a peça em tempo real.
                    std::size_t frozenMainPosition = 0;
                    for (std::size_t i = 0; i < previewSlots.size(); ++i)
                    {
                        if (!previewSlots[i].main) continue;
                        if (frozenMainPosition < g_frozenPreviewMainPositions.size())
                            slotPositions[i] =
                                g_frozenPreviewMainPositions[frozenMainPosition];
                        ++frozenMainPosition;
                    }
                }
                else
                {
                    if (transformingPiece)
                    {
                        g_frozenPreviewSlotT.clear();
                        for (const auto& slot : previewSlots)
                            if (slot.main && !slot.terminal)
                                g_frozenPreviewSlotT.push_back(slot.circuitT);
                        g_frozenPreviewMainPositions.clear();
                        for (const auto& slot : previewSlots)
                            if (slot.main)
                                g_frozenPreviewMainPositions.push_back(slot.position);
                        g_previewSlotsFrozen = !slotPositions.empty();
                    }
                    else
                    {
                        g_lastPreviewSlotT.clear();
                        for (const auto& slot : previewSlots)
                            if (slot.main && !slot.terminal)
                                g_lastPreviewSlotT.push_back(slot.circuitT);
                        g_lastPreviewMainPositions.clear();
                        for (const auto& slot : previewSlots)
                            if (slot.main)
                                g_lastPreviewMainPositions.push_back(slot.position);
                    }
                }

                // CircuitSlot::position é a referência visual única do
                // Editor e do WheelSettings. Reconstruir a posição a partir
                // de circuitT fazia o preview atravessar o trilho externo
                // mesmo quando não havia excedentes.
                if (slotPositions.size() == editorIcons.size())
                {
                    const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f);
                    g_previewPhase += (g_previewTargetPhase - g_previewPhase) *
                        (1.0f - std::exp(-13.0f * dt));
                    for (int i = 0; i < iconCount; ++i)
                    {
                        const float slot = static_cast<float>(i) + g_previewPhase;
                        const float floorSlot = std::floor(slot);
                        const float fraction = slot - floorSlot;
                        const auto wrapSlot = [&](int value) {
                            value %= iconCount;
                            return value < 0 ? value + iconCount : value;
                        };
                        const float smooth = fraction * fraction * (3.0f - 2.0f * fraction);
                        const ImVec2& fromPosition = slotPositions[
                            static_cast<std::size_t>(wrapSlot(
                                static_cast<int>(floorSlot)))];
                        const ImVec2& toPosition = slotPositions[
                            static_cast<std::size_t>(wrapSlot(
                                static_cast<int>(floorSlot) + 1))];
                        const int fromIndex = wrapSlot(static_cast<int>(floorSlot));
                        const int toIndex = wrapSlot(static_cast<int>(floorSlot) + 1);
                        const bool followsOverflowCircuit =
                            !previewSlots[static_cast<std::size_t>(fromIndex)].main ||
                            !previewSlots[static_cast<std::size_t>(toIndex)].main;
                        ImVec2 position(
                            fromPosition.x + (toPosition.x - fromPosition.x) * smooth,
                            fromPosition.y + (toPosition.y - fromPosition.y) * smooth);
                        // Só o excedente percorre o trilho externo durante o
                        // giro. Os slots principais continuam usando sua
                        // posição real, igual ao WheelSettings sem excedentes.
                        if (followsOverflowCircuit && circuitLength > 0.001f)
                        {
                            float from = previewSlots[static_cast<std::size_t>(fromIndex)].circuitT *
                                circuitLength;
                            float to = previewSlots[static_cast<std::size_t>(toIndex)].circuitT *
                                circuitLength;
                            if (to <= from) to += circuitLength;
                            float distance = from + (to - from) * smooth;
                            while (distance >= circuitLength) distance -= circuitLength;
                            position = Track::SamplePolyline(circuit, distance).position;
                        }
                        else
                        {
                            // Entre dois slots principais o preview deve
                            // seguir o contorno do radial, não cortar uma
                            // corda reta através do círculo.
                            constexpr float pi = 3.141592654f;
                            constexpr float twoPi = 6.283185307f;
                            const float fromAngle = std::atan2(
                                fromPosition.y - center.y,
                                fromPosition.x - center.x);
                            const float toAngle = std::atan2(
                                toPosition.y - center.y,
                                toPosition.x - center.x);
                            float arc = toAngle - fromAngle;
                            while (arc > pi) arc -= twoPi;
                            while (arc < -pi) arc += twoPi;
                            const float angle = fromAngle + arc * smooth;
                            position = RadialShape::PositionAtAngles(
                                CurrentRadialShape(), center, radius,
                                angle - layout.radialRotation, angle);
                        }
                        draw->AddCircleFilled(position, 18.0f, IM_COL32(32, 34, 41, 225), 32);
                        draw->AddCircle(position, 18.0f, IM_COL32(225, 215, 195, 170), 32, 1.2f);
                        if (editorIcons[static_cast<std::size_t>(i)].texture)
                            draw->AddImage(editorIcons[static_cast<std::size_t>(i)].texture,
                                ImVec2(position.x - 13.0f, position.y - 13.0f),
                                ImVec2(position.x + 13.0f, position.y + 13.0f),
                                ImVec2(0, 0), ImVec2(1, 1),
                                editorIcons[static_cast<std::size_t>(i)].color);
                    }
                }
            }
        }
        if (g_drawing)
        {
            if (g_drawPoints.empty() || Track::Distance(g_drawPoints.back(), mouse) > 5.0f)
                g_drawPoints.push_back(mouse);
            if (g_drawPoints.size() > 1)
                draw->AddPolyline(g_drawPoints.data(), static_cast<int>(g_drawPoints.size()),
                    IM_COL32(245, 215, 135, 255), 0, 3.0f);
        }
        if (g_gateDragging)
        {
            g_gateMouse = mouse;
            const ImU32 color = g_gateToolKind == Track::GateKind::Entry
                ? IM_COL32(105, 210, 145, 255) : IM_COL32(225, 95, 85, 255);
            ImVec2 snapPoint{};
            int targetPiece = -1;
            int targetPort = -1;
            const bool pieceTarget = NearestOtherPort(mouse, g_gatePieceID,
                snapPoint, targetPiece, targetPort);
            const float angle = std::atan2(mouse.y - center.y,
                mouse.x - center.x);
            const ImVec2 radialPoint = EditingRadialPoint(center, radius, angle);
            const bool radialTarget = !pieceTarget &&
                Track::Distance(mouse, radialPoint) <= 46.0f;
            if (!pieceTarget && !radialTarget) snapPoint = mouse;
            else if (radialTarget) snapPoint = radialPoint;
            draw->AddLine(g_gateStart, snapPoint, color, 3.0f);
            if (pieceTarget || radialTarget)
            {
                draw->AddCircleFilled(snapPoint, 9.0f, color, 28);
                draw->AddCircle(snapPoint, 11.0f,
                    IM_COL32(255, 245, 220, 245), 28, 1.5f);
            }
        }
        // Os terminais ficam sempre acima dos ícones de preview. Isto evita
        // que um item grande esconda a pizza de entrada/saída/remoção.
        g_portControls.clear();
        bool keepStickyPort = false;
        for (const auto& piece : Track::CustomLayout().pieces)
        {
            if (piece.id != g_stickyPortPiece ||
                g_stickyPortIndex < 0 || g_stickyPortIndex > 1)
                continue;
            const ImVec2 p = Track::GetPort(piece, g_stickyPortIndex,
                center, radius).position;
            if (Track::Distance(p, mouse) <= 30.0f)
            {
                g_portControls.push_back(PortControl{
                    piece.id, g_stickyPortIndex, p, p });
                keepStickyPort = true;
            }
            break;
        }
        if (!keepStickyPort)
        {
            g_stickyPortPiece = -1;
            g_stickyPortIndex = -1;
            float nearestPortDistance = 34.0f;
            for (const auto& piece : Track::CustomLayout().pieces)
            {
                for (int port = 0; port < 2; ++port)
                {
                    const ImVec2 p = Track::GetPort(piece, port, center, radius).position;
                    const float distance = Track::Distance(p, mouse);
                    if (distance <= nearestPortDistance)
                    {
                        nearestPortDistance = distance;
                        g_portControls.assign(1, PortControl{
                            piece.id, port, p, p });
                        g_stickyPortPiece = piece.id;
                        g_stickyPortIndex = port;
                    }
                }
            }
        }
        for (const auto& control : g_portControls)
        {
            const ImVec2 p = control.center;
            const auto slice = [&](float from, float to, ImU32 color) {
                draw->PathLineTo(p);
                draw->PathArcTo(p, 27.0f, from, to, 18);
                draw->PathLineTo(p);
                draw->PathFillConvex(color);
            };
            slice(0.5235988f, 2.6179939f, IM_COL32(48, 50, 58, 255));
            slice(2.6179939f, 4.7123890f, IM_COL32(225, 95, 85, 255));
            slice(4.7123890f, 6.8067841f, IM_COL32(90, 210, 135, 255));
            draw->AddCircle(p, 27.0f, IM_COL32(255, 250, 230, 255), 40, 2.2f);
        }
        if (g_eraseTool && mouse.x < g_panelLeft)
            draw->AddCircle(mouse, std::max(8.0f, g_brushSize * radius),
                IM_COL32(255, 255, 255, 210), 64, 1.0f);

        // Sempre por último no canvas: nenhuma linha, item ou terminal pode
        // cobrir o painel de manipulação da peça selecionada.
        DrawGizmoControls(draw);

        g_presetRows.clear();
        if (g_presetListOpen)
        {
            const auto presets = Track::Presets();
            const float width = 430.0f;
            const float height = std::min(420.0f, 72.0f + presets.size() * 38.0f);
            const Rect box{ ImVec2(g_panelLeft - width - 24.0f, g_viewportMin.y + 150.0f),
                ImVec2(g_panelLeft - 24.0f, g_viewportMin.y + 150.0f + height) };
            draw->AddRectFilled(box.min, box.max, IM_COL32(15, 17, 22, 242), 8.0f);
            draw->AddRect(box.min, box.max, IM_COL32(205, 185, 140, 210), 8.0f);
            draw->AddText(ImVec2(box.min.x + 18, box.min.y + 15),
                IM_COL32(235, 220, 185, 255), Tr("load_preset"));
            float y = box.min.y + 48.0f;
            const char* hoveredActionHint = nullptr;
            for (const std::string& preset : presets)
            {
                PresetRow row;
                row.name = preset;
                row.builtIn = Track::IsBuiltInPreset(preset);
                row.load = { ImVec2(box.min.x + 12, y),
                    ImVec2(box.max.x - 112, y + 31) };
                row.rename = { ImVec2(box.max.x - 104, y + 2),
                    ImVec2(box.max.x - 76, y + 29) };
                row.duplicate = { ImVec2(box.max.x - 70, y + 2),
                    ImVec2(box.max.x - 42, y + 29) };
                row.remove = { ImVec2(box.max.x - 36, y + 2),
                    ImVec2(box.max.x - 8, y + 29) };
                g_presetRows.push_back(row);
                const std::string fittedName = FitText(preset,
                    row.load.max.x - row.load.min.x - 16.0f);
                TransparentButton(draw, row.load, fittedName.c_str());
                Button(draw, row.rename, "R");
                Button(draw, row.duplicate, "+");
                if (row.builtIn)
                {
                    draw->AddRectFilled(row.remove.min, row.remove.max,
                        IM_COL32(28, 29, 34, 150), 4.0f);
                    draw->AddText(ImVec2(row.remove.min.x + 9.0f,
                        row.remove.min.y + 5.0f), IM_COL32(95, 95, 100, 150), "X");
                }
                else
                    Button(draw, row.remove, "X");
                const char* actionHint = Inside(mouse, row.rename) ? Tr("rename") :
                    Inside(mouse, row.duplicate) ? Tr("duplicate") :
                    Inside(mouse, row.remove) ? Tr("delete") : nullptr;
                if (actionHint) hoveredActionHint = actionHint;
                y += 38.0f;
            }
            if (presets.empty())
                draw->AddText(ImVec2(box.min.x + 18, y), IM_COL32(170, 170, 175, 230),
                    Tr("no_saved_presets"));
            // Tooltip por último: nenhuma linha de preset subsequente pode
            // cobrir Rename/Duplicate/Delete.
            if (hoveredActionHint)
            {
                const ImVec2 hintSize = ImGui::CalcTextSize(hoveredActionHint);
                const ImVec2 hintMin(mouse.x - hintSize.x - 14.0f,
                    mouse.y + 14.0f);
                draw->AddRectFilled(ImVec2(hintMin.x - 5.0f, hintMin.y - 3.0f),
                    ImVec2(hintMin.x + hintSize.x + 5.0f,
                        hintMin.y + hintSize.y + 3.0f),
                    IM_COL32(12, 14, 18, 252), 3.0f);
                draw->AddText(hintMin, IM_COL32(235, 225, 205, 255),
                    hoveredActionHint);
            }
        }
        if (g_presetNameOpen)
        {
            const ImVec2 dialogCenter((g_viewportMin.x + g_panelLeft) * 0.5f,
                (g_viewportMin.y + g_viewportMax.y) * 0.5f);
            const Rect box{ ImVec2(dialogCenter.x - 210, dialogCenter.y - 85),
                ImVec2(dialogCenter.x + 210, dialogCenter.y + 85) };
            draw->AddRectFilled(box.min, box.max, IM_COL32(14, 16, 21, 248), 8.0f);
            draw->AddRect(box.min, box.max, IM_COL32(220, 195, 140, 230), 8.0f, 0, 1.5f);
            draw->AddText(ImVec2(box.min.x + 18, box.min.y + 16),
                IM_COL32(235, 220, 185, 255), Tr("preset_name"));
            const Rect field{ ImVec2(box.min.x + 18, box.min.y + 48),
                ImVec2(box.max.x - 18, box.min.y + 83) };
            draw->AddRectFilled(field.min, field.max, IM_COL32(30, 32, 39, 245), 4.0f);
            draw->AddRect(field.min, field.max, IM_COL32(200, 190, 165, 190), 4.0f);
            draw->AddText(ImVec2(field.min.x + 10, field.min.y + 8),
                IM_COL32(245, 242, 232, 255), g_presetName.c_str());
            g_presetOK = { ImVec2(box.min.x + 52, box.max.y - 48),
                ImVec2(box.min.x + 192, box.max.y - 14) };
            g_presetCancel = { ImVec2(box.max.x - 192, box.max.y - 48),
                ImVec2(box.max.x - 52, box.max.y - 14) };
            Button(draw, g_presetOK, Tr("ok"));
            Button(draw, g_presetCancel, Tr("cancel"));
        }
        draw->AddCircleFilled(mouse, 4.0f, IM_COL32(255, 255, 255, 230), 20);
        draw->PopClipRect();
    }

    bool MouseDown(const ImVec2& mouse, int button)
    {
        if (!g_open) return false;
        if (button == 0 || button == 1) BeginHistoryAction();
        if (button == 1)
        {
            if (g_gateToolActive || g_freeDrawTool || g_geometryDrawTool || g_eraseTool)
            {
                g_gateToolActive = false;
                ClearLiveGate(Track::CustomLayout());
                g_gateDragging = false;
                g_freeDrawTool = false;
                g_geometryDrawTool = false;
                g_eraseTool = false;
                g_erasing = false;
                g_drawing = false;
                g_drawPoints.clear();
                g_selectTool = true;
                CommitHistoryAction();
                return true;
            }
            auto& gates = Track::CustomLayout().gates;
            const int hitPiece = HitPiece(mouse);
            if (hitPiece >= 0)
            {
                std::erase_if(Track::CustomLayout().pieces,
                    [&](const Track::Piece& piece) { return piece.id == hitPiece; });
                std::erase_if(gates,
                    [&](const Track::Gate& gate) { return gate.pieceID == hitPiece; });
                RemovePieceConnections(Track::CustomLayout(), hitPiece);
                if (g_selectedID == hitPiece) g_selectedID = -1;
                CommitHistoryAction();
                return true;
            }
            CommitHistoryAction();
            return true;
        }
        if (button != 0) return true;
        if (g_presetNameOpen)
        {
            if (Inside(mouse, g_presetOK))
            {
                if (!g_presetName.empty())
                {
                    if (g_presetNameAction == PresetNameAction::RenamePreset)
                        Track::RenamePreset(g_presetSource, g_presetName);
                    else
                        Track::SavePreset(g_presetName);
                }
                g_presetNameOpen = false;
            }
            else if (Inside(mouse, g_presetCancel)) g_presetNameOpen = false;
            return true;
        }
        if (g_presetListOpen)
        {
            for (const auto& row : g_presetRows)
            {
                if (Inside(mouse, row.rename))
                {
                    g_presetNameAction = PresetNameAction::RenamePreset;
                    g_presetSource = row.name;
                    g_presetName = row.name;
                    g_presetNameOpen = true;
                    g_presetListOpen = false;
                    return true;
                }
                if (Inside(mouse, row.duplicate))
                {
                    Track::DuplicatePreset(row.name);
                    return true;
                }
                if (Inside(mouse, row.remove))
                {
                    if (!row.builtIn) Track::DeletePreset(row.name);
                    return true;
                }
                if (Inside(mouse, row.load))
                {
                    Track::LoadPreset(row.name);
                    g_selectedID = -1;
                    g_gizmoOwner = -1;
                    g_presetListOpen = false;
                    return true;
                }
            }
            if (mouse.x < g_panelLeft) { g_presetListOpen = false; return true; }
        }
        // Fixed controls must have priority over scrolled controls whose
        // logical hitboxes may currently be clipped behind the header/footer.
        if (Inside(mouse, g_cancel)) { Cancel(); return true; }
        if (Inside(mouse, g_save))
        {
            if (Track::CommitEdit()) { Config::g_customRadial = true; Config::SaveConfig(); g_open = false; }
            return true;
        }
        if (Inside(mouse, g_zoomOut)) { g_zoom = std::max(0.35f, g_zoom - 0.1f); return true; }
        if (Inside(mouse, g_zoomIn)) { g_zoom = std::min(2.5f, g_zoom + 0.1f); return true; }
        if (Inside(mouse, g_reset)) { g_zoom = 1.0f; g_pan = {}; return true; }
        if (Inside(mouse, g_presetSave))
        {
            if (Track::IsValid(Track::CustomLayout()))
            {
                g_presetName = Tr("default_preset_name");
                g_presetNameAction = PresetNameAction::SaveCurrent;
                g_presetSource.clear();
                g_presetNameOpen = true;
                g_presetListOpen = false;
            }
            return true;
        }
        if (Inside(mouse, g_presetLoad))
        {
            g_presetListOpen = !g_presetListOpen;
            return true;
        }
        if (Inside(mouse, g_presetReset))
        {
            Track::CustomLayout() = Track::Layout{};
            g_selectedID = -1;
            g_gizmoOwner = -1;
            return true;
        }
        if (g_panelMaxScroll > 0.0f && Inside(mouse, g_scrollThumb))
        {
            g_scrollDragging = true;
            g_scrollGrabOffset = mouse.y - g_scrollThumb.min.y - 3.0f;
            return true;
        }
        const bool insideScrollableClip = mouse.y >= g_panelClipTop &&
            mouse.y <= g_panelClipBottom;
        if (insideScrollableClip && Inside(mouse, g_radialQuantitySlider))
        {
            g_activeSlider = EditorSlider::RadialQuantity;
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_radialStretchSlider))
        {
            g_activeSlider = EditorSlider::RadialStretch;
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_ghostItemsSlider))
        {
            g_activeSlider = EditorSlider::GhostItems;
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_radialOpacitySlider))
        {
            g_activeSlider = EditorSlider::RadialOpacity;
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_radialShapeSlider))
        {
            g_activeSlider = EditorSlider::RadialShape;
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_radialRotationSlider))
        {
            g_activeSlider = EditorSlider::RadialRotation;
            return true;
        }
        if (insideScrollableClip && g_eraseTool && Inside(mouse, g_brushSizeSlider))
        {
            g_activeSlider = EditorSlider::BrushSize;
            return true;
        }
        if (insideScrollableClip && g_eraseTool && Inside(mouse, g_brushHardnessSlider))
        {
            g_activeSlider = EditorSlider::BrushHardness;
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_lineOpacitySlider))
        {
            g_activeSlider = EditorSlider::OverflowOpacity;
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_smoothDrawButton))
        {
            g_smoothDraw = !g_smoothDraw;
            if (!g_smoothDraw && g_activeSlider == EditorSlider::DrawSmoothing)
                g_activeSlider = EditorSlider::None;
            return true;
        }
        if (insideScrollableClip && g_smoothDraw &&
            Inside(mouse, g_drawSmoothingSlider))
        {
            g_activeSlider = EditorSlider::DrawSmoothing;
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_geometryDrawButton))
        {
            g_geometryDrawTool = !g_geometryDrawTool;
            g_selectTool = !g_geometryDrawTool;
            g_freeDrawTool = false;
            g_eraseTool = false;
            g_drawing = false;
            g_drawPoints.clear();
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_duplicateSelectedButton) && Selected())
        {
            DuplicateSelected();
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_flipPoles))
        {
            Track::CustomLayout().gates.clear();
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_eraseRadialButton))
        {
            g_eraseTool = !g_eraseTool;
            g_selectTool = !g_eraseTool;
            g_freeDrawTool = false;
            g_geometryDrawTool = false;
            if (!g_eraseTool && (g_activeSlider == EditorSlider::BrushSize ||
                g_activeSlider == EditorSlider::BrushHardness))
                g_activeSlider = EditorSlider::None;
            return true;
        }
        if (insideScrollableClip && g_eraseTool &&
            Inside(mouse, g_eraserRadialMode))
        {
            g_eraseOverflowMode = false;
            return true;
        }
        if (insideScrollableClip && g_eraseTool &&
            Inside(mouse, g_eraserOverflowMode))
        {
            g_eraseOverflowMode = true;
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_resetRadialDrawing))
        {
            if (g_eraseOverflowMode)
                Track::CustomLayout().overflowErases.clear();
            else
                Track::CustomLayout().radialErases.clear();
            g_resetRadialFlash = 0.16f;
            return true;
        }
        if (Inside(mouse, g_centerReverseFlow))
        {
            Track::CustomLayout().flowDirection *= -1;
            return true;
        }

        // Keep visible gizmo and hitboxes on the exact same transform.
        UpdateGizmoRects();
        if (Inside(mouse, g_gizmoScaleUp) && Selected())
        {
            g_frozenPreviewSlotT = g_lastPreviewSlotT;
            g_frozenPreviewMainPositions = g_lastPreviewMainPositions;
            g_previewSlotsFrozen = !g_frozenPreviewMainPositions.empty();
            g_scaling = true;
            g_scaleDirection = 1;
            g_scaleStartY = mouse.y;
            g_scaleStartValue = Selected()->scale;
            return true;
        }
        if (Inside(mouse, g_gizmoDelete) && Selected())
        {
            const int selected = g_selectedID;
            std::erase_if(Track::CustomLayout().pieces,
                [&](const Track::Piece& piece) { return piece.id == selected; });
            std::erase_if(Track::CustomLayout().gates,
                [&](const Track::Gate& gate) { return gate.pieceID == selected; });
            RemovePieceConnections(Track::CustomLayout(), selected);
            g_selectedID = -1;
            return true;
        }
        if (Inside(mouse, g_gizmoMirror) && Selected())
        {
            Selected()->mirrored = !Selected()->mirrored;
            return true;
        }
        if (Inside(mouse, g_gizmoRotate) && Selected())
        {
            const ImVec2 pieceCenter = Track::TransformPoint(
                *Selected(), ImVec2{}, WorldCenter(), WorldRadius());
            g_rotateOffset = Selected()->rotation -
                std::atan2(mouse.y - pieceCenter.y, mouse.x - pieceCenter.x);
            g_frozenPreviewSlotT = g_lastPreviewSlotT;
            g_frozenPreviewMainPositions = g_lastPreviewMainPositions;
            g_previewSlotsFrozen = !g_frozenPreviewMainPositions.empty();
            g_rotating = true;
            return true;
        }
        if (Inside(mouse, g_gizmoMove) && Selected())
        {
            const ImVec2 center = WorldCenter();
            const float radius = WorldRadius();
            g_dragOffset = ImVec2((mouse.x - center.x) / radius - Selected()->position.x,
                (mouse.y - center.y) / radius - Selected()->position.y);
            g_frozenPreviewSlotT = g_lastPreviewSlotT;
            g_frozenPreviewMainPositions = g_lastPreviewMainPositions;
            g_previewSlotsFrozen = !g_frozenPreviewMainPositions.empty();
            g_dragging = true;
            return true;
        }
        if (Inside(mouse, g_gizmoPanel) && Selected())
            return true;
        if (Inside(mouse, g_autoFlow))
        {
            Track::AutoDirection(Track::CustomLayout());
            return true;
        }
        if (Inside(mouse, g_reverseFlow))
        {
            Track::CustomLayout().flowDirection *= -1;
            return true;
        }
        for (std::size_t i = 0; i < kPieceKinds.size(); ++i)
        {
            if (!insideScrollableClip || !InsideCircle(mouse, g_pieceButtons[i])) continue;
            auto& layout = Track::CustomLayout();
            Track::Piece piece;
            piece.id = layout.nextID++;
            piece.kind = kPieceKinds[i];
            piece.position = ImVec2(0.0f, -0.9f + 0.18f * static_cast<float>(layout.pieces.size()));
            layout.pieces.push_back(piece);
            g_selectedID = piece.id;
            g_selectTool = true;
            g_panTool = false;
            g_gateToolActive = false;
            g_freeDrawTool = false;
            g_geometryDrawTool = false;
            g_eraseTool = false;
            return true;
        }
        if (insideScrollableClip && Inside(mouse, g_drawPathButton))
        {
            g_freeDrawTool = !g_freeDrawTool;
            g_selectTool = !g_freeDrawTool;
            g_geometryDrawTool = false;
            g_eraseTool = false;
            g_panTool = false;
            g_gateToolActive = false;
            g_drawing = false;
            g_drawPoints.clear();
            return true;
        }
        if (g_eraseTool && mouse.x < g_panelLeft)
        {
            g_erasing = true;
            EraseRadialAt(mouse);
            return true;
        }
        if (g_freeDrawTool || g_geometryDrawTool)
        {
            g_drawing = true;
            g_drawPoints = { mouse };
            return true;
        }
        if (g_spaceHeld && mouse.x < g_panelLeft)
        {
            g_panning = true;
            g_lastMouse = mouse;
            return true;
        }
        const bool overDisplayedPortControl = std::ranges::any_of(
            g_portControls, [&](const PortControl& control) {
                return Track::Distance(control.center, mouse) <= 30.0f;
            });
        if (g_selectTool && !overDisplayedPortControl)
            for (const auto& piece : Track::CustomLayout().pieces)
        {
            for (int port = 0; port < 2; ++port)
            {
                const ImVec2 p = Track::GetPort(piece, port, WorldCenter(), WorldRadius()).position;
                if (Track::Distance(p, mouse) <= 30.0f)
                {
                    auto& layout = Track::CustomLayout();
                    const ImVec2 relative(mouse.x - p.x, mouse.y - p.y);
                    const bool removeTerminal = relative.y >
                        std::abs(relative.x) * 0.577350269f;
                    if (removeTerminal)
                    {
                        std::erase_if(layout.gates, [&](const Track::Gate& gate) {
                            return gate.pieceID == piece.id && gate.portIndex == port;
                        });
                        g_selectedID = piece.id;
                        g_selectedPort = port;
                        return true;
                    }
                    const Track::GateKind kind = relative.x < 0.0f
                        ? Track::GateKind::Exit : Track::GateKind::Entry;
                    std::erase_if(layout.gates, [&](const Track::Gate& gate) {
                        return (gate.pieceID == piece.id && gate.portIndex == port) ||
                            (gate.pieceID == piece.id && gate.kind == kind);
                    });
                    const float angle = std::atan2(p.y - WorldCenter().y,
                        p.x - WorldCenter().x) - layout.radialRotation;
                    layout.gates.push_back(Track::Gate{ layout.nextGateID++, kind,
                        piece.id, port, angle, -1.0f });
                    g_selectedID = piece.id;
                    g_selectedPort = port;
                    return true;
                }
            }
        }
        if (g_panTool)
        {
            g_panning = true; g_lastMouse = mouse; return true;
        }
        if (!g_selectTool) return true;
        for (const PortControl& control : g_portControls)
        {
            if (Track::Distance(control.center, mouse) > 30.0f) continue;
            auto& layout = Track::CustomLayout();
            const ImVec2 relative(mouse.x - control.center.x,
                mouse.y - control.center.y);
            const bool removeTerminal = relative.y >
                std::abs(relative.x) * 0.577350269f;
            if (removeTerminal)
            {
                std::erase_if(layout.gates, [&](const Track::Gate& gate) {
                    return gate.pieceID == control.pieceID &&
                        gate.portIndex == control.port;
                });
            }
            else
            {
                g_gateToolKind = relative.x < 0.0f
                    ? Track::GateKind::Exit : Track::GateKind::Entry;
                ClearLiveGate(layout);
                // Retirar o terminal anterior no início do gesto evita que a
                // seta antiga permaneça presa até o botão ser solto.
                ClearEndpoint(layout, control.pieceID, control.port);
                g_gateDragging = true;
                g_gatePieceID = control.pieceID;
                g_gatePort = control.port;
                g_gateStart = control.portPosition;
                g_gatePressMouse = mouse;
                g_gateMouse = mouse;
            }
            g_selectedID = control.pieceID;
            g_selectedPort = control.port;
            return true;
        }
        g_selectedID = HitPiece(mouse);
        g_selectedPort = -1;
        return true;
    }

    bool MouseUp(const ImVec2& mouse, int button)
    {
        if (!g_open) return false;
        if (button == 0 && g_dragging) SnapSelected();
        if (button == 0 && g_drawing)
        {
            FinishFreeDraw();
            g_drawing = false;
            g_freeDrawTool = false;
            g_geometryDrawTool = false;
            g_selectTool = true;
        }
        if (button == 0 && g_gateDragging)
        {
            auto& layout = Track::CustomLayout();
            ImVec2 targetPortPosition{};
            int targetPiece = -1;
            int targetPort = -1;
            const bool pieceTarget = NearestOtherPort(mouse, g_gatePieceID,
                targetPortPosition, targetPiece, targetPort);
            const float screenAngle = std::atan2(
                mouse.y - WorldCenter().y, mouse.x - WorldCenter().x);
            const ImVec2 radialPoint = EditingRadialPoint(
                WorldCenter(), WorldRadius(), screenAngle);
            const bool radialTarget = !pieceTarget &&
                Track::Distance(mouse, radialPoint) <= 46.0f;

            if (pieceTarget)
            {
                ClearLiveGate(layout);
                ClearEndpoint(layout, g_gatePieceID, g_gatePort);
                ClearEndpoint(layout, targetPiece, targetPort);
                layout.connections.push_back(Track::Connection{
                    layout.nextConnectionID++, g_gateToolKind,
                    g_gatePieceID, g_gatePort, targetPiece, targetPort });
            }
            else if (radialTarget)
            {
                const float angle = screenAngle - layout.radialRotation;
                const auto live = std::ranges::find_if(layout.gates,
                    [](const Track::Gate& gate) { return gate.id == g_liveGateID; });
                if (live != layout.gates.end())
                {
                    live->mainAngle = angle;
                    g_liveGateID = -1; // o preview passa a ser o terminal final
                }
                else
                {
                    ClearEndpoint(layout, g_gatePieceID, g_gatePort);
                    std::erase_if(layout.gates, [&](const Track::Gate& gate) {
                        return gate.pieceID == g_gatePieceID &&
                            gate.kind == g_gateToolKind;
                    });
                    layout.gates.push_back(Track::Gate{ layout.nextGateID++,
                        g_gateToolKind, g_gatePieceID, g_gatePort, angle, -1.0f });
                }
            }
            else
                ClearLiveGate(layout);
            g_gateDragging = false;
            g_gatePieceID = -1;
            g_gatePort = -1;
        }
        if (button == 0 && (g_activeSlider == EditorSlider::RadialQuantity ||
            g_activeSlider == EditorSlider::RadialStretch ||
            g_activeSlider == EditorSlider::RadialShape))
            Config::SaveConfig();
        g_activeSlider = EditorSlider::None;
        g_dragging = false;
        g_panning = false;
        g_scrollDragging = false;
        g_rotating = false;
        g_scaling = false;
        g_frozenPreviewSlotT.clear();
        g_frozenPreviewMainPositions.clear();
        g_previewSlotsFrozen = false;
        g_erasing = false;
        CommitHistoryAction();
        return true;
    }

    bool Scroll(int direction)
    {
        if (!g_open) return false;
        if (g_ctrlHeld)
        {
            g_zoom = std::clamp(g_zoom + direction * 0.08f, 0.35f, 2.5f);
            return true;
        }
        const ImVec2 virtualMouse = Resolution::ToVirtual(ImGui::GetIO().MousePos);
        if (virtualMouse.x >= g_panelLeft)
        {
            g_panelScroll = std::clamp(g_panelScroll - direction * 55.0f,
                0.0f, g_panelMaxScroll);
            return true;
        }
        g_previewTargetPhase += static_cast<float>(direction);
        return true;
    }

    void SetPanModifier(bool held) { g_spaceHeld = held; }
    void SetZoomModifier(bool held) { g_ctrlHeld = held; }
    bool IsOverflowEraserSelected() { return g_eraseOverflowMode; }
    void SetOverflowEraserSelected(bool selected)
    {
        g_eraseOverflowMode = selected;
    }

    bool KeyboardInput(std::uint32_t scanCode, bool pressed)
    {
        if (!g_open || !pressed) return false;
        const UINT key = MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX);
        const bool control = g_ctrlHeld ||
            (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        if (control && key == 'C')
        {
            DuplicateSelected();
            return true;
        }
        if (control && key == 'Z')
        {
            Undo();
            return true;
        }
        if (control && key == 'Y')
        {
            Redo();
            return true;
        }
        if (!g_presetNameOpen) return false;
        if (key == VK_ESCAPE) { g_presetNameOpen = false; return true; }
        if (key == VK_RETURN)
        {
            if (!g_presetName.empty())
            {
                if (g_presetNameAction == PresetNameAction::RenamePreset)
                    Track::RenamePreset(g_presetSource, g_presetName);
                else
                    Track::SavePreset(g_presetName);
            }
            g_presetNameOpen = false;
            return true;
        }
        if (key == VK_BACK)
        {
            if (!g_presetName.empty()) g_presetName.pop_back();
            return true;
        }
        if (g_presetName.size() >= 28) return true;
        if (key >= 'A' && key <= 'Z')
        {
            const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            g_presetName.push_back(static_cast<char>(shift ? key : key + ('a' - 'A')));
            return true;
        }
        if (key >= '0' && key <= '9')
        {
            g_presetName.push_back(static_cast<char>(key));
            return true;
        }
        if (key == VK_SPACE || key == VK_OEM_MINUS)
        {
            g_presetName.push_back(key == VK_SPACE ? ' ' : '-');
            return true;
        }
        return true;
    }
}
