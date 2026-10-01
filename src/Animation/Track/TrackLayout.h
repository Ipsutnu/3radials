#pragma once

#include "Animation/Track/TrackTypes.h"
#include "Animation/RadialShape.h"

#include <filesystem>
#include <string>

namespace Track
{
    struct CircuitSlot
    {
        ImVec2 position{};
        bool main{};
        int ordinal{};
        bool terminal{};
        float circuitT{};
    };

    Layout& CustomLayout();
    const Layout& SavedLayout();
    void BeginEdit();
    void CancelEdit();
    bool CommitEdit();
    bool IsValid(const Layout& layout, std::string* reason = nullptr);
    std::vector<ImVec2> BuildClosedPath(const Layout& layout,
        const ImVec2& center, float radius);
    std::vector<std::vector<ImVec2>> BuildPaths(const Layout& layout,
        const ImVec2& center, float radius);
    std::vector<ImVec2> BuildCircuit(const Layout& layout,
        const ImVec2& center, float radius, bool leftSide = true,
        RadialShape::Style shape = RadialShape::Style::ClassicOrbit);
    std::vector<ImVec2> Circuit(
        const ImVec2& center, float radius, bool leftSide,
        RadialShape::Style shape = RadialShape::Style::ClassicOrbit);
    ImVec2 ProjectToCircuit(const ImVec2& point,
        const ImVec2& center, float radius, bool leftSide,
        RadialShape::Style shape = RadialShape::Style::ClassicOrbit);
    std::vector<CircuitSlot> CircuitSlots(const ImVec2& center, float radius,
        bool leftSide, int mainCount, int overflowCount,
        RadialShape::Style shape = RadialShape::Style::ClassicOrbit);
    std::vector<CircuitSlot> CircuitSlots(const Layout& layout,
        const ImVec2& center, float radius, bool leftSide,
        int mainCount, int overflowCount,
        RadialShape::Style shape = RadialShape::Style::ClassicOrbit);
    ImVec2 OverflowPosition(const ImVec2& center, float radius,
        int index, int count, bool leftSide);
    std::vector<std::vector<ImVec2>> GuidePaths(
        const ImVec2& center, float radius, bool leftSide);
    bool HasValidSavedLayout();
    int FlowDirection();
    float LineOpacity();
    float RadialLineOpacity();
    void SetLineOpacity(float value);
    void SetRadialLineOpacity(float value);
    float RadialRotation();
    void SetRadialRotation(float value);
    float RadialLineVisibility(float angle);
    float OverflowLineVisibility(const ImVec2& point,
        const ImVec2& center, float radius);
    std::pair<ImVec2, ImVec2> GatePositions(GateKind kind, int ordinal,
        const ImVec2& center, float radius, bool leftSide,
        RadialShape::Style shape = RadialShape::Style::ClassicOrbit);
    int GateCount(GateKind kind);
    void AutoDirection(Layout& layout);
    bool Load();
    bool Save();
    bool SavePreset(std::string_view name);
    bool LoadPreset(std::string_view name);
    bool RenamePreset(std::string_view oldName, std::string_view newName);
    bool DuplicatePreset(std::string_view name, std::string* createdName = nullptr);
    bool DeletePreset(std::string_view name);
    bool IsBuiltInPreset(std::string_view name);
    std::vector<std::string> Presets();
    void ResetEditingToLegacy();
    std::filesystem::path Path();
}
