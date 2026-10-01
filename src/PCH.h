#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <cfloat>
#include <unordered_set>
#include <ShlObj.h>
#include <format>
#include <filesystem>

#include <iomanip>

#include <fstream>
#include <mutex>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

#define IMGUI_DEFINE_MATH_OPERATORS

#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>

enum class RadialMode
{
    Gameplay,   // fora do inventário
    Inventory   // dentro do inventário
};

enum class InventoryDragMode
{
    None,
    KeyDrag,
    MouseDrag,
    Placed,
    SnapAnimating
};

inline InventoryDragMode g_inventoryDragMode =
    InventoryDragMode::None;

enum class RadialSide
{
    None,
    Left,
    Right,
    Top,
    Bottom
};

inline RadialMode g_radialMode = RadialMode::Gameplay;
inline RadialSide g_radialSide = RadialSide::None;

inline float g_globalAlpha = 0.0f;
inline float g_menuAlpha = 0.0f;

inline bool g_inventoryItemJustGrabbed = false;
inline ImVec2 g_inventoryDraggedPosition{ 0.0f, 0.0f };

// Variável estática inline (se quiser manter visibilidade global sem duplicação de símbolo)
inline float g_scrollOffset = 0.0f;       // Posição de rolagem contínua
inline float g_targetScrollOffset = 0.0f; // Para animação suave (LERP)

inline float g_topScrollOffset = 0.0f;       // Posição de rolagem contínua
inline float g_targetTopScrollOffset = 0.0f; // Para animação suave (LERP)


namespace Menu {

    void CheckGKey();
    bool CanDrawWheelMenu();
    bool IsRadialLockedOpen();
    bool IsCurrentRadialMouseUnlocked();

    void DrawMenu();
    bool IsInventoryOpen();
    void HandleInventoryKeyPressed();
    void HandleGameplayKeyPressed();
    void HandleInventoryKeyReleased();
    void HandleGameplayKeyReleased();
    void HandleGPressed();
    void HandleGReleased();
    void UpdateSideRadialScroll();
    void ScrollSideRadial(int direction);
    void ScrollTopBottomRadial(int direction);
    void UpdatePendingWeaponSwitch();
    void UpdatePendingNormalWeaponEquip();
    void UpdatePendingWeaponAction();
    void HandleSettingsScroll();

    void ResetSettingsItemInfo();

    void CloseRadialMenu();
    void ResetRadialLockedOpen();
    //bool IsRadialCloseMenuCommand(RE::ButtonEvent* button);

    bool SettingsWheelKeyClick();
    bool CaptureWheelKey(
        RE::ButtonEvent* buttonEvent);

    void GamepadMoveCursor(float x, float y, bool rightStick);
    bool GamepadOpenRadial(RadialSide side);
    void GamepadShoulder(int direction);
    void GamepadStepSideSelection(int direction);
    bool GamepadSettingsPrimaryDown();
    void GamepadSettingsPrimaryUp();
    bool GamepadSettingsDelete();
    void GamepadSettingsNavigate(int horizontal, int vertical);
    void GamepadSettingsAdjustSlider(int direction);
    void GamepadSettingsDirectional(int horizontal, int vertical);
    void GamepadSettingsCycleRadial(int direction);
    void GamepadSettingsClose();
    void UsePhysicalSettingsCursor();

    bool SettingsGameplayDescriptionClick();

    ImVec2 GetSkyrimMousePos();

    bool SettingsCloseButtonClick();

    void DrawTextWithShadow(
        ImDrawList* draw,
        const ImVec2& position,
        ImU32 color,
        const char* text,
        float alpha);

    bool RemoveItemFromRadials(
            RE::TESForm* form,
            std::uint16_t uniqueID = 0,
            bool hasUniqueID = false
        );
    
    bool ScrollSettingsItemInfo(int direction);
    bool SettingsLockClick();
    bool HandleMouseUnlockClick();

    //void ProcessSettingsOpenRequest();

    inline ImU32 FadeColor(ImU32 color, float alpha);

    //bool IsSettingsOpen();

    void OpenSettingsMenu();
    void ToggleSettingsMenuFromAltKey();

    void CloseSettingsMenu();

    void DrawSettingsMenu();

    bool SettingsMouseDown();
    bool SettingsLayoutKeyboardInput(std::uint32_t scanCode, bool pressed);

    void SettingsMouseUp();

    bool SettingsRightClick();
    void ScrollSettingsRadial(int direction);
    
    struct RadialItem
    {
        RE::TESForm* form = nullptr;

         // Identificação da instância.
        std::uint16_t uniqueID = 0;

        // Indica se o item possui UniqueID válido.
        bool hasUniqueID = false;

        std::string name;

        ImTextureID icon = ImTextureID(0);

        int slot = -1;

        bool valid = false;
    };

    extern std::vector<RadialItem> g_topItems;
    extern std::vector<RadialItem> g_bottomItems;
    extern std::vector<RadialItem> g_sideItems;


    
    
}



inline bool g_showWindow = false;

inline bool g_lastGState = false;

inline bool g_radialToggleLocked = false;

inline bool g_radialLockedOpen = false;
inline bool g_ignoreNextGRelease = false;

inline bool g_unlockTopMouse = false;
inline bool g_unlockBottomMouse = false;

inline bool g_lockTopRadial = false;
inline bool g_lockBottomRadial = false;
inline bool g_lockSideRadial = false;



using namespace std::literals;
