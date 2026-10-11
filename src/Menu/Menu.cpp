#include "PCH.h"
#include "Logger.h"
#include "Config.h"
#include "Input.h"
#include "Icon.h"
#include "IconCustom.h"
#include "ItemInfo.h"
//#include "Blur.h"
#include "Item.h"
#include "Preview.h"
#include "RenderHooks.h"
#include "RenderManager.h"
#include "Blur.h"
#include "Resolution.h"
#include "Gamepad.h"
#include "Font.h"
#include "Animation/Animation.h"
#include "Animation/RadialShape.h"
#include "Animation/OverflowMechanism.h"
#include "Animation/Track/TrackLayout.h"
#include "Animation/Track/TrackEditor.h"
#include "Animation/Track/TrackMovement.h"
#include "Language.h"
#include "Serialization.h"
#include "Slowtime.h"

#include <fstream>
#include <string>
#include <filesystem>
#include <mutex>
#include <cstdlib>
#include <cstdint>
#include <unordered_map>
#include <array>

HWND g_gameWindow = nullptr;

// Posição atual de origem da linha
ImVec2 g_radialOrigin = ImVec2(0.0f, 0.0f);

// O menu já foi escolhido?
bool g_radialLocked = false;



// Raio necessário para escolher um menu
constexpr float RADIAL_DEADZONE = 50.0f;
constexpr float MENU_INNER_RADIUS = 45.0f;
constexpr float MENU_INNER_RADIUSTOPBOTTOM = 15.0f;

constexpr float INVENTORY_MENU_ALPHA = 0.25f;


bool g_inventoryMode = false;

bool g_inventoryDragging = false;
bool g_inventoryItemReleased = false;
bool g_inventoryOverflowMorphActive = false;

RE::TESForm* g_draggedInventoryItem = nullptr;

std::uint16_t g_draggedUniqueID = 0;

bool g_draggedHasUniqueID = false;

std::uint16_t g_draggedInventoryUniqueID = 0;
bool g_draggedInventoryHasUniqueID = false;

// Marca se o item atualmente arrastado já foi efetivamente
// colocado em algum radial (evita perdas de sincronização
// entre PlaceDraggedItem / CloseInventoryRadial).
bool g_inventoryItemWasPlaced = false;


ImVec2 g_lastInventoryRadialPosition{ 0.0f, 0.0f };
bool g_hasLastInventoryRadialPosition = false;

//POINT g_inventoryLastPhysicalMousePos{ 0, 0 };

//ImVec2 g_inventoryMouseStart{0.0f, 0.0f};
//ImVec2 g_inventoryDragStartPosition{0.0f, 0.0f};
//bool g_inventoryDragMouseInitialized = false;

RadialSide g_inventoryHoverSide = RadialSide::None;

ImVec2 g_radialMouseVelocity{ 0.0f, 0.0f };

float g_mouseSmooth = 0.18f;
float g_mouseSensitivity = 1.0f;

ImVec2 g_radialVector;
ImVec2 g_lastMousePos;

bool g_radialMouseInitialized = false;

bool g_inventorySnapAnimating = false;

float g_inventorySnapProgress = 0.0f;

// Uma alteração na quantidade redistribui todos os slots do circuito. Scrolls
// recebidos durante essa curta acomodação são preservados e executados assim
// que a nova topologia estiver estável.
float g_inventoryTopologySettleRemaining = 0.0f;
int g_pendingInventoryScroll = 0;
float g_settingsTopologySettleRemaining = 0.0f;
int g_pendingSettingsScroll = 0;
float g_sideScrollStardustEnergy = 0.0f;

// O input pode entregar vários notches antes do próximo frame. Guardar só o
// offset final perde uma volta inteira quando ele volta ao mesmo índice (por
// exemplo, cinco passos em um radial de cinco slots). Esta fila preserva cada
// passo exclusivamente para a rotação do radial lateral no WheelSettings.
std::deque<int> g_settingsSideScrollQueue;
int g_settingsSideScrollAnimationDirection = 0;

constexpr float INVENTORY_SNAP_DURATION = 0.20f;

ImVec2 g_inventorySnapStart;
ImVec2 g_inventorySnapEnd;


int g_sideScrollOffset = 0;


float g_sideScrollAccumulator = 0.0f;



// ================================================================
// Velocidade de transição usada por todas as animações de hover
// (raio, alpha, glow) dos itens do radial. Quanto maior, mais
// rápida a transição entre estado normal <-> hover.
// ================================================================
constexpr float ITEM_TRANSITION_SPEED = 14.0f;

#include "RE/U/UIBlurManager.h"
#include "RE/I/ImageSpaceManager.h"


namespace Menu
{

    constexpr float PI = 3.14159265358979323846f;
    int g_sideScrollDirection = 0;
    static void QueueSettingsSideScrollStep(int direction);
    static void ProcessSettingsSideScrollQueue();
    static float g_settingsSideLockAnimatedX = 0.0f;
    static float g_settingsSideMouseAnimatedX = 0.0f;
    static float g_settingsPanelAnimatedMinX = 0.0f;
    static float g_settingsPanelTargetMinX = 0.0f;
    static float g_settingsPanelOffsetY = 0.0f;
    static float g_settingsSectionOffsetY = 0.0f;
    static float g_settingsSectionMorphT = 0.0f;
    static float g_settingsPanelRightX = 0.0f;
    static float g_settingsPanelTopY = 0.0f;
    static bool g_settingsSectionHorizontalTarget = false;
    static float g_settingsHorizontalShortfall = 0.0f;
    static float g_settingsCloseButtonAnimatedY = 0.0f;

    // Ajustes manuais do layout fixo do painel/seletor do WheelSettings.
    // Positivo em X move ambos para a direita; negativo em Y move ambos para cima.
    constexpr float kSettingsFixedOffsetX = 30.0f;
    constexpr float kSettingsFixedOffsetY = -50.0f;

    struct WheelLayout
    {
        ImVec2 min{};
        ImVec2 max{};
        ImVec2 center{};
        ImVec2 leftRadial{};
        ImVec2 rightRadial{};
        ImVec2 topRadial{};
        ImVec2 bottomRadial{};
    };

    static float LayoutLerp(float a, float b, float percent)
    {
        return a + (b - a) * std::clamp(percent, 0.0f, 100.0f) * 0.01f;
    }

    static WheelLayout GetWheelLayout()
    {
        WheelLayout layout{};
        layout.min = Resolution::ToVirtual(ImVec2(0.0f, 0.0f));
        layout.max = Resolution::ToVirtual(Resolution::GetRealSize());
        layout.center = ImVec2(
            (layout.min.x + layout.max.x) * 0.5f,
            (layout.min.y + layout.max.y) * 0.5f);

        const float height = layout.max.y - layout.min.y;
        const float leftX = LayoutLerp(
            layout.min.x, layout.center.x, Config::g_sideRadialPosition);
        const float topY = LayoutLerp(
            layout.min.y + 34.0f,
            layout.min.y + height * 0.25f,
            Config::g_topVerticalPosition);
        const float bottomY = LayoutLerp(
            layout.max.y - height * 0.25f,
            layout.max.y - 34.0f,
            Config::g_bottomVerticalPosition);

        layout.leftRadial = ImVec2(leftX, layout.center.y);
        layout.rightRadial = ImVec2(layout.center.x * 2.0f - leftX, layout.center.y);
        layout.topRadial = ImVec2(layout.center.x, topY);
        layout.bottomRadial = ImVec2(layout.center.x, bottomY);
        return layout;
    }

    static void GetFixedSettingsPanelBounds(
        const WheelLayout& wheelLayout, ImVec2& panelMin, ImVec2& panelMax)
    {
        const float visibleWidth = wheelLayout.max.x - wheelLayout.min.x;
        const float visibleHeight = wheelLayout.max.y - wheelLayout.min.y;
        const float descriptorScale = 1.0f +
            (std::clamp(Config::g_fontSizeScale, 1.0f, 2.5f) - 1.0f) * (2.0f / 3.0f);
        const float panelWidth = 175.0f * descriptorScale * 1.5f;
        const float panelHeight = 350.0f * descriptorScale * 1.5f;
        const float centerX = wheelLayout.min.x + visibleWidth * 0.85f +
            kSettingsFixedOffsetX;
        constexpr float maxRightFontScale = 2.20f;
        const float maxRightDescriptorScale = 1.0f +
            (maxRightFontScale - 1.0f) * (2.0f / 3.0f);
        const float fixedRightEdge = std::min(
            centerX + 175.0f * maxRightDescriptorScale * 1.5f * 0.5f,
            wheelLayout.max.x - 22.0f);
        const float panelRight = std::min(
            centerX + panelWidth * 0.5f,
            fixedRightEdge);
        const float baseTop = wheelLayout.center.y - 175.0f -
            (350.0f * descriptorScale * 0.25f) + visibleHeight * 0.05f - 38.0f;
        panelMin = ImVec2(
            std::max(wheelLayout.min.x + 10.0f, panelRight - panelWidth),
            baseTop + kSettingsFixedOffsetY);
        panelMax = ImVec2(panelMin.x + panelWidth, panelMin.y + panelHeight);
    }

    static ImVec2 GetSettingsCloseButtonTarget(const WheelLayout& layout)
    {
        const float height = layout.max.y - layout.min.y;
        const float baseY = layout.min.y + height * 0.93f;

        // A posição original permanece intacta enquanto o Bottom está longe.
        // Quando seu centro entra nos últimos 60 px antes do botão, ele passa
        // a empurrá-lo. No extremo inferior, o botão percorre exatamente
        // metade do espaço entre a posição original e a borda da tela.
        constexpr float proximity = 60.0f;
        const float triggerY = baseY - proximity;
        const float lowestBottomY = layout.max.y - 34.0f;
        float pushT = (layout.bottomRadial.y - triggerY) /
            std::max(lowestBottomY - triggerY, 1.0f);
        pushT = std::clamp(pushT, 0.0f, 1.0f);
        pushT = pushT * pushT * (3.0f - 2.0f * pushT);

        const float lowestButtonY = baseY + (layout.max.y - baseY) * 0.5f;
        return ImVec2(layout.center.x,
            baseY + (lowestButtonY - baseY) * pushT);
    }

    static int GetSideVisibleLimit()
    {
        const float stretchCapacity = 25.0f +
            std::clamp(Config::g_radialStretch, 0.0f, 80.0f) * (25.0f / 80.0f);
        const int maximum = std::clamp(static_cast<int>(std::floor(stretchCapacity)), 25, 50);
        return std::clamp(static_cast<int>(std::lround(Config::g_radialQuantity)), 3, maximum);
    }

    static float GetSideRadialRadius()
    {
        constexpr float baseRadius = 175.0f;
        constexpr float itemRadius = 34.0f;
        constexpr float edgeClearance = 10.0f;
        const WheelLayout layout = GetWheelLayout();
        const float verticalSpace = std::min(
            layout.center.y - layout.min.y,
            layout.max.y - layout.center.y);
        const float maximumRadius = std::max(
            baseRadius,
            verticalSpace - itemRadius - edgeClearance);
        return LayoutLerp(baseRadius, maximumRadius, Config::g_radialStretch);
    }

    static float GetSettingsLockColumnX()
    {
        const WheelLayout layout = GetWheelLayout();
        return layout.min.x + (layout.max.x - layout.min.x) * 0.11f;
    }

    static ImVec2 GetSettingsSideLockCenter()
    {
        const WheelLayout layout = GetWheelLayout();
        ImVec2 center = layout.leftRadial;
        if (g_settingsSideLockAnimatedX > 0.0f)
            center.x = g_settingsSideLockAnimatedX;
        return center;
    }

    static ImVec2 GetSettingsSideScrollCenter(float lockRadius)
    {
        const ImVec2 sideLockCenter = GetSettingsSideLockCenter();
        const float sideScrollRadius = lockRadius * 0.5f;
        return ImVec2(
            sideLockCenter.x + lockRadius + sideScrollRadius + 12.0f,
            sideLockCenter.y);
    }

    static float GetSettingsSideMouseTargetX(float lockRadius)
    {
        const ImVec2 sideLockCenter = GetSettingsSideLockCenter();
        const float sideMouseRadius = lockRadius * 0.5f;
        const float spacing = lockRadius + sideMouseRadius + 12.0f;
        const float leftX = sideLockCenter.x - spacing;
        const WheelLayout layout = GetWheelLayout();

        // Se a posição normal sair pela borda esquerda, o controle passa para
        // a direita do Lock Scroll. O X animado é atualizado no DrawSettings.
        const bool wouldLeaveScreen =
            leftX - sideMouseRadius < layout.min.x;
        return wouldLeaveScreen
            ? sideLockCenter.x + spacing * 2.0f
            : leftX;
    }

    static ImVec2 GetSettingsSideMouseCenter(float lockRadius)
    {
        const ImVec2 sideLockCenter = GetSettingsSideLockCenter();
        const float targetX = GetSettingsSideMouseTargetX(lockRadius);
        const float x = g_settingsSideMouseAnimatedX > 0.0f
            ? g_settingsSideMouseAnimatedX
            : targetX;
        return ImVec2(x, sideLockCenter.y);
    }

    static float GetTopBottomCurveWidth(bool isTop)
    {
        const WheelLayout layout = GetWheelLayout();
        const float visibleWidth = layout.max.x - layout.min.x;
        const float percent = isTop
            ? Config::g_topHorizontalStretch
            : Config::g_bottomHorizontalStretch;
        return visibleWidth * LayoutLerp(0.25f, 0.95f, percent);
    }
        
/* Legacy resolution helper intentionally disabled; Resolution owns scaling.
    namespace UIScale
    {
        // ============================================================
        // RESOLUÇÃO DE REFERÊNCIA
        // ============================================================

        constexpr float BASE_WIDTH = 1920.0f;
        constexpr float BASE_HEIGHT = 1080.0f;

        // ============================================================
        // ESCALA ATUAL
        // ============================================================

        inline float scale = 1.0f;

        inline ImVec2 screenSize{
            BASE_WIDTH,
            BASE_HEIGHT
        };

        // ============================================================
        // ATUALIZA A ESCALA
        // ============================================================

        inline void Update()
        {
            const ImVec2 display =
                ImGui::GetIO().DisplaySize;

            if (display.x <= 0.0f ||
                display.y <= 0.0f)
            {
                return;
            }

            screenSize = display;

            const float scaleX =
                display.x / BASE_WIDTH;

            const float scaleY =
                display.y / BASE_HEIGHT;

            // Utiliza a menor escala para evitar que
            // a interface ultrapasse as bordas da tela.
            scale = std::min(
                scaleX,
                scaleY
            );
        }

        // ============================================================
        // CONVERTE TAMANHOS
        // ============================================================

        inline float Size(float value)
        {
            return value * scale;
        }

        // ============================================================
        // CONVERTE POSIÇÕES
        // ============================================================

        inline ImVec2 Position(
            const ImVec2& value)
        {
            return ImVec2(
                value.x * scale,
                value.y * scale
            );
        }

        // ============================================================
        // CONVERTE POSIÇÕES RELATIVAS AO CENTRO
        // ============================================================

        inline ImVec2 CenterOffset(
            const ImVec2& offset)
        {
            const ImVec2 center(
                screenSize.x * 0.5f,
                screenSize.y * 0.5f
            );

            return ImVec2(
                center.x + offset.x * scale,
                center.y + offset.y * scale
            );
        }

        // ============================================================
        // RETORNA A ESCALA
        // ============================================================

        inline float Get()
        {
            return scale;
        }
    }*/

    
    // ============================================================
    // SETTINGS EDITOR
    // ============================================================

    struct SettingsItemHitbox
    {
        RE::TESForm* form = nullptr;

        RadialSide side = RadialSide::None;

        int index = -1;

        ImVec2 position{ 0.0f, 0.0f };

        float radius = 34.0f;

        std::uint16_t uniqueID = 0;
        bool hasUniqueID = false;

    };

    // ============================================================
    // SETTINGS
    // ============================================================
    
    struct SettingsDragState
    {
        bool active = false;

        RE::TESForm* form = nullptr;

        // Identificação da instância.
        std::uint16_t uniqueID = 0;
        bool hasUniqueID = false;

        // A instância real é retirada do radial ao iniciar o drag e permanece
        // aqui até ser reinserida no destino (ou devolvida à origem).
        RadialItem item{};

        RadialSide sourceSide = RadialSide::None;

        int sourceIndex = -1;

        ImVec2 position{ 0.0f, 0.0f };

        ImVec2 offset{ 0.0f, 0.0f };

        bool returning = false;

        ImVec2 returnPosition{ 0.0f, 0.0f };

        // A primeira lista de hitboxes ainda contém o item antes de ele ser
        // removido. Só usamos posições visuais para o drop depois de um novo
        // frame ter redesenhado os itens restantes.
        std::uint64_t hitboxGeneration = 0;

        // O radial lateral é uma lista circular. Guardamos a origem visual
        // antes de remover a instância para distinguir corretamente uma
        // peça principal de uma excedente mesmo quando o anel está girado.
        int sideScrollOffsetBeforeRemoval = 0;

    };

    static std::vector<SettingsItemHitbox>
        g_settingsItemHitboxes;
    static std::uint64_t g_settingsHitboxGeneration = 0;

    static std::unordered_map<
        RE::TESForm*,
        float
    > g_settingsOverflowHoverCache;

    static SettingsDragState g_settingsDrag;

    static const SettingsItemHitbox* GetSettingsHoveredHitbox(
        const ImVec2& mouse);
    static bool IsSettingsItemCoveredByControlPanel(
        const ImVec2& position, float radius);
    static void FinishSettingsDrag();

    static float g_settingsOverflowHoverT = 0.0f;

    // ============================================================
    // SETTINGS - GAMEPLAY DESCRIPTION
    // ============================================================

    static ImVec2 g_gameplayDescriptionButtonCenter{
        0.0f,
        0.0f
    };
    static ImVec2 g_gameplayPreviewButtonCenter{
        0.0f,
        0.0f
    };

    static float g_gameplayDescriptionButtonRadius = 12.0f;
    static float g_gameplayPreviewButtonRadius = 12.0f;

    static float g_gameplayDescriptionHoverT = 0.0f;
    static float g_gameplayPreviewHoverT = 0.0f;

    static std::array<ImVec2, 4> g_gameplayIconButtonCenters{};
    static std::array<float, 4> g_gameplayIconButtonHoverT{};
    static float g_gameplayIconButtonRadius = 12.0f;
    static ImVec2 g_customIconsReloadCenter{};
    static float g_customIconsReloadRadius = 8.0f;
    static float g_customIconsReloadHoverT = 0.0f;
    static ImVec2 g_fastDragButtonCenter{};
    static float g_fastDragButtonRadius = 12.0f;
    static float g_fastDragButtonHoverT = 0.0f;

    // ============================================================

    static RE::TESForm* g_settingsHoveredItem = nullptr;
    static std::uint16_t g_settingsHoveredUniqueID = 0;
    static bool g_settingsHoveredHasUniqueID = false;

    // ============================================================
    // QUICK DRAW
    //
    // Gestos são armazenados em coordenadas normalizadas ao círculo. Isso
    // torna o desenho independente de resolução, proporção de tela e do
    // raio visual usado no editor/gameplay.
    // ============================================================
    struct QuickDrawKey
    {
        RE::FormID formID = 0;
        std::uint16_t uniqueID = 0;
        bool hasUniqueID = false;
    };

    struct QuickDrawGesture
    {
        QuickDrawKey key{};
        std::vector<ImVec2> points;
    };

    static std::vector<QuickDrawGesture> g_quickDrawGestures;
    static bool g_quickDrawEditorOpen = false;
    static RadialItem g_quickDrawEditorItem{};
    static std::vector<ImVec2> g_quickDrawEditorStroke;
    static bool g_quickDrawEditorDrawing = false;
    static ImVec2 g_quickDrawEditorResetMin{};
    static ImVec2 g_quickDrawEditorResetMax{};
    static ImVec2 g_quickDrawEditorOkMin{};
    static ImVec2 g_quickDrawEditorOkMax{};

    static bool g_quickDrawGameplayActive = false;
    static bool g_quickDrawGameplayDrawing = false;
    // O botão que iniciou o gesto decide a mão de equipamento, não o radial
    // onde o item estava salvo.
    static bool g_quickDrawGameplayEquipLeft = false;
    static float g_quickDrawGameplayExpandT = 0.0f;
    static std::vector<ImVec2> g_quickDrawGameplayStroke;
    static RadialItem* g_quickDrawSelectionOverride = nullptr;
    static RadialSide g_quickDrawSelectionSide = RadialSide::None;
    static constexpr float QUICK_DRAW_EDITOR_RADIUS = RADIAL_DEADZONE * 5.0f;
    static constexpr float QUICK_DRAW_MATCH_THRESHOLD = 0.50f;

    static ImVec2 g_settingsMousePos{ 0.0f, 0.0f };
    static bool g_gamepadSettingsCursorActive = false;
    static bool g_gamepadSettingsPrimaryOwned = false;
    static bool g_gamepadSettingsDragClickArmed = false;
    static bool g_gamepadSettingsNavigationInPanel = false;
    static int g_gamepadSettingsNavigationIndex = 0;
    static bool g_gamepadSettingsNeedsCursorSync = false;
    static int g_gamepadHorizontalSelectionLatch = 0;
    static int g_gamepadSideSelection = -1;
    static ImVec2 g_gamepadAnalogSmoothedDelta{ 0.0f, 0.0f };
    static bool g_gamepadSettingsConfigNavigation = false;
    static RadialSide g_gamepadSettingsRadialFocus = RadialSide::None;
    static int g_gamepadSettingsRadialItemIndex = -1;

    int GetSideRadialItem(const ImVec2& mouse, const ImVec2& center,
        bool leftSide, int itemCount, float innerRadius);
    static int WrapSideIndex(int index, int count);
    static int WrapIndex(int index, int count);
    static int GetSideActualIndex(int visibleIndex);
    static void ClearTopBottomGamepadSelection(RadialSide side);
    static ImU32 MakeGameplayIconColor(const RadialItem& item, int alpha,
        float brightness = 1.0f, bool applyBaseOpacity = true,
        RadialSide side = RadialSide::Left);
    static void DrawSettingsDragQuickDrawIndicator(
        ImDrawList* draw, const ImVec2& position, float radius);

    // Eventos recebidos pelo InputSink.
    // Serão processados dentro do DrawSettingsMenu().
    static bool g_settingsLeftPressed = false;
    static bool g_settingsLeftReleased = false;
    static bool g_settingsRightPressed = false;
    static bool FinishQuickDrawGameplay();
    static void CancelQuickDrawGameplay();
    void UseSelectedRadialItem();
    void DrawSettingsCursor();

    enum class LayoutSlider : std::size_t
    {
        FontSize,
        DrawMarkDistance,
        ItemOpacity,
        RadialLineOpacity,
        GeneralItemSize,
        SlotSize,
        ItemBackgroundOpacity,
        ItemBorderOpacity,
        IconSize,
        BaseIconOpacity,
        OverflowLineOpacity,
        OverflowOpacity,
        OverflowSize,
        OverflowBackgroundOpacity,
        OverflowBorderOpacity,
        ItemNameOpacity,
        ItemNamePositionY,
        ItemNamePositionX,
        CenterOpacity,
        RadialQuantity,
        StardustFade,
        RadialStretch,
        RadialRotation,
        SidePosition,
        SideOpacity,
        TopPosition,
        TopStretch,
        TopOpacity,
        BottomPosition,
        BottomStretch,
        BottomOpacity,
        TopItemQuantity,
        TopGeneralItemSize,
        TopItemOpacity,
        TopLineOpacity,
        TopSlotSize,
        TopBackgroundOpacity,
        TopBorderOpacity,
        TopIconSize,
        TopIconOpacity,
        BottomItemQuantity,
        BottomGeneralItemSize,
        BottomItemOpacity,
        BottomLineOpacity,
        BottomSlotSize,
        BottomBackgroundOpacity,
        BottomBorderOpacity,
        BottomIconSize,
        BottomIconOpacity,
        PreviewMenuItemSize,
        PreviewMenuItemPositionY,
        PreviewMenuItemPositionX,
        PreviewMenuNameOpacity,
        PreviewMenuNamePositionY,
        PreviewMenuNamePositionX,
        PreviewTopItemSize,
        PreviewTopItemPositionY,
        PreviewTopItemPositionX,
        PreviewTopNameOpacity,
        PreviewTopNamePositionY,
        PreviewTopNamePositionX,
        PreviewBottomItemSize,
        PreviewBottomItemPositionY,
        PreviewBottomItemPositionX,
        PreviewBottomNameOpacity,
        PreviewBottomNamePositionY,
        PreviewBottomNamePositionX,
        PreviewRightItemSize,
        PreviewRightItemPositionY,
        PreviewRightItemPositionX,
        PreviewRightNameOpacity,
        PreviewRightNamePositionY,
        PreviewRightNamePositionX,
        PreviewLeftItemSize,
        PreviewLeftItemPositionY,
        PreviewLeftItemPositionX,
        PreviewLeftNameOpacity,
        PreviewLeftNamePositionY,
        PreviewLeftNamePositionX,
        PreviewSpellSizeMultiplier,
        PreviewWeaponSizeMultiplier,
        PreviewPotionSizeMultiplier,
        PreviewArmorSizeMultiplier,
        PreviewAmmoSizeMultiplier,
        PreviewBookSizeMultiplier,
        PreviewMiscSizeMultiplier,
        PreviewKeySizeMultiplier,
        PreviewSoulGemSizeMultiplier,
        PreviewIngredientSizeMultiplier,
        PreviewScrollSizeMultiplier,
        Count,
        None = Count
    };

    struct LayoutSliderHitbox
    {
        ImVec2 min{};
        ImVec2 max{};
    };
    static LayoutSliderHitbox g_fontFamilyButtonHitbox{};
    static LayoutSliderHitbox g_fontFamilyResetHitbox{};
    static LayoutSliderHitbox g_secondaryKeyResetHitbox{};
    static LayoutSliderHitbox g_altConfigKeyResetHitbox{};
    static ImVec2 g_showItemQuantityButtonCenter{};
    static float g_showItemQuantityButtonRadius = 0.0f;
    static float g_showItemQuantityHoverT = 0.0f;
    static ImVec2 g_showOverflowIconButtonCenter{};
    static float g_showOverflowIconButtonRadius = 0.0f;
    static float g_showOverflowIconHoverT = 0.0f;
    static ImVec2 g_stardustButtonCenter{};
    static float g_stardustButtonRadius = 0.0f;
    static float g_stardustButtonHoverT = 0.0f;

    struct LanguageOptionHitbox
    {
        ImVec2 min{};
        ImVec2 max{};
        std::string language;
    };

    static ImVec2 g_languageButtonMin{};
    static ImVec2 g_languageButtonMax{};
    static bool g_languageListOpen = false;
    static std::vector<LanguageOptionHitbox> g_languageOptionHitboxes;

    static std::array<LayoutSliderHitbox,
        static_cast<std::size_t>(LayoutSlider::Count)> g_layoutSliderHitboxes{};
    static std::array<LayoutSliderHitbox,
        static_cast<std::size_t>(LayoutSlider::Count)> g_layoutResetHitboxes{};
    static std::array<LayoutSliderHitbox,
        static_cast<std::size_t>(LayoutSlider::Count)> g_layoutCopyHitboxes{};
    static std::array<LayoutSliderHitbox,
        static_cast<std::size_t>(LayoutSlider::Count)> g_layoutMirrorHitboxes{};
    static std::array<float,
        static_cast<std::size_t>(LayoutSlider::Count)> g_layoutResetFlash{};
    static std::array<float,
        static_cast<std::size_t>(LayoutSlider::Count)> g_layoutCopyFlash{};
    static std::array<float,
        static_cast<std::size_t>(LayoutSlider::Count)> g_layoutMirrorFlash{};
    enum class LayoutGroup : std::size_t
    {
        Item,
        ItemPreview,
        PreviewMenu,
        PreviewTop,
        PreviewBottom,
        PreviewRight,
        PreviewLeft,
        ItemRadial,
        ItemOverflow,
        ItemColors,
        ItemSlot,
        ItemIcon,
        ItemTop,
        ItemTopSlot,
        ItemTopIcon,
        ItemBottom,
        ItemBottomSlot,
        ItemBottomIcon,
        Draw,
        Radial,
        Top,
        Bottom,
        Count
    };
    static std::array<LayoutSliderHitbox,
        static_cast<std::size_t>(LayoutGroup::Count)> g_layoutGroupHitboxes{};
    static std::array<bool,
        static_cast<std::size_t>(LayoutGroup::Count)> g_layoutGroupExpanded{};
    static std::array<int,
        static_cast<std::size_t>(LayoutSlider::Count)> g_layoutSliderRows{};
    static std::array<bool,
        static_cast<std::size_t>(LayoutSlider::Count)> g_layoutSliderVisible{};
    static float g_layoutPanelScroll = 0.0f;
    static float g_layoutPanelMaxScroll = 0.0f;
    static LayoutSliderHitbox g_layoutScrollbarHitbox{};
    static bool g_layoutScrollbarDragging = false;
    static float g_gameplayPanelScroll = 0.0f;
    static float g_gameplayPanelMaxScroll = 0.0f;
    static LayoutSliderHitbox g_gameplayScrollbarHitbox{};
    static bool g_gameplayScrollbarDragging = false;
    static float g_settingsPanelScroll = 0.0f;
    static float g_settingsPanelMaxScroll = 0.0f;
    static LayoutSliderHitbox g_settingsScrollbarHitbox{};
    static bool g_settingsScrollbarDragging = false;
    static LayoutSlider g_activeLayoutSlider = LayoutSlider::None;
    static bool g_previewLayoutButtonHeld = false;

    enum class PreviewLayoutField : std::size_t
    {
        ItemSize,
        ItemPositionY,
        ItemPositionX,
        NameOpacity,
        NamePositionY,
        NamePositionX
    };

    static bool GetPreviewSliderInfo(LayoutSlider slider,
        Config::ItemPreviewProfile& profile, PreviewLayoutField& field)
    {
        const auto first = static_cast<std::size_t>(LayoutSlider::PreviewMenuItemSize);
        const auto value = static_cast<std::size_t>(slider);
        const auto last = static_cast<std::size_t>(LayoutSlider::PreviewLeftNamePositionX);
        if (value < first || value > last) return false;
        const std::size_t offset = value - first;
        profile = static_cast<Config::ItemPreviewProfile>(offset / 6);
        field = static_cast<PreviewLayoutField>(offset % 6);
        return true;
    }

    static bool GetPreviewCategorySliderInfo(LayoutSlider slider,
        std::size_t& categoryIndex)
    {
        const auto first = static_cast<std::size_t>(
            LayoutSlider::PreviewSpellSizeMultiplier);
        const auto value = static_cast<std::size_t>(slider);
        const auto last = static_cast<std::size_t>(
            LayoutSlider::PreviewScrollSizeMultiplier);
        if (value < first || value > last) return false;
        categoryIndex = value - first;
        return categoryIndex < Config::g_itemPreviewCategoryMultipliers.size();
    }

    static bool GetActivePreviewCategory(
        Config::ItemPreviewCategory& category)
    {
        std::size_t categoryIndex = 0;
        if (!GetPreviewCategorySliderInfo(
                g_activeLayoutSlider, categoryIndex))
            return false;
        category = static_cast<Config::ItemPreviewCategory>(categoryIndex);
        return true;
    }

    static Config::ItemPreviewProfile g_previewLayoutButtonProfile =
        Config::ItemPreviewProfile::Menu;
    static PreviewLayoutField g_previewLayoutButtonField =
        PreviewLayoutField::ItemSize;

    static bool GetActivePreviewControl(
        Config::ItemPreviewProfile& profile, PreviewLayoutField& field)
    {
        if (GetPreviewSliderInfo(g_activeLayoutSlider, profile, field))
            return true;
        if (!g_previewLayoutButtonHeld)
            return false;
        profile = g_previewLayoutButtonProfile;
        field = g_previewLayoutButtonField;
        return true;
    }

    static float& PreviewLayoutValue(Config::ItemPreviewProfile profile,
        PreviewLayoutField field)
    {
        auto& preview = Config::GetItemPreviewLayout(profile);
        switch (field)
        {
        case PreviewLayoutField::ItemSize: return preview.itemSize;
        case PreviewLayoutField::ItemPositionY: return preview.itemPositionY;
        case PreviewLayoutField::ItemPositionX: return preview.itemPositionX;
        case PreviewLayoutField::NameOpacity: return preview.itemNameOpacity;
        case PreviewLayoutField::NamePositionY: return preview.itemNamePositionY;
        case PreviewLayoutField::NamePositionX: return preview.itemNamePositionX;
        }
        return preview.itemSize;
    }

    static Config::ItemPreviewProfile OppositePreviewProfile(
        Config::ItemPreviewProfile profile)
    {
        switch (profile)
        {
        case Config::ItemPreviewProfile::Top: return Config::ItemPreviewProfile::Bottom;
        case Config::ItemPreviewProfile::Bottom: return Config::ItemPreviewProfile::Top;
        case Config::ItemPreviewProfile::Right: return Config::ItemPreviewProfile::Left;
        case Config::ItemPreviewProfile::Left: return Config::ItemPreviewProfile::Right;
        default: return Config::ItemPreviewProfile::Menu;
        }
    }

    static float DefaultPreviewLayoutValue(PreviewLayoutField field)
    {
        switch (field)
        {
        case PreviewLayoutField::ItemSize: return Config::kDefaultItemPreviewSize;
        case PreviewLayoutField::ItemPositionY: return Config::kDefaultItemPreviewPositionY;
        case PreviewLayoutField::ItemPositionX: return Config::kDefaultItemPreviewPositionX;
        case PreviewLayoutField::NameOpacity: return Config::kDefaultItemNameOpacity;
        case PreviewLayoutField::NamePositionY: return Config::kDefaultItemNamePositionY;
        case PreviewLayoutField::NamePositionX: return Config::kDefaultItemNamePositionX;
        }
        return 0.0f;
    }

    static bool PreviewFieldSupportsMirror(PreviewLayoutField field)
    {
        return field != PreviewLayoutField::ItemSize &&
            field != PreviewLayoutField::NameOpacity;
    }

    static Config::ItemPreviewProfile PreviewProfileForActiveSlider()
    {
        Config::ItemPreviewProfile profile = Config::ItemPreviewProfile::Menu;
        PreviewLayoutField field{};
        GetActivePreviewControl(profile, field);
        return profile;
    }

    static Config::ItemPreviewProfile PreviewProfileForSide(RadialSide side)
    {
        switch (side)
        {
        case RadialSide::Top: return Config::ItemPreviewProfile::Top;
        case RadialSide::Bottom: return Config::ItemPreviewProfile::Bottom;
        case RadialSide::Right: return Config::ItemPreviewProfile::Right;
        case RadialSide::Left: return Config::ItemPreviewProfile::Left;
        default: return Config::ItemPreviewProfile::Menu;
        }
    }

    static ImVec2 PreviewRadialCenter(Config::ItemPreviewProfile profile,
        const WheelLayout& layout)
    {
        switch (profile)
        {
        case Config::ItemPreviewProfile::Top: return layout.topRadial;
        case Config::ItemPreviewProfile::Bottom: return layout.bottomRadial;
        case Config::ItemPreviewProfile::Right: return layout.rightRadial;
        case Config::ItemPreviewProfile::Left: return layout.leftRadial;
        default: return layout.center;
        }
    }

    static bool GetVisibleItemPreviewMask(ImVec2& center, float& radius)
    {
        if (!ItemPreview::IsVisible())
            return false;

        Config::ItemPreviewProfile profile = Config::ItemPreviewProfile::Menu;
        PreviewLayoutField field{};
        if (SettingsMenu::WheelSettingsMenu::IsOpen())
            GetActivePreviewControl(profile, field);
        else
            profile = PreviewProfileForSide(g_radialSide);

        const auto& preview = Config::GetItemPreviewLayoutConst(profile);
        const WheelLayout layout = GetWheelLayout();
        center = ImVec2(
            LayoutLerp(layout.min.x, layout.max.x, preview.itemPositionX),
            LayoutLerp(layout.min.y, layout.max.y, preview.itemPositionY));
        radius = std::clamp(64.0f * preview.itemSize * 0.01f,
            12.0f, 360.0f);
        return true;
    }
    enum class LayoutColorControl : std::size_t
    {
        Background,
        Border,
        Icon,
        OverflowBackground,
        OverflowBorder,
        PotionHealth,
        PotionStamina,
        PotionMagicka,
        PotionPoison,
        PotionFire,
        PotionFrost,
        PotionShock,
        SchoolAlteration,
        SchoolConjuration,
        SchoolDestruction,
        SchoolIllusion,
        SchoolRestoration,
        MagicFire,
        MagicFrost,
        MagicShock,
        EnchantFire,
        EnchantFrost,
        EnchantShock,
        EnchantPoison,
        EnchantDefault,
        TopBackground,
        TopBorder,
        TopIcon,
        BottomBackground,
        BottomBorder,
        BottomIcon,
        Count
    };
    static std::array<LayoutSliderHitbox,
        static_cast<std::size_t>(LayoutColorControl::Count)> g_layoutColorHitboxes{};
    static std::array<LayoutSliderHitbox,
        static_cast<std::size_t>(LayoutColorControl::Count)> g_layoutColorResetHitboxes{};
    static LayoutSliderHitbox g_layoutColorPickerHitbox{};
    static int g_openLayoutColor = -1;
    static int g_activeLayoutColor = -1;
    static LayoutSliderHitbox g_layoutSaveButtonHitbox{};
    static LayoutSliderHitbox g_layoutLoadButtonHitbox{};
    static LayoutSliderHitbox g_layoutNameOkHitbox{};
    static LayoutSliderHitbox g_layoutNameCancelHitbox{};
    static bool g_layoutNameOpen = false;
    static bool g_layoutLoadOpen = false;
    static std::string g_layoutPresetName = "My Layout";
    struct LayoutPresetHitbox
    {
        LayoutSliderHitbox hitbox{};
        LayoutSliderHitbox deleteHitbox{};
        std::string name;
    };
    static std::vector<LayoutPresetHitbox> g_layoutPresetHitboxes;

    enum class GeneralSlider : std::size_t
    {
        MouseSensitivity,
        MouseSmooth,
        AnalogSensitivity,
        AnalogSmooth,
        Count,
        None = Count
    };
    static std::array<LayoutSliderHitbox,
        static_cast<std::size_t>(GeneralSlider::Count)> g_generalSliderHitboxes{};
    static std::array<LayoutSliderHitbox,
        static_cast<std::size_t>(GeneralSlider::Count)> g_generalResetHitboxes{};
    static GeneralSlider g_activeGeneralSlider = GeneralSlider::None;
    static LayoutSliderHitbox g_resetAllConfigHitbox{};


    // ============================================================
    // SETTINGS - MENU DE SEÇÕES
    // ============================================================

    enum class SettingsSection
    {
        ItemInfo,
        Settings,
        Gameplay,
        Layout
    };

    // Seção atualmente exibida.
    static SettingsSection g_settingsSection =
        SettingsSection::ItemInfo;

    // Animação de abertura do menu.
    static float g_settingsSectionOpenT = 0.0f;

    // Animação do hover de cada botão.
    static float g_settingsCenterHoverT = 0.0f;
    static float g_settingsGameplayHoverT = 0.0f;
    static float g_settingsLayoutHoverT = 0.0f;

    // Estado de abertura.
    static bool g_settingsSectionExpanded = false;

    // Após fechar um painel, mantém os três atalhos disponíveis por alguns
    // segundos. Isso evita precisar reabrir o seletor central para trocar de
    // seção logo em seguida.
    static float g_settingsSectionLingerRemaining = 0.0f;
    static bool g_settingsSectionLingerActive = false;
    constexpr float kSettingsSectionLingerDuration = 10.0f;

    // Identificação do item associado ao seletor.
    static RE::TESForm* g_sectionPreviewForm = nullptr;

    static std::uint16_t g_sectionPreviewUniqueID = 0;

    static bool g_sectionPreviewHasUniqueID = false;

    
    // ============================================================
    // SETTINGS - CAPTURA DA TECLA DO WHEEL
    // ============================================================

    static bool g_waitingWheelKey = false;
    static int g_waitingWheelKeySlot = 0;

    static ImVec2 g_wheelKeyButtonMin{ 0.0f, 0.0f };

    static ImVec2 g_wheelKeyButtonMax{ 0.0f, 0.0f };
    static ImVec2 g_secondaryKeyButtonMin{ 0.0f, 0.0f };
    static ImVec2 g_secondaryKeyButtonMax{ 0.0f, 0.0f };
    static ImVec2 g_altConfigKeyButtonMin{ 0.0f, 0.0f };
    static ImVec2 g_altConfigKeyButtonMax{ 0.0f, 0.0f };
    static ImVec2 g_automaticArrowButtonCenter{ 0.0f, 0.0f };
    static float g_automaticArrowButtonRadius = 0.0f;
    static ImVec2 g_radialAnimationButtonMin{ 0.0f, 0.0f };
    static ImVec2 g_radialAnimationButtonMax{ 0.0f, 0.0f };
    static bool g_radialAnimationListOpen = false;
    static std::vector<LayoutSliderHitbox> g_radialAnimationOptionHitboxes;
    static ImVec2 g_radialShapeButtonMin{ 0.0f, 0.0f };
    static ImVec2 g_radialShapeButtonMax{ 0.0f, 0.0f };
    static bool g_radialShapeListOpen = false;
    static std::vector<LayoutSliderHitbox> g_radialShapeOptionHitboxes;
    static bool g_radialShapeSliderDragging = false;
    static bool g_radialAnimationSliderDragging = false;
    static ImVec2 g_trackModeButtonMin{ 0.0f, 0.0f };
    static ImVec2 g_trackModeButtonMax{ 0.0f, 0.0f };
    static ImVec2 g_trackEditorButtonMin{ 0.0f, 0.0f };
    static ImVec2 g_trackEditorButtonMax{ 0.0f, 0.0f };
    static ImVec2 g_slowTimeButtonCenter{ 0.0f, 0.0f };
    static float g_slowTimeButtonRadius = 0.0f;
    static ImVec2 g_slowTimeSliderMin{ 0.0f, 0.0f };
    static ImVec2 g_slowTimeSliderMax{ 0.0f, 0.0f };
    static LayoutSliderHitbox g_slowTimeResetHitbox{};
    static bool g_slowTimeSliderDragging = false;
    static std::array<ImVec2, 4> g_slowTimeScopeCenters{};
    static float g_slowTimeScopeRadius = 0.0f;
    static std::array<ImVec2, 4> g_blurScopeCenters{};
    static float g_blurScopeRadius = 0.0f;
    static bool g_gameplayBlurApplied = false;

        
    // ============================================================
    // POSIÇÕES DO SELETOR DE CONFIGURAÇÕES
    // ============================================================

    struct SettingsSectionLayout
    {
        ImVec2 center{};
        ImVec2 gameplay{};
        ImVec2 layout{};

        float centerRadius = 16.0f;
        float expandedRadius = 26.0f;

        float secondaryRadius = 16.0f;

        float verticalDistance = 90.0f;
    };


    static SettingsSectionLayout GetSettingsSectionLayout()
    {
        SettingsSectionLayout result{};

        const WheelLayout wheelLayout = GetWheelLayout();

        ImVec2 panelMin{};
        ImVec2 panelMax{};
        GetFixedSettingsPanelBounds(wheelLayout, panelMin, panelMax);
        const float horizontalY = std::max(
            wheelLayout.min.y + result.expandedRadius + 10.0f,
            panelMin.y - result.expandedRadius - 14.0f);
        result.gameplay = ImVec2(panelMin.x, horizontalY);
        result.layout = ImVec2(panelMax.x, horizontalY);
        result.center = ImVec2((panelMin.x + panelMax.x) * 0.5f, horizontalY);
        result.verticalDistance = std::max(1.0f, (panelMax.x - panelMin.x) * 0.5f);

        return result;
    }

    // ============================================================
    // ITEM SELECIONADO DURANTE GAMEPLAY
    // ============================================================

    static RadialItem g_gameplayDescriptionItem{};

    static bool g_gameplayDescriptionHasItem = false;
    
    static void SetGameplayDescriptionItem(
        const RadialItem& item)
    {
        if (!item.form)
            return;

        g_gameplayDescriptionItem = item;

        g_gameplayDescriptionHasItem = true;
    }

    // ============================================================
    // VERIFICA SE UMA TECLA PODE SER CONFIGURADA
    // ============================================================

    static bool IsSupportedWheelKey(int virtualKey)
    {
        if ((virtualKey >= 'A' && virtualKey <= 'Z') ||
            (virtualKey >= '0' && virtualKey <= '9'))
        {
            return true;
        }

        if (virtualKey >= VK_F1 &&
            virtualKey <= VK_F24)
        {
            return true;
        }

        if (virtualKey >= VK_NUMPAD0 &&
            virtualKey <= VK_NUMPAD9)
        {
            return true;
        }

        switch (virtualKey)
        {
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_UP:
        case VK_DOWN:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_SPACE:
        case VK_TAB:
        case VK_RETURN:
        case VK_BACK:
        case VK_CAPITAL:
            return true;

        default:
            return false;
        }
    }
    
    // ============================================================
    // FECHAMENTO DO SELETOR
    // ============================================================

    static void CloseLayoutPresetDialogs()
    {
        // Equivale a cancelar o SAVE e recolher a lista de LOAD, sem
        // modificar o layout nem o texto que o usuário já digitou.
        g_layoutNameOpen = false;
        g_layoutLoadOpen = false;
    }

    static void CloseSettingsSectionMenu()
    {
        CloseLayoutPresetDialogs();

        g_settingsSectionExpanded = false;

        g_settingsSectionLingerRemaining = 0.0f;
        g_settingsSectionLingerActive = false;

        g_settingsSection =
            SettingsSection::ItemInfo;

        // Não zeramos os valores de animação.
        // O Update reduzirá os valores gradualmente.
    }

    static void CloseSettingsPanelKeepSectionShortcuts()
    {
        CloseLayoutPresetDialogs();

        // Fecha o painel de conteúdo imediatamente, mas mantém o seletor
        // expandido e interativo durante a janela de retorno.
        g_settingsSection = SettingsSection::ItemInfo;
        g_settingsSectionExpanded = true;
        // Esta função também pode ser chamada por uma atualização de seleção
        // enquanto o contador já está em curso. Não o reinicie nesse caso.
        if (!g_settingsSectionLingerActive)
        {
            g_settingsSectionLingerRemaining = kSettingsSectionLingerDuration;
            g_settingsSectionLingerActive = true;
        }
    }

    // ============================================================
    // HITBOX CIRCULAR
    // ============================================================

    static bool IsSettingsCircleHovered(
        const ImVec2& mouse,
        const ImVec2& center,
        float radius)
    {
        const float dx =
            mouse.x - center.x;

        const float dy =
            mouse.y - center.y;

        return dx * dx + dy * dy <=
            radius * radius;
    }

    
    // ============================================================
    // INTERPOLAÇÃO SUAVE
    // ============================================================

    static float AnimateSettingsValue(
        float current,
        float target,
        float speed,
        float deltaTime)
    {
        deltaTime = std::clamp(
            deltaTime,
            0.0f,
            1.0f / 30.0f
        );

        const float factor =
            1.0f - std::exp(-speed * deltaTime);

        return current +
            (target - current) * factor;
    }


    // ============================================================
    // UPDATE - SELETOR DE CONFIGURAÇÕES
    // ============================================================

    //helper  
    static float SmoothSettingsProgress(float value)
    {
        value = std::clamp(
            value,
            0.0f,
            1.0f
        );

        return value * value *
            (3.0f - 2.0f * value);
    }

        
    static void UpdateSettingsSectionMenu(
        float deltaTime)
    {
        const ImVec2 screen =
            ImGui::GetIO().DisplaySize;

        const ImVec2 mouse =
            g_settingsMousePos;

        const SettingsSectionLayout layout =
            GetSettingsSectionLayout();

        const bool morphStable =
            g_settingsSectionMorphT <= 0.02f ||
            g_settingsSectionMorphT >= 0.98f;

        const bool settingsOpen =
            SettingsMenu::WheelSettingsMenu::IsOpen();

        // ========================================================
        // HOVER DO BOTÃO CENTRAL
        // ========================================================

        const float currentCenterRadius =
            layout.centerRadius +
            (layout.expandedRadius - layout.centerRadius) *
            g_settingsCenterHoverT;

        const bool centerHovered =
            settingsOpen &&
            morphStable &&
            IsSettingsCircleHovered(
                mouse,
                layout.center,
                currentCenterRadius
            );

        // O botão central precisa abrir o seletor antes de os círculos
        // secundários serem calculados nesta atualização.
        if (!settingsOpen)
        {
            CloseSettingsSectionMenu();
        }
        else if (centerHovered)
        {
            g_settingsSectionExpanded = true;
        }

        // ========================================================
        // ANIMAÇÃO PRINCIPAL
        //
        // Antes: velocidade 12.
        // Agora: velocidade 4.5.
        //
        // Abertura e fechamento mais lentos.
        // ========================================================

        const float openTarget =
            settingsOpen &&
            g_settingsSectionExpanded
                ? 1.0f
                : 0.0f;

        constexpr float openSpeed = 4.5f;
        constexpr float closeSpeed = 3.5f;

        const float animationSpeed =
            g_settingsSectionExpanded
                ? openSpeed
                : closeSpeed;

        g_settingsSectionOpenT =
            AnimateSettingsValue(
                g_settingsSectionOpenT,
                openTarget,
                animationSpeed,
                deltaTime
            );

        // ========================================================
        // PROGRESSO DA LINHA
        //
        // Primeiro a linha cresce.
        // Os círculos aparecem somente depois.
        // ========================================================

        const float lineT =
            SmoothSettingsProgress(
                g_settingsSectionOpenT / 0.65f
            );

        const float circleT =
            SmoothSettingsProgress(
                (g_settingsSectionOpenT - 0.65f) /
                0.25f
            );

        // ========================================================
        // POSIÇÕES DOS BOTÕES
        //
        // As posições finais são fixas.
        // A linha cresce até elas.
        //
        // Não movimentamos os próprios círculos
        // junto com a ponta da linha.
        // ========================================================

        const ImVec2 gameplayPosition =
            layout.gameplay;

        const ImVec2 layoutPosition =
            layout.layout;

        // ========================================================
        // HOVER DOS BOTÕES SECUNDÁRIOS
        // ========================================================

        const bool secondaryEnabled =
            settingsOpen &&
            morphStable &&
            g_settingsSectionExpanded &&
            circleT > 0.95f &&
            lineT > 0.99f;

        const float gameplayRadius =
            layout.secondaryRadius +
            (layout.expandedRadius - layout.secondaryRadius) *
            g_settingsGameplayHoverT;

        const float layoutRadius =
            layout.secondaryRadius +
            (layout.expandedRadius - layout.secondaryRadius) *
            g_settingsLayoutHoverT;

        const bool gameplayHovered =
            secondaryEnabled &&
            IsSettingsCircleHovered(
                mouse,
                gameplayPosition,
                gameplayRadius
            );

        const bool layoutHovered =
            secondaryEnabled &&
            IsSettingsCircleHovered(
                mouse,
                layoutPosition,
                layoutRadius
            );

        // ========================================================
        // HOVER DO BOTÃO CENTRAL
        // ========================================================

        g_settingsCenterHoverT =
            AnimateSettingsValue(
                g_settingsCenterHoverT,
                centerHovered &&
                    g_settingsSectionExpanded
                        ? 1.0f
                        : 0.0f,
                7.0f,
                deltaTime
            );

        // ========================================================
        // HOVER GAMEPLAY
        // ========================================================

        g_settingsGameplayHoverT =
            AnimateSettingsValue(
                g_settingsGameplayHoverT,
                gameplayHovered ? 1.0f : 0.0f,
                7.0f,
                deltaTime
            );

        // ========================================================
        // HOVER LAYOUT
        // ========================================================

        g_settingsLayoutHoverT =
            AnimateSettingsValue(
                g_settingsLayoutHoverT,
                layoutHovered ? 1.0f : 0.0f,
                7.0f,
                deltaTime
            );

        // ========================================================
        // SEÇÃO EXIBIDA
        // ========================================================

        if (g_settingsSectionExpanded)
        {
            if (gameplayHovered)
            {
                if (g_settingsSection == SettingsSection::Layout)
                    CloseLayoutPresetDialogs();
                g_settingsSection =
                    SettingsSection::Gameplay;
            }
            else if (layoutHovered)
            {
                g_settingsSection =
                    SettingsSection::Layout;
            }
            else if (centerHovered)
            {
                if (g_settingsSection == SettingsSection::Layout)
                    CloseLayoutPresetDialogs();
                g_settingsSection =
                    SettingsSection::Settings;
            }
        }

        // ========================================================
        // TEMPORIZADOR DE RECOLHIMENTO
        //
        // Hover em qualquer uma das três bolinhas restaura o contador e o
        // pausa. Enquanto o respectivo painel permanece aberto, ele também
        // fica pausado. Só contamos quando o cursor saiu da área do seletor
        // ou do painel de configurações.
        // ========================================================

        if (settingsOpen && g_settingsSectionExpanded)
        {
            const bool selectorHovered = centerHovered || gameplayHovered || layoutHovered;

            if (selectorHovered)
            {
                g_settingsSectionLingerRemaining = kSettingsSectionLingerDuration;
                g_settingsSectionLingerActive = false;
            }
            else if (g_settingsSection != SettingsSection::ItemInfo)
            {
                // Mantém a lógica original do painel: mover do seletor para
                // os seus controles não o fecha. Só cruzar o centro da tela
                // encerra a seção aberta.
                if (mouse.x < screen.x * 0.50f)
                    CloseSettingsPanelKeepSectionShortcuts();
                else
                {
                    g_settingsSectionLingerRemaining = kSettingsSectionLingerDuration;
                    g_settingsSectionLingerActive = false;
                }
            }

            if (g_settingsSection == SettingsSection::ItemInfo &&
                !selectorHovered)
            {
                // O conteúdo já foi fechado por um gatilho válido (cruzar o
                // centro ou mudar de item). A partir daqui só as três bolinhas
                // permanecem por dez segundos.
                if (!g_settingsSectionLingerActive)
                {
                    g_settingsSectionLingerRemaining = kSettingsSectionLingerDuration;
                    g_settingsSectionLingerActive = true;
                }

                g_settingsSectionLingerRemaining = std::max(
                    0.0f, g_settingsSectionLingerRemaining - deltaTime);
                if (g_settingsSectionLingerRemaining <= 0.0f)
                    CloseSettingsSectionMenu();
            }
        }

        if (g_settingsSection != SettingsSection::Settings)
        {
            g_waitingWheelKey = false;
            g_waitingWheelKeySlot = 0;
        }
    }

        
    // ============================================================
    // DRAW - SELETOR DE CONFIGURAÇÕES
    // ============================================================

        
    static void DrawSettingsSectionMenu(
        float alpha)
    {
        ImDrawList* draw =
            ImGui::GetForegroundDrawList();

        if (!draw)
            return;

        const SettingsSectionLayout layout =
            GetSettingsSectionLayout();

        // ========================================================
        // PROGRESSO DAS ANIMAÇÕES
        // ========================================================

        const float openT =
            std::clamp(
                g_settingsSectionOpenT,
                0.0f,
                1.0f
            );

        // A linha cresce primeiro.
        const float lineT =
            SmoothSettingsProgress(
                openT / 0.65f
            );

        // Contornos aparecem depois da linha.
        const float circleT =
            SmoothSettingsProgress(
                (openT - 0.65f) / 0.25f
            );

        // Preenchimento aparece por último.
        const float fillT =
            SmoothSettingsProgress(
                (openT - 0.80f) / 0.20f
            );

        const float morphT = std::clamp(g_settingsSectionMorphT, 0.0f, 1.0f);
        const float gameplayMorphVisibility = std::max(
            1.0f - SmoothSettingsProgress(morphT / 0.28f),
            SmoothSettingsProgress((morphT - 0.78f) / 0.22f));
        const float settingsMorphVisibility = std::max(
            1.0f - SmoothSettingsProgress((morphT - 0.25f) / 0.15f),
            SmoothSettingsProgress((morphT - 0.62f) / 0.14f));
        const float layoutMorphVisibility = gameplayMorphVisibility;

                
        // ============================================================
        // LINHA VERTICAL
        //
        // A linha é dividida em dois segmentos.
        //
        // Não atravessa o círculo central nem os círculos
        // superiores e inferiores quando eles aparecem.
        // ============================================================

        if (lineT > 0.001f && morphT <= 0.001f)
        {
            // ========================================================
            // DISTÂNCIA ATUAL DA LINHA
            // ========================================================

            const float distance =
                layout.verticalDistance * lineT;

            // ========================================================
            // RAIO ATUAL DO CÍRCULO CENTRAL
            // ========================================================

            const float centerRadius =
                layout.centerRadius +
                (
                    layout.expandedRadius -
                    layout.centerRadius
                ) *
                g_settingsCenterHoverT;

            // ========================================================
            // RAIO ATUAL DO BOTÃO GAMEPLAY
            // ========================================================

            const float gameplayRadius =
                layout.secondaryRadius +
                (
                    layout.expandedRadius -
                    layout.secondaryRadius
                ) *
                g_settingsGameplayHoverT;

            // ========================================================
            // RAIO ATUAL DO BOTÃO LAYOUT
            // ========================================================

            const float layoutRadius =
                layout.secondaryRadius +
                (
                    layout.expandedRadius -
                    layout.secondaryRadius
                ) *
                g_settingsLayoutHoverT;

            // ========================================================
            // MARGEM ENTRE LINHA E CÍRCULOS
            // ========================================================

            constexpr float lineGap = 2.0f;

            // ========================================================
            // INÍCIO DOS SEGMENTOS
            //
            // Deixa o espaço ocupado pelo círculo central vazio.
            // ========================================================

            const float lineStart =
                centerRadius + lineGap;

            // ========================================================
            // EXTREMIDADES DA LINHA
            //
            // Enquanto circleT = 0, a linha cresce normalmente.
            //
            // Quando os círculos aparecem, a linha recua
            // até suas bordas.
            // ========================================================

            const float topLineEnd =
                distance -
                (gameplayRadius + lineGap);

            const float bottomLineEnd =
                distance -
                (layoutRadius + lineGap);

            // ========================================================
            // COR DA LINHA
            // ========================================================

            const ImU32 lineColor =
                FadeColor(
                    IM_COL32(235, 230, 215, 120),
                    alpha
                );

            // ========================================================
            // SEGMENTO SUPERIOR
            // ========================================================

            if (topLineEnd > lineStart)
            {
                draw->AddLine(
                    ImVec2(
                        layout.center.x,
                        layout.center.y - lineStart
                    ),

                    ImVec2(
                        layout.center.x,
                        layout.center.y - topLineEnd
                    ),

                    lineColor,
                    1.5f
                );
            }

            // ========================================================
            // SEGMENTO INFERIOR
            // ========================================================

            if (bottomLineEnd > lineStart)
            {
                draw->AddLine(
                    ImVec2(
                        layout.center.x,
                        layout.center.y + lineStart
                    ),

                    ImVec2(
                        layout.center.x,
                        layout.center.y + bottomLineEnd
                    ),

                    lineColor,
                    1.5f
                );
            }
        }

        if (lineT > 0.001f && morphT > 0.001f)
        {
            const ImU32 lineColor = FadeColor(
                IM_COL32(235, 230, 215, 120), alpha);
            constexpr float lineGap = 2.0f;
            const float gameplayRadius = layout.secondaryRadius +
                (layout.expandedRadius - layout.secondaryRadius) *
                    g_settingsGameplayHoverT;
            const float settingsRadius = layout.centerRadius +
                (layout.expandedRadius - layout.centerRadius) *
                    g_settingsCenterHoverT;
            const float layoutRadius = layout.secondaryRadius +
                (layout.expandedRadius - layout.secondaryRadius) *
                    g_settingsLayoutHoverT;

            if (morphT < 0.55f)
            {
                const float extent =
                    (1.0f - SmoothSettingsProgress(morphT / 0.32f)) * lineT;
                const float topLimit = layout.center.y -
                    (layout.center.y - layout.gameplay.y) * extent;
                const float bottomLimit = layout.center.y +
                    (layout.layout.y - layout.center.y) * extent;
                const float topStart = std::max(
                    topLimit,
                    layout.gameplay.y + gameplayRadius + lineGap);
                const float topEnd = layout.center.y - settingsRadius - lineGap;
                if (topEnd > topStart)
                {
                    draw->AddLine(
                        ImVec2(layout.center.x, topStart),
                        ImVec2(layout.center.x, topEnd), lineColor, 1.5f);
                }
                const float bottomStart = layout.center.y + settingsRadius + lineGap;
                const float bottomEnd = std::min(
                    bottomLimit,
                    layout.layout.y - layoutRadius - lineGap);
                if (bottomEnd > bottomStart)
                {
                    draw->AddLine(
                        ImVec2(layout.center.x, bottomStart),
                        ImVec2(layout.center.x, bottomEnd), lineColor, 1.5f);
                }
            }
            else
            {
                const float extent =
                    SmoothSettingsProgress((morphT - 0.72f) / 0.28f) * lineT;
                const float lineLeft = layout.center.x -
                    (layout.center.x - layout.gameplay.x) * extent;
                const float lineRight = layout.center.x +
                    (layout.layout.x - layout.center.x) * extent;

                const auto drawHorizontalSegment = [&](float startX, float endX) {
                    startX = std::max(startX, lineLeft);
                    endX = std::min(endX, lineRight);
                    if (endX > startX)
                    {
                        draw->AddLine(
                            ImVec2(startX, layout.layout.y),
                            ImVec2(endX, layout.layout.y),
                            lineColor,
                            1.5f);
                    }
                };

                drawHorizontalSegment(
                    layout.gameplay.x + gameplayRadius + lineGap,
                    layout.center.x - settingsRadius - lineGap);
                drawHorizontalSegment(
                    layout.center.x + settingsRadius + lineGap,
                    layout.layout.x - layoutRadius - lineGap);
            }
        }

        // ========================================================
        // FUNÇÃO AUXILIAR PARA DESENHAR OS BOTÕES
        // ========================================================

        auto DrawSectionButton =
            [&](const ImVec2& position,
                float baseRadius,
                float hoverT,
                float visibility,
                float outlineT,
                const char* label,
                bool isCenter)
        {
            visibility =
                std::clamp(
                    visibility,
                    0.0f,
                    1.0f
                );

            outlineT =
                std::clamp(
                    outlineT,
                    0.0f,
                    1.0f
                );

            if (visibility <= 0.001f &&
                outlineT <= 0.001f)
            {
                return;
            }

            // ====================================================
            // RAIO ANIMADO
            // ====================================================

            const float radius =
                baseRadius +
                (layout.expandedRadius - baseRadius) *
                hoverT;

            // ====================================================
            // OPACIDADE ANIMADA DO BOTÃO
            //
            // Sem hover = 50%
            // Hover completo = 80%
            //
            // A animação utiliza o hoverT que já existe.
            // ====================================================

            const float buttonOpacity =
                0.50f + 0.30f * hoverT;

            const float buttonAlpha =
                alpha * buttonOpacity;

            // ====================================================
            // FUNDO
            //
            // Normal: cinza escuro.
            // Hover: branco.
            // ====================================================

            const int brightness =
                static_cast<int>(
                    20.0f + 215.0f * hoverT
                );

            const int backgroundAlpha =
                static_cast<int>(
                    235.0f - 85.0f * hoverT
                );

            // O botão central existe mesmo com o menu fechado.
            // Os secundários aparecem após a linha.
            const float backgroundVisibility =
                isCenter ? visibility : std::max(visibility, outlineT);

            draw->AddCircleFilled(
                position,
                radius,
                FadeColor(
                    IM_COL32(
                        brightness,
                        brightness,
                        brightness,
                        backgroundAlpha
                    ),
                    buttonAlpha * backgroundVisibility
                ),
                48
            );

            // ====================================================
            // CONTORNO CIRCULAR PROGRESSIVO
            //
            // Nos botões secundários, o contorno é desenhado
            // gradualmente após a linha chegar ao destino.
            // ====================================================

            constexpr int segments = 48;

            if (isCenter)
            {
                draw->AddCircle(
                    position,
                    radius,
                    FadeColor(
                        IM_COL32(255, 255, 255, 180),
                        buttonAlpha * visibility
                    ),
                    segments,
                    1.5f
                );
            }
            else if (outlineT > 0.001f)
            {
                ImVec2 points[segments + 1];

                const float startAngle =
                    -PI * 0.5f;

                const int visibleSegments =
                    std::clamp(
                        static_cast<int>(
                            std::ceil(
                                segments * outlineT
                            )
                        ),
                        1,
                        segments
                    );

                for (int i = 0;
                    i <= visibleSegments;
                    ++i)
                {
                    const float progress =
                        static_cast<float>(i) /
                        static_cast<float>(segments);

                    const float angle =
                        startAngle +
                        progress * 2.0f * PI;

                    points[i] = ImVec2(
                        position.x +
                            std::cos(angle) * radius,

                        position.y +
                            std::sin(angle) * radius
                    );
                }

                draw->AddPolyline(
                    points,
                    visibleSegments + 1,
                    FadeColor(
                        IM_COL32(
                            255, 255, 255, 180
                        ),
                        buttonAlpha * outlineT
                    ),
                    false,
                    1.5f
                );
            }

            // ====================================================
            // TEXTO PRETO
            //
            // Apenas quando hovered.
            // Gameplay e Layout aparecem dentro do círculo.
            // ====================================================

            //const float textVisibility =
            //    hoverT *
            //    (isCenter ? 1.0f : visibility);

            //if (!label ||
            //    textVisibility <= 0.001f)
            //{
            //    return;
            //}

            //ImFont* font =
            //    ImGui::GetFont();

            //if (!font)
            //    return;

            // ====================================================
            // AJUSTA O TEXTO PARA CABER NO CÍRCULO
            // ====================================================

            //const ImVec2 normalTextSize =
            //    ImGui::CalcTextSize(label);

            //const float availableWidth =
            //    radius * 1.72f;

            //const float textScale =
            //    normalTextSize.x > availableWidth
            //        ? availableWidth /
            //            normalTextSize.x
            //        : 1.0f;

            //const float fontSize =
            //    ImGui::GetFontSize() * textScale;

            //const ImVec2 textSize =
            //    font->CalcTextSizeA(
            //        fontSize,
            //        FLT_MAX,
            //        0.0f,
            //        label
            //    );

            //const ImVec2 textPos(
            //    position.x - textSize.x * 0.5f,
            //    position.y - textSize.y * 0.5f
            //);

            // ====================================================
            // TEXTO PRETO COM ALTA OPACIDADE
            // ====================================================

            //draw->AddText(
            //    font,
            //    fontSize,
            //    textPos,
            //    FadeColor(
            //        IM_COL32(0, 0, 0, 255),
            //        alpha * textVisibility
            //    ),
            //    label
            //);
        };

        // ========================================================
        // BOTÃO CENTRAL - SETTINGS
        // ========================================================

        DrawSectionButton(
            layout.center,
            layout.centerRadius,
            g_settingsCenterHoverT,
            settingsMorphVisibility,
            settingsMorphVisibility,
            Language::Get("settings").c_str(),
            true
        );

        // ========================================================
        // BOTÃO SUPERIOR - GAMEPLAY
        //
        // O botão só aparece depois que a linha cresce.
        // ========================================================

        if (circleT > 0.001f)
        {
            DrawSectionButton(
                layout.gameplay,
                layout.secondaryRadius,
                g_settingsGameplayHoverT,
                fillT * gameplayMorphVisibility,
                circleT * gameplayMorphVisibility,
                Language::Get("gameplay").c_str(),
                false
            );
        }

        // ========================================================
        // BOTÃO INFERIOR - LAYOUT
        // ========================================================

        if (circleT > 0.001f)
        {
            DrawSectionButton(
                layout.layout,
                layout.secondaryRadius,
                g_settingsLayoutHoverT,
                fillT * layoutMorphVisibility,
                circleT * layoutMorphVisibility,
                Language::Get("layout").c_str(),
                false
            );
        }

                
        // ============================================================
        // NOME DOURADO DA ÚLTIMA SEÇÃO SELECIONADA
        // ============================================================

        const char* activeLabel = nullptr;

        ImVec2 activePosition = layout.center;

        float activeRadius = layout.expandedRadius;

        float labelVisibility = 0.0f;

        switch (g_settingsSection)
        {
        case SettingsSection::Settings:

            activeLabel = Language::Get("settings").c_str();

            activePosition = layout.center;

            activeRadius =
                layout.centerRadius +
                (layout.expandedRadius - layout.centerRadius) *
                g_settingsCenterHoverT;

            labelVisibility = g_settingsSectionOpenT * settingsMorphVisibility;

            break;

        case SettingsSection::Gameplay:

            activeLabel = Language::Get("gameplay").c_str();

            activePosition = layout.gameplay;

            activeRadius =
                layout.secondaryRadius +
                (layout.expandedRadius - layout.secondaryRadius) *
                g_settingsGameplayHoverT;

            labelVisibility = g_settingsSectionOpenT * gameplayMorphVisibility;

            break;

        case SettingsSection::Layout:

            activeLabel = Language::Get("layout").c_str();

            activePosition = layout.layout;

            activeRadius =
                layout.secondaryRadius +
                (layout.expandedRadius - layout.secondaryRadius) *
                g_settingsLayoutHoverT;

            labelVisibility = g_settingsSectionOpenT * layoutMorphVisibility;

            break;

        default:
            break;
        }

        // ============================================================
        // DESENHA SOMENTE O NOME DA SEÇÃO ATIVA
        // ============================================================

        if (activeLabel && labelVisibility > 0.001f)
        {
            // Text glyphs are CPU-clipped by ImGui before the final resolution
            // transform. Use the complete viewport here so labels beside the
            // right-side section buttons remain visible on ultrawide layouts.
            draw->PushClipRect(
                Resolution::ToVirtual(ImVec2(0.0f, 0.0f)),
                Resolution::ToVirtual(Resolution::GetRealSize()),
                false);
            const ImVec2 textSize =
                ImGui::CalcTextSize(activeLabel);

            const bool horizontal = g_settingsSectionMorphT >= 0.55f;
            const ImVec2 textPos = horizontal
                ? ImVec2(
                    layout.center.x - textSize.x * 0.5f,
                    layout.center.y + layout.expandedRadius + 12.0f)
                : ImVec2(
                    activePosition.x + activeRadius + 14.0f,
                    activePosition.y - textSize.y * 0.5f);

            // Sombra.
            draw->AddText(
                ImVec2(
                    textPos.x + 1.0f,
                    textPos.y + 1.0f
                ),
                FadeColor(
                    IM_COL32(0, 0, 0, 190),
                    alpha * labelVisibility
                ),
                activeLabel
            );

            // Nome dourado.
            draw->AddText(
                textPos,
                FadeColor(
                    IM_COL32(215, 195, 150, 255),
                    alpha * labelVisibility
                ),
                activeLabel
            );
            draw->PopClipRect();
        }

    }

    enum class WeaponActionStage
    {
        None,
        WaitingForSheathe,
        WaitingForEquip
    };

    struct SettingsDeleteParticle
    {
        ImVec2 position;
        ImVec2 velocity;

        float radius = 1.0f;
        float life = 1.0f;
        float maxLife = 1.0f;

        ImU32 color = IM_COL32(255, 255, 255, 255);
    };

    static std::vector<SettingsDeleteParticle>
        g_settingsDeleteParticles;

    struct PendingWeaponAction
    {
        WeaponActionStage stage = WeaponActionStage::None;

        RE::ActorHandle actor;

        RE::FormID weaponID = 0;

        bool leftSide = false;
        bool wasDrawn = false;

        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point stageTime;
    };

    static PendingWeaponAction g_pendingWeaponAction;

    enum class EquippedVisual
    {
        None,
        Left,
        Right,
        Both
    };

    // ============================================================
    // SETTINGS - SCROLL CHARGE
    // ============================================================

    static float g_settingsCharge = 0.0f;

    static constexpr float SETTINGS_SCROLL_STEP = 0.40f;
    static constexpr float SETTINGS_DECAY_SPEED = 1.9f;
    static constexpr float SETTINGS_GROW_SPEED = 9.0f;

    // Raio visual da bolinha central
    static constexpr float SETTINGS_DOT_MIN_RADIUS = 3.0f;

    // Controla a animação visual
    static float g_settingsVisualCharge = 0.0f;

    static bool g_settingsOpening = false;

    

    // ============================================================
    // SCROLL DO PAINEL DE INFORMAÇÕES
    // ============================================================

    static float g_settingsInfoScroll = 0.0f;

    static float g_settingsInfoContentHeight = 0.0f;

    static constexpr float SETTINGS_INFO_SCROLL_STEP = 35.0f;

    //reset das informaçoes do painel
    static RE::TESForm* g_settingsInfoLastForm = nullptr;

    static std::uint16_t g_settingsInfoLastUniqueID = 0;

    static bool g_settingsInfoLastHasUniqueID = false;

    // ============================================================
    // OVERFLOW HOVER
    // ============================================================

    static int g_hoveredOverflowIndex = -1;

    static ImVec2 g_hoveredOverflowPosition{
        0.0f,
        0.0f
    };

    static float g_overflowRepulsionStrength = 0.0f;

    // ============================================================
    // SETTINGS - SELEÇÃO TEMPORÁRIA DO PREVIEW
    // ============================================================

    static RadialItem g_settingsPreviewSelection{};

    static bool g_settingsPreviewSelectionActive = false;

    static double g_settingsPreviewLastHoverTime = 0.0;

    static constexpr double SETTINGS_PREVIEW_TIMEOUT = 369.0;

    // ============================================================
    // RESET COMPLETO DO PAINEL DE INFORMAÇÕES
    // ============================================================

    void ResetSettingsItemInfo()
    {
        // Reinicia a posição do scroll.
        g_settingsInfoScroll = 0.0f;

        // Reinicia a altura calculada do conteúdo.
        g_settingsInfoContentHeight = 0.0f;

        // Limpa a identificação do último item.
        g_settingsInfoLastForm = nullptr;

        g_settingsInfoLastUniqueID = 0;

        g_settingsInfoLastHasUniqueID = false;

        // ========================================================
        // RESET DA SELEÇÃO TEMPORÁRIA
        // ========================================================

        g_settingsPreviewSelection = {};

        g_settingsPreviewSelectionActive = false;

        g_settingsPreviewLastHoverTime = 0.0;

        // ========================================================
        // RESET DO SELETOR
        // ========================================================

        CloseSettingsSectionMenu();

        g_sectionPreviewForm = nullptr;
        g_sectionPreviewUniqueID = 0;
        g_sectionPreviewHasUniqueID = false;

        g_settingsSectionOpenT = 0.0f;

        g_settingsCenterHoverT = 0.0f;
        g_settingsGameplayHoverT = 0.0f;
        g_settingsLayoutHoverT = 0.0f;

        //RESETSETTINGS
        g_waitingWheelKey = false;
        g_waitingWheelKeySlot = 0;

        g_wheelKeyButtonMin = ImVec2(0.0f, 0.0f);
        g_wheelKeyButtonMax = ImVec2(0.0f, 0.0f);
        g_secondaryKeyButtonMin = ImVec2(0.0f, 0.0f);
        g_secondaryKeyButtonMax = ImVec2(0.0f, 0.0f);
        g_altConfigKeyButtonMin = ImVec2(0.0f, 0.0f);
        g_altConfigKeyButtonMax = ImVec2(0.0f, 0.0f);

    }

    // ============================================================
    // Estado de animação por item (interpolação suave de hover)
    // ============================================================
    struct RadialItemAnimation
    {
        float hoverT = 0.0f;      // 0 = normal, 1 = hover total
        float currentRadius = 0.0f;
        float currentAlpha = 1.0f;
        bool  initialized = false;

        float radialSizeT = 1.0f;

        ImVec2 previousPos = ImVec2(0.0f, 0.0f);
        ImVec2 velocity = ImVec2(0.0f, 0.0f);

        // NOVO: posição interpolada. Sem isso, sempre que a lista muda
        // de tamanho (ex.: vai de 1 para 2 ou 3 itens no menu de cima/
        // baixo) os itens pulam instantaneamente para a nova posição
        // em vez de "andarem" até ela.
        ImVec2 currentPos{ 0.0f, 0.0f };
        bool   posInitialized = false;

        // ============================================================
        // ANIMAÇÃO POLAR DO RADIAL LATERAL
        // ============================================================

        float sideAngle = 0.0f;
        float sideRadius = 0.0f;
        bool  sidePolarInitialized = false;

        bool sideWrapTargetInitialized = false;

        bool sideWrapActive = false;

        float sideWrapStartAngle = 0.0f;
        float sideWrapTargetAngle = 0.0f;
        float sideWrapCurrentAngle = 0.0f;

        float sideWrapStartRadius = 0.0f;
        float sideWrapTargetRadius = 0.0f;

        float sideWrapT = 0.0f;

        int sideWrapDirection = 0; // -1 ou +1

        float settingsHoverT = 0.0f;

        ImVec2 settingsRepulsion{ 0.0f, 0.0f };

        // Hover individual dos itens excedentes.
        float overflowHoverT = 0.0f;
        // Transformação puramente visual usada durante o drag do Inventory.
        float inventoryOverflowMorph = 0.0f;

        // Deslocamento visual causado pela repulsão.
        ImVec2 overflowRepulsion{ 0.0f, 0.0f };
        RadialAnimation::State gameplayRadialAnimation{};
        OverflowMechanism::State overflowMechanismAnimation{};
        TrackMovement::State customTrackAnimation{};

        // Acomodação mais lenta usada apenas quando o editor altera a lista
        // em tempo real durante um drag.
        bool settingsDropSettling = false;

        // Entrada visual exclusiva do circuito custom no WheelSettings. O
        // TrackMovement mantém distância própria no trilho; sem este estado,
        // um item solto fora dele reaparecia no slot antigo antes de seguir
        // até o novo. Esta transição começa exatamente no cursor e termina
        // no slot recém-calculado.
        bool settingsCustomDropEntering = false;
        float settingsCustomDropProgress = 0.0f;
        ImVec2 settingsCustomDropStart{ 0.0f, 0.0f };

        // Reorganização visual exclusiva do menu de inventário. Este estado
        // não pode compartilhar progresso com o circuito/Gameplay: ao fechar
        // o drag, aquele controlador interpreta a posição como uma rota e
        // pode reiniciar indefinidamente um movimento a partir do centro.
        bool inventoryDropSettling = false;
        float inventoryDropProgress = 0.0f;
        ImVec2 inventoryDropStart{ 0.0f, 0.0f };

        // Os radiais laterais compartilham g_sideItems, mas cada lado precisa
        // de posição própria durante o Inventory. Compartilhar currentPos
        // fazia o desenho esquerdo atravessar a tela até o radial direito.
        std::array<bool, 2> inventorySideSettling{};
        std::array<float, 2> inventorySideProgress{};
        std::array<ImVec2, 2> inventorySideStart{};
        std::array<ImVec2, 2> inventorySidePosition{};
        std::array<bool, 2> inventorySideInitialized{};

        // O Inventory desenha a mesma lista nos dois lados. Quando a lista
        // muda e o usuário já começa a girar, cada apresentação precisa
        // continuar caminhando pelo circuito (e não interpolar em linha reta
        // até um alvo que acabou de mudar).
        std::array<TrackMovement::State, 2> inventoryCircuitAnimation{};

        // Rastro estelar usa um histórico por apresentação para que os dois
        // radiais laterais do Inventory não confundam suas posições.
        std::array<ImVec2, 4> stardustLastPosition{};
        std::array<float, 4> stardustEmission{};
        std::array<bool, 4> stardustInitialized{};


    };

    

    // Radial que permanece aberto.
    RadialSide g_lockedRadialSide = RadialSide::None;

    std::vector<RadialItem> g_topItems;
    std::vector<RadialItem> g_bottomItems;
    std::vector<RadialItem> g_sideItems;

    

    struct RadialAnimKey
    {
        RE::TESForm* form = nullptr;
        std::uint16_t uniqueID = 0;
        bool hasUniqueID = false;

        bool operator==(const RadialAnimKey& other) const
        {
            return form == other.form &&
                uniqueID == other.uniqueID &&
                hasUniqueID == other.hasUniqueID;
        }
    };

    struct RadialAnimKeyHash
    {
        std::size_t operator()(const RadialAnimKey& key) const
        {
            std::size_t hash =
                std::hash<RE::TESForm*>{}(key.form);

            hash ^= std::hash<std::uint16_t>{}(key.uniqueID)
                + 0x9e3779b9 + (hash << 6) + (hash >> 2);

            hash ^= std::hash<bool>{}(key.hasUniqueID)
                + 0x9e3779b9 + (hash << 6) + (hash >> 2);

            return hash;
        }
    };

    // Cache de animação por form. Usar o form como chave garante que
    // a transição continue suave mesmo quando a lista de itens muda
    // de ordem, é filtrada ou tem itens adicionados/removidos.
    static std::unordered_map<
        RadialAnimKey,
        RadialItemAnimation,
        RadialAnimKeyHash
    > g_itemAnimCache;

    struct PendingNormalWeaponEquip
    {
        bool active = false;

        RE::ActorHandle actor;

        RE::FormID weaponID = 0;

        std::uint16_t uniqueID = 0;
        bool hasUniqueID = false;

        RE::FormID previousWeaponID = 0;

        std::uint16_t previousUniqueID = 0;
        bool previousHasUniqueID = false;

        bool targetLeft = false;

        // Um arco/besta/arma de duas mãos bloqueia as duas mãos; um escudo
        // bloqueia apenas a mão solicitada. Guardamos esse bloqueador antes
        // do fluxo normal para liberar o slot sem perder a identificação da
        // arma que o jogador realmente escolheu.
        RE::FormID blockerFormID = 0;
        bool blockerIsTwoHanded = false;
        bool blockerLeft = false;
        int resumePhaseAfterBlocker = 2;

        int phase = 0;
        int attempts = 0;

        bool unequipOnly = false;

        bool previousWeaponWasEquipped = false;

        

        // Aguarda a arma ser guardada antes
        // de tentar uma operação problemática.
        bool waitingForSheathe = false;

        // Estado das armas antes de iniciar a operação.
        bool wasWeaponDrawn = false;

        // Controla se a animação de sacar já foi solicitada.
        bool redrawRequested = false;

        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point phaseTime;
    };

    static PendingNormalWeaponEquip g_pendingNormalWeaponEquip;

    // Fluxo separado para arco/besta. O Skyrim costuma lembrar a última
    // arma de uma mão ao equipar uma arma de duas mãos e restaurá-la quando
    // o arco sai. Limpamos as mãos antes de enviar o EquipObject do arco.
    enum class TwoHandedEquipStage
    {
        None,
        WaitingForSheathe,
        WaitingForHandsClear,
        WaitingForEquip,
        WaitingForBowUnequip,
        WaitingForFinalState
    };

    struct PendingTwoHandedWeaponEquip
    {
        TwoHandedEquipStage stage = TwoHandedEquipStage::None;
        RE::ActorHandle actor;
        RE::FormID weaponID = 0;
        bool wasWeaponDrawn = false;
        bool unequipOnly = false;
        bool sheatheRequested = false;
        bool clearRequested = false;
        bool equipRequested = false;
        bool finalStateRequested = false;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point stageTime;
    };

    static PendingTwoHandedWeaponEquip g_pendingTwoHandedWeaponEquip;

    enum class SpellHandRefreshStage
    {
        None,
        WaitingForSheathe,
        WaitingForRedraw
    };

    struct PendingSpellHandRefresh
    {
        SpellHandRefreshStage stage = SpellHandRefreshStage::None;
        RE::ActorHandle actor;
        RE::FormID spellID = 0;
        bool leftSide = false;
        bool wasWeaponDrawn = false;
        bool spellChangeApplied = false;
        bool redrawRequested = false;
        std::uint8_t stableEmptyHandFrames = 0;
        std::chrono::steady_clock::time_point stageTime;
    };

    static PendingSpellHandRefresh g_pendingSpellHandRefresh;

    struct SpellUnequipAnimationCapture
    {
        RE::ActorHandle actor;
        std::chrono::steady_clock::time_point expiresAt;
        bool active = false;
    };

    static SpellUnequipAnimationCapture g_spellUnequipAnimationCapture;

    class SpellUnequipAnimationListener final :
        public RE::BSTEventSink<RE::BSAnimationGraphEvent>
    {
    public:
        static SpellUnequipAnimationListener* GetSingleton()
        {
            static SpellUnequipAnimationListener instance;
            return &instance;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::BSAnimationGraphEvent* event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
        {
            auto& capture = g_spellUnequipAnimationCapture;
            if (!capture.active || !event ||
                std::chrono::steady_clock::now() > capture.expiresAt)
            {
                capture.active = false;
                return RE::BSEventNotifyControl::kContinue;
            }

            auto actor = capture.actor.get();
            if (!actor || event->holder != actor.get())
                return RE::BSEventNotifyControl::kContinue;

            //spdlog::info(
            //    "SPELL UNEQUIP ANIM EVENT | tag='{}' | payload='{}'",
            //    event->tag.c_str(), event->payload.c_str());
            
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    static void BeginSpellUnequipAnimationCapture(RE::Actor* actor)
    {
        if (!actor)
            return;

        auto& capture = g_spellUnequipAnimationCapture;
        capture.actor = actor->GetHandle();
        capture.expiresAt = std::chrono::steady_clock::now() +
            std::chrono::seconds(3);
        const bool registeredNow = actor->AddAnimationGraphEventSink(
            SpellUnequipAnimationListener::GetSingleton());
        // AddAnimationGraphEventSink retorna false também quando o listener
        // já está conectado ao graph; nesse caso a captura continua válida.
        capture.active = true;

        //spdlog::info("SPELL UNEQUIP ANIM CAPTURE | active=true | registeredNow={}",
        //    registeredNow);
    }

        
    class RadialUniqueIDListener :
        public RE::BSTEventSink<RE::TESUniqueIDChangeEvent>
    {
    public:

        static RadialUniqueIDListener* GetSingleton()
        {
            static RadialUniqueIDListener instance;
            return &instance;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESUniqueIDChangeEvent* event,
            RE::BSTEventSource<RE::TESUniqueIDChangeEvent>*)
            override
        {
            if (!event)
                return RE::BSEventNotifyControl::kContinue;

            auto* player =
                RE::PlayerCharacter::GetSingleton();

            if (!player)
                return RE::BSEventNotifyControl::kContinue;

            const RE::FormID playerID =
                player->GetFormID();

            // ========================================================
            // VERIFICA SE O EVENTO ENVOLVE O JOGADOR
            // ========================================================

            if (event->oldBaseID != playerID &&
                event->newBaseID != playerID)
            {
                return RE::BSEventNotifyControl::kContinue;
            }

            if (event->oldUniqueID == event->newUniqueID)
                return RE::BSEventNotifyControl::kContinue;

            // ========================================================
            // ATUALIZA AS LISTAS DO RADIAL
            // ========================================================

            bool radialItemUpdated = false;

            auto updateRadial =
                [&](std::vector<RadialItem>& items)
            {
                for (auto& item : items)
                {
                    if (!item.form || !item.hasUniqueID)
                        continue;

                    if (item.form->GetFormID() !=
                        event->objectID)
                    {
                        continue;
                    }

                    if (item.uniqueID !=
                        event->oldUniqueID)
                    {
                        continue;
                    }

                    spdlog::info(
                        "RADIAL UNIQUE ID CHANGED | form={:08X} | oldID={} | newID={}",
                        event->objectID,
                        item.uniqueID,
                        event->newUniqueID
                    );

                    item.uniqueID = event->newUniqueID;
                    radialItemUpdated = true;
                }
            };

            updateRadial(g_sideItems);
            updateRadial(g_topItems);
            updateRadial(g_bottomItems);

            // ============================================================
            // INVALIDA A ANIMAÇÃO DA INSTÂNCIA ANTIGA
            // ============================================================

            if (radialItemUpdated)
            {
                g_itemAnimCache.erase(
                    RadialAnimKey{
                        RE::TESForm::LookupByID(event->objectID),
                        event->oldUniqueID,
                        true
                    }
                );
            }

            // ============================================================
            // ATUALIZA OPERAÇÃO PENDENTE
            // ============================================================

            auto& pending = g_pendingNormalWeaponEquip;

            if (pending.active &&
                pending.weaponID == event->objectID &&
                pending.hasUniqueID &&
                pending.uniqueID == event->oldUniqueID)
            {
                pending.uniqueID =
                    event->newUniqueID;

                spdlog::info(
                    "PENDING WEAPON UNIQUE ID UPDATED | form={:08X} | uniqueID={}",
                    pending.weaponID,
                    pending.uniqueID
                );
            }

            // ============================================================
            // ATUALIZA A IDENTIDADE DA ARMA ANTERIOR
            // ============================================================

            if (pending.active &&
                pending.previousWeaponID == event->objectID &&
                pending.previousHasUniqueID &&
                pending.previousUniqueID == event->oldUniqueID)
            {
                pending.previousUniqueID =
                    event->newUniqueID;
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    // ============================================================
    // VERIFICA SE O RADIAL POSSUI TRAVA ATIVADA
    // ============================================================

    static bool IsRadialLockEnabled(RadialSide side)
    {
        switch (side)
        {
        case RadialSide::Top:
            return g_lockTopRadial;

        case RadialSide::Bottom:
            return g_lockBottomRadial;

        case RadialSide::Left:
        case RadialSide::Right:
            return g_lockSideRadial;

        default:
            return false;
        }
    }

    bool HandleMouseUnlockClick()
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return false;

        // This handler runs from the input sink between frames, when ImGui has
        // restored its physical viewport. Button geometry remains virtual.
        const ImVec2 screen = Resolution::GetVirtualSize();

        const ImVec2 mouse =
            g_settingsMousePos;

        // ========================================================
        // POSIÇÕES DOS BOTÕES PRINCIPAIS
        //
        // Substitua pelas posições reais das suas travas.
        // ========================================================

        const WheelLayout wheelLayout = GetWheelLayout();
        const float visibleWidth = wheelLayout.max.x - wheelLayout.min.x;
        const float visibleHeight = wheelLayout.max.y - wheelLayout.min.y;
        const ImVec2 topLockCenter(
            wheelLayout.min.x + visibleWidth * 0.11f,
            wheelLayout.min.y + visibleHeight * 0.19f
        );

        const ImVec2 bottomLockCenter(
            wheelLayout.min.x + visibleWidth * 0.11f,
            wheelLayout.min.y + visibleHeight * 0.81f
        );

        constexpr float lockRadius = 15.0f;

        const float radius = lockRadius * 0.5f;
        const float spacing = 12.0f;

        auto checkButton = [&](
            const ImVec2& lockCenter,
            bool& unlocked,
            bool isTop)
        {
            const ImVec2 center(
                lockCenter.x,
                lockCenter.y +
                    (isTop ? 1.0f : -1.0f) *
                    (lockRadius + radius + spacing)
            );

            const float dx =
                mouse.x - center.x;

            const float dy =
                mouse.y - center.y;

            if (dx * dx + dy * dy > radius * radius)
                return false;

            unlocked = !unlocked;

            Logger::GetSingleton().Print(
                "RADIAL MOUSE UNLOCK | {} | {}",
                isTop ? "TOP" : "BOTTOM",
                unlocked ? "ENABLED" : "DISABLED"
            );

            return true;
        };

        // ========================================================
        // TOP
        // ========================================================

        if (checkButton(
            topLockCenter,
            g_unlockTopMouse,
            true))
        {
            return true;
        }

        // ========================================================
        // BOTTOM
        // ========================================================

        if (checkButton(
            bottomLockCenter,
            g_unlockBottomMouse,
            false))
        {
            return true;
        }

        return false;
    }

    static bool HandleSettingsLockClick()
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return false;

        // Clicks are handled by the SKSE input sink between render frames.
        // Do not read ImGui's physical post-frame size or cursor there.
        const ImVec2 screen = Resolution::GetVirtualSize();
        const ImVec2 mouse = g_settingsMousePos;

        constexpr float lockRadius = 15.0f;

        // ========================================================
        // POSIÇÕES DOS BOTÕES
        // Devem ser iguais às utilizadas no DrawSettingsMenu.
        // ========================================================

        struct LockButton
        {
            ImVec2 position;
            bool* locked;
            const char* name;
        };

        const WheelLayout wheelLayout = GetWheelLayout();
        const float visibleWidth = wheelLayout.max.x - wheelLayout.min.x;
        const float visibleHeight = wheelLayout.max.y - wheelLayout.min.y;
        const ImVec2 leftCenter = GetSettingsSideLockCenter();

        const float sideMouseRadius = lockRadius * 0.5f;
        const ImVec2 sideMouseCenter = GetSettingsSideMouseCenter(lockRadius);
        const float mouseDx = mouse.x - sideMouseCenter.x;
        const float mouseDy = mouse.y - sideMouseCenter.y;
        if (mouseDx * mouseDx + mouseDy * mouseDy <= sideMouseRadius * sideMouseRadius)
        {
            g_lockSideMouse = !g_lockSideMouse;
            return true;
        }

        const float sideScrollRadius = lockRadius * 0.5f;
        const ImVec2 sideScrollCenter = GetSettingsSideScrollCenter(lockRadius);
        const float scrollDx = mouse.x - sideScrollCenter.x;
        const float scrollDy = mouse.y - sideScrollCenter.y;
        if (scrollDx * scrollDx + scrollDy * scrollDy <= sideScrollRadius * sideScrollRadius)
        {
            Config::g_lockSideScroll = !Config::g_lockSideScroll;
            Config::SaveConfig();
            return true;
        }

        LockButton buttons[] =
        {
            {
                ImVec2(wheelLayout.min.x + visibleWidth * 0.11f,
                    wheelLayout.min.y + visibleHeight * 0.19f),
                &g_lockTopRadial,
                "TOP"
            },
            {
                //ImVec2(screen.x * 0.11f, screen.y * 0.50f),
                leftCenter,
                &g_lockSideRadial,
                "SIDE"
            },
            {
                ImVec2(wheelLayout.min.x + visibleWidth * 0.11f,
                    wheelLayout.min.y + visibleHeight * 0.81f),
                &g_lockBottomRadial,
                "BOTTOM"
            }
        };

        // ========================================================
        // DETECTA QUAL BOTÃO FOI CLICADO
        // ========================================================

        for (auto& button : buttons)
        {
            const float dx =
                mouse.x - button.position.x;

            const float dy =
                mouse.y - button.position.y;

            if (dx * dx + dy * dy <= lockRadius * lockRadius)
            {
                *button.locked = !*button.locked;

                Logger::GetSingleton().Print(
                    "RADIAL LOCK | {} | {}",
                    button.name,
                    *button.locked ? "ENABLED" : "DISABLED"
                );

                return true;
            }
        }

        return false;
    }

    bool Menu::SettingsLockClick()
    {
        return HandleSettingsLockClick();
    }

    bool IsCurrentRadialLocked()
    {
        switch (g_radialSide)
        {
        case RadialSide::Top:
            return g_lockTopRadial;

        case RadialSide::Bottom:
            return g_lockBottomRadial;

        case RadialSide::Left:
        case RadialSide::Right:
            return g_lockSideRadial;

        default:
            return false;
        }
    }

    bool IsCurrentRadialMouseUnlocked()
    {
        if (!g_showWindow)
            return false;

        if (g_radialMode != RadialMode::Gameplay)
            return false;

        switch (g_radialSide)
        {
        case RadialSide::Top:
            return g_unlockTopMouse;

        case RadialSide::Bottom:
            return g_unlockBottomMouse;

        case RadialSide::Left:
        case RadialSide::Right:
            return !g_lockSideMouse;

        default:
            return false;
        }
    }

    bool IsRadialLockedOpen()
    {
        return g_radialLockedOpen;
    }
    
    void ResetRadialLockedOpen()
    {
        g_radialLockedOpen = false;
        g_lockedRadialSide = RadialSide::None;
    }

    // ============================================================
    // BOTÕES DE TRAVA DOS RADIAIS
    // ============================================================

    static void DrawRadialLockButton(
        const ImVec2& position,
        float radius,
        bool& locked,
        const char* lockLabel,
        const char* unlockLabel,
        bool forceLabelAbove = false)
    {
        ImDrawList* draw =
            ImGui::GetForegroundDrawList();

        ImGuiIO& io =
            ImGui::GetIO();

        const ImVec2 mouse =
            io.MousePos;

        const float dx =
            mouse.x - position.x;

        const float dy =
            mouse.y - position.y;

        const bool hovered =
            dx * dx + dy * dy <= radius * radius;

        // ========================================================
        // CORES
        // ========================================================

        const ImU32 backgroundColor =
            locked
            ? IM_COL32(235, 235, 235, 110)
            : IM_COL32(20, 20, 25, 235);

        const ImU32 borderColor =
            hovered
            ? IM_COL32(255, 255, 255, 230)
            : IM_COL32(255, 255, 255, 90);

        // ========================================================
        // FUNDO
        // ========================================================

        draw->AddCircleFilled(
            position,
            radius,
            backgroundColor,
            48
        );

        // ========================================================
        // BORDA
        // ========================================================

        draw->AddCircle(
            position,
            radius,
            borderColor,
            48,
            hovered ? 2.0f : 1.2f
        );

        // ========================================================
        // HOVER E DESCRIÇÃO
        // ========================================================

        if (hovered)
        {
            const char* label = locked ? unlockLabel : lockLabel;
            draw->AddCircle(
                position,
                radius + 5.0f,
                IM_COL32(255, 255, 255, 110),
                48,
                1.5f
            );

            const ImVec2 textSize =
                ImGui::CalcTextSize(label);

            // TOP: descrição acima do botão.
            // BOTTOM e SIDE: descrição abaixo.

            const bool isTop = forceLabelAbove || (&locked == &g_lockTopRadial);

            const float textY =
                isTop
                ? position.y - radius - textSize.y - 12.0f
                : position.y + radius + 12.0f;

            draw->PushClipRect(
                Resolution::ToVirtual(ImVec2(0.0f, 0.0f)),
                Resolution::ToVirtual(Resolution::GetRealSize()), false);
            const WheelLayout layout = GetWheelLayout();
            constexpr float labelSafetyMargin = 10.0f;
            const float textX = std::clamp(
                position.x - textSize.x * 0.5f,
                layout.min.x + labelSafetyMargin,
                std::max(
                    layout.min.x + labelSafetyMargin,
                    layout.max.x - labelSafetyMargin - textSize.x));
            draw->AddText(
                ImVec2(textX, textY),
                IM_COL32(235, 230, 215, 255),
                label
            );
            draw->PopClipRect();
        }
    }

    static void DrawMouseUnlockButton(
        ImDrawList* draw,
        const ImVec2& lockCenter,
        float lockRadius,
        bool& unlocked,
        float alpha,
        bool isTop)
    {
        if (!draw)
            return;

        // ========================================================
        // TAMANHO E POSIÇÃO
        // ========================================================

        const float radius = lockRadius * 0.5f;
        const float spacing = 12.0f;

        const ImVec2 center(
            lockCenter.x,
            lockCenter.y +
                (isTop ? 1.0f : -1.0f) *
                (lockRadius + radius + spacing)
        );

        // ========================================================
        // HOVER
        // ========================================================

        const ImVec2 mouse = g_settingsMousePos;

        const float dx = mouse.x - center.x;
        const float dy = mouse.y - center.y;

        const bool hovered =
            dx * dx + dy * dy <= radius * radius;

        // ========================================================
        // ANIMAÇÃO DE HOVER
        // ========================================================

        static float topHover = 0.0f;
        static float bottomHover = 0.0f;

        float& hoverT =
            isTop ? topHover : bottomHover;

        const float dt = std::clamp(
            ImGui::GetIO().DeltaTime,
            0.0f,
            0.05f
        );

        const float factor =
            1.0f - std::exp(-12.0f * dt);

        hoverT +=
            ((hovered ? 1.0f : 0.0f) - hoverT)
            * factor;

        // ========================================================
        // COR
        // ========================================================

        const bool movementLocked = !unlocked;
        const int bgAlpha = movementLocked ? 105 : 235;

        const int borderAlpha =
            static_cast<int>(
                75.0f + hoverT * 150.0f
            );

        // ========================================================
        // FUNDO
        // ========================================================

        draw->AddCircleFilled(
            center,
            radius,
            FadeColor(
                movementLocked
                    ? IM_COL32(255, 255, 255, bgAlpha)
                    : IM_COL32(20, 20, 25, bgAlpha),
                alpha
            ),
            32
        );

        // ========================================================
        // BORDA
        // ========================================================

        draw->AddCircle(
            center,
            radius,
            FadeColor(
                IM_COL32(
                    255,
                    255,
                    255,
                    borderAlpha
                ),
                alpha
            ),
            32,
            1.5f
        );

        // ========================================================
        // DESCRIÇÃO
        // ========================================================

        if (hovered)
        {
            const char* label = movementLocked
                ? Language::Get("unlock_mouse_movement").c_str()
                : Language::Get("lock_mouse_movement").c_str();

            const ImVec2 textSize =
                ImGui::CalcTextSize(label);

            const float textY =
                isTop
                    ? center.y + radius + 12.0f
                    : center.y - radius - textSize.y - 12.0f;

            draw->PushClipRect(
                Resolution::ToVirtual(ImVec2(0.0f, 0.0f)),
                Resolution::ToVirtual(Resolution::GetRealSize()), false);
            draw->AddText(
                ImVec2(
                    center.x - textSize.x * 0.5f,
                    textY
                ),
                FadeColor(
                    IM_COL32(235, 230, 215, 255),
                    alpha
                ),
                label
            );
            draw->PopClipRect();
        }
    }

    static bool MatchesEquippedInstance(
        RE::ExtraDataList* extraList,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        if (!extraList)
            return false;

        // Item sem identificação individual:
        // não confundir com uma instância que possui ID.
        if (!hasUniqueID)
        {
            return !extraList->HasType<RE::ExtraUniqueID>();
        }

        const auto* extraID =
            extraList->GetByType<RE::ExtraUniqueID>();

        if (!extraID)
            return false;

        return extraID->uniqueID == uniqueID;
    }

        
    static EquippedVisual GetItemEquippedVisual(
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        if (!form)
            return EquippedVisual::None;

        auto* player =
            RE::PlayerCharacter::GetSingleton();

        if (!player)
            return EquippedVisual::None;

        // ========================================================
        // SLOT DE VOZ - SHOUTS E POWERS
        // ========================================================

        auto* voiceSlot =
            RE::TESForm::LookupByID<RE::BGSEquipSlot>(
                0x00025BEE
            );

        if (voiceSlot)
        {
            auto* equippedVoice =
                player->GetEquippedObjectInSlot(voiceSlot);

            if (equippedVoice == form)
            {
                return EquippedVisual::Both;
            }
        }

        if (form->GetFormType() == RE::FormType::Shout)
        {
            return player->GetCurrentShout() == form
                ? EquippedVisual::Both
                : EquippedVisual::None;
        }

        // ========================================================
        // MAGIAS / PODERES
        //
        // Não possuem instância física no inventário.
        // ========================================================

        if (form->GetFormType() == RE::FormType::Spell)
        {
            const bool left =
                player->GetEquippedObject(true) == form;

            const bool right =
                player->GetEquippedObject(false) == form;

            if (left && right)
                return EquippedVisual::Both;

            if (left)
                return EquippedVisual::Left;

            if (right)
                return EquippedVisual::Right;

            return EquippedVisual::None;
        }

        // ========================================================
        // ITENS FÍSICOS
        // ========================================================

        // GetInventory monta um mapa completo. Todos os itens desenhados no
        // mesmo frame compartilham o mesmo snapshot, mantendo a leitura exata
        // daquele quadro sem reconstruí-lo uma vez por ícone.
        using InventorySnapshot = decltype(player->GetInventory());
        static InventorySnapshot inventory;
        static int inventoryFrame = -1;
        const int currentFrame = ImGui::GetFrameCount();
        if (inventoryFrame != currentFrame)
        {
            inventory = player->GetInventory();
            inventoryFrame = currentFrame;
        }

        auto* boundObject =
            form->As<RE::TESBoundObject>();

        if (!boundObject)
            return EquippedVisual::None;

        const auto it =
            inventory.find(boundObject);

        if (it == inventory.end())
            return EquippedVisual::None;

        const auto& entry =
            it->second.second;

        if (!entry)
            return EquippedVisual::None;

        bool equippedLeft = false;
        bool equippedRight = false;
        bool equippedOther = false;

        // ========================================================
        // VERIFICA CADA INSTÂNCIA DO ITEM
        // ========================================================

        if (!entry->extraLists)
            return EquippedVisual::None;

        for (auto* extraList : *entry->extraLists)
        {
            if (!MatchesEquippedInstance(
                extraList,
                uniqueID,
                hasUniqueID))
            {
                continue;
            }

            const bool wornLeft =
                extraList->HasType<RE::ExtraWornLeft>();

            const bool worn =
                extraList->HasType<RE::ExtraWorn>();

            if (!wornLeft && !worn)
                continue;

            // ====================================================
            // EQUIPAMENTO NAS MÃOS
            // ====================================================

            const bool isLeftHandObject =
                player->GetEquippedObject(true) == form;

            const bool isRightHandObject =
                player->GetEquippedObject(false) == form;

            if (wornLeft && isLeftHandObject)
                equippedLeft = true;

            if (worn && isRightHandObject)
                equippedRight = true;

            // ====================================================
            // ARMADURAS / ROUPAS / OUTROS SLOTS
            // ====================================================

            if (worn && !isLeftHandObject &&
                !isRightHandObject)
            {
                equippedOther = true;
            }
        }

        // ========================================================
        // RESULTADO
        // ========================================================

        if (equippedOther ||
            (equippedLeft && equippedRight))
        {
            return EquippedVisual::Both;
        }

        if (equippedLeft)
            return EquippedVisual::Left;

        if (equippedRight)
            return EquippedVisual::Right;

        return EquippedVisual::None;
    }


    struct ItemVisualStyle
    {
        float quantity{};
        float generalSize{};
        float opacity{};
        float lineOpacity{};
        float slotSize{};
        std::uint32_t backgroundColor{};
        float backgroundOpacity{};
        std::uint32_t borderColor{};
        float borderOpacity{};
        float iconSize{};
        std::uint32_t iconColor{};
        float iconOpacity{};
    };

    static ItemVisualStyle GetItemVisualStyle(RadialSide side)
    {
        if (side == RadialSide::Top || side == RadialSide::Bottom)
        {
            const auto& source = side == RadialSide::Top
                ? Config::g_topItemStyle : Config::g_bottomItemStyle;
            return { source.quantity, source.generalSize, source.opacity,
                source.lineOpacity, source.slotSize, source.backgroundColor,
                source.backgroundOpacity, source.borderColor, source.borderOpacity,
                source.iconSize, source.iconColor, source.iconOpacity };
        }
        return { Config::g_radialQuantity, Config::g_generalItemSize,
            Config::g_itemOpacity, Track::RadialLineOpacity(), Config::g_slotSize,
            Config::g_itemBackgroundColor, Config::g_itemBackgroundOpacity,
            Config::g_itemBorderColor, Config::g_itemBorderOpacity,
            Config::g_iconSize, Config::g_baseIconColor, Config::g_baseIconOpacity };
    }

    static void DrawEquippedItemBackground(
        ImDrawList* draw,
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID,
        const ImVec2& position,
        float radius,
        float alpha,
        RadialSide side = RadialSide::Left)
    {
        if (!draw)
            return;

        const EquippedVisual equipped =
            GetItemEquippedVisual(
                form,
                uniqueID,
                hasUniqueID
            );

        const ItemVisualStyle style = GetItemVisualStyle(side);
        const int backgroundAlpha = static_cast<int>(255.0f *
            std::clamp(style.backgroundOpacity * 0.01f, 0.0f, 1.0f));
        const ImU32 normalColor = FadeColor(IM_COL32(
            (style.backgroundColor >> 16) & 0xFF,
            (style.backgroundColor >> 8) & 0xFF,
            style.backgroundColor & 0xFF,
            backgroundAlpha), alpha);

        // Branco suave, sem ficar excessivamente brilhante.
        const ImU32 equippedColor =
            FadeColor(
                IM_COL32(255, 255, 255, static_cast<int>(55.0f *
                    std::clamp(style.backgroundOpacity * 0.01f, 0.0f, 1.0f))),
                alpha
            );

        // ========================================================
        // FUNDO ORIGINAL
        // ========================================================

        draw->AddCircleFilled(
            position,
            radius,
            normalColor,
            48
        );

        // Não equipado: mantém o fundo original.
        if (equipped == EquippedVisual::None)
            return;

        // ========================================================
        // EQUIPADO NAS DUAS MÃOS OU EM OUTRO SLOT
        // ========================================================

        if (equipped == EquippedVisual::Both)
        {
            draw->AddCircleFilled(
                position,
                radius,
                equippedColor,
                48
            );

            return;
        }

        // ========================================================
        // EQUIPADO NA MÃO ESQUERDA
        // ========================================================

        if (equipped == EquippedVisual::Left)
        {
            draw->PathClear();

            draw->PathLineTo(position);

            // Metade esquerda do círculo.
            draw->PathArcTo(
                position,
                radius,
                PI * 0.5f,
                PI * 1.5f,
                32
            );

            draw->PathFillConvex(equippedColor);

            return;
        }

        // ========================================================
        // EQUIPADO NA MÃO DIREITA
        // ========================================================

        if (equipped == EquippedVisual::Right)
        {
            draw->PathClear();

            draw->PathLineTo(position);

            // Metade direita do círculo.
            draw->PathArcTo(
                position,
                radius,
                -PI * 0.5f,
                PI * 0.5f,
                32
            );

            draw->PathFillConvex(equippedColor);
        }
    }

    void RegisterRadialUniqueIDListener()
    {
        auto* source =
            RE::ScriptEventSourceHolder::GetSingleton();

        if (!source)
            return;

        source->AddEventSink(
            RadialUniqueIDListener::GetSingleton()
        );

        spdlog::info(
            "RADIAL UNIQUE ID LISTENER REGISTERED"
        );
    }

    bool SettingsLayoutKeyboardInput(std::uint32_t scanCode, bool pressed)
    {
        if (!pressed || g_settingsSection != SettingsSection::Layout || !g_layoutNameOpen)
            return false;
        const UINT key = MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX);
        if (key == VK_ESCAPE) { g_layoutNameOpen = false; return true; }
        if (key == VK_RETURN)
        {
            Config::SaveLayoutPreset(g_layoutPresetName);
            g_layoutNameOpen = false;
            return true;
        }
        if (key == VK_BACK)
        {
            if (!g_layoutPresetName.empty()) g_layoutPresetName.pop_back();
            return true;
        }
        if (g_layoutPresetName.size() >= 32) return true;
        if (key >= 'A' && key <= 'Z')
        {
            const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            g_layoutPresetName.push_back(static_cast<char>(shift ? key : key + ('a' - 'A')));
            return true;
        }
        if (key >= '0' && key <= '9')
        {
            g_layoutPresetName.push_back(static_cast<char>(key));
            return true;
        }
        if (key == VK_SPACE || key == VK_OEM_MINUS)
        {
            g_layoutPresetName.push_back(key == VK_SPACE ? ' ' : '-');
            return true;
        }
        return true;
    }

    bool SettingsMouseDown()
    {
        if (g_settingsSection == SettingsSection::Settings)
        {
            if (g_settingsMousePos.x >= g_settingsScrollbarHitbox.min.x &&
                g_settingsMousePos.x <= g_settingsScrollbarHitbox.max.x &&
                g_settingsMousePos.y >= g_settingsScrollbarHitbox.min.y &&
                g_settingsMousePos.y <= g_settingsScrollbarHitbox.max.y)
            {
                g_settingsScrollbarDragging = true;
                return true;
            }

            if (g_settingsMousePos.x >= g_resetAllConfigHitbox.min.x &&
                g_settingsMousePos.x <= g_resetAllConfigHitbox.max.x &&
                g_settingsMousePos.y >= g_resetAllConfigHitbox.min.y &&
                g_settingsMousePos.y <= g_resetAllConfigHitbox.max.y)
            {
                Config::ResetToDefaults();
                return true;
            }

            for (std::size_t i = 0; i < g_generalSliderHitboxes.size(); ++i)
            {
                const auto& reset = g_generalResetHitboxes[i];
                if (g_settingsMousePos.x >= reset.min.x && g_settingsMousePos.x <= reset.max.x &&
                    g_settingsMousePos.y >= reset.min.y && g_settingsMousePos.y <= reset.max.y)
                {
                    if (static_cast<GeneralSlider>(i) == GeneralSlider::MouseSensitivity)
                        Config::g_sideMouseSensitivity = Config::kDefaultSideMouseSensitivity;
                    else if (static_cast<GeneralSlider>(i) == GeneralSlider::MouseSmooth)
                        Config::g_sideMouseSmooth = Config::kDefaultSideMouseSmooth;
                    else if (static_cast<GeneralSlider>(i) == GeneralSlider::AnalogSensitivity)
                        Config::g_gamepadAnalogSensitivity = Config::kDefaultGamepadAnalogSensitivity;
                    else
                        Config::g_gamepadAnalogSmooth = Config::kDefaultGamepadAnalogSmooth;
                    Config::SaveConfig();
                    return true;
                }

                const auto& hitbox = g_generalSliderHitboxes[i];
                if (g_settingsMousePos.x >= hitbox.min.x && g_settingsMousePos.x <= hitbox.max.x &&
                    g_settingsMousePos.y >= hitbox.min.y && g_settingsMousePos.y <= hitbox.max.y)
                {
                    g_activeGeneralSlider = static_cast<GeneralSlider>(i);
                    return true;
                }
            }
        }

        if (g_settingsSection == SettingsSection::Gameplay &&
            g_settingsMousePos.x >= g_gameplayScrollbarHitbox.min.x &&
            g_settingsMousePos.x <= g_gameplayScrollbarHitbox.max.x &&
            g_settingsMousePos.y >= g_gameplayScrollbarHitbox.min.y &&
            g_settingsMousePos.y <= g_gameplayScrollbarHitbox.max.y)
        {
            g_gameplayScrollbarDragging = true;
            return true;
        }

        if (g_settingsSection == SettingsSection::Layout)
        {
            const auto inside = [&](const LayoutSliderHitbox& hitbox) {
                return g_settingsMousePos.x >= hitbox.min.x && g_settingsMousePos.x <= hitbox.max.x &&
                    g_settingsMousePos.y >= hitbox.min.y && g_settingsMousePos.y <= hitbox.max.y;
            };
            if (inside(g_layoutSaveButtonHitbox))
            {
                g_layoutNameOpen = true;
                g_layoutLoadOpen = false;
                if (g_layoutPresetName.empty()) g_layoutPresetName = Language::Get("default_layout_name");
                return true;
            }
            if (inside(g_layoutLoadButtonHitbox))
            {
                g_layoutLoadOpen = !g_layoutLoadOpen;
                g_layoutNameOpen = false;
                return true;
            }
            if (g_layoutNameOpen && inside(g_layoutNameOkHitbox))
            {
                Config::SaveLayoutPreset(g_layoutPresetName);
                g_layoutNameOpen = false;
                return true;
            }
            if (g_layoutNameOpen && inside(g_layoutNameCancelHitbox))
            {
                g_layoutNameOpen = false;
                return true;
            }
            if (g_layoutLoadOpen)
            {
                for (const auto& preset : g_layoutPresetHitboxes)
                {
                    if (inside(preset.deleteHitbox))
                    {
                        Config::DeleteLayoutPreset(preset.name);
                        return true;
                    }
                    if (!inside(preset.hitbox)) continue;
                    Config::LoadLayoutPreset(preset.name);
                    g_layoutLoadOpen = false;
                    return true;
                }
            }
            if (g_settingsMousePos.x >= g_layoutScrollbarHitbox.min.x &&
                g_settingsMousePos.x <= g_layoutScrollbarHitbox.max.x &&
                g_settingsMousePos.y >= g_layoutScrollbarHitbox.min.y &&
                g_settingsMousePos.y <= g_layoutScrollbarHitbox.max.y)
            {
                g_layoutScrollbarDragging = true;
                return true;
            }

            for (std::size_t i = 0; i < g_layoutGroupHitboxes.size(); ++i)
            {
                const auto& hitbox = g_layoutGroupHitboxes[i];
                if (g_settingsMousePos.x >= hitbox.min.x &&
                    g_settingsMousePos.x <= hitbox.max.x &&
                    g_settingsMousePos.y >= hitbox.min.y &&
                    g_settingsMousePos.y <= hitbox.max.y)
                {
                    g_layoutGroupExpanded[i] = !g_layoutGroupExpanded[i];
                    g_activeLayoutSlider = LayoutSlider::None;
                    return true;
                }
            }

            for (std::size_t i = 0; i < g_layoutColorResetHitboxes.size(); ++i)
            {
                const auto& reset = g_layoutColorResetHitboxes[i];
                if (g_settingsMousePos.x < reset.min.x || g_settingsMousePos.x > reset.max.x ||
                    g_settingsMousePos.y < reset.min.y || g_settingsMousePos.y > reset.max.y)
                    continue;
                const auto control = static_cast<LayoutColorControl>(i);
                switch (control)
                {
                case LayoutColorControl::Background: Config::g_itemBackgroundColor = Config::kDefaultItemBackgroundColor; break;
                case LayoutColorControl::Border: Config::g_itemBorderColor = Config::kDefaultItemBorderColor; break;
                case LayoutColorControl::Icon: Config::g_baseIconColor = Config::kDefaultBaseIconColor; break;
                case LayoutColorControl::OverflowBackground: Config::g_overflowBackgroundColor = Config::kDefaultOverflowBackgroundColor; break;
                case LayoutColorControl::OverflowBorder: Config::g_overflowBorderColor = Config::kDefaultOverflowBorderColor; break;
                case LayoutColorControl::TopBackground: Config::g_topItemStyle.backgroundColor = Config::kDefaultItemBackgroundColor; break;
                case LayoutColorControl::TopBorder: Config::g_topItemStyle.borderColor = Config::kDefaultItemBorderColor; break;
                case LayoutColorControl::TopIcon: Config::g_topItemStyle.iconColor = Config::kDefaultBaseIconColor; break;
                case LayoutColorControl::BottomBackground: Config::g_bottomItemStyle.backgroundColor = Config::kDefaultItemBackgroundColor; break;
                case LayoutColorControl::BottomBorder: Config::g_bottomItemStyle.borderColor = Config::kDefaultItemBorderColor; break;
                case LayoutColorControl::BottomIcon: Config::g_bottomItemStyle.iconColor = Config::kDefaultBaseIconColor; break;
                default:
                    if (control >= LayoutColorControl::PotionHealth && control <= LayoutColorControl::PotionShock)
                    {
                        const auto index = static_cast<std::size_t>(control) -
                            static_cast<std::size_t>(LayoutColorControl::PotionHealth);
                        Config::g_potionColors[index] = Config::kDefaultPotionColors[index];
                    }
                    else if (control >= LayoutColorControl::SchoolAlteration && control <= LayoutColorControl::SchoolRestoration)
                    {
                        const auto index = static_cast<std::size_t>(control) -
                            static_cast<std::size_t>(LayoutColorControl::SchoolAlteration);
                        Config::g_schoolColors[index] = Config::kDefaultSchoolColors[index];
                    }
                    else if (control >= LayoutColorControl::MagicFire && control <= LayoutColorControl::MagicShock)
                    {
                        const auto index = static_cast<std::size_t>(control) -
                            static_cast<std::size_t>(LayoutColorControl::MagicFire);
                        Config::g_magicElementColors[index] = Config::kDefaultMagicElementColors[index];
                    }
                    else if (control >= LayoutColorControl::EnchantFire && control <= LayoutColorControl::EnchantDefault)
                    {
                        const auto index = static_cast<std::size_t>(control) -
                            static_cast<std::size_t>(LayoutColorControl::EnchantFire);
                        Config::g_enchantColors[index] = Config::kDefaultEnchantColors[index];
                    }
                    break;
                }
                Config::SaveConfig();
                return true;
            }

            for (std::size_t i = 0; i < g_layoutColorHitboxes.size(); ++i)
            {
                const auto& hitbox = g_layoutColorHitboxes[i];
                if (g_settingsMousePos.x >= hitbox.min.x &&
                    g_settingsMousePos.x <= hitbox.max.x &&
                    g_settingsMousePos.y >= hitbox.min.y &&
                    g_settingsMousePos.y <= hitbox.max.y)
                {
                    // Trocar ou fechar um controle nunca pode reaproveitar um
                    // arraste iniciado no círculo anterior.
                    g_activeLayoutColor = -1;
                    g_openLayoutColor = g_openLayoutColor == static_cast<int>(i)
                        ? -1 : static_cast<int>(i);
                    return true;
                }
            }
            if (g_openLayoutColor >= 0 &&
                g_settingsMousePos.x >= g_layoutColorPickerHitbox.min.x &&
                g_settingsMousePos.x <= g_layoutColorPickerHitbox.max.x &&
                g_settingsMousePos.y >= g_layoutColorPickerHitbox.min.y &&
                g_settingsMousePos.y <= g_layoutColorPickerHitbox.max.y)
            {
                g_activeLayoutColor = g_openLayoutColor;
                return true;
            }

            if (g_settingsMousePos.x >= g_fontFamilyButtonHitbox.min.x &&
                g_settingsMousePos.x <= g_fontFamilyButtonHitbox.max.x &&
                g_settingsMousePos.y >= g_fontFamilyButtonHitbox.min.y &&
                g_settingsMousePos.y <= g_fontFamilyButtonHitbox.max.y)
            {
                Config::g_fontFamily = (Config::g_fontFamily + 1) % Font::Count();
                Config::SaveConfig();
                return true;
            }
            if (g_settingsMousePos.x >= g_fontFamilyResetHitbox.min.x &&
                g_settingsMousePos.x <= g_fontFamilyResetHitbox.max.x &&
                g_settingsMousePos.y >= g_fontFamilyResetHitbox.min.y &&
                g_settingsMousePos.y <= g_fontFamilyResetHitbox.max.y)
            {
                Config::g_fontFamily = 0;
                Config::SaveConfig();
                return true;
            }

            for (std::size_t i = 0; i < g_layoutSliderHitboxes.size(); ++i)
            {
                const auto slider = static_cast<LayoutSlider>(i);
                Config::ItemPreviewProfile previewProfile{};
                PreviewLayoutField previewField{};
                const bool previewControl = GetPreviewSliderInfo(
                    slider, previewProfile, previewField);
                if (previewControl && previewProfile != Config::ItemPreviewProfile::Menu)
                {
                    const auto& copy = g_layoutCopyHitboxes[i];
                    if (inside(copy))
                    {
                        PreviewLayoutValue(previewProfile, previewField) =
                            PreviewLayoutValue(OppositePreviewProfile(previewProfile), previewField);
                        g_previewLayoutButtonHeld = true;
                        g_previewLayoutButtonProfile = previewProfile;
                        g_previewLayoutButtonField = previewField;
                        g_settingsPreviewSelection = {};
                        g_settingsPreviewSelectionActive = false;
                        g_settingsPreviewLastHoverTime = 0.0;
                        g_layoutCopyFlash[i] = 1.0f;
                        Config::SaveConfig();
                        return true;
                    }
                    const auto& mirror = g_layoutMirrorHitboxes[i];
                    if (PreviewFieldSupportsMirror(previewField) && inside(mirror))
                    {
                        float value = PreviewLayoutValue(
                            OppositePreviewProfile(previewProfile), previewField);
                        const bool horizontalPole =
                            previewProfile == Config::ItemPreviewProfile::Left ||
                            previewProfile == Config::ItemPreviewProfile::Right;
                        const bool verticalPole =
                            previewProfile == Config::ItemPreviewProfile::Top ||
                            previewProfile == Config::ItemPreviewProfile::Bottom;
                        if ((horizontalPole &&
                                (previewField == PreviewLayoutField::ItemPositionX ||
                                 previewField == PreviewLayoutField::NamePositionX)) ||
                            (verticalPole &&
                                (previewField == PreviewLayoutField::ItemPositionY ||
                                 previewField == PreviewLayoutField::NamePositionY)))
                            value = 100.0f - value;
                        PreviewLayoutValue(previewProfile, previewField) = value;
                        g_previewLayoutButtonHeld = true;
                        g_previewLayoutButtonProfile = previewProfile;
                        g_previewLayoutButtonField = previewField;
                        g_settingsPreviewSelection = {};
                        g_settingsPreviewSelectionActive = false;
                        g_settingsPreviewLastHoverTime = 0.0;
                        g_layoutMirrorFlash[i] = 1.0f;
                        Config::SaveConfig();
                        return true;
                    }
                }
                const auto& reset = g_layoutResetHitboxes[i];
                if (g_settingsMousePos.x >= reset.min.x &&
                    g_settingsMousePos.x <= reset.max.x &&
                    g_settingsMousePos.y >= reset.min.y &&
                    g_settingsMousePos.y <= reset.max.y)
                {
                    std::size_t previewCategoryIndex = 0;
                    if (GetPreviewCategorySliderInfo(
                            slider, previewCategoryIndex))
                    {
                        Config::g_itemPreviewCategoryMultipliers[
                            previewCategoryIndex] = 1.0f;
                        ItemPreview::InvalidateSizeScale();
                    }
                    else switch (slider)
                    {
                    case LayoutSlider::ItemOpacity: Config::g_itemOpacity = Config::kDefaultItemOpacity; break;
                    case LayoutSlider::RadialLineOpacity: Track::SetRadialLineOpacity(100.0f); break;
                    case LayoutSlider::GeneralItemSize: Config::g_generalItemSize = Config::kDefaultGeneralItemSize; break;
                    case LayoutSlider::SlotSize: Config::g_slotSize = Config::kDefaultSlotSize; break;
                    case LayoutSlider::ItemBackgroundOpacity: Config::g_itemBackgroundOpacity = Config::kDefaultItemBackgroundOpacity; break;
                    case LayoutSlider::ItemBorderOpacity: Config::g_itemBorderOpacity = Config::kDefaultItemBorderOpacity; break;
                    case LayoutSlider::IconSize: Config::g_iconSize = Config::kDefaultIconSize; break;
                    case LayoutSlider::BaseIconOpacity: Config::g_baseIconOpacity = Config::kDefaultBaseIconOpacity; break;
                    case LayoutSlider::OverflowLineOpacity: Track::SetLineOpacity(100.0f); break;
                    case LayoutSlider::OverflowSize: Config::g_overflowSize = Config::kDefaultOverflowSize; break;
                    case LayoutSlider::OverflowBackgroundOpacity: Config::g_overflowBackgroundOpacity = Config::kDefaultOverflowBackgroundOpacity; break;
                    case LayoutSlider::OverflowBorderOpacity: Config::g_overflowBorderOpacity = Config::kDefaultOverflowBorderOpacity; break;
                    case LayoutSlider::OverflowOpacity: Config::g_overflowOpacity = Config::kDefaultOverflowOpacity; break;
                    case LayoutSlider::ItemNameOpacity: Config::g_itemNameOpacity = Config::kDefaultItemNameOpacity; break;
                    case LayoutSlider::ItemNamePositionX: Config::g_itemNamePositionX = Config::kDefaultItemNamePositionX; break;
                    case LayoutSlider::ItemNamePositionY: Config::g_itemNamePositionY = Config::kDefaultItemNamePositionY; break;
                    case LayoutSlider::RadialQuantity: Config::g_radialQuantity = Config::kDefaultRadialQuantity; break;
                    case LayoutSlider::StardustFade: Config::g_stardustFade = Config::kDefaultStardustFade; break;
                    case LayoutSlider::CenterOpacity: Config::g_centerOpacity = Config::kDefaultCenterOpacity; break;
                    case LayoutSlider::SideOpacity: Config::g_sideOpacity = Config::kDefaultSideOpacity; break;
                    case LayoutSlider::TopOpacity: Config::g_topOpacity = Config::kDefaultTopOpacity; break;
                    case LayoutSlider::BottomOpacity: Config::g_bottomOpacity = Config::kDefaultBottomOpacity; break;
                    case LayoutSlider::TopItemQuantity: Config::g_topItemStyle.quantity = Config::kDefaultRadialQuantity; break;
                    case LayoutSlider::TopGeneralItemSize: Config::g_topItemStyle.generalSize = Config::kDefaultGeneralItemSize; break;
                    case LayoutSlider::TopItemOpacity: Config::g_topItemStyle.opacity = Config::kDefaultItemOpacity; break;
                    case LayoutSlider::TopLineOpacity: Config::g_topItemStyle.lineOpacity = 100.0f; break;
                    case LayoutSlider::TopSlotSize: Config::g_topItemStyle.slotSize = Config::kDefaultSlotSize; break;
                    case LayoutSlider::TopBackgroundOpacity: Config::g_topItemStyle.backgroundOpacity = Config::kDefaultItemBackgroundOpacity; break;
                    case LayoutSlider::TopBorderOpacity: Config::g_topItemStyle.borderOpacity = Config::kDefaultItemBorderOpacity; break;
                    case LayoutSlider::TopIconSize: Config::g_topItemStyle.iconSize = Config::kDefaultIconSize; break;
                    case LayoutSlider::TopIconOpacity: Config::g_topItemStyle.iconOpacity = Config::kDefaultBaseIconOpacity; break;
                    case LayoutSlider::BottomItemQuantity: Config::g_bottomItemStyle.quantity = Config::kDefaultRadialQuantity; break;
                    case LayoutSlider::BottomGeneralItemSize: Config::g_bottomItemStyle.generalSize = Config::kDefaultGeneralItemSize; break;
                    case LayoutSlider::BottomItemOpacity: Config::g_bottomItemStyle.opacity = Config::kDefaultItemOpacity; break;
                    case LayoutSlider::BottomLineOpacity: Config::g_bottomItemStyle.lineOpacity = 100.0f; break;
                    case LayoutSlider::BottomSlotSize: Config::g_bottomItemStyle.slotSize = Config::kDefaultSlotSize; break;
                    case LayoutSlider::BottomBackgroundOpacity: Config::g_bottomItemStyle.backgroundOpacity = Config::kDefaultItemBackgroundOpacity; break;
                    case LayoutSlider::BottomBorderOpacity: Config::g_bottomItemStyle.borderOpacity = Config::kDefaultItemBorderOpacity; break;
                    case LayoutSlider::BottomIconSize: Config::g_bottomItemStyle.iconSize = Config::kDefaultIconSize; break;
                    case LayoutSlider::BottomIconOpacity: Config::g_bottomItemStyle.iconOpacity = Config::kDefaultBaseIconOpacity; break;
                    case LayoutSlider::FontSize: Config::g_fontSizeScale = Config::kDefaultFontSizeScale; break;
                    case LayoutSlider::DrawMarkDistance: Config::g_drawMarkDistance = Config::kDefaultDrawMarkDistance; break;
                    case LayoutSlider::SidePosition: Config::g_sideRadialPosition = Config::kDefaultSideRadialPosition; break;
                    case LayoutSlider::TopPosition: Config::g_topVerticalPosition = Config::kDefaultTopVerticalPosition; break;
                    case LayoutSlider::BottomPosition: Config::g_bottomVerticalPosition = Config::kDefaultBottomVerticalPosition; break;
                    case LayoutSlider::RadialStretch:
                        Config::g_radialStretch = Config::kDefaultRadialStretch;
                        Config::g_radialQuantity = std::min(Config::g_radialQuantity, 25.0f);
                        break;
                    case LayoutSlider::RadialRotation: Track::SetRadialRotation(0.0f); break;
                    case LayoutSlider::TopStretch: Config::g_topHorizontalStretch = Config::kDefaultTopHorizontalStretch; break;
                    case LayoutSlider::BottomStretch: Config::g_bottomHorizontalStretch = Config::kDefaultBottomHorizontalStretch; break;
                    default: break;
                    }
                    if (previewControl)
                    {
                        PreviewLayoutValue(previewProfile, previewField) =
                            DefaultPreviewLayoutValue(previewField);
                        g_previewLayoutButtonHeld = true;
                        g_previewLayoutButtonProfile = previewProfile;
                        g_previewLayoutButtonField = previewField;
                        g_settingsPreviewSelection = {};
                        g_settingsPreviewSelectionActive = false;
                        g_settingsPreviewLastHoverTime = 0.0;
                    }
                    g_layoutResetFlash[i] = 1.0f;
                    Config::SaveConfig();
                    return true;
                }

                const auto& hitbox = g_layoutSliderHitboxes[i];
                if (g_settingsMousePos.x >= hitbox.min.x &&
                    g_settingsMousePos.x <= hitbox.max.x &&
                    g_settingsMousePos.y >= hitbox.min.y &&
                    g_settingsMousePos.y <= hitbox.max.y)
                {
                    g_activeLayoutSlider = static_cast<LayoutSlider>(i);
                    if (previewControl)
                    {
                        g_settingsPreviewSelection = {};
                        g_settingsPreviewSelectionActive = false;
                        g_settingsPreviewLastHoverTime = 0.0;
                    }
                    return true;
                }
            }
        }
        if (GetSettingsHoveredHitbox(g_settingsMousePos))
        {
            g_settingsLeftPressed = true;
            return true;
        }

        return false;
    }

    void SettingsMouseUp()
    {
        g_previewLayoutButtonHeld = false;

        // O seletor de cor acompanha o cursor somente enquanto o botão está
        // realmente pressionado. Sem esta liberação, o marcador continuava
        // ativo depois do clique e prendia o cursor dentro do círculo.
        if (g_activeLayoutColor >= 0)
        {
            g_activeLayoutColor = -1;
            Config::SaveConfig();
            return;
        }

        if (g_radialShapeSliderDragging ||
            g_radialAnimationSliderDragging ||
            g_slowTimeSliderDragging)
        {
            g_radialShapeSliderDragging = false;
            g_radialAnimationSliderDragging = false;
            g_slowTimeSliderDragging = false;
            Config::SaveConfig();
            return;
        }
        if (g_activeGeneralSlider != GeneralSlider::None)
        {
            g_activeGeneralSlider = GeneralSlider::None;
            Config::SaveConfig();
            return;
        }

        if (g_layoutScrollbarDragging)
        {
            g_layoutScrollbarDragging = false;
            return;
        }

        if (g_gameplayScrollbarDragging)
        {
            g_gameplayScrollbarDragging = false;
            return;
        }

        if (g_settingsScrollbarDragging)
        {
            g_settingsScrollbarDragging = false;
            return;
        }

        if (g_activeLayoutSlider != LayoutSlider::None)
        {
            g_activeLayoutSlider = LayoutSlider::None;
            Config::SaveConfig();
            return;
        }
        g_settingsLeftReleased = true;
    }

    bool SettingsRightClick()
    {
        if (!GetSettingsHoveredHitbox(g_settingsMousePos))
            return false;

        g_settingsRightPressed = true;
        return true;
    }

    static bool IsWeaponSheathed(RE::Actor* actor)
    {
        if (!actor)
            return false;

        return !actor->IsWeaponDrawn();
    }

    void ResetSettingsCursor()
    {
        ImGuiIO& io = ImGui::GetIO();

        const ImVec2 center = Resolution::GetVirtualCenter();

        g_settingsMousePos = center;

        // Reposiciona também o cursor interno do ImGui.
        io.MousePos = Resolution::ToReal(center);
    }

    struct GamepadMagnetTarget
    {
        ImVec2 center{};
        float radius{ 0.0f };
        std::uint64_t token{ 0 };
        bool item{ false };
    };
    static void AddGamepadMagnetTarget(std::vector<GamepadMagnetTarget>& targets,
        const ImVec2& center, float radius, std::uint64_t token, bool item = false)
    {
        if (std::isfinite(center.x) && std::isfinite(center.y) && radius > 0.0f)
            targets.push_back({ center, radius, token, item });
    }

    static void AddGamepadMagnetRect(std::vector<GamepadMagnetTarget>& targets,
        const LayoutSliderHitbox& hitbox, std::uint64_t token)
    {
        if (!std::isfinite(hitbox.min.x) || !std::isfinite(hitbox.min.y) ||
            !std::isfinite(hitbox.max.x) || !std::isfinite(hitbox.max.y) ||
            hitbox.max.x < hitbox.min.x || hitbox.max.y < hitbox.min.y)
            return;
        const ImVec2 center((hitbox.min.x + hitbox.max.x) * 0.5f,
            (hitbox.min.y + hitbox.max.y) * 0.5f);
        const float radius = std::max(6.0f,
            std::min((hitbox.max.x - hitbox.min.x) * 0.5f,
                (hitbox.max.y - hitbox.min.y) * 0.5f));
        AddGamepadMagnetTarget(targets, center, radius, token);
    }

    static std::vector<GamepadMagnetTarget> GetGamepadSettingsMagnetTargets()
    {
        std::vector<GamepadMagnetTarget> targets;
        targets.reserve(64);
        std::uint64_t token = 1;
        
        auto add = [&](const ImVec2& center, float radius) {
            AddGamepadMagnetTarget(targets, center, radius, token++);
        };
        
        auto addRect = [&](const LayoutSliderHitbox& hitbox) {
            AddGamepadMagnetRect(targets, hitbox, token++);
        };

        const WheelLayout wheel = GetWheelLayout();
        
        const float width = wheel.max.x - wheel.min.x;
        
        const float height = wheel.max.y - wheel.min.y;
        
        constexpr float lockRadius = 15.0f;
        
        const ImVec2 topLock(wheel.min.x + width * 0.11f, wheel.min.y + height * 0.19f);
        
        const ImVec2 bottomLock(wheel.min.x + width * 0.11f, wheel.min.y + height * 0.81f);
        
        add(topLock, lockRadius);
        add(ImVec2(topLock.x, topLock.y + lockRadius + lockRadius * 0.5f + 12.0f), lockRadius * 0.5f);
        add(GetSettingsSideLockCenter(), lockRadius);
        add(GetSettingsSideMouseCenter(lockRadius), lockRadius * 0.5f);
        add(GetSettingsSideScrollCenter(lockRadius), lockRadius * 0.5f);
        
        add(bottomLock, lockRadius);
        add(ImVec2(bottomLock.x, bottomLock.y - lockRadius - lockRadius * 0.5f - 12.0f), lockRadius * 0.5f);
        add(ImVec2(wheel.center.x, wheel.min.y + height * 0.93f), 12.0f);

        const SettingsSectionLayout section = GetSettingsSectionLayout();
        
        add(section.gameplay, 18.0f);
        add(section.center, 18.0f);
        add(section.layout, 18.0f);

        for (const auto& item : g_settingsItemHitboxes)
        {
            if (!IsSettingsItemCoveredByControlPanel(item.position, item.radius))
                AddGamepadMagnetTarget(
                    targets, item.position, item.radius, token++, true);
        }

        if (g_settingsSection == SettingsSection::Settings)
        {
            addRect({ g_wheelKeyButtonMin, g_wheelKeyButtonMax });
            addRect({ g_secondaryKeyButtonMin, g_secondaryKeyButtonMax });
            addRect(g_secondaryKeyResetHitbox);
            addRect({ g_altConfigKeyButtonMin, g_altConfigKeyButtonMax });
            addRect(g_altConfigKeyResetHitbox);
        
            add(g_automaticArrowButtonCenter, g_automaticArrowButtonRadius);
        
            for (const auto& hitbox : g_generalSliderHitboxes) addRect(hitbox);
            for (const auto& hitbox : g_generalResetHitboxes) addRect(hitbox);
        
            addRect(g_resetAllConfigHitbox);
            addRect(g_settingsScrollbarHitbox);
        }
        else if (g_settingsSection == SettingsSection::Gameplay)
        {
        
            addRect({ g_languageButtonMin, g_languageButtonMax });
        
            for (const auto& center : g_slowTimeScopeCenters)
                add(center, std::max(6.0f, g_slowTimeScopeRadius));
        
            for (const auto& center : g_blurScopeCenters)
                add(center, std::max(6.0f, g_blurScopeRadius));
        
            addRect({ g_slowTimeSliderMin, g_slowTimeSliderMax });
            addRect(g_slowTimeResetHitbox);
            addRect({ g_radialShapeButtonMin, g_radialShapeButtonMax });
            addRect({ g_trackModeButtonMin, g_trackModeButtonMax });
            addRect({ g_trackEditorButtonMin, g_trackEditorButtonMax });
            addRect({ g_radialAnimationButtonMin, g_radialAnimationButtonMax });
        
            for (const auto& option : g_radialShapeOptionHitboxes) addRect(option);
            for (const auto& option : g_radialAnimationOptionHitboxes) addRect(option);
            for (const auto& center : g_gameplayIconButtonCenters)
                add(center, std::max(12.0f, g_gameplayIconButtonRadius));
        
            add(g_fastDragButtonCenter, std::max(12.0f, g_fastDragButtonRadius));
        
            if (IconCustom::HasValidConfiguration())
                add(g_customIconsReloadCenter, std::max(8.0f, g_customIconsReloadRadius));
        
            for (const auto& option : g_languageOptionHitboxes)
                addRect({ option.min, option.max });
        }
        else if (g_settingsSection == SettingsSection::Layout)
        {
            for (const auto& hitbox : g_layoutGroupHitboxes) addRect(hitbox);
        
            for (const auto& hitbox : g_layoutColorHitboxes) addRect(hitbox);
        
            for (const auto& hitbox : g_layoutColorResetHitboxes) addRect(hitbox);
        
            addRect(g_layoutColorPickerHitbox);
            addRect(g_fontFamilyButtonHitbox);
            addRect(g_fontFamilyResetHitbox);
        
            add(g_gameplayPreviewButtonCenter, std::max(12.0f, g_gameplayPreviewButtonRadius));
            add(g_gameplayDescriptionButtonCenter, std::max(12.0f, g_gameplayDescriptionButtonRadius));
        
            for (const auto& hitbox : g_layoutSliderHitboxes) addRect(hitbox);
            for (const auto& hitbox : g_layoutResetHitboxes) addRect(hitbox);
        
            addRect(g_layoutScrollbarHitbox);
        }
        return targets;
    }

    static void ApplyGamepadSettingsMagnet(ImVec2& cursor, const ImVec2& movement)
    {
        
        static std::uint64_t lastTarget = 0;
        
        const auto targets = GetGamepadSettingsMagnetTargets();
        
        const GamepadMagnetTarget* nearest = nullptr;
        
        float nearestDistance = FLT_MAX;
        
        for (const auto& target : targets)
        {
            const float dx = target.center.x - cursor.x;
            const float dy = target.center.y - cursor.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            const float reach = target.radius + 144.0f;
        
            if (distance <= reach && distance < nearestDistance)
            {
                nearest = &target;
                nearestDistance = distance;
            }
        }

        if (!nearest)
        {
            lastTarget = 0;
            return;
        }
        if (g_activeLayoutColor >= 0)
        {
            g_activeLayoutColor = -1;
            Config::SaveConfig();
            return;
        }
        if (g_activeLayoutColor >= 0)
        {
            g_activeLayoutColor = -1;
            Config::SaveConfig();
            return;
        }

        const ImVec2 towardTarget(
            nearest->center.x - cursor.x,
            nearest->center.y - cursor.y);
        if (movement.x * towardTarget.x + movement.y * towardTarget.y < 0.0f)
        {
            // Qualquer intenção de saída vence o ímã imediatamente. O alcance
            // continua grande, mas não cria uma prisão em torno do controle.
            lastTarget = 0;
            return;
        }

        const float reach = nearest->radius + 144.0f;
        const float proximity = 1.0f - std::clamp(nearestDistance / reach, 0.0f, 1.0f);
        const float pull = std::min(0.92f, 0.28f + 0.56f * proximity);
        
        cursor.x += (nearest->center.x - cursor.x) * pull;
        cursor.y += (nearest->center.y - cursor.y) * pull;

        if (lastTarget != nearest->token && nearestDistance <= nearest->radius + 8.0f)
        {
            lastTarget = nearest->token;
            if (nearest->item)
                Gamepad::PulseItemSelection();
            else
                Gamepad::PulseSelection();
        }
    }

    void GamepadMoveCursor(float x, float y, bool rightStick)
    {
        constexpr float deadzone = 0.18f;
        
        const float length = std::sqrt(x * x + y * y);

        if (!SettingsMenu::WheelSettingsMenu::IsOpen() && g_showWindow &&
            g_radialMode == RadialMode::Gameplay &&
            (g_radialSide == RadialSide::Top || g_radialSide == RadialSide::Bottom))
        {
            constexpr float selectThreshold = 0.55f;
            constexpr float releaseThreshold = 0.28f;
        
            if (std::abs(y) > selectThreshold && std::abs(y) > std::abs(x))
            {
                ClearTopBottomGamepadSelection(g_radialSide);
                g_gamepadHorizontalSelectionLatch = 0;
                return;
            }
        
            if (std::abs(x) <= releaseThreshold)
            {
                g_gamepadHorizontalSelectionLatch = 0;
            }
            else
            {
                const int direction = x > selectThreshold ? 1 : x < -selectThreshold ? -1 : 0;
                if (direction != 0 && direction != g_gamepadHorizontalSelectionLatch)
                {
                    ScrollTopBottomRadial(direction);
                    g_gamepadHorizontalSelectionLatch = direction;
                    Gamepad::PulseItemSelection();
                }
            }
        
            return;
        }

        if (length < deadzone)
        {
            g_gamepadAnalogSmoothedDelta = ImVec2(0.0f, 0.0f);
            return;
        }

        const float normalized = (length - deadzone) / (1.0f - deadzone);
        
        const float speed = 22.0f * std::clamp(normalized, 0.0f, 1.0f) *
            std::clamp(Config::g_gamepadAnalogSensitivity, 0.25f, 3.0f);
        
        const ImVec2 targetDelta(x / length * speed, -y / length * speed);
        
        const float smoothFactor = LayoutLerp(
            1.0f, 0.08f, Config::g_gamepadAnalogSmooth);
        
        g_gamepadAnalogSmoothedDelta.x +=
            (targetDelta.x - g_gamepadAnalogSmoothedDelta.x) * smoothFactor;
        
        g_gamepadAnalogSmoothedDelta.y +=
            (targetDelta.y - g_gamepadAnalogSmoothedDelta.y) * smoothFactor;
        
        const ImVec2 delta = g_gamepadAnalogSmoothedDelta;
        
        const ImVec2 screen = Resolution::GetVirtualSize();

        if (SettingsMenu::WheelSettingsMenu::IsOpen())
        {
            if (!g_gamepadSettingsCursorActive)
            {
                g_settingsMousePos = GetSkyrimMousePos();
                g_gamepadSettingsCursorActive = true;
            }
            g_settingsMousePos.x = std::clamp(g_settingsMousePos.x + delta.x, 0.0f, screen.x);
            g_settingsMousePos.y = std::clamp(g_settingsMousePos.y + delta.y, 0.0f, screen.y);
            ApplyGamepadSettingsMagnet(g_settingsMousePos, delta);
            ImGui::GetIO().MousePos = g_settingsMousePos;
            return;
        }

        if (g_radialMode == RadialMode::Inventory &&
            g_inventoryDragMode == InventoryDragMode::KeyDrag)
        {
            g_inventoryDraggedPosition.x = std::clamp(g_inventoryDraggedPosition.x + delta.x, 0.0f, screen.x);
            g_inventoryDraggedPosition.y = std::clamp(g_inventoryDraggedPosition.y + delta.y, 0.0f, screen.y);
            return;
        }

        if (g_showWindow && g_radialMode == RadialMode::Gameplay)
        {
            g_radialVector.x += delta.x;
            g_radialVector.y += delta.y;
            const float lengthSq =
                g_radialVector.x * g_radialVector.x +
                g_radialVector.y * g_radialVector.y;
            constexpr float safeMaximum = 900.0f;
            if (lengthSq > safeMaximum * safeMaximum)
            {
                const float invLength = safeMaximum / std::sqrt(lengthSq);
                g_radialVector.x *= invLength;
                g_radialVector.y *= invLength;
            }
            g_radialMouseVelocity = ImVec2(0.0f, 0.0f);
            g_lastMousePos = ImGui::GetIO().MousePos;

            if (g_radialSide == RadialSide::Left || g_radialSide == RadialSide::Right)
            {
                const int visibleCount = std::min(
                    static_cast<int>(g_sideItems.size()), GetSideVisibleLimit());
                
                const ImVec2 selector(
                    g_radialOrigin.x + g_radialVector.x,
                    g_radialOrigin.y + g_radialVector.y);
                
                const int visibleIndex = GetSideRadialItem(
                    selector, g_radialOrigin, g_radialSide == RadialSide::Left,
                    visibleCount, MENU_INNER_RADIUS);
                
                const int selectedIndex = visibleIndex >= 0 && !g_sideItems.empty()
                    ? GetSideActualIndex(visibleIndex) : -1;
                
                if (selectedIndex >= 0 && selectedIndex != g_gamepadSideSelection)
                {
                    g_gamepadSideSelection = selectedIndex;
                    Gamepad::PulseItemSelection();
                }
                else if (selectedIndex < 0)
                {
                    g_gamepadSideSelection = -1;
                }
            }
        }
    }

    void UsePhysicalSettingsCursor()
    {
        g_gamepadSettingsCursorActive = false;
        g_gamepadSettingsNavigationInPanel = false;
    }

    void OpenSettingsMenu()
    {
        Slowtime::End();
        g_showWindow = false;
        g_radialToggleLocked = false;
        g_gamepadSettingsCursorActive = true;
        g_gamepadSettingsNavigationInPanel = false;
        g_gamepadSettingsConfigNavigation = false;
        g_gamepadSettingsRadialFocus = RadialSide::None;
        g_gamepadSettingsRadialItemIndex = -1;
        ResetSettingsCursor();
        SettingsMenu::WheelSettingsMenu::Open();
    }

    void ToggleSettingsMenuFromAltKey()
    {
        if (SettingsMenu::WheelSettingsMenu::IsOpen())
        {
            // O atalho direto respeita o mesmo retorno seguro do Cancelar.
            if (TrackEditor::IsOpen())
            {
                TrackEditor::Cancel();
                return;
            }
            SettingsMenu::WheelSettingsMenu::Close();
            ResetSettingsItemInfo();
            ItemInfo::InvalidatePreviewCache();
            return;
        }
        OpenSettingsMenu();
    }

    bool GamepadOpenRadial(RadialSide side)
    {
        const bool hasItems =
            (side == RadialSide::Left || side == RadialSide::Right) ? !g_sideItems.empty() :
            side == RadialSide::Top ? !g_topItems.empty() :
            side == RadialSide::Bottom ? !g_bottomItems.empty() : false;
        if (!g_showWindow || g_radialMode != RadialMode::Gameplay || !hasItems)
            return false;

        const WheelLayout layout = GetWheelLayout();
        g_radialSide = side;
        g_radialLocked = true;
        g_gamepadHorizontalSelectionLatch = 0;
        g_gamepadSideSelection = -1;
        if (side == RadialSide::Left) g_radialOrigin = layout.leftRadial;
        else if (side == RadialSide::Right) g_radialOrigin = layout.rightRadial;
        else if (side == RadialSide::Top) g_radialOrigin = layout.topRadial;
        else if (side == RadialSide::Bottom) g_radialOrigin = layout.bottomRadial;

        if (side == RadialSide::Left)
            g_radialVector = ImVec2(GetSideRadialRadius(), 0.0f);
        else if (side == RadialSide::Right)
            g_radialVector = ImVec2(-GetSideRadialRadius(), 0.0f);
        else
            g_radialVector = ImVec2(0.0f, 0.0f);
        g_radialMouseVelocity = ImVec2(0.0f, 0.0f);
        g_lastMousePos = ImGui::GetIO().MousePos;
        Gamepad::PulseSelection();
        return true;
    }
    //VAI TOMANDO!!!
    void GamepadShoulder(int direction)
    {
        if (direction == 0)
            return;
        if (SettingsMenu::WheelSettingsMenu::IsOpen())
        {
            GamepadSettingsNavigate(direction, 0);
            return;
        }
        if (!g_showWindow || g_radialMode != RadialMode::Gameplay)
            return;

        if (g_radialSide == RadialSide::None)
        {
            const bool hasTop = !g_topItems.empty();
            const bool hasBottom = !g_bottomItems.empty();
            if (hasTop != hasBottom)
                GamepadOpenRadial(hasTop ? RadialSide::Top : RadialSide::Bottom);
            else
                return;
        }

        if (g_radialSide == RadialSide::Left || g_radialSide == RadialSide::Right)
        {
            if (g_sideItems.empty())
                return;
            ScrollSideRadial(direction);
            g_radialVector = g_radialSide == RadialSide::Left
                ? ImVec2(GetSideRadialRadius(), 0.0f)
                : ImVec2(-GetSideRadialRadius(), 0.0f);
            Gamepad::PulseItemSelection();
        }
        else
        {
            ScrollTopBottomRadial(direction);
            Gamepad::PulseItemSelection();
        }
    }

    void GamepadStepSideSelection(int direction)
    {
        if (direction == 0 || !g_showWindow ||
            g_radialMode != RadialMode::Gameplay ||
            (g_radialSide != RadialSide::Left &&
             g_radialSide != RadialSide::Right))
        {
            return;
        }

        const int totalItems = static_cast<int>(g_sideItems.size());
        if (totalItems <= 1)
            return;

        g_sideScrollDirection = direction < 0 ? -1 : 1;

        // A seleção fixa do gamepad precisa percorrer os itens mesmo quando
        // Lock Scroll impede a rotação manual de uma lista sem excedentes.
        // O seletor permanece no lado interno e a lista gira sob ele.
        g_sideScrollOffset = WrapSideIndex(
            g_sideScrollOffset + direction,
            totalItems);
        g_radialVector = g_radialSide == RadialSide::Left
            ? ImVec2(GetSideRadialRadius(), 0.0f)
            : ImVec2(-GetSideRadialRadius(), 0.0f);
        g_gamepadSideSelection = WrapSideIndex(
            g_sideScrollOffset,
            totalItems);
        Gamepad::PulseItemSelection();
    }

    bool GamepadSettingsPrimaryDown()
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return false;
        if (TrackEditor::IsOpen())
            return true;

        if (g_activeGeneralSlider != GeneralSlider::None ||
            g_activeLayoutSlider != LayoutSlider::None)
        {
            SettingsMouseUp();
            g_gamepadSettingsPrimaryOwned = false;
            return true;
        }

        if (!g_gamepadSettingsNavigationInPanel)
        {
            g_gamepadSettingsNavigationInPanel = true;
            g_gamepadSettingsNavigationIndex = 0;
            GamepadSettingsNavigate(0, 0);
            return true;
        }

        if (g_settingsDrag.active)
        {
            g_settingsLeftReleased = true;
            g_gamepadSettingsDragClickArmed = false;
            return true;
        }

        if (GetSettingsHoveredHitbox(g_settingsMousePos))
        {
            g_settingsLeftPressed = true;
            g_gamepadSettingsDragClickArmed = true;
            return true;
        }

        bool handled = SettingsCloseButtonClick() || SettingsWheelKeyClick() ||
            SettingsGameplayDescriptionClick() || SettingsLockClick() ||
            HandleMouseUnlockClick();
        if (!handled)
            handled = SettingsMouseDown();
        const bool sliderSelected =
            g_activeGeneralSlider != GeneralSlider::None ||
            g_activeLayoutSlider != LayoutSlider::None;
        g_gamepadSettingsPrimaryOwned = handled && !sliderSelected;
        if (handled)
            Gamepad::PulseSelection();
        return handled;
    }

    void GamepadSettingsPrimaryUp()
    {
        if (g_gamepadSettingsDragClickArmed)
            return;
        if (g_activeGeneralSlider != GeneralSlider::None ||
            g_activeLayoutSlider != LayoutSlider::None)
            return;
        if (g_gamepadSettingsPrimaryOwned)
            SettingsMouseUp();
        g_gamepadSettingsPrimaryOwned = false;
    }

    bool GamepadSettingsDelete()
    {
        if (TrackEditor::IsOpen())
            return true;
        return SettingsMenu::WheelSettingsMenu::IsOpen() && SettingsRightClick();
    }

    void GamepadSettingsClose()
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return;
        if (TrackEditor::IsOpen())
            return;
        if (g_settingsSectionExpanded || g_settingsSectionOpenT > 0.05f)
        {
            CloseSettingsSectionMenu();
            g_gamepadSettingsConfigNavigation = false;
            g_gamepadSettingsNavigationInPanel = false;
            g_gamepadSettingsRadialFocus = RadialSide::None;
            g_gamepadSettingsRadialItemIndex = -1;
            g_settingsMousePos = GetWheelLayout().leftRadial;
            ImGui::GetIO().MousePos = g_settingsMousePos;
            return;
        }
        if (g_settingsDrag.active)
        {
            g_settingsMousePos = ImVec2(-10000.0f, -10000.0f);
            FinishSettingsDrag();
        }
        SettingsMenu::WheelSettingsMenu::Close();
        g_gamepadSettingsConfigNavigation = false;
        g_gamepadSettingsRadialFocus = RadialSide::None;
        g_gamepadSettingsRadialItemIndex = -1;
    }

    void GamepadSettingsDirectional(int horizontal, int vertical)
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return;
        if (TrackEditor::IsOpen())
            return;

        const auto focusSection = [&](SettingsSection section) {
            if (g_settingsSection == SettingsSection::Layout &&
                section != SettingsSection::Layout)
            {
                CloseLayoutPresetDialogs();
            }
            g_settingsSection = section;
            g_settingsSectionExpanded = true;
            g_gamepadSettingsConfigNavigation = true;
            g_gamepadSettingsNavigationInPanel = false;
            g_gamepadSettingsRadialFocus = RadialSide::None;
            const SettingsSectionLayout layout = GetSettingsSectionLayout();
            g_settingsMousePos = section == SettingsSection::Gameplay
                ? layout.gameplay
                : section == SettingsSection::Layout ? layout.layout : layout.center;
            ImGui::GetIO().MousePos = g_settingsMousePos;
            Gamepad::PulseSelection();
        };
        
        const auto returnToRadial = [&]() {
            g_gamepadSettingsConfigNavigation = false;
            g_gamepadSettingsNavigationInPanel = false;
            g_gamepadSettingsRadialFocus = RadialSide::Left;
            g_gamepadSettingsRadialItemIndex = -1;
            g_settingsMousePos = GetWheelLayout().leftRadial;
            ImGui::GetIO().MousePos = g_settingsMousePos;
            Gamepad::PulseSelection();
        };

        if (horizontal != 0)
        {
            if (g_gamepadSettingsConfigNavigation)
            {
                // Dentro da lista, a saída pertence exclusivamente ao D-pad Up
                // quando o primeiro controle está selecionado.
                if (g_gamepadSettingsNavigationInPanel)
                    return;

                if (horizontal > 0)
                {
                    if (g_settingsSection == SettingsSection::Gameplay)
                        focusSection(SettingsSection::Settings);
                    else if (g_settingsSection == SettingsSection::Settings)
                        focusSection(SettingsSection::Layout);
                    else
                        returnToRadial();
                }
                else
                {
                    if (g_settingsSection == SettingsSection::Layout)
                        focusSection(SettingsSection::Settings);
                    else if (g_settingsSection == SettingsSection::Settings)
                        focusSection(SettingsSection::Gameplay);
                    else
                        returnToRadial();
                }
                return;
            }

            if (horizontal > 0)
                focusSection(SettingsSection::Settings);
            else
                returnToRadial();
            return;
        }

        if (g_gamepadSettingsConfigNavigation)
        {
            if (vertical > 0 && !g_gamepadSettingsNavigationInPanel)
            {
                g_gamepadSettingsNavigationInPanel = true;
                g_gamepadSettingsNavigationIndex = 0;
                GamepadSettingsNavigate(0, 0);
                Gamepad::PulseSelection();
                return;
            }
            GamepadSettingsNavigate(horizontal, vertical);
            return;
        }

        RadialSide side = RadialSide::None;
        
        if (vertical < 0) side = RadialSide::Top;
        else if (vertical > 0) side = RadialSide::Bottom;
        else if (horizontal < 0) side = RadialSide::Left;
        
        if (side == RadialSide::None)
            return;

        g_gamepadSettingsRadialFocus = side;
        g_gamepadSettingsRadialItemIndex = -1;
        g_gamepadSettingsNavigationInPanel = false;

        const WheelLayout layout = GetWheelLayout();
        g_settingsMousePos = side == RadialSide::Top ? layout.topRadial :
            side == RadialSide::Bottom ? layout.bottomRadial : layout.leftRadial;
        ImGui::GetIO().MousePos = g_settingsMousePos;
        Gamepad::PulseSelection();
    }

    void GamepadSettingsCycleRadial(int direction)
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen() || direction == 0 ||
            g_gamepadSettingsConfigNavigation ||
            g_gamepadSettingsRadialFocus == RadialSide::None)
            return;

        const RadialSide focus = g_gamepadSettingsRadialFocus;
        
        std::vector<const SettingsItemHitbox*> candidates;
        
        for (const auto& hitbox : g_settingsItemHitboxes)
        {
        
            const bool sameRadial = focus == RadialSide::Left
                ? hitbox.side == RadialSide::Left || hitbox.side == RadialSide::Right
                : hitbox.side == focus;
        
            if (sameRadial && !IsSettingsItemCoveredByControlPanel(hitbox.position, hitbox.radius))
                candidates.push_back(&hitbox);
        }
        
        if (candidates.empty())
            return;

        std::sort(candidates.begin(), candidates.end(), [](const auto* a, const auto* b) {
            return a->index < b->index;
        });

        if (focus == RadialSide::Left &&
            static_cast<int>(g_sideItems.size()) > GetSideVisibleLimit())
        {
            g_radialSide = RadialSide::Left;
        
            ScrollSideRadial(direction);
        
            const ImVec2 anchor(
                GetWheelLayout().leftRadial.x + GetSideRadialRadius(),
                GetWheelLayout().leftRadial.y);
        
            const auto* fixed = *std::min_element(candidates.begin(), candidates.end(),
                [&](const auto* a, const auto* b) {
                    const float adx = a->position.x - anchor.x;
                    const float ady = a->position.y - anchor.y;
                    const float bdx = b->position.x - anchor.x;
                    const float bdy = b->position.y - anchor.y;
                    return adx * adx + ady * ady < bdx * bdx + bdy * bdy;
                });
        
            g_settingsMousePos = fixed->position;
        }
        else
        {
            const int count = static_cast<int>(candidates.size());
        
            g_gamepadSettingsRadialItemIndex = g_gamepadSettingsRadialItemIndex < 0
                ? (direction > 0 ? 0 : count - 1)
                : WrapIndex(g_gamepadSettingsRadialItemIndex + direction, count);
        
            g_settingsMousePos = candidates[g_gamepadSettingsRadialItemIndex]->position;
        }
        
        ImGui::GetIO().MousePos = g_settingsMousePos;
        
        Gamepad::PulseItemSelection();
    }

    void GamepadSettingsNavigate(int horizontal, int vertical)
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return;

        g_gamepadSettingsCursorActive = true;
        if (!g_gamepadSettingsNavigationInPanel)
        {
            const SettingsSectionLayout layout = GetSettingsSectionLayout();
            g_settingsMousePos = g_settingsSection == SettingsSection::Gameplay
                ? layout.gameplay : g_settingsSection == SettingsSection::Layout
                    ? layout.layout : layout.center;
            ImGui::GetIO().MousePos = g_settingsMousePos;
            return;
        }

        if (vertical < 0 && g_gamepadSettingsNavigationIndex == 0)
        {
            g_gamepadSettingsNavigationInPanel = false;
            const SettingsSectionLayout layout = GetSettingsSectionLayout();
            g_settingsMousePos = g_settingsSection == SettingsSection::Gameplay
                ? layout.gameplay : g_settingsSection == SettingsSection::Layout
                    ? layout.layout : layout.center;
            ImGui::GetIO().MousePos = g_settingsMousePos;
            Gamepad::PulseSelection();
            return;
        }

        if (g_settingsSection == SettingsSection::Layout)
        {
            const int direction = vertical > 0 ? 1 : vertical < 0 ? -1 : 0;
            std::vector<int> visibleSliders;
            visibleSliders.reserve(static_cast<std::size_t>(LayoutSlider::Count));
            for (int i = 0; i < static_cast<int>(LayoutSlider::Count); ++i)
            {
                if (g_layoutSliderVisible[static_cast<std::size_t>(i)])
                    visibleSliders.push_back(i);
            }
            std::sort(visibleSliders.begin(), visibleSliders.end(), [](int a, int b) {
                return g_layoutSliderRows[static_cast<std::size_t>(a)] <
                    g_layoutSliderRows[static_cast<std::size_t>(b)];
            });
            int candidate = visibleSliders.empty() ? 0 : visibleSliders.front();
            if (!visibleSliders.empty())
            {
                auto current = std::find(visibleSliders.begin(), visibleSliders.end(),
                    g_gamepadSettingsNavigationIndex);
                int position = current == visibleSliders.end() ? 0 :
                    static_cast<int>(std::distance(visibleSliders.begin(), current));
                if (direction != 0)
                {
                    position = std::clamp(position + direction, 0,
                        static_cast<int>(visibleSliders.size()) - 1);
                }
                candidate = visibleSliders[static_cast<std::size_t>(position)];
            }
            g_gamepadSettingsNavigationIndex = candidate;
            const float controlScale = 1.0f +
                (std::clamp(Config::g_fontSizeScale, 1.0f, 2.5f) - 1.0f) * (2.0f / 3.0f);
            const float rowHeight = std::max(
                45.0f * controlScale, ImGui::GetFontSize() + 25.0f * controlScale);
            const int row = g_layoutSliderRows[
                static_cast<std::size_t>(g_gamepadSettingsNavigationIndex)];
            g_layoutPanelScroll = std::clamp(
                rowHeight * static_cast<float>(row - 2), 0.0f, g_layoutPanelMaxScroll);
            g_gamepadSettingsNeedsCursorSync = true;
            return;
        }

        std::vector<LayoutSliderHitbox> targets;
        
        if (g_settingsSection == SettingsSection::Settings)
        {
        
            LayoutSliderHitbox firstKey{};
        
            firstKey.min = g_wheelKeyButtonMin;
        
            firstKey.max = g_wheelKeyButtonMax;
        
            targets.push_back(firstKey);
        
            LayoutSliderHitbox secondKey{};
        
            secondKey.min = g_secondaryKeyButtonMin;
        
            secondKey.max = g_secondaryKeyButtonMax;
        
            targets.push_back(secondKey);
        
            LayoutSliderHitbox automatic{};
        
            automatic.min = ImVec2(
                g_automaticArrowButtonCenter.x - g_automaticArrowButtonRadius,
                g_automaticArrowButtonCenter.y - g_automaticArrowButtonRadius);
        
            automatic.max = ImVec2(
                g_automaticArrowButtonCenter.x + g_automaticArrowButtonRadius,
                g_automaticArrowButtonCenter.y + g_automaticArrowButtonRadius);
        
            targets.push_back(automatic);
        
            targets.insert(targets.end(),
                g_generalSliderHitboxes.begin(), g_generalSliderHitboxes.end());
        
            targets.push_back(g_resetAllConfigHitbox);
        }
        else if (g_settingsSection == SettingsSection::Gameplay)
        {
        
            const auto addCircle = [&](const ImVec2& center, float radius) {
                LayoutSliderHitbox hitbox{};
                hitbox.min = ImVec2(center.x - radius, center.y - radius);
                hitbox.max = ImVec2(center.x + radius, center.y + radius);
                targets.push_back(hitbox);
            };
        
            LayoutSliderHitbox language{};
            language.min = g_languageButtonMin;
            language.max = g_languageButtonMax;
            targets.push_back(language);
        
            for (const auto& center : g_slowTimeScopeCenters)
                addCircle(center, std::max(g_slowTimeScopeRadius, 10.0f));
        
            for (const auto& center : g_blurScopeCenters)
                addCircle(center, std::max(g_blurScopeRadius, 10.0f));
        
            targets.push_back({ g_slowTimeSliderMin, g_slowTimeSliderMax });
            targets.push_back(g_slowTimeResetHitbox);
        
            for (const auto& center : g_gameplayIconButtonCenters)
                addCircle(center, std::max(g_gameplayIconButtonRadius, 18.0f));
        
            addCircle(g_fastDragButtonCenter,
                std::max(g_fastDragButtonRadius, 18.0f));
        
            if (IconCustom::HasValidConfiguration())
                addCircle(g_customIconsReloadCenter, std::max(g_customIconsReloadRadius, 10.0f));
        
            targets.push_back({ g_radialShapeButtonMin, g_radialShapeButtonMax });
            targets.push_back({ g_trackModeButtonMin, g_trackModeButtonMax });
            targets.push_back({ g_trackEditorButtonMin, g_trackEditorButtonMax });
            targets.push_back({ g_radialAnimationButtonMin, g_radialAnimationButtonMax });
        
            addCircle(g_gameplayPreviewButtonCenter, 18.0f);
        
            addCircle(g_gameplayDescriptionButtonCenter, 18.0f);
        }
        
        if (targets.empty())
            return;

        g_gamepadSettingsNavigationIndex = std::clamp(
            g_gamepadSettingsNavigationIndex + (vertical > 0 ? 1 : vertical < 0 ? -1 : 0),
            0, static_cast<int>(targets.size()) - 1);
        
        const auto& target = targets[g_gamepadSettingsNavigationIndex];
        
        g_settingsMousePos = ImVec2(
            (target.min.x + target.max.x) * 0.5f,
            (target.min.y + target.max.y) * 0.5f);
        
        if (horizontal != 0)
            g_settingsMousePos.x = std::clamp(
                g_settingsMousePos.x + static_cast<float>(horizontal) * 18.0f,
                target.min.x, target.max.x);
        
        ImGui::GetIO().MousePos = g_settingsMousePos;
        
        Gamepad::PulseSelection();
    }

    void GamepadSettingsAdjustSlider(int direction)
    {
        if (!g_gamepadSettingsNavigationInPanel || direction == 0)
            return;

        if (g_settingsSection == SettingsSection::Settings)
        {
            const int sliderIndex = g_gamepadSettingsNavigationIndex - 3;
            if (sliderIndex == 0)
                Config::g_sideMouseSensitivity = std::clamp(
                    Config::g_sideMouseSensitivity + 0.10f * direction, 0.25f, 3.0f);
            else if (sliderIndex == 1)
                Config::g_sideMouseSmooth = std::clamp(
                    Config::g_sideMouseSmooth + 5.0f * direction, 0.0f, 100.0f);
            else if (sliderIndex == 2)
                Config::g_gamepadAnalogSensitivity = std::clamp(
                    Config::g_gamepadAnalogSensitivity + 0.10f * direction, 0.25f, 3.0f);
            else if (sliderIndex == 3)
                Config::g_gamepadAnalogSmooth = std::clamp(
                    Config::g_gamepadAnalogSmooth + 5.0f * direction, 0.0f, 100.0f);
            else
                return;
            Config::SaveConfig();
            return;
        }

        if (g_settingsSection != SettingsSection::Layout ||
            g_gamepadSettingsNavigationIndex < 0 ||
            g_gamepadSettingsNavigationIndex >= static_cast<int>(LayoutSlider::Count))
            return;

        float* value = nullptr;
        float minimum = 0.0f;
        float maximum = 100.0f;
        float step = 2.0f;
        const auto selectedLayoutSlider = static_cast<LayoutSlider>(
            g_gamepadSettingsNavigationIndex);
        if (selectedLayoutSlider == LayoutSlider::RadialLineOpacity)
        {
            Track::SetRadialLineOpacity(std::clamp(
                Track::RadialLineOpacity() + static_cast<float>(direction), 0.0f, 100.0f));
            Config::SaveLayoutConfig();
            return;
        }
        if (selectedLayoutSlider == LayoutSlider::OverflowLineOpacity)
        {
            Track::SetLineOpacity(std::clamp(
                Track::LineOpacity() + static_cast<float>(direction), 0.0f, 100.0f));
            Config::SaveLayoutConfig();
            return;
        }
        if (selectedLayoutSlider == LayoutSlider::RadialRotation)
        {
            const float degrees = std::clamp(
                Track::RadialRotation() * 57.2957795f + 2.0f * direction,
                -180.0f, 180.0f);
            Track::SetRadialRotation(degrees * 0.01745329252f);
            Config::SaveLayoutConfig();
            return;
        }
        Config::ItemPreviewProfile previewProfile{};
        PreviewLayoutField previewField{};
        if (GetPreviewSliderInfo(selectedLayoutSlider, previewProfile, previewField))
        {
            value = &PreviewLayoutValue(previewProfile, previewField);
            if (previewField == PreviewLayoutField::ItemSize)
            {
                minimum = 1.0f;
                maximum = 500.0f;
                step = 5.0f;
            }
        }
        else if (std::size_t previewCategoryIndex = 0;
            GetPreviewCategorySliderInfo(
                selectedLayoutSlider, previewCategoryIndex))
        {
            value = &Config::g_itemPreviewCategoryMultipliers[
                previewCategoryIndex];
            minimum = 0.2f;
            maximum = 3.0f;
            step = 0.05f;
        }
        else switch (selectedLayoutSlider)
        {
        case LayoutSlider::FontSize: value = &Config::g_fontSizeScale; minimum = 1.0f; maximum = 2.5f; step = 0.05f; break;
        case LayoutSlider::DrawMarkDistance: value = &Config::g_drawMarkDistance; minimum = 0.0f; maximum = 100.0f; step = 1.0f; break;
        case LayoutSlider::ItemOpacity: value = &Config::g_itemOpacity; break;
        case LayoutSlider::GeneralItemSize: value = &Config::g_generalItemSize; minimum = 25.0f; maximum = 200.0f; break;
        case LayoutSlider::SlotSize: value = &Config::g_slotSize; minimum = 25.0f; maximum = 200.0f; break;
        case LayoutSlider::ItemBackgroundOpacity: value = &Config::g_itemBackgroundOpacity; break;
        case LayoutSlider::ItemBorderOpacity: value = &Config::g_itemBorderOpacity; break;
        case LayoutSlider::IconSize: value = &Config::g_iconSize; minimum = 25.0f; maximum = 200.0f; break;
        case LayoutSlider::BaseIconOpacity: value = &Config::g_baseIconOpacity; break;
        case LayoutSlider::OverflowSize: value = &Config::g_overflowSize; minimum = 25.0f; maximum = 200.0f; break;
        case LayoutSlider::OverflowBackgroundOpacity: value = &Config::g_overflowBackgroundOpacity; break;
        case LayoutSlider::OverflowBorderOpacity: value = &Config::g_overflowBorderOpacity; break;
        case LayoutSlider::OverflowOpacity: value = &Config::g_overflowOpacity; break;
        case LayoutSlider::ItemNameOpacity: value = &Config::g_itemNameOpacity; break;
        case LayoutSlider::ItemNamePositionY: value = &Config::g_itemNamePositionY; break;
        case LayoutSlider::ItemNamePositionX: value = &Config::g_itemNamePositionX; break;
        case LayoutSlider::CenterOpacity: value = &Config::g_centerOpacity; break;
        case LayoutSlider::RadialQuantity: value = &Config::g_radialQuantity; minimum = 3.0f; maximum = 50.0f; step = 1.0f; break;
        case LayoutSlider::StardustFade: value = &Config::g_stardustFade; break;
        case LayoutSlider::RadialStretch: value = &Config::g_radialStretch; break;
        case LayoutSlider::SidePosition: value = &Config::g_sideRadialPosition; break;
        case LayoutSlider::SideOpacity: value = &Config::g_sideOpacity; break;
        case LayoutSlider::TopPosition: value = &Config::g_topVerticalPosition; break;
        case LayoutSlider::TopStretch: value = &Config::g_topHorizontalStretch; break;
        case LayoutSlider::TopOpacity: value = &Config::g_topOpacity; break;
        case LayoutSlider::BottomPosition: value = &Config::g_bottomVerticalPosition; break;
        case LayoutSlider::BottomStretch: value = &Config::g_bottomHorizontalStretch; break;
        case LayoutSlider::BottomOpacity: value = &Config::g_bottomOpacity; break;
        case LayoutSlider::TopItemQuantity: value = &Config::g_topItemStyle.quantity; minimum = 3.0f; maximum = 50.0f; step = 1.0f; break;
        case LayoutSlider::TopGeneralItemSize: value = &Config::g_topItemStyle.generalSize; minimum = 25.0f; maximum = 200.0f; break;
        case LayoutSlider::TopItemOpacity: value = &Config::g_topItemStyle.opacity; break;
        case LayoutSlider::TopLineOpacity: value = &Config::g_topItemStyle.lineOpacity; break;
        case LayoutSlider::TopSlotSize: value = &Config::g_topItemStyle.slotSize; minimum = 25.0f; maximum = 200.0f; break;
        case LayoutSlider::TopBackgroundOpacity: value = &Config::g_topItemStyle.backgroundOpacity; break;
        case LayoutSlider::TopBorderOpacity: value = &Config::g_topItemStyle.borderOpacity; break;
        case LayoutSlider::TopIconSize: value = &Config::g_topItemStyle.iconSize; minimum = 25.0f; maximum = 200.0f; break;
        case LayoutSlider::TopIconOpacity: value = &Config::g_topItemStyle.iconOpacity; break;
        case LayoutSlider::BottomItemQuantity: value = &Config::g_bottomItemStyle.quantity; minimum = 3.0f; maximum = 50.0f; step = 1.0f; break;
        case LayoutSlider::BottomGeneralItemSize: value = &Config::g_bottomItemStyle.generalSize; minimum = 25.0f; maximum = 200.0f; break;
        case LayoutSlider::BottomItemOpacity: value = &Config::g_bottomItemStyle.opacity; break;
        case LayoutSlider::BottomLineOpacity: value = &Config::g_bottomItemStyle.lineOpacity; break;
        case LayoutSlider::BottomSlotSize: value = &Config::g_bottomItemStyle.slotSize; minimum = 25.0f; maximum = 200.0f; break;
        case LayoutSlider::BottomBackgroundOpacity: value = &Config::g_bottomItemStyle.backgroundOpacity; break;
        case LayoutSlider::BottomBorderOpacity: value = &Config::g_bottomItemStyle.borderOpacity; break;
        case LayoutSlider::BottomIconSize: value = &Config::g_bottomItemStyle.iconSize; minimum = 25.0f; maximum = 200.0f; break;
        case LayoutSlider::BottomIconOpacity: value = &Config::g_bottomItemStyle.iconOpacity; break;
        default: break;
        }
        if (!value)
            return;
        *value = std::clamp(*value + step * direction, minimum, maximum);
        if (selectedLayoutSlider == LayoutSlider::RadialQuantity)
        {
            *value = std::round(*value);
            Config::g_radialStretch = std::max(
                Config::g_radialStretch, std::max(0.0f, (*value - 25.0f) * (80.0f / 25.0f)));
        }
        else if (selectedLayoutSlider == LayoutSlider::RadialStretch)
        {
            const float maxQuantity = 25.0f +
                std::clamp(Config::g_radialStretch, 0.0f, 80.0f) * (25.0f / 80.0f);
            Config::g_radialQuantity = std::min(Config::g_radialQuantity, std::floor(maxQuantity));
        }
        std::size_t changedPreviewCategory = 0;
        if (GetPreviewCategorySliderInfo(
                selectedLayoutSlider, changedPreviewCategory))
            ItemPreview::InvalidateSizeScale();
        Config::SaveConfig();
    }

    static bool SameSettingsItem(
        RE::TESForm* formA,
        std::uint16_t uniqueA,
        bool hasUniqueA,
        RE::TESForm* formB,
        std::uint16_t uniqueB,
        bool hasUniqueB)
    {
        return formA == formB &&
            hasUniqueA == hasUniqueB &&
            (!hasUniqueA || uniqueA == uniqueB);
    }

    static std::vector<RadialItem>* GetSettingsItemList(
        RadialSide side)
    {
        switch (side)
        {
        case RadialSide::Left:
        case RadialSide::Right:
            return &g_sideItems;

        case RadialSide::Top:
            return &g_topItems;

        case RadialSide::Bottom:
            return &g_bottomItems;

        default:
            return nullptr;
        }
    }


    static void RegisterSettingsItem(
        RE::TESForm* form,
        RadialSide side,
        int index,
        const ImVec2& position,
        float radius)
    {
        if (!form)
            return;

        auto* items = GetSettingsItemList(side);

        if (!items ||
            index < 0 ||
            index >= static_cast<int>(items->size()))
            return;

        const RadialItem& item = (*items)[index];

        if (item.form != form)
            return;

        // Oculta somente a instância arrastada.
        if (g_settingsDrag.active &&
            SameSettingsItem(
                item.form,
                item.uniqueID,
                item.hasUniqueID,
                g_settingsDrag.form,
                g_settingsDrag.uniqueID,
                g_settingsDrag.hasUniqueID))
        {
            return;
        }

        SettingsItemHitbox hitbox{};

        hitbox.form = item.form;
        hitbox.uniqueID = item.uniqueID;
        hitbox.hasUniqueID = item.hasUniqueID;

        hitbox.side = side;
        hitbox.index = index;
        hitbox.position = position;
        hitbox.radius = radius;

        g_settingsItemHitboxes.push_back(hitbox);
    }

    static bool IsSettingsItemCoveredByControlPanel(const ImVec2& position, float radius);

    static const SettingsItemHitbox* GetSettingsHoveredHitbox(
        const ImVec2& mouse)
    {
        const SettingsItemHitbox* closest = nullptr;

        float closestDistance = FLT_MAX;

        for (const auto& hitbox : g_settingsItemHitboxes)
        {
            if (IsSettingsItemCoveredByControlPanel(hitbox.position, hitbox.radius))
                continue;

            const float dx =
                mouse.x - hitbox.position.x;

            const float dy =
                mouse.y - hitbox.position.y;

            const float distanceSquared =
                dx * dx + dy * dy;

            const float radiusSquared =
                hitbox.radius * hitbox.radius;

            if (distanceSquared > radiusSquared)
                continue;

            if (distanceSquared < closestDistance)
            {
                closestDistance = distanceSquared;

                closest = &hitbox;
            }
        }

        return closest;
    }

    
    static void BeginSettingsDrag(
        const SettingsItemHitbox& hitbox)
    {
        if (!hitbox.form)
            return;

        auto* items = GetSettingsItemList(hitbox.side);

        if (!items ||
            hitbox.index < 0 ||
            hitbox.index >= static_cast<int>(items->size()))
            return;

        const RadialItem& item = (*items)[hitbox.index];

        // Verifica se a hitbox ainda representa
        // a instância presente naquela posição.
        if (!SameSettingsItem(
            item.form,
            item.uniqueID,
            item.hasUniqueID,
            hitbox.form,
            hitbox.uniqueID,
            hitbox.hasUniqueID))
        {
            return;
        }

        g_settingsDrag = {};

        g_settingsDrag.active = true;
        g_settingsDrag.hitboxGeneration = g_settingsHitboxGeneration;
        g_settingsDrag.sideScrollOffsetBeforeRemoval = g_sideScrollOffset;

        g_settingsDrag.form = item.form;
        g_settingsDrag.uniqueID = item.uniqueID;
        g_settingsDrag.hasUniqueID = item.hasUniqueID;
        g_settingsDrag.item = item;

        g_settingsDrag.sourceSide = hitbox.side;
        g_settingsDrag.sourceIndex = hitbox.index;

        g_settingsDrag.position = hitbox.position;

        g_settingsDrag.returning = false;
        g_settingsDrag.returnPosition = hitbox.position;

        g_settingsDrag.offset = ImVec2(
            hitbox.position.x - g_settingsMousePos.x,
            hitbox.position.y - g_settingsMousePos.y
        );

        // Remove imediatamente a instância real. Os vizinhos passam a ocupar
        // o espaço livre durante o próprio drag usando as molas já existentes.
        items->erase(items->begin() + hitbox.index);
        for (int i = 0; i < static_cast<int>(items->size()); ++i)
            (*items)[i].slot = i;

        const bool sideTopologyChanged =
            hitbox.side == RadialSide::Left || hitbox.side == RadialSide::Right;
        if (sideTopologyChanged)
        {
            // Os itens laterais são lidos a partir de g_sideScrollOffset.
            // Remover uma entrada fisicamente anterior a esse ponto desloca
            // todos os índices seguintes em -1. Sem compensar, o primeiro
            // excedente e o último principal trocavam de apresentação até o
            // drop, embora a quantidade de excedentes continuasse correta.
            if (hitbox.index < g_sideScrollOffset)
                --g_sideScrollOffset;
            g_sideScrollOffset = items->empty()
                ? 0
                : WrapSideIndex(g_sideScrollOffset,
                    static_cast<int>(items->size()));
            g_settingsTopologySettleRemaining = 0.30f;
        }
        for (const auto& remaining : *items)
        {
            const RadialAnimKey key{
                remaining.form, remaining.uniqueID, remaining.hasUniqueID };
            if (auto it = g_itemAnimCache.find(key); it != g_itemAnimCache.end())
                it->second.settingsDropSettling = true;
        }
    }

        
    static void UpdateSettingsDrag(float deltaTime)
    {
        if (!g_settingsDrag.active)
            return;

        deltaTime = std::clamp(deltaTime, 0.0f, 0.05f);

        const float factor =
            1.0f - std::exp(-18.0f * deltaTime);

        // ============================================================
        // RETORNANDO À POSIÇÃO ORIGINAL
        // ============================================================

        if (g_settingsDrag.returning)
        {
            const ImVec2 target =
                g_settingsDrag.returnPosition;

            ImVec2& pos = g_settingsDrag.position;

            pos.x += (target.x - pos.x) * factor;
            pos.y += (target.y - pos.y) * factor;


            const float dx = target.x - pos.x;
            const float dy = target.y - pos.y;

            if (dx * dx + dy * dy < 1.0f)
            {
                g_settingsDrag = {};
            }

            return;
        }

        // ============================================================
        // DRAG NORMAL
        // ============================================================

        const ImVec2 target(
            g_settingsMousePos.x + g_settingsDrag.offset.x,
            g_settingsMousePos.y + g_settingsDrag.offset.y
        );


        g_settingsDrag.position.x +=
            (target.x - g_settingsDrag.position.x) * factor;

        g_settingsDrag.position.y +=
            (target.y - g_settingsDrag.position.y) * factor;

    }

    static RadialSide GetSettingsDropSide(
        const ImVec2& mouse,
        const ImVec2& screen)
    {
        (void)screen;
        const WheelLayout layout = GetWheelLayout();
        const ImVec2 leftCenter = layout.leftRadial;
        const ImVec2 topCenter = layout.topRadial;
        const ImVec2 bottomCenter = layout.bottomRadial;

        // ============================================================
        // TOP
        // ============================================================

        if (std::abs(mouse.y - topCenter.y) < 150.0f &&
            std::abs(mouse.x - topCenter.x) < GetTopBottomCurveWidth(true) * 0.65f)
        {
            return RadialSide::Top;
        }

        // ============================================================
        // BOTTOM
        // ============================================================

        if (std::abs(mouse.y - bottomCenter.y) < 150.0f &&
            std::abs(mouse.x - bottomCenter.x) < GetTopBottomCurveWidth(false) * 0.65f)
        {
            return RadialSide::Bottom;
        }

        // ============================================================
        // LEFT
        // ============================================================

        const float dx =
            mouse.x - leftCenter.x;

        const float dy =
            mouse.y - leftCenter.y;

        const float sideHoverRadius = GetSideRadialRadius() + 65.0f;
        if (dx * dx + dy * dy < sideHoverRadius * sideHoverRadius)
        {
            return RadialSide::Left;
        }

        return RadialSide::None;
    }

        
    // Detector isolado para o radial lateral customizado do WheelSettings.
    // A ordem vem dos slots do circuito, mas os segmentos usam as posições
    // finais que foram desenhadas neste frame. Assim a decisão respeita a
    // acomodação, a flutuação e a repulsão do drag, sem alterar os efeitos.
    static int GetSettingsCustomSideCircuitInsertIndex(
        RadialSide side,
        const ImVec2& mouse)
    {
        if ((side != RadialSide::Left && side != RadialSide::Right) ||
            !Config::g_customRadial ||
            !Track::HasValidSavedLayout())
        {
            return -1;
        }

        const int total = static_cast<int>(g_sideItems.size());
        if (total <= 0)
            return 0;

        const bool leftSide = side == RadialSide::Left;
        
        const WheelLayout layout = GetWheelLayout();
        
        const ImVec2 center = leftSide ? layout.leftRadial : layout.rightRadial;
        
        const int mainCount = std::min(total, GetSideVisibleLimit());
        
        const auto shape = static_cast<RadialShape::Style>(std::clamp(
            Config::g_radialShape, 0, RadialShape::Count() - 1));
        
        const auto slots = Track::CircuitSlots(
            center, GetSideRadialRadius(), leftSide, mainCount,
            total - mainCount, shape);
        
        if (slots.size() != static_cast<std::size_t>(total))
            return -1;

        struct CircuitNode
        {
            int ordinal{};
            int itemIndex{};
            ImVec2 position{};
        };
        
        std::vector<CircuitNode> nodes;
        
        nodes.reserve(slots.size());
        
        const bool visualHitboxesReady = g_settingsDrag.active &&
            g_settingsHitboxGeneration > g_settingsDrag.hitboxGeneration;
        
        for (const auto& slot : slots)
        {
            const int itemIndex = WrapSideIndex(
                g_sideScrollOffset + slot.ordinal, total);
            ImVec2 visualPosition = slot.position;

            // Só usa hitboxes após um frame com a lista já sem o item
            // arrastado. Antes disso o índice ainda representaria a ordem
            // antiga e o fallback para o slot-base é mais seguro.
            if (visualHitboxesReady)
            {
                const auto hitbox = std::ranges::find_if(
                    g_settingsItemHitboxes,
                    [&](const SettingsItemHitbox& candidate) {
                        return candidate.side == side &&
                            candidate.index == itemIndex;
                    });
        
                if (hitbox != g_settingsItemHitboxes.end())
                    visualPosition = hitbox->position;
            }

            nodes.push_back({
                slot.ordinal,
                itemIndex,
                visualPosition
            });
        }
        
        std::ranges::sort(nodes, {}, &CircuitNode::ordinal);

        if (nodes.size() == 1)
            return 0;

        float closestDistanceSq = FLT_MAX;
        
        std::size_t nextNode = 0;
        
        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            const ImVec2 a = nodes[i].position;
            const ImVec2 b = nodes[(i + 1) % nodes.size()].position;
            
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            
            const float lengthSq = dx * dx + dy * dy;
            
            if (lengthSq <= 0.001f)
                continue;

            const float t = std::clamp(
                ((mouse.x - a.x) * dx + (mouse.y - a.y) * dy) / lengthSq,
                0.0f,
                1.0f);
            
            const float projectedX = a.x + dx * t;
            const float projectedY = a.y + dy * t;
            
            const float distanceX = mouse.x - projectedX;
            const float distanceY = mouse.y - projectedY;
            
            const float distanceSq = distanceX * distanceX + distanceY * distanceY;
            
            if (distanceSq < closestDistanceSq)
            {
                closestDistanceSq = distanceSq;
                nextNode = (i + 1) % nodes.size();
            }
        }

        // Inserir antes do próximo nó equivale a colocá-lo exatamente entre
        // os dois itens que delimitam o ponto projetado no circuito.
        return nodes[nextNode].itemIndex;
    }

    static int GetSettingsInsertIndex(
        RadialSide side,
        const ImVec2& mouse)
    {
        auto* list = GetSettingsItemList(side);

        if (!list)
            return -1;

        const int total = static_cast<int>(list->size());

        if (total == 0)
            return 0;

        if (const int circuitIndex = GetSettingsCustomSideCircuitInsertIndex(
                side, mouse);
            circuitIndex >= 0)
        {
            return std::clamp(circuitIndex, 0, total);
        }

        const SettingsItemHitbox* closest = nullptr;

        float closestDistance = FLT_MAX;

        for (const auto& hitbox : g_settingsItemHitboxes)
        {
            if (hitbox.side != side)
                continue;

            if (SameSettingsItem(
                hitbox.form,
                hitbox.uniqueID,
                hitbox.hasUniqueID,
                g_settingsDrag.form,
                g_settingsDrag.uniqueID,
                g_settingsDrag.hasUniqueID))
            {
                continue;
            }

            const float dx =
                mouse.x - hitbox.position.x;

            const float dy =
                mouse.y - hitbox.position.y;

            const float distance = dx * dx + dy * dy;

            if (distance < closestDistance)
            {
                closestDistance = distance;
                closest = &hitbox;
            }
        }

        if (!closest)
            return 0;

        int index = closest->index;

        // ============================================================
        // TOP / BOTTOM
        // ============================================================

        if (side == RadialSide::Top ||
            side == RadialSide::Bottom)
        {
            if (mouse.x > closest->position.x)
                ++index;
        }

        // ============================================================
        // LEFT
        // ============================================================

        else if (side == RadialSide::Left)
        {
            const ImVec2 center = GetWheelLayout().leftRadial;
            const auto radialCoordinate = [&](const ImVec2& position) {
                float angle = std::atan2(
                    position.y - center.y, position.x - center.x) + PI * 0.5f;
                // O radial esquerdo usa a orientação angular espelhada.
                angle = -angle;
                while (angle < 0.0f) angle += PI * 2.0f;
                while (angle >= PI * 2.0f) angle -= PI * 2.0f;
                return angle;
            };

            float delta = radialCoordinate(mouse) - radialCoordinate(closest->position);
            while (delta > PI) delta -= PI * 2.0f;
            while (delta < -PI) delta += PI * 2.0f;
            if (delta > 0.0f)
                ++index;
        }

        return std::clamp(index, 0, total);
    }

    static void FinishSettingsDrag()
    {
        if (!g_settingsDrag.active)
            return;

        const auto drag = g_settingsDrag;

        
        auto* sourceList =
            GetSettingsItemList(drag.sourceSide);

        if (!sourceList)
            return;

        const ImVec2 screen =
            ImGui::GetIO().DisplaySize;

        const RadialSide targetSide =
            GetSettingsDropSide(
                g_settingsMousePos,
                screen
            );

        auto* targetList =
            GetSettingsItemList(targetSide);

        auto seedAnimationAtDrop = [&](const ImVec2& position,
                                       RadialSide dropSide) {
            const RadialAnimKey key{ drag.form, drag.uniqueID, drag.hasUniqueID };
            
            auto [it, inserted] = g_itemAnimCache.try_emplace(key);
            auto& anim = it->second;
            
            anim.currentPos = position;
            anim.previousPos = position;
            
            anim.velocity = ImVec2(0.0f, 0.0f);
            
            anim.posInitialized = true;
            
            anim.sidePolarInitialized = false;
            anim.sideWrapActive = false;
            anim.sideWrapTargetInitialized = false;
            anim.settingsDropSettling = true;

            // Um radial custom só usa TrackMovement quando há excedentes.
            // Com apenas os itens principais, ele mantém a animação polar
            // normal do radial. Escolher o controlador real aqui evita que
            // a semente seja aplicada ao estado que não será desenhado.
            const bool customUsesCircuit =
                Config::g_customRadial &&
                Track::HasValidSavedLayout() &&
                static_cast<int>(g_sideItems.size()) > GetSideVisibleLimit();

            if (customUsesCircuit &&
                (dropSide == RadialSide::Left || dropSide == RadialSide::Right))
            {
                // A distância armazenada pertence ao slot de origem. O
                // próximo UpdateCircuit precisa resolver o novo slot, mas a
                // apresentação deve continuar visível a partir do cursor.
                anim.customTrackAnimation = {};
                anim.settingsCustomDropStart = position;
                anim.settingsCustomDropProgress = 0.0f;
                anim.settingsCustomDropEntering = true;
            }

            // No radial Legacy, o estado polar anterior ainda apontava para
            // o slot de onde o item foi retirado. RadialAnimation::Update()
            // desenhava esse estado por um frame antes de seguir ao novo
            // destino, criando o "fantasma" no slot antigo. Semeia o estado
            // diretamente na posição do item em drag para ele ir ao slot
            // novo sem reaparecer no anterior. O custom sem excedentes usa
            // este mesmo controlador polar; apenas o custom com circuito
            // completo usa o TrackMovement acima.
            if (!customUsesCircuit &&
                (dropSide == RadialSide::Left || dropSide == RadialSide::Right))
            {
                const WheelLayout layout = GetWheelLayout();
            
                const ImVec2 center = dropSide == RadialSide::Left
                    ? layout.leftRadial : layout.rightRadial;
            
                const float dx = position.x - center.x;
                const float dy = position.y - center.y;
            
                auto& state = anim.gameplayRadialAnimation;
            
                state = {};
            
                state.position = position;
                state.lastTarget = position;
            
                state.angle = std::atan2(dy, dx);
                state.radius = std::sqrt(dx * dx + dy * dy);
            
                state.initialized = true;
            }
            
            (void)inserted;
        };

        const auto settleList = [&](const std::vector<RadialItem>& items) {
            for (const auto& item : items)
            {
                const RadialAnimKey key{ item.form, item.uniqueID, item.hasUniqueID };
                if (auto it = g_itemAnimCache.find(key); it != g_itemAnimCache.end())
                {
                    it->second.settingsDropSettling = true;
                }
            }
        };

        // Destino inválido: reinsere a instância real na origem. A mola parte
        // da posição atual do cursor e a leva de volta ao slot original.
        if (!targetList)
        {
            const int restoreIndex = std::clamp(
                drag.sourceIndex, 0, static_cast<int>(sourceList->size()));
            
            sourceList->insert(sourceList->begin() + restoreIndex, drag.item);
            
            for (int i = 0; i < static_cast<int>(sourceList->size()); ++i)
                (*sourceList)[i].slot = i;
            
            seedAnimationAtDrop(drag.position, drag.sourceSide);
            
            settleList(*sourceList);
            
            if (sourceList == &g_sideItems)
                g_settingsTopologySettleRemaining = 0.30f;
            
            g_settingsDrag = {};
            
            return;
        }

        RadialItem movedItem = drag.item;

        // ============================================================
        // CALCULA DESTINO ANTES DE MODIFICAR AS LISTAS
        // ============================================================

        int insertIndex =
            GetSettingsInsertIndex(
                targetSide,
                g_settingsMousePos
            );

        if (insertIndex < 0)
            return;

        insertIndex = std::clamp(
            insertIndex,
            0,
            static_cast<int>(targetList->size())
        );

        // ============================================================
        // INSERE NO DESTINO
        // ============================================================

        targetList->insert(
            targetList->begin() + insertIndex,
            std::move(movedItem)
        );
        
        seedAnimationAtDrop(drag.position, targetSide);
        
        settleList(*sourceList);
        
        if (sourceList != targetList)
            settleList(*targetList);
        
            if (sourceList == &g_sideItems || targetList == &g_sideItems)
        {
            g_sideScrollOffset = g_sideItems.empty()
                ? 0
                : WrapSideIndex(g_sideScrollOffset,
                    static_cast<int>(g_sideItems.size()));
            g_settingsTopologySettleRemaining = 0.30f;
        }

        // ============================================================
        // ATUALIZA OS SLOTS
        // ============================================================

        auto updateSlots = [](std::vector<RadialItem>& items)
        {
            for (int i = 0;
                i < static_cast<int>(items.size());
                ++i)
            {
                items[i].slot = i;
            }
        };

        updateSlots(*sourceList);

        if (sourceList != targetList)
        {
            updateSlots(*targetList);
        }

        g_settingsDrag = {};

        Logger::GetSingleton().Print(
            "SETTINGS DRAG COMPLETE | "
            "source={} | target={} | index={}",
            static_cast<int>(drag.sourceSide),
            static_cast<int>(targetSide),
            insertIndex
        );
    }



    bool RemoveItemFromRadials(
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        if (!form)
            return false;

        auto removeFrom = [&](std::vector<RadialItem>& items)
        {
            auto it = std::find_if(
                items.begin(),
                items.end(),
                [&](const RadialItem& item)
                {
                    if (item.form != form)
                        return false;

                    if (hasUniqueID)
                    {
                        return item.hasUniqueID &&
                            item.uniqueID == uniqueID;
                    }

                    return !item.hasUniqueID;
                }
            );

            if (it == items.end())
                return false;

            const RadialAnimKey removedKey{
                it->form, it->uniqueID, it->hasUniqueID };
            items.erase(it);
            g_itemAnimCache.erase(removedKey);

            for (int i = 0; i < static_cast<int>(items.size()); ++i)
            {
                items[i].slot = i;
            }

            // Uma alteração de tamanho invalida os ordinais e o percurso
            // guardado pelo controlador do circuito. Se a remoção acontece
            // enquanto outra entrada ainda está acomodando, reaproveitar esse
            // estado antigo faz o item interpretar o novo slot como uma volta
            // inteira. Mantemos somente as posições visuais independentes do
            // Inventory e descartamos os controladores que dependem do índice.
            const bool sideList = &items == &g_sideItems;
            const bool settingsOpen =
                SettingsMenu::WheelSettingsMenu::IsOpen();
            if (sideList)
            {
                g_sideScrollOffset = items.empty()
                    ? 0
                    : WrapSideIndex(g_sideScrollOffset,
                        static_cast<int>(items.size()));
                if (g_radialMode == RadialMode::Inventory)
                    g_inventoryTopologySettleRemaining = 0.30f;
                if (settingsOpen)
                    g_settingsTopologySettleRemaining = 0.30f;
            }
            for (int index = 0; index < static_cast<int>(items.size()); ++index)
            {
                const auto& remaining = items[static_cast<std::size_t>(index)];
                const RadialAnimKey remainingKey{
                    remaining.form, remaining.uniqueID, remaining.hasUniqueID };
                RadialItemAnimation& anim =
                    g_itemAnimCache.try_emplace(remainingKey).first->second;
                if (settingsOpen)
                {
                    // No WheelSettings o controlador já conhece a posição
                    // anterior no circuito. Preservá-lo permite acomodar a
                    // nova ordinal ao longo do trilho.
                    anim.settingsDropSettling = true;
                    continue;
                }
                anim.sideWrapActive = false;
                anim.sideWrapTargetInitialized = false;
                anim.sideWrapT = 0.0f;
                anim.customTrackAnimation = {};
                anim.gameplayRadialAnimation = {};
                anim.overflowMechanismAnimation = {};
                anim.posInitialized = false;
                anim.velocity = ImVec2(0.0f, 0.0f);
                if (sideList)
                {
                    for (std::size_t side = 0; side < 2; ++side)
                    {
                        if (!anim.inventorySideInitialized[side])
                            continue;
                        anim.inventorySideStart[side] =
                            anim.inventorySidePosition[side];
                        anim.inventorySideProgress[side] = 0.0f;
                        anim.inventorySideSettling[side] = true;
                    }
                    anim.radialSizeT = index < GetSideVisibleLimit() ? 1.0f : 0.0f;
                }
                else
                {
                    anim.inventoryDropStart = anim.currentPos;
                    anim.inventoryDropProgress = 0.0f;
                    anim.inventoryDropSettling = true;
                }
            }

            return true;
        };

        if (removeFrom(g_sideItems))
            return true;

        if (removeFrom(g_topItems))
            return true;

        if (removeFrom(g_bottomItems))
            return true;

        return false;
    }

    static void SpawnSettingsDeleteParticles(
        const ImVec2& position,
        float itemRadius)
    {
        constexpr int particleCount = 48;

        constexpr float PI2 = 6.28318530718f;

        for (int i = 0; i < particleCount; ++i)
        {
            SettingsDeleteParticle p;

            // Distribuição circular com pequenas variações.
            const float angle =
                (static_cast<float>(i) / particleCount) * PI2 +
                (static_cast<float>(std::rand() % 100) / 100.0f) * 0.35f;

            const float distance =
                static_cast<float>(std::rand() % 100) / 100.0f;

            const float speed =
                35.0f +
                static_cast<float>(std::rand() % 100) * 0.85f;

            // Partículas começam espalhadas dentro do item.
            const float spawnRadius =
                std::sqrt(distance) * itemRadius * 0.75f;

            p.position = ImVec2(
                position.x + std::cos(angle) * spawnRadius,
                position.y + std::sin(angle) * spawnRadius
            );

            // Movimento radial para fora.
            p.velocity = ImVec2(
                std::cos(angle) * speed,
                std::sin(angle) * speed
            );

            // Partículas de tamanhos diferentes.
            p.radius =
                0.7f +
                static_cast<float>(std::rand() % 100) / 100.0f * 2.0f;

            // Tempo de vida individual.
            p.maxLife =
                0.30f +
                static_cast<float>(std::rand() % 100) / 100.0f * 0.30f;

            p.life = p.maxLife;

            // Branco e dourado suave, combinando com o HUD.
            if (i % 4 == 0)
            {
                p.color =
                    IM_COL32(215, 195, 150, 255);
            }
            else
            {
                p.color =
                    IM_COL32(235, 230, 215, 255);
            }

            g_settingsDeleteParticles.push_back(p);
        }
    }

    static void DrawSettingsDeleteParticles(float deltaTime)
    {
        if (g_settingsDeleteParticles.empty())
            return;

        ImDrawList* draw =
            ImGui::GetForegroundDrawList();

        deltaTime =
            std::clamp(deltaTime, 0.0f, 0.05f);

        for (auto& p : g_settingsDeleteParticles)
        {
            // ========================================================
            // TEMPO DE VIDA
            // ========================================================

            p.life -= deltaTime;

            if (p.life <= 0.0f)
                continue;

            const float t =
                std::clamp(
                    p.life / p.maxLife,
                    0.0f,
                    1.0f
                );

            // ========================================================
            // MOVIMENTO
            // ========================================================

            p.position.x +=
                p.velocity.x * deltaTime;

            p.position.y +=
                p.velocity.y * deltaTime;

            // Desaceleração suave.
            const float damping =
                std::exp(-3.0f * deltaTime);

            p.velocity.x *= damping;
            p.velocity.y *= damping;

            // Leve gravidade para criar efeito de poeira.
            p.velocity.y += 18.0f * deltaTime;

            // ========================================================
            // FADE E TAMANHO
            // ========================================================

            const float fade =
                t * t;

            const float radius =
                p.radius * (0.35f + 0.65f * t);

            const ImU32 color =
                IM_COL32(
                    (p.color >> IM_COL32_R_SHIFT) & 0xFF,
                    (p.color >> IM_COL32_G_SHIFT) & 0xFF,
                    (p.color >> IM_COL32_B_SHIFT) & 0xFF,
                    static_cast<int>(210.0f * fade)
                );

            // ========================================================
            // DESENHA
            // ========================================================

            draw->AddCircleFilled(
                p.position,
                radius,
                color,
                8
            );
        }

        // Remove partículas que terminaram.
        std::erase_if(
            g_settingsDeleteParticles,
            [](const SettingsDeleteParticle& p)
            {
                return p.life <= 0.0f;
            }
        );
    }


    static void DeleteSettingsHoveredItem()
    {
        const auto* hitbox =
            GetSettingsHoveredHitbox(g_settingsMousePos);

        if (!hitbox || !hitbox->form)
            return;

        if (g_settingsDrag.active)
            return;

        // ============================================================
        // IDENTIFICA A LISTA
        // ============================================================

        std::vector<RadialItem>* items =
            GetSettingsItemList(hitbox->side);

        if (!items)
            return;

        // ============================================================
        // IDENTIFICA O ITEM PELO ÍNDICE
        // ============================================================

        const int index = hitbox->index;

        if (index < 0 ||
            index >= static_cast<int>(items->size()))
            return;

        // Copia antes de modificar o vector.
        const RadialItem item = (*items)[index];

        // ============================================================
        // VERIFICA A INSTÂNCIA EXATA
        // ============================================================

        if (!SameSettingsItem(
            item.form,
            item.uniqueID,
            item.hasUniqueID,
            hitbox->form,
            hitbox->uniqueID,
            hitbox->hasUniqueID))
        {
            return;
        }

        // ============================================================
        // REMOVE SOMENTE A INSTÂNCIA SELECIONADA
        // ============================================================

        if (RemoveItemFromRadials(
            item.form,
            item.uniqueID,
            item.hasUniqueID))
        {
            // ========================================================
            // EXPLOSÃO DE PARTÍCULAS
            // ========================================================

            SpawnSettingsDeleteParticles(
                hitbox->position,
                hitbox->radius
            );

            // ========================================================
            // REMOVE ANIMAÇÃO DA INSTÂNCIA
            // ========================================================

            g_itemAnimCache.erase(
                RadialAnimKey{
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID
                }
            );

            // ========================================================
            // LIMPA HOVER
            // ========================================================

            g_settingsHoveredItem = nullptr;
            g_settingsHoveredUniqueID = 0;
            g_settingsHoveredHasUniqueID = false;

            g_settingsItemHitboxes.clear();

            Logger::GetSingleton().Print(
                "SETTINGS ITEM REMOVED | form={:08X} | uniqueID={} | hasUniqueID={}",
                item.form->GetFormID(),
                item.uniqueID,
                item.hasUniqueID
            );
        }
    }

    static void ProcessSettingsEditor(float deltaTime)
    {
        if (g_settingsTopologySettleRemaining > 0.0f)
        {
            g_settingsTopologySettleRemaining = std::max(
                0.0f, g_settingsTopologySettleRemaining - deltaTime);
        }
        if (g_settingsTopologySettleRemaining <= 0.0f &&
            g_pendingSettingsScroll != 0)
        {
            const int queuedDirection = g_pendingSettingsScroll < 0 ? -1 : 1;
            g_pendingSettingsScroll -= queuedDirection;
            QueueSettingsSideScrollStep(queuedDirection);
        }

        ProcessSettingsSideScrollQueue();

        // ============================================================
        // ATUALIZA CURSOR
        // ============================================================

        if (!g_gamepadSettingsCursorActive)
        {
            g_settingsMousePos =
                GetSkyrimMousePos();
        }

        // ============================================================
        // HOVER
        // ============================================================

        const SettingsItemHitbox* hovered =
            GetSettingsHoveredHitbox(
                g_settingsMousePos
            );

        g_settingsHoveredItem =
            hovered ? hovered->form : nullptr;

        g_settingsHoveredUniqueID =
            hovered ? hovered->uniqueID : 0;

        g_settingsHoveredHasUniqueID =
            hovered ? hovered->hasUniqueID : false;

        // ============================================================
        // BOTÃO ESQUERDO - INICIAR DRAG
        // ============================================================

        if (g_settingsLeftPressed)
        {
            g_settingsLeftPressed = false;

            if (!g_settingsDrag.active && hovered)
            {
                BeginSettingsDrag(*hovered);
            }
        }

        // ============================================================
        // ATUALIZAR DRAG
        // ============================================================

        UpdateSettingsDrag(deltaTime);

        // ============================================================
        // BOTÃO ESQUERDO - SOLTAR
        // ============================================================

        if (g_settingsLeftReleased)
        {
            g_settingsLeftReleased = false;

            if (g_settingsDrag.active)
            {
                FinishSettingsDrag();
            }
        }

        // ============================================================
        // BOTÃO DIREITO - REMOVER
        // ============================================================

        if (g_settingsRightPressed)
        {
            g_settingsRightPressed = false;

            DeleteSettingsHoveredItem();
        }
    }
        
    
    static void DrawSettingsDraggedItem()
    {
        if (!g_settingsDrag.active)
            return;

        RE::TESForm* form =
            g_settingsDrag.form;

        if (!form)
            return;

        ImDrawList* draw =
            ImGui::GetForegroundDrawList();

        const ImVec2 position =
            g_settingsDrag.position;

        const ItemVisualStyle style = GetItemVisualStyle(g_settingsDrag.sourceSide);
        const float radius = 34.0f * (style.generalSize * 0.01f) *
            (style.slotSize * 0.01f);

        // ============================================================
        // FUNDO SUAVE
        // ============================================================

        draw->AddCircleFilled(
            position,
            radius + 5.0f,
            IM_COL32(255, 255, 255, 35),
            48
        );

        //draw->AddCircleFilled(
        //    position,
        //    radius,
        //    IM_COL32(20, 20, 25, 235),
        //    48
        //)
        
        DrawEquippedItemBackground(
            draw,
            g_settingsDrag.form,
            g_settingsDrag.uniqueID,
            g_settingsDrag.hasUniqueID,
            position,
            radius,
            1.0f,
            g_settingsDrag.sourceSide
        );

        // ============================================================
        // BORDA
        // ============================================================

        draw->AddCircle(
            position,
            radius,
            IM_COL32((style.borderColor >> 16) & 0xFF,
                (style.borderColor >> 8) & 0xFF,
                style.borderColor & 0xFF,
                static_cast<int>(255.0f * std::clamp(style.borderOpacity * 0.01f, 0.0f, 1.0f))),
            48,
            1.5f
        );

        // ============================================================
        // ÍCONE REAL DO ITEM
        // ============================================================

        auto* icon =
            ItemIcon::Get(form);

        if (icon)
        {
            const float iconRadius = radius * 0.53f *
                (style.iconSize / std::max(style.slotSize, 1.0f));

            const ImVec2 iconMin(
                position.x - iconRadius,
                position.y - iconRadius
            );

            const ImVec2 iconMax(
                position.x + iconRadius,
                position.y + iconRadius
            );

            draw->AddImage(
                reinterpret_cast<ImTextureID>(icon),
                iconMin,
                iconMax,
                ImVec2(0.0f, 0.0f),
                ImVec2(1.0f, 1.0f),
                MakeGameplayIconColor(g_settingsDrag.item, 235, 1.0f, true,
                    g_settingsDrag.sourceSide)
            );
        }

        // Mantém o mesmo indicador de Quick Draw do slot enquanto a instância
        // está sendo arrastada, sem reiniciar sua órbita nem sua cauda.
        DrawSettingsDragQuickDrawIndicator(draw, position, radius);
    }

    //=======================

    static int g_selectedTopIndex    = 0;
    static int g_selectedBottomIndex = 0;
    static int g_selectedSideIndex   = 0;

    enum class TopBottomSelectionMode
    {
        Mouse,
        Scroll
    };

    

    //globais para bolinhas excedentes no radial
    constexpr float SIDE_OVERFLOW_RADIUS_STEP = 42.0f;

    constexpr float SIDE_OVERFLOW_ITEM_RADIUS = 12.0f;

    // Distância angular entre cada bolinha da cauda.
    constexpr float SIDE_OVERFLOW_ANGLE_STEP =
        12.0f * (PI / 180.0f);

    //nós vamos querer obrigar o item a continuar na direção da rotação
    // e não simplesmente escolher o menor caminho.
    //Isso depende de sabermos qual foi o último sentido do wheel.

    // vars internas pro menu
    bool g_imguiInitialized = false;

    bool g_lastGState = false;


    TopBottomSelectionMode g_topSelectionMode =
        TopBottomSelectionMode::Mouse;

    TopBottomSelectionMode g_bottomSelectionMode =
        TopBottomSelectionMode::Mouse;

    ImVec2 g_lastTopBottomMousePos{ 0.0f, 0.0f };
    bool g_topBottomMouseInitialized = false;

    int g_topSelectedIndex = 0;
    int g_bottomSelectedIndex = 0;

    bool g_topHasSelection = false;
    bool g_bottomHasSelection = false;

    // Top/Bottom não representa uma mão por si só. Guardamos o último
    // sentido do scroll para decidir a mão quando não houver clique direto.
    static bool g_topBottomLastScrollEquipLeft = false;

    enum class TopBottomHandOverride
    {
        None,
        Left,
        Right
    };

    static TopBottomHandOverride g_topBottomHandOverride =
        TopBottomHandOverride::None;

    static void ClearTopBottomGamepadSelection(RadialSide side)
    {
        if (side == RadialSide::Top)
            g_topHasSelection = false;
        else if (side == RadialSide::Bottom)
            g_bottomHasSelection = false;
    }

    // Movimento visual causado por um passo do scroll.
    // 0 = parado.
    // +1 / -1 = ainda deslocando um slot.
    float g_topScrollAnim = 0.0f;
    float g_bottomScrollAnim = 0.0f;

    struct PendingWeaponSwitch
    {
        bool active = false;

        RE::ActorHandle actor;
        RE::FormID weaponID = 0;

        bool targetLeft = false;
        bool sourceLeft = false;
        bool unequipOnly = false;

        int framesWaiting = 0;
        int attempts = 0;

        // Momento em que a operação começou.
        std::chrono::steady_clock::time_point startTime{};
    };

    static PendingWeaponSwitch g_pendingWeaponSwitch;

    struct RadialParticle
    {
        ImVec2 position;
        ImVec2 velocity;

        float life = 0.0f;
        float maxLife = 0.0f;
        float radius = 1.0f;
    };

    static std::vector<RadialParticle> g_radialParticles;
    constexpr std::size_t STARDUST_MAX_PARTICLES = 220;

    static bool IsInventoryDragVisualActive()
    {
        return g_radialMode == RadialMode::Inventory &&
            g_showWindow &&
            g_draggedInventoryItem != nullptr &&
            g_inventoryOverflowMorphActive;
    }

    // AJUSTES DO PÓ ESTELAR -----------------------------------------
    constexpr int STARDUST_MAX_PER_ITEM_FRAME = 3;
    constexpr float STARDUST_TRAIL_FORCE = 0.055f;
    constexpr float STARDUST_MAX_ALPHA = 185.0f;
    constexpr float STARDUST_LIFE_MIN = 0.30f;
    constexpr float STARDUST_LIFE_RANGE = 0.28f;
    constexpr float STARDUST_RADIUS_MIN = 0.80f;
    constexpr float STARDUST_RADIUS_RANGE = 1.45f;

    static void EmitRadialParticles(
        const ImVec2& position,
        const ImVec2& velocity,
        int particleCount)
    {
        if (particleCount <= 0)
            return;

        if (g_radialParticles.size() >= STARDUST_MAX_PARTICLES)
            return;
        particleCount = std::min(
            particleCount,
            static_cast<int>(STARDUST_MAX_PARTICLES - g_radialParticles.size()));

        const float speed = std::sqrt(
            velocity.x * velocity.x + velocity.y * velocity.y);
        const ImVec2 direction = speed > 0.001f
            ? ImVec2(velocity.x / speed, velocity.y / speed)
            : ImVec2(0.0f, 0.0f);
        const ImVec2 perpendicular(-direction.y, direction.x);

        for (int i = 0; i < particleCount; ++i)
        {
            RadialParticle p;
            const float random01 = static_cast<float>(std::rand()) /
                static_cast<float>(RAND_MAX);
            const float lateral = (random01 - 0.5f) * 10.0f;
            const float behind = 3.0f +
                (static_cast<float>(std::rand()) / RAND_MAX) * 7.0f;
            p.position = ImVec2(
                position.x - direction.x * behind + perpendicular.x * lateral,
                position.y - direction.y * behind + perpendicular.y * lateral);
            p.velocity = ImVec2(
                -velocity.x * STARDUST_TRAIL_FORCE + perpendicular.x * lateral,
                -velocity.y * STARDUST_TRAIL_FORCE + perpendicular.y * lateral);
            p.maxLife = STARDUST_LIFE_MIN + random01 * STARDUST_LIFE_RANGE;
            p.life = p.maxLife;
            p.radius = STARDUST_RADIUS_MIN + random01 * STARDUST_RADIUS_RANGE;
            g_radialParticles.push_back(p);
        }
    }

    static void EmitItemStardust(
        RadialItemAnimation& anim, const ImVec2& position,
        float deltaTime, RadialSide side)
    {
        if (!Config::g_stardustEnabled)
            return;

        const bool settingsOpen = SettingsMenu::WheelSettingsMenu::IsOpen();
        if (g_radialMode != RadialMode::Gameplay &&
            g_radialMode != RadialMode::Inventory && !settingsOpen)
            return;

        const std::size_t stream = side == RadialSide::Top ? 0u :
            side == RadialSide::Bottom ? 1u :
            side == RadialSide::Left ? 2u : 3u;
        if (!anim.stardustInitialized[stream])
        {
            anim.stardustLastPosition[stream] = position;
            anim.stardustInitialized[stream] = true;
            return;
        }

        const float dt = std::clamp(deltaTime, 1.0f / 240.0f, 1.0f / 30.0f);
        const ImVec2 previous = anim.stardustLastPosition[stream];
        anim.stardustLastPosition[stream] = position;
        ImVec2 velocity(
            (position.x - previous.x) / dt,
            (position.y - previous.y) / dt);
        float speed = std::sqrt(
            velocity.x * velocity.x + velocity.y * velocity.y);

        // Inventory e WheelSettings só soltam partículas quando os passos de
        // scroll chegam em sequência. Um único movimento lento não acumula
        // energia suficiente para ativar o efeito.
        if ((g_radialMode == RadialMode::Inventory || settingsOpen) &&
            g_sideScrollStardustEnergy < 1.15f)
        {
            anim.stardustEmission[stream] = 0.0f;
            return;
        }

        // O scroll rápido pode deslocar um item bastante entre dois frames.
        // Limitar a velocidade preserva a trilha sem converter esse salto em
        // uma explosão de partículas (nem descartar justamente o giro rápido).
        constexpr float maxTrailSpeed = 1500.0f;
        if (speed > maxTrailSpeed)
        {
            const float scale = maxTrailSpeed / speed;
            velocity.x *= scale;
            velocity.y *= scale;
            speed = maxTrailSpeed;
        }
        const float moveX = position.x - previous.x;
        const float moveY = position.y - previous.y;
        if (moveX * moveX + moveY * moveY < 0.015f)
        {
            anim.stardustEmission[stream] = std::max(
                0.0f, anim.stardustEmission[stream] - dt * 2.0f);
            return;
        }

        // Qualquer deslocamento real deixa um pó discreto. A emissão não
        // participa do cálculo da animação e não depende de um limiar de
        // velocidade, facilitando validar o efeito também no Inventory.
        const float baseEmission =
            (g_radialMode == RadialMode::Inventory || settingsOpen)
            ? 1.15f
            : 0.42f;
        anim.stardustEmission[stream] += std::clamp(
            baseEmission + speed / 1600.0f,
            baseEmission,
            (g_radialMode == RadialMode::Inventory || settingsOpen)
                ? 2.25f : 0.95f);
        const int count = std::min(
            STARDUST_MAX_PER_ITEM_FRAME,
            static_cast<int>(anim.stardustEmission[stream]));
        if (count > 0)
        {
            anim.stardustEmission[stream] -= static_cast<float>(count);
            EmitRadialParticles(position, velocity, count);
        }
    }

    static void UpdateAndDrawRadialParticles(
        ImDrawList* draw,
        float deltaTime,
        float alpha)
    {
        for (auto& p : g_radialParticles)
        {
            p.life -= deltaTime;

            if (p.life <= 0.0f)
                continue;

            // Movimento.
            p.position.x += p.velocity.x * deltaTime;
            p.position.y += p.velocity.y * deltaTime;

            // Desaceleração suave.
            const float drag =
                std::exp(-3.5f * deltaTime);

            p.velocity.x *= drag;
            p.velocity.y *= drag;


            const float lifeT =
                std::clamp(
                    p.life / p.maxLife,
                    0.0f,
                    1.0f
                );

            // Some suavemente.
            const float particleAlpha =
                lifeT * lifeT *
                std::clamp(Config::g_stardustFade * 0.01f, 0.0f, 1.0f);

            // Também encolhe enquanto desaparece.
            const float radius =
                p.radius *
                (0.35f + 0.65f * lifeT);


            draw->AddCircleFilled(
                p.position,
                radius,
                FadeColor(
                    IM_COL32(
                        255,
                        255,
                        255,
                        static_cast<int>(
                            STARDUST_MAX_ALPHA * particleAlpha
                        )
                    ),
                    alpha
                ),
                8
            );
        }


        std::erase_if(
            g_radialParticles,
            [](const RadialParticle& p)
            {
                return p.life <= 0.0f;
            }
        );
    }

    

    static RadialItemAnimation& GetOrCreateAnim(
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        RadialAnimKey key{
            form,
            hasUniqueID ? uniqueID : std::uint16_t{0},
            hasUniqueID
        };

        auto it = g_itemAnimCache.find(key);

        if (it != g_itemAnimCache.end())
            return it->second;

        auto [inserted, ok] =
            g_itemAnimCache.emplace(
                key,
                RadialItemAnimation{}
            );

        return inserted->second;
    }

    static void QueueSettingsSideScrollStep(int direction)
    {
        if (direction == 0)
            return;

        // Uma fila limitada preserva o gesto rápido sem permitir que uma roda
        // física muito sensível deixe uma sequência antiga rodando por tempo
        // indefinido depois que o jogador já parou.
        constexpr std::size_t kMaxQueuedSideScrollSteps = 24;
        if (g_settingsSideScrollQueue.size() >= kMaxQueuedSideScrollSteps)
            return;

        g_settingsSideScrollQueue.push_back(direction < 0 ? -1 : 1);
    }

    static bool IsSettingsSideScrollSettled()
    {
        const bool customCircuit = Config::g_customRadial &&
            Track::HasValidSavedLayout() &&
            static_cast<int>(g_sideItems.size()) > GetSideVisibleLimit();

        for (const RadialItem& item : g_sideItems)
        {
            if (!item.form)
                continue;

            RadialItemAnimation& anim = GetOrCreateAnim(
                item.form, item.uniqueID, item.hasUniqueID);
            if (anim.settingsDropSettling || anim.settingsCustomDropEntering)
                return false;

            if (customCircuit)
            {
                const TrackMovement::State& state = anim.customTrackAnimation;
                if (state.initialized && state.progress < 0.995f)
                    return false;
                continue;
            }

            // O Legacy não possui um contador de progresso. A posição e as
            // velocidades do seu estado polar/cartesiano informam se o slot
            // já terminou a transição antes de aceitarmos o próximo notch.
            const RadialAnimation::State& state = anim.gameplayRadialAnimation;
            if (!state.initialized)
                continue;
            const float dx = state.lastTarget.x - state.position.x;
            const float dy = state.lastTarget.y - state.position.y;
            const float distanceSq = dx * dx + dy * dy;
            const float velocitySq = state.velocity.x * state.velocity.x +
                state.velocity.y * state.velocity.y;
            if (distanceSq > 1.0f || velocitySq > 4.0f ||
                std::abs(state.angularVelocity) > 0.015f ||
                std::abs(state.radialVelocity) > 0.50f)
            {
                return false;
            }
        }
        return true;
    }

    static void ProcessSettingsSideScrollQueue()
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
        {
            g_settingsSideScrollQueue.clear();
            g_settingsSideScrollAnimationDirection = 0;
            return;
        }

        if (g_settingsTopologySettleRemaining > 0.0f)
        {
            return;
        }

        if (g_settingsSideScrollQueue.empty())
        {
            // Após o último passo, devolvemos as mudanças normais de layout
            // ao modo shortest-path. O arco dirigido só existe enquanto a
            // sequência de scroll ainda está visualmente em andamento.
            if (IsSettingsSideScrollSettled())
                g_settingsSideScrollAnimationDirection = 0;
            return;
        }

        const int direction = g_settingsSideScrollQueue.front();
        g_settingsSideScrollQueue.pop_front();
        g_settingsSideScrollAnimationDirection = direction;
        ScrollSideRadial(direction);
    }

    static ImVec2 GetSettingsRepulsionPosition(
    RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID,
        const ImVec2& originalPosition,
        float deltaTime)
    {
        if (!form)
            return originalPosition;

        RadialItemAnimation& anim =
            GetOrCreateAnim(
                form,
                uniqueID,
                hasUniqueID
            );

        // ============================================================
        // CONFIGURAÇÕES DA REPULSÃO
        // ============================================================

        constexpr float influenceRadius = 125.0f;
        constexpr float maxRepulsion = 35.0f;
        constexpr float animationSpeed = 14.0f;

        ImVec2 targetOffset(0.0f, 0.0f);

        // ============================================================
        // CALCULA REPULSÃO
        // ============================================================

        if (g_settingsDrag.active &&
            g_settingsDrag.form != form)
        {
            const ImVec2 dragPos =
                g_settingsDrag.position;

            const float dx =
                originalPosition.x - dragPos.x;

            const float dy =
                originalPosition.y - dragPos.y;

            const float distance =
                std::sqrt(dx * dx + dy * dy);

            if (distance > 0.001f &&
                distance < influenceRadius)
            {
                float influence =
                    1.0f - distance / influenceRadius;

                // Smoothstep.
                influence =
                    influence * influence *
                    (3.0f - 2.0f * influence);

                const float force =
                    influence * maxRepulsion;

                targetOffset.x =
                    (dx / distance) * force;

                targetOffset.y =
                    (dy / distance) * force;
            }
        }

        // ============================================================
        // ANIMAÇÃO SUAVE
        // ============================================================

        const float dt =
            std::clamp(deltaTime, 0.0f, 0.05f);

        const float factor =
            1.0f - std::exp(
                -animationSpeed * dt
            );

        anim.settingsRepulsion.x +=
            (targetOffset.x -
                anim.settingsRepulsion.x) * factor;

        anim.settingsRepulsion.y +=
            (targetOffset.y -
                anim.settingsRepulsion.y) * factor;

        // ============================================================
        // POSIÇÃO FINAL
        // ============================================================

        return ImVec2(
            originalPosition.x +
                anim.settingsRepulsion.x,

            originalPosition.y +
                anim.settingsRepulsion.y
        );
    }
        

    // Avança a interpolação de hover de um item em direção ao alvo
    // (1.0 se está em hover, 0.0 caso contrário), com suavização
    // baseada em deltaTime para ficar consistente em qualquer FPS.
    static float StepHoverAnimation(RadialItemAnimation& anim, bool hovered, float deltaTime)
    {
        const float target = hovered ? 1.0f : 0.0f;

        if (!anim.initialized)
        {
            anim.hoverT = target;
            anim.initialized = true;
        }
        else
        {
            const float factor = 1.0f - std::exp(-ITEM_TRANSITION_SPEED * deltaTime);
            anim.hoverT += (target - anim.hoverT) * factor;
        }

        if (std::abs(anim.hoverT - target) < 0.001f)
            anim.hoverT = target;

        return anim.hoverT;
    }

    constexpr float ITEM_POSITION_TRANSITION_SPEED = 12.0f;

    // Move anim.currentPos suavemente em direção a "target". Usado por
    // todos os menus (esquerda/direita/cima/baixo) para que, quando a
    // lista de itens muda (adiciona/remove/reordena), os itens andem
    // até a nova posição em vez de teleportar.
    static ImVec2 StepPositionAnimation(
        RadialItemAnimation& anim,
        const ImVec2& target,
        float deltaTime)
    {
        // Evita explosão da física em frame hitch.
        deltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 30.0f);

        if (!anim.posInitialized)
        {
            anim.currentPos = target;
            anim.previousPos = target;
            anim.velocity = ImVec2(0.0f, 0.0f);
            anim.posInitialized = true;

            return anim.currentPos;
        }

        anim.previousPos = anim.currentPos;

        // ============================================================
        // SPRING
        //
        // stiffness:
        //     força puxando o item para o destino.
        //
        // damping:
        //     freio da mola.
        //
        // Menos damping = mais overshoot.
        // ============================================================

        constexpr float stiffness = 315.0f;
        constexpr float damping = 25.0f;

        const float dx = target.x - anim.currentPos.x;
        const float dy = target.y - anim.currentPos.y;

        const float accelerationX =
            dx * stiffness - anim.velocity.x * damping;

        const float accelerationY =
            dy * stiffness - anim.velocity.y * damping;

        anim.velocity.x += accelerationX * deltaTime;
        anim.velocity.y += accelerationY * deltaTime;

        anim.currentPos.x += anim.velocity.x * deltaTime;
        anim.currentPos.y += anim.velocity.y * deltaTime;

        // ============================================================
        // SNAP SOMENTE QUANDO REALMENTE PAROU
        // ============================================================

        const float distanceSq =
            dx * dx +
            dy * dy;

        const float velocitySq =
            anim.velocity.x * anim.velocity.x +
            anim.velocity.y * anim.velocity.y;

        if (distanceSq < 0.01f &&
            velocitySq < 0.01f)
        {
            anim.currentPos = target;
            anim.velocity = ImVec2(0.0f, 0.0f);
        }

        return anim.currentPos;
    }

    // Gameplay usa interpolação contínua. Inventory anima somente as
    // alterações reais da lista, nos pontos de desenho de cada radial. Manter
    // a mola global ativa no Inventory reutilizava posições de outros modos
    // (inclusive o centro dos previews) quando o menu era aberto.
    static ImVec2 GetRadialItemAnimatedPosition(
        RadialItemAnimation& anim, const ImVec2& target, float deltaTime)
    {
        if (g_radialMode == RadialMode::Gameplay)
        {
            return StepPositionAnimation(anim, target, deltaTime);
        }

        // Sincroniza o cache com a posição real sem animar.
        anim.currentPos = target;
        anim.posInitialized = true;
        return target;
    }

    

    struct RadialDropTarget
    {
        RadialSide side = RadialSide::None;
        int slot = -1;

        bool valid = false;
    };

    RadialDropTarget g_inventorySnapTarget;

    
    bool IsInventoryOpen()
    {
        auto* ui = RE::UI::GetSingleton();

        if (!ui)
            return false;

        // BUGFIX: UI::IsItemMenuOpen() cobre InventoryMenu/ContainerMenu/
        // BarterMenu/GiftMenu, mas NÃO o MagicMenu. Por isso, ao tentar
        // pegar um feitiço/grito com o Menu de Magias aberto, o código
        // caía no ramo "Gameplay" em vez de "Inventory" e nada era
        // capturado. Adicionamos o MagicMenu explicitamente aqui.
        return ui->IsItemMenuOpen() ||
               ui->IsMenuOpen(RE::MagicMenu::MENU_NAME);
    }


    static void ResetSettingsScrollCharge()
    {
        // Zera a força acumulada.
        g_settingsCharge = 0.0f;

        // Cancela uma abertura ainda pendente.
        g_settingsOpening = false;
    }

    static void SetGameplayBlurApplied(bool desired)
    {
        if (desired == g_gameplayBlurApplied)
            return;
        Blur::SetTarget(desired);
        g_gameplayBlurApplied = desired;
    }

    void ResetForPreLoadGame()
    {
        // O save pode ser carregado no mesmo processo enquanto o radial ainda
        // está aberto. Não dependemos do fade/outro frame: zeramos o estado
        // visual e o pós-processamento antes de o novo jogo ser aplicado.
        Slowtime::End();
        Blur::Reset();
        g_gameplayBlurApplied = false;

        g_showWindow = false;
        g_globalAlpha = 0.0f;
        g_menuAlpha = 0.0f;
        g_radialSide = RadialSide::None;
        g_radialLocked = false;
        g_radialToggleLocked = false;
        g_ignoreNextGRelease = false;
        ResetRadialLockedOpen();

        spdlog::info("PRELOAD RESET | radial closed and blur cleared");
    }


    // ============================================================
    // G KEY
    // ============================================================

    void OpenRadialMenu()
    {
        g_showWindow = true;

        //BlurController::GetSingleton().SetTargetState(true);
        ResetSettingsScrollCharge();
        //auto* blurManager = RE::UIBlurManager::GetSingleton();
        //if (blurManager)
        //{
        //    blurManager->IncrementBlurCount();
        //}

        // Reseta o estado para começar limpo no centro
        g_radialSide = RadialSide::None;
        g_radialLocked = false;
        g_radialVector = ImVec2(0.0f, 0.0f);

        ImGuiIO& io = ImGui::GetIO();
        // Input can open the radial between render frames, when ImGui still
        // exposes physical pixels. Keep persistent radial state virtual.
        const ImVec2 center = Resolution::GetVirtualCenter();

        g_radialOrigin = center;
        g_lastMousePos = Resolution::ToVirtual(io.MousePos);
        g_radialMouseInitialized = true;
    }

    int GetSideDropInsertionIndex(
        const ImVec2& position,
        const ImVec2& center,
        bool leftSide,
        int currentItemCount)
    {
        // Se não existe nenhum item ainda,
        // o novo item será o primeiro.
        if (currentItemCount <= 0)
            return 0;

        const float dx = position.x - center.x;
        const float dy = position.y - center.y;

        float dropAngle = std::atan2(dy, dx);

        // ============================================================
        // ESPELHA A LÓGICA DO MENU ESQUERDO
        // ============================================================

        if (leftSide)
        {
            dropAngle = PI - dropAngle;
        }

        // ============================================================
        // NORMALIZA PARA 0 .. 2PI
        // ============================================================

        dropAngle += PI * 0.5f;

        while (dropAngle < 0.0f)
            dropAngle += PI * 2.0f;

        while (dropAngle >= PI * 2.0f)
            dropAngle -= PI * 2.0f;

        // ============================================================
        // AGORA TEMOS currentItemCount ITENS.
        // ============================================================

        const float itemStep = (PI * 2.0f) / static_cast<float>(currentItemCount);

        int insertionIndex = static_cast<int>(std::floor(dropAngle / itemStep + 0.5f));

        // Permite inserir no final
        insertionIndex = std::clamp(insertionIndex, 0, currentItemCount);

        return insertionIndex;
    }

    static float GetDirectedAngularDelta(
        float startAngle,
        float targetAngle,
        int direction)
    {
        float delta =
            std::fmod(
                targetAngle - startAngle,
                2.0f * PI
            );

        if (direction > 0)
        {
            // Sempre percorre aumentando o ângulo.
            if (delta < 0.0f)
                delta += 2.0f * PI;
        }
        else
        {
            // Sempre percorre diminuindo o ângulo.
            if (delta > 0.0f)
                delta -= 2.0f * PI;
        }

        return delta;
    }

    static ImVec2 GetSideWrapAnimatedPosition(
        RadialItemAnimation& anim,
        const ImVec2& targetPos,
        const ImVec2& center,
        float deltaTime)
    {
        if (!anim.sideWrapActive)
            return targetPos;

        // ============================================================
        // CONGELA O DESTINO SOMENTE NO PRIMEIRO FRAME
        // ============================================================

        if (!anim.sideWrapTargetInitialized)
        {
            const float dx =
                targetPos.x - center.x;

            const float dy =
                targetPos.y - center.y;

            anim.sideWrapTargetAngle =
                std::atan2(dy, dx);

            anim.sideWrapTargetRadius =
                std::sqrt(
                    dx * dx +
                    dy * dy
                );

            anim.sideWrapTargetInitialized = true;
        }

        // ============================================================
        // PROGRESSO
        // ============================================================

        constexpr float wrapSpeed = 7.0f;

        anim.sideWrapT +=
            deltaTime * wrapSpeed;

        const float t =
            std::clamp(
                anim.sideWrapT,
                0.0f,
                1.0f
            );

        const float smoothT =
            t * t * (3.0f - 2.0f * t);

        // ============================================================
        // TRAJETÓRIA CIRCULAR
        // ============================================================

        const float angularDelta =
            GetDirectedAngularDelta(
                anim.sideWrapStartAngle,
                anim.sideWrapTargetAngle,
                anim.sideWrapDirection
            );

        const float angle =
            anim.sideWrapStartAngle +
            angularDelta * smoothT;

        const float currentRadius =
            anim.sideWrapStartRadius +
            (
                anim.sideWrapTargetRadius -
                anim.sideWrapStartRadius
            ) *
            smoothT;

        ImVec2 pos(
            center.x +
                std::cos(angle) *
                currentRadius,

            center.y +
                std::sin(angle) *
                currentRadius
        );

        anim.currentPos = pos;
        anim.previousPos = pos;
        anim.velocity = ImVec2(0.0f, 0.0f);
        anim.posInitialized = true;

        // ============================================================
        // FINAL
        // ============================================================

        if (t >= 1.0f)
        {
            anim.sideWrapActive = false;
            anim.sideWrapT = 0.0f;
            anim.sideWrapDirection = 0;
            anim.sideWrapTargetInitialized = false;

            // NÃO joga para targetPos.
            // A mola normal continua daqui no próximo frame.
            return anim.currentPos;
        }

        return pos;
    }

    int GetSideRadialItem(
        const ImVec2& mouse,
        const ImVec2& center,
        bool leftSide,
        int itemCount,
        float innerRadius)
    {
        if (itemCount <= 0)
            return -1;

        float dx = mouse.x - center.x;
        float dy = mouse.y - center.y;

        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < innerRadius)
            return -1;

        if (Config::g_customRadial && Track::HasValidSavedLayout())
        {
            const float radius = GetSideRadialRadius();
            const float mouseLength = std::max(distance, 0.001f);
            const ImVec2 mouseDirection(dx / mouseLength, dy / mouseLength);
            int nearestIndex = -1;
            float nearestAlignment = -FLT_MAX;
            const int totalItems = static_cast<int>(g_sideItems.size());
            const auto slots = Track::CircuitSlots(center, radius, leftSide,
                itemCount, std::max(0, totalItems - itemCount),
                static_cast<RadialShape::Style>(std::clamp(
                    Config::g_radialShape, 0, RadialShape::Count() - 1)));
            int mainIndex = 0;
            for (const auto& slot : slots)
            {
                if (!slot.main) continue;
                const ImVec2 target = slot.position;
                const float targetX = target.x - center.x;
                const float targetY = target.y - center.y;
                const float targetLength = std::sqrt(
                    targetX * targetX + targetY * targetY);
                if (targetLength <= 0.001f)
                    continue;
                const float alignment = mouseDirection.x * targetX / targetLength +
                    mouseDirection.y * targetY / targetLength;
                if (alignment > nearestAlignment)
                {
                    nearestAlignment = alignment;
                    nearestIndex = mainIndex;
                }
                ++mainIndex;
            }
            return nearestIndex;
        }

        float mouseAngle = std::atan2(dy, dx);

        // ============================================================
        // Transformamos o mouse para o mesmo sistema angular
        // usado pelos itens.
        // ============================================================

        float relative = mouseAngle + PI / 2.0f;

        if (leftSide)
        {
            relative = -relative;
        }

        // Normaliza
        while (relative < 0.0f)
            relative += 2.0f * PI;

        while (relative >= 2.0f * PI)
            relative -= 2.0f * PI;

        float sector = 2.0f * PI / static_cast<float>(itemCount);

        int index = static_cast<int>(std::floor((relative + sector * 0.5f) / sector));

        return index % itemCount;
    }

    int GetTopRadialItem(
        const ImVec2& mouse,
        const ImVec2& center,
        int itemCount,
        float innerRadius)
    {
        if (itemCount <= 0)
            return -1;

        float dx = mouse.x - center.x;
        float dy = mouse.y - center.y;

        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < innerRadius)
            return -1;

        // Usando std::abs(dy), o ângulo depende apenas do eixo X (esquerda/direita)
        // e da magnitude vertical, ignorando se o mouse está acima ou abaixo do centro.
        float angle = std::atan2(dx, std::abs(dy));

        // Ângulo máximo da lua em radianos (70 graus)
        constexpr float maxAngle = 70.0f * 3.14159265f / 180.0f;

        // Fora do leque/faixa de seleção da lua
        if (angle < -maxAngle || angle > maxAngle)
        {
            return -1;
        }

        // Normaliza de 0.0 até 1.0 dentro do arco
        float normalized = (angle + maxAngle) / (2.0f * maxAngle);

        int index = static_cast<int>(normalized * static_cast<float>(itemCount));

        return std::clamp(index, 0, itemCount - 1);
    }

    int GetBottomRadialItem(
        const ImVec2& mouse,
        const ImVec2& center,
        int itemCount,
        float innerRadius)
    {
        // A lógica é exatamente a mesma: focada no X e indiferente ao Y positivo/negativo.
        return GetTopRadialItem(mouse, center, itemCount, innerRadius);
    }

    int GetNearestSideRadialSlot(
        const ImVec2& position,
        const ImVec2& center,
        bool leftSide,
        int itemCount)
    {
        if (itemCount <= 0)
            return -1;

        const float radius = GetSideRadialRadius();

        int nearestSlot = -1;

        float nearestDistanceSq = FLT_MAX;

        for (int i = 0; i < itemCount; ++i)
        {
            float angle =
                (-PI / 2.0f) +
                (2.0f * PI * static_cast<float>(i) / static_cast<float>(itemCount));

            if (leftSide)
                angle = PI - angle;

            ImVec2 slotPosition(
                center.x + std::cos(angle) * radius,
                center.y + std::sin(angle) * radius
            );

            const float dx = position.x - slotPosition.x;
            const float dy = position.y - slotPosition.y;

            const float distanceSq = dx * dx + dy * dy;

            if (distanceSq < nearestDistanceSq)
            {
                nearestDistanceSq = distanceSq;
                nearestSlot = i;
            }
        }

        return nearestSlot;
    }

    ImVec2 GetTopBottomCurvePoint(const ImVec2& center, float t, bool isTop)
    {
        ImGuiIO& io = ImGui::GetIO();

        const float width = GetTopBottomCurveWidth(isTop);
        const float curveHeight = 65.0f;

        const float startX = center.x - width * 0.5f;
        const float endX = center.x + width * 0.5f;

        const float curve = 4.0f * t * (1.0f - t);
        const float x = startX + (endX - startX) * t;
        const float y = isTop ? (center.y + curve * curveHeight) : (center.y - curve * curveHeight);

        return ImVec2(x, y);
    }

    

    bool IsInsideTopBottomDropArea(
        const ImVec2& position,
        const ImVec2& center,
        bool isTop,
        float& outT)
    {
        ImGuiIO& io = ImGui::GetIO();

        const float width = GetTopBottomCurveWidth(isTop);

        const float startX = center.x - width * 0.5f;
        const float endX = center.x + width * 0.5f;

        constexpr float horizontalPadding = 70.0f;
        constexpr float verticalPadding = 100.0f;

        if (position.x < startX - horizontalPadding || position.x > endX + horizontalPadding)
            return false;

        float t = (position.x - startX) / (endX - startX);
        t = std::clamp(t, 0.0f, 1.0f);

        const ImVec2 curvePoint = GetTopBottomCurvePoint(center, t, isTop);

        if (std::abs(position.y - curvePoint.y) > verticalPadding)
            return false;

        outT = t;
        return true;
    }

    bool IsInsideSideRadialDropArea(
        const ImVec2& position,
        const ImVec2& center)
    {
        const float radialRadius = GetSideRadialRadius();

        // Área extra para facilitar o drop
        constexpr float dropPadding = 90.0f;

        const float maxRadius = radialRadius + dropPadding;

        const float dx = position.x - center.x;
        const float dy = position.y - center.y;

        const float distanceSq = dx * dx + dy * dy;

        return distanceSq <= maxRadius * maxRadius;
    }

    static int GetTopBottomVisibleLimit(RadialSide side)
    {
        return std::clamp(static_cast<int>(std::lround(
            side == RadialSide::Top ? Config::g_topItemStyle.quantity :
                Config::g_bottomItemStyle.quantity)), 3, 50);
    }

    //Centraliza logica dos radiais para sincronizar posiçao de snap e drag 
    static float GetTopBottomCenteredItemT(
        int visibleIndex,
        int visibleCount,
        int visibleLimit)
    {
        if (visibleCount <= 1)
            return 0.5f;

        const float fullSpacing =
            1.0f / static_cast<float>(std::max(visibleLimit - 1, 1));

        const float centerIndex =
            (static_cast<float>(visibleCount) - 1.0f) * 0.5f;

        return 0.5f +
            (static_cast<float>(visibleIndex) - centerIndex) *
            fullSpacing;
    }

    static int GetTopBottomDropInsertionIndex(
        float t,
        int totalItems,
        float scrollOffset,
        int visibleLimit)
    {
        if (totalItems <= 0)
            return 0;

        const int visibleCount =
            std::min(
                totalItems,
                visibleLimit
            );

        const int scrollInt =
            static_cast<int>(
                std::round(scrollOffset)
            );

        // ============================================================
        // PROCURA O SLOT VISUAL MAIS PRÓXIMO
        // ============================================================

        int nearestVisibleIndex = 0;
        float nearestDistance = FLT_MAX;

        for (int i = 0; i < visibleCount; ++i)
        {
            const float itemT =
                GetTopBottomCenteredItemT(
                    i,
                    visibleCount,
                    visibleLimit
                );

            const float distance =
                std::abs(t - itemT);

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearestVisibleIndex = i;
            }
        }

        int insertIndex =
            scrollInt + nearestVisibleIndex;

        // Decide se está antes ou depois do item.
        const float nearestT =
            GetTopBottomCenteredItemT(
                nearestVisibleIndex,
                visibleCount,
                visibleLimit
            );

        if (t > nearestT)
            ++insertIndex;

        return std::clamp(
            insertIndex,
            0,
            totalItems
        );
    }

    RadialDropTarget GetInventoryDropTarget()
    {
        RadialDropTarget result;

        ImGuiIO& io = ImGui::GetIO();
        const WheelLayout wheelLayout = GetWheelLayout();

        const ImVec2 position = g_inventoryDraggedPosition;

        // No radial custom o alvo real pode estar fora do círculo principal.
        // Procura primeiro o slot físico mais próximo em todo o circuito,
        // incluindo os excedentes, e converte seu ordinal para o índice real
        // da lista. Isso faz o drop respeitar o ponto mostrado ao usuário.
        if (Config::g_customRadial && Track::HasValidSavedLayout() &&
            !g_sideItems.empty())
        {
            float nearestDistanceSq = 105.0f * 105.0f;
            RadialSide nearestSide = RadialSide::None;
            int nearestIndex = -1;
            const int total = static_cast<int>(g_sideItems.size());
            const int mainCount = std::min(total, GetSideVisibleLimit());
            const int overflowCount = total - mainCount;
            const auto shape = static_cast<RadialShape::Style>(std::clamp(
                Config::g_radialShape, 0, RadialShape::Count() - 1));
            const auto inspectSide = [&](RadialSide side, const ImVec2& center,
                                         bool leftSide) {
                const auto slots = Track::CircuitSlots(center,
                    GetSideRadialRadius(), leftSide, mainCount,
                    overflowCount, shape);
                for (const auto& slot : slots)
                {
                    const float dx = position.x - slot.position.x;
                    const float dy = position.y - slot.position.y;
                    const float distanceSq = dx * dx + dy * dy;
                    if (distanceSq >= nearestDistanceSq) continue;
                    nearestDistanceSq = distanceSq;
                    nearestSide = side;
                    nearestIndex = WrapSideIndex(
                        g_sideScrollOffset + slot.ordinal, total);
                }
            };
            inspectSide(RadialSide::Left, wheelLayout.leftRadial, true);
            inspectSide(RadialSide::Right, wheelLayout.rightRadial, false);
            if (nearestIndex >= 0)
            {
                result.side = nearestSide;
                result.slot = nearestIndex;
                result.valid = true;
                return result;
            }
        }

        // ============================================================
        // LEFT
        // ============================================================
        {
            const ImVec2 center = wheelLayout.leftRadial;

            if (IsInsideSideRadialDropArea(position, center))
            {
                result.side = RadialSide::Left;

                result.slot = GetSideDropInsertionIndex(
                    position,
                    center,
                    true,
                    static_cast<int>(g_sideItems.size())
                );

                result.valid = result.slot >= 0;

                return result;
            }
        }

        // ============================================================
        // RIGHT
        // ============================================================
        {
            const ImVec2 center = wheelLayout.rightRadial;

            if (IsInsideSideRadialDropArea(position, center))
            {
                result.side = RadialSide::Right;

                result.slot = GetSideDropInsertionIndex(
                    position,
                    center,
                    false,
                    static_cast<int>(g_sideItems.size())
                );

                result.valid = result.slot >= 0;

                return result;
            }
        }

        // ============================================================
        // TOP
        // ============================================================
        {
            const ImVec2 center = wheelLayout.topRadial;

            float t = 0.0f;

            if (IsInsideTopBottomDropArea(position, center, true, t))
            {
                const int totalItems = static_cast<int>(g_topItems.size());
                const int visibleCount = std::min(std::max(totalItems, 1),
                    GetTopBottomVisibleLimit(RadialSide::Top));

                const int insertIndex =
                    GetTopBottomDropInsertionIndex(
                        t,
                        totalItems,
                        g_topScrollOffset,
                        GetTopBottomVisibleLimit(RadialSide::Top)
                    );

                result.side = RadialSide::Top;
                result.slot = insertIndex;
                result.valid = true;

                return result;
            }
        }

        // ============================================================
        // BOTTOM
        // ============================================================
        {
            const ImVec2 center = wheelLayout.bottomRadial;

            float t = 0.0f;

            if (IsInsideTopBottomDropArea(position, center, false, t))
            {
                const int totalItems = static_cast<int>(g_bottomItems.size());
                const int visibleCount = std::min(std::max(totalItems, 1),
                    GetTopBottomVisibleLimit(RadialSide::Bottom));

                const int insertIndex =
                    GetTopBottomDropInsertionIndex(
                        t,
                        totalItems,
                        g_scrollOffset,
                        GetTopBottomVisibleLimit(RadialSide::Bottom)
                    );

                result.side = RadialSide::Bottom;
                result.slot = insertIndex;
                result.valid = true;

                return result;
            }
        }

        return result;
    }

    bool IsDraggedItemInsideRadial()
    {
        return GetInventoryDropTarget().valid;
    }

    // ============================================================
    // TOP / BOTTOM: LAYOUT COMPARTILHADO
    //
    // BUGFIX: antes, a posição de um item de cima/baixo era calculada
    // por um índice ABSOLUTO fixo em uma "régua" de 13 posições,
    // então com poucos itens (1, 2, 3...) eles ficavam grudados na
    // ponta esquerda da régua em vez de centralizados na tela. Além
    // disso, não existia nenhuma detecção de "soltar item aqui" para
    // cima/baixo, então dava pra tentar colocar item lá e nada
    // acontecia. As funções abaixo resolvem os dois problemas:
    // os itens visíveis são distribuídos igualmente ao longo do arco
    // (igual ao menu lateral), e existe uma área de drop de verdade.
    // ============================================================

    

    float GetTopBottomItemT(int visibleIndex, int visibleCount)
    {
        if (visibleCount <= 1)
            return 0.5f;

        return static_cast<float>(visibleIndex) / static_cast<float>(visibleCount - 1);
    }


    
    

    ImVec2 GetSideItemPosition(
        const ImVec2& center,
        int itemIndex,
        int itemCount,
        bool leftSide)
    {
        if (itemCount <= 0)
            return center;

        float angle =
            (-PI / 2.0f) +
            (PI * 2.0f * static_cast<float>(itemIndex) / static_cast<float>(itemCount));

        if (leftSide)
        {
            angle = PI - angle;
        }

        const float radius = GetSideRadialRadius();

        return ImVec2(
            center.x + std::cos(angle) * radius,
            center.y + std::sin(angle) * radius
        );
    }

    ImVec2 GetTopSlotPosition(int slot)
    {
        ImGuiIO& io = ImGui::GetIO();

        const ImVec2 center = GetWheelLayout().topRadial;

        const int totalItems =
            static_cast<int>(g_topItems.size());

        const int visibleCount =
            std::min(
                std::max(totalItems, 1),
                GetTopBottomVisibleLimit(RadialSide::Top)
            );

        const int scrollInt =
            static_cast<int>(
                std::round(g_topScrollOffset)
            );

        const int visibleIndex =
            std::clamp(
                slot - scrollInt,
                0,
                visibleCount - 1
            );

        const float t =
            GetTopBottomCenteredItemT(
                visibleIndex,
                visibleCount,
                GetTopBottomVisibleLimit(RadialSide::Top)
            );

        return GetTopBottomCurvePoint(
            center,
            t,
            true
        );
    }


    ImVec2 GetBottomSlotPosition(int slot)
    {
        ImGuiIO& io = ImGui::GetIO();

        const ImVec2 center = GetWheelLayout().bottomRadial;

        const int totalItems =
            static_cast<int>(g_bottomItems.size());

        const int visibleCount =
            std::min(
                std::max(totalItems, 1),
                GetTopBottomVisibleLimit(RadialSide::Bottom)
            );

        const int scrollInt =
            static_cast<int>(
                std::round(g_scrollOffset)
            );

        const int visibleIndex =
            std::clamp(
                slot - scrollInt,
                0,
                visibleCount - 1
            );

        const float t =
            GetTopBottomCenteredItemT(
                visibleIndex,
                visibleCount,
                GetTopBottomVisibleLimit(RadialSide::Bottom)
            );

        return GetTopBottomCurvePoint(
            center,
            t,
            false
        );
    }

    ImVec2 GetRadialSlotPosition(
        const RadialDropTarget& target,
        int itemIndex)
    {
        ImGuiIO& io = ImGui::GetIO();

        switch (target.side)
        {
        case RadialSide::Left:
        {
            const ImVec2 center = GetWheelLayout().leftRadial;

            if (Config::g_customRadial && Track::HasValidSavedLayout() &&
                !g_sideItems.empty())
            {
                const int total = static_cast<int>(g_sideItems.size());
                const int mainCount = std::min(total, GetSideVisibleLimit());
                const auto slots = Track::CircuitSlots(center,
                    GetSideRadialRadius(), true, mainCount,
                    total - mainCount,
                    static_cast<RadialShape::Style>(std::clamp(
                        Config::g_radialShape, 0, RadialShape::Count() - 1)));
                for (const auto& slot : slots)
                    if (WrapSideIndex(g_sideScrollOffset + slot.ordinal, total) == itemIndex)
                        return slot.position;
            }

            return GetSideItemPosition(
                center,
                itemIndex,
                static_cast<int>(g_sideItems.size()),
                true
            );
        }

        case RadialSide::Right:
        {
            const ImVec2 center = GetWheelLayout().rightRadial;

            if (Config::g_customRadial && Track::HasValidSavedLayout() &&
                !g_sideItems.empty())
            {
                const int total = static_cast<int>(g_sideItems.size());
                const int mainCount = std::min(total, GetSideVisibleLimit());
                const auto slots = Track::CircuitSlots(center,
                    GetSideRadialRadius(), false, mainCount,
                    total - mainCount,
                    static_cast<RadialShape::Style>(std::clamp(
                        Config::g_radialShape, 0, RadialShape::Count() - 1)));
                for (const auto& slot : slots)
                    if (WrapSideIndex(g_sideScrollOffset + slot.ordinal, total) == itemIndex)
                        return slot.position;
            }

            // BUGFIX: o lado direito NÃO é espelhado (leftSide = false).
            // Antes estava fixo em "true", o que fazia o item recém
            // solto animar para uma posição espelhada/errada no menu direito.
            return GetSideItemPosition(
                center,
                itemIndex,
                static_cast<int>(g_sideItems.size()),
                false
            );
        }

        case RadialSide::Top:
            return GetTopSlotPosition(target.slot);

        case RadialSide::Bottom:
            return GetBottomSlotPosition(target.slot);

        default:
            return g_inventoryDraggedPosition;
        }
    }

    static int WrapSideIndex(int index, int count)
    {
        if (count <= 0)
            return 0;

        index %= count;

        if (index < 0)
            index += count;

        return index;
    }

    static int WrapIndex(int index, int count)
    {
        if (count <= 0)
            return 0;

        index %= count;

        if (index < 0)
            index += count;

        return index;
    }
        
    static int GetSideActualIndex(int visibleIndex)
    {
        const int totalItems =
            static_cast<int>(g_sideItems.size());

        if (totalItems <= 0)
            return -1;

        if (visibleIndex < 0 ||
            visibleIndex >= std::min(totalItems, GetSideVisibleLimit()))
        {
            return -1;
        }

        if (Config::g_customRadial && Track::HasValidSavedLayout() &&
            (g_radialSide == RadialSide::Left || g_radialSide == RadialSide::Right))
        {
            const int mainCount = std::min(totalItems, GetSideVisibleLimit());
            const WheelLayout layout = GetWheelLayout();
            const bool leftSide = g_radialSide == RadialSide::Left;
            const ImVec2 center = leftSide ? layout.leftRadial : layout.rightRadial;
            const auto slots = Track::CircuitSlots(center, GetSideRadialRadius(),
                leftSide, mainCount, totalItems - mainCount,
                static_cast<RadialShape::Style>(std::clamp(
                    Config::g_radialShape, 0, RadialShape::Count() - 1)));
            int mainIndex = 0;
            for (const auto& slot : slots)
            {
                if (!slot.main) continue;
                if (mainIndex++ == visibleIndex)
                    return WrapSideIndex(g_sideScrollOffset + slot.ordinal, totalItems);
            }
        }

        return WrapSideIndex(
            g_sideScrollOffset + visibleIndex,
            totalItems
        );
    }

    void StartInventorySnapAnimation(
        const RadialDropTarget& target,
        int itemIndex)
    {
        if (!target.valid)
            return;

        g_inventorySnapStart = g_inventoryDraggedPosition;
        g_inventorySnapEnd = GetRadialSlotPosition(target, itemIndex);
        g_inventorySnapTarget = target;
        g_inventorySnapProgress = 0.0f;
        g_inventorySnapAnimating = true;

        g_inventoryDragMode = InventoryDragMode::SnapAnimating;
    }

    void PlaceDraggedItem(const RadialDropTarget& target)
    {
        if (!target.valid)
            return;

        if (!g_draggedInventoryItem)
            return;

        g_lastInventoryRadialPosition = g_inventoryDraggedPosition;
        g_hasLastInventoryRadialPosition = true;
        g_fastDragZoneActive = false;
        g_fastDragReturningToCursor = false;
        const std::size_t previousSideItemCount = g_sideItems.size();

        auto removeItem = [](
            std::vector<RadialItem>& items,
            RE::TESForm* form,
            std::uint16_t uniqueID,
            bool hasUniqueID)
        {
            items.erase(
                std::remove_if(
                    items.begin(),
                    items.end(),
                    [&](const RadialItem& item)
                    {
                        if (item.form != form)
                            return false;

                        if (hasUniqueID)
                        {
                            return item.hasUniqueID &&
                                item.uniqueID == uniqueID;
                        }

                        return !item.hasUniqueID;
                    }
                ),
                items.end()
            );
        };

        // ============================================================
        // REMOVE O ITEM DE QUALQUER RADIAL ANTERIOR
        // ============================================================

                
        removeItem(
            g_topItems,
            g_draggedInventoryItem,
            g_draggedInventoryUniqueID,
            g_draggedInventoryHasUniqueID
        );

        removeItem(
            g_bottomItems,
            g_draggedInventoryItem,
            g_draggedInventoryUniqueID,
            g_draggedInventoryHasUniqueID
        );

        removeItem(
            g_sideItems,
            g_draggedInventoryItem,
            g_draggedInventoryUniqueID,
            g_draggedInventoryHasUniqueID
        );

        // ============================================================
        // CRIA O ITEM
        // ============================================================

        

        RadialItem newItem;

        newItem.form = g_draggedInventoryItem;

        newItem.uniqueID = g_draggedInventoryUniqueID;

        newItem.hasUniqueID = g_draggedInventoryHasUniqueID;

        newItem.name = g_draggedInventoryItem->GetName();

        newItem.valid = true;

        newItem.icon = ImTextureID(0);

        newItem.slot = -1;

        Logger::GetSingleton().Print(
            "RADIAL INSERT | form={:08X} | uniqueID={} | hasUniqueID={}",
            newItem.form->GetFormID(),
            newItem.uniqueID,
            newItem.hasUniqueID
        );


        // ============================================================
        // ESCOLHE A LISTA
        // ============================================================

        std::vector<RadialItem>* targetList = nullptr;

        switch (target.side)
        {
        case RadialSide::Top:
            targetList = &g_topItems;
            break;

        case RadialSide::Bottom:
            targetList = &g_bottomItems;
            break;

        case RadialSide::Left:
        case RadialSide::Right:
            targetList = &g_sideItems;
            break;

        default:
            return;
        }

        if (!targetList)
            return;

        // ============================================================
        // POSIÇÃO DE INSERÇÃO
        // "posição no vector", não um slot físico.
        // ============================================================

        const int insertIndex = std::clamp(
            target.slot,
            0,
            static_cast<int>(targetList->size())
        );

        // ============================================================
        // INSERE
        // ============================================================

        targetList->insert(targetList->begin() + insertIndex, newItem);

        if (targetList == &g_sideItems ||
            g_sideItems.size() != previousSideItemCount)
        {
            g_sideScrollOffset = g_sideItems.empty()
                ? 0
                : WrapSideIndex(g_sideScrollOffset,
                    static_cast<int>(g_sideItems.size()));
            g_inventoryTopologySettleRemaining = 0.30f;
        }

        // Faz o item real nascer no ponto de soltura. A interpolação do modo
        // Inventory leva o item e os vizinhos aos novos slots suavemente.
        {
            RadialItemAnimation& anim = GetOrCreateAnim(
                newItem.form, newItem.uniqueID, newItem.hasUniqueID);
            anim.currentPos = g_inventoryDraggedPosition;
            anim.previousPos = g_inventoryDraggedPosition;
            anim.velocity = ImVec2(0.0f, 0.0f);
            anim.posInitialized = true;
            anim.sidePolarInitialized = false;
            anim.customTrackAnimation = {};
            anim.inventoryDropSettling = false;
            anim.inventoryDropProgress = 0.0f;
        }

        // Somente uma mudança real na lista do Inventory inicia a acomodação.
        // Ela usa um estado visual independente e nunca altera o controlador
        // persistente do circuito. O item novo continua usando apenas o snap.
        int settlingIndex = 0;
        for (const auto& radialItem : *targetList)
        {
            if (!radialItem.form)
                continue;
            RadialItemAnimation& anim = GetOrCreateAnim(
                radialItem.form, radialItem.uniqueID, radialItem.hasUniqueID);
            const bool insertedItem =
                radialItem.form == newItem.form &&
                radialItem.uniqueID == newItem.uniqueID &&
                radialItem.hasUniqueID == newItem.hasUniqueID;

            if (targetList == &g_sideItems)
            {
                // A inserção muda os ordinais do circuito imediatamente,
                // embora o item arrastado ainda esteja no snap visual. Nunca
                // deixe o próximo giro consumir uma rota calculada antes da
                // mudança da lista.
                anim.sideWrapActive = false;
                anim.sideWrapTargetInitialized = false;
                anim.sideWrapT = 0.0f;
                anim.customTrackAnimation = {};
                anim.gameplayRadialAnimation = {};
                anim.overflowMechanismAnimation = {};
                anim.posInitialized = false;
                anim.velocity = ImVec2(0.0f, 0.0f);
                anim.radialSizeT = settlingIndex < GetSideVisibleLimit()
                    ? 1.0f : 0.0f;
                // Left e Right mostram a mesma lista, mas cada radial guarda
                // sua própria origem. O item recém-inserido continua sendo
                // representado pela animação de snap já existente.
                for (std::size_t side = 0; side < 2; ++side)
                {
                    anim.inventorySideSettling[side] = !insertedItem &&
                        anim.inventorySideInitialized[side];
                    anim.inventorySideProgress[side] = 0.0f;
                    anim.inventorySideStart[side] =
                        anim.inventorySidePosition[side];
                }
                anim.inventoryDropSettling = false;
            }
            else
            {
                anim.inventoryDropSettling =
                    !insertedItem && anim.posInitialized;
                anim.inventoryDropProgress = 0.0f;
                anim.inventoryDropStart = anim.currentPos;
            }
            ++settlingIndex;
        }

        // ============================================================
        // ANIMAÇÃO
        // ============================================================

        StartInventorySnapAnimation(target, insertIndex);

        // ============================================================
        // ATUALIZA ÍNDICES APENAS COMO REFERÊNCIA
        // ============================================================

        for (int i = 0; i < static_cast<int>(targetList->size()); ++i)
        {
            (*targetList)[i].slot = i;
        }

        // ============================================================
        // FINALIZA O DRAG
        //
        // BUGFIX DE SINCRONIZAÇÃO: o código antigo comentava a
        // atribuição de "Placed", deixando g_inventoryItemReleased
        // e o modo de drag sem refletir que o item já tinha sido
        // efetivamente posicionado. Isso fazia CloseInventoryRadial()
        // remover itens que na real já tinham sido colocados.
        // Usamos uma flag dedicada para não depender do enum de modo,
        // que é sobrescrito pela animação de snap logo em seguida.
        // ============================================================

        g_inventoryItemWasPlaced = true;
        g_inventoryItemReleased = true;
    }

    void CloseInventoryRadial()
    {
        // Se o item não foi colocado em nenhum radial,
        // ele desaparece ao fechar o editor.
        if (!g_inventoryItemWasPlaced)
        {
            g_draggedInventoryItem = nullptr;
            g_inventoryDragMode = InventoryDragMode::None;
            g_inventoryItemReleased = false;
            //g_inventoryDragMouseInitialized = false;
        }

        // Reseta a flag para o próximo ciclo de drag, independente
        // do resultado, para não vazar estado entre arrastos.
        g_inventoryItemWasPlaced = false;
        g_inventoryOverflowMorphActive = false;
        g_fastDragZoneActive = false;
        g_fastDragReturningToCursor = false;

        g_showWindow = false;
        g_radialMode = RadialMode::Inventory;
    }

    void CloseRadialMenu()
    {
        // Restaura imediatamente, inclusive em cancelamentos e fechamentos
        // provocados por outro menu; não espera o fade visual terminar.
        Slowtime::End();
        SetGameplayBlurApplied(false);

        // Apenas avisa que deve fechar (inicia o fade-out)
        g_showWindow = false;

        // ============================================================
        // RESETA O ESTADO TEMPORÁRIO DO TOGGLE
        // ============================================================

        g_radialToggleLocked = false;

        //BlurController::GetSingleton().SetTargetState(false);

        //auto* blurManager = RE::UIBlurManager::GetSingleton();
        //if (blurManager)
        //{
        //    blurManager->DecrementBlurCount();
        //}

        // NÃO RESETE g_radialSide OU g_radialLocked AQUI!
        // O reset de estado agora acontece no DrawMenu() assim que g_globalAlpha chegar a 0.0f
    }

    void HandleGameplayKeyPressed()
    {
        // Reafirma a prioridade do filtro de mouse antes de abrir o radial.
        // Isso evita que uma reinicialização tardia do input do Skyrim deixe a
        // câmera receber o movimento antes do filtro.
        MaintainInputSinkPriority();
        g_radialMode = RadialMode::Gameplay;

        //ResetSettingsScrollCharge();

        OpenRadialMenu();
    }

    bool IsRadialCloseMenuCommand(
        RE::ButtonEvent* button)
    {
        if (!button)
            return false;

        // Apenas teclado.
        if (button->GetDevice() !=
            RE::INPUT_DEVICE::kKeyboard)
        {
            return false;
        }

        // Apenas o primeiro pressionamento.
        if (!button->IsDown() ||
            button->GetRuntimeData().heldDownSecs > 0.0f)
        {
            return false;
        }

        // ========================================================
        // COMANDOS DE ABERTURA DE MENUS
        // ========================================================

        const auto userEvent =
            button->GetUserEvent();

        // ESC / PAUSE MENU
        if (userEvent == "Pause")
            return true;

        // JOURNAL MENU
        if (userEvent == "Journal")
            return true;

        // CONSOLE
        if (userEvent == "Console")
            return true;

        return false;
    }

        
    // ============================================================
    // SETTINGS - CAPTURA DA NOVA TECLA
    //
    // Recebe o scan code do evento do Skyrim.
    // ============================================================

    bool CaptureWheelKey(
        RE::ButtonEvent* buttonEvent)
    {
        if (!buttonEvent)
            return false;

        if (!g_waitingWheelKey)
            return false;

        // ========================================================
        // IGNORA REPETIÇÕES E SOLTURAS
        // ========================================================

        if (!buttonEvent->IsDown())
            return true;

        if (buttonEvent->GetRuntimeData().heldDownSecs > 0.0f)
            return true;

        if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kGamepad)
        {
            const int binding = Gamepad::Encode(buttonEvent->GetIDCode());
            if (g_waitingWheelKeySlot == 2)
                Config::g_secondaryKey = binding;
            else if (g_waitingWheelKeySlot == 3)
                Config::g_altConfigKey = binding;
            else
                Config::g_toggleKey = binding;
            Config::SaveConfig();
            g_waitingWheelKey = false;
            g_waitingWheelKeySlot = 0;
            Logger::GetSingleton().Print(
                "SETTINGS | Wheel Key changed to {}",
                Config::KeyToString(binding));
            return true;
        }

        if (buttonEvent->GetDevice() != RE::INPUT_DEVICE::kKeyboard)
            return false;

        // ========================================================
        // CONVERTE SCAN CODE PARA VK
        // ========================================================

        const UINT scanCode =
            static_cast<UINT>(
                buttonEvent->GetIDCode()
            );

        const UINT virtualKey =
            MapVirtualKeyW(
                scanCode,
                MAPVK_VSC_TO_VK_EX
            );

        // ========================================================
        // VERIFICA SE A TECLA É PERMITIDA
        // ========================================================

        if (!IsSupportedWheelKey(
            static_cast<int>(virtualKey)))
        {
            // Continua aguardando uma tecla válida.
            return true;
        }

        // ========================================================
        // ATUALIZA A TECLA
        // ========================================================

        const int binding = static_cast<int>(virtualKey);
        if (g_waitingWheelKeySlot == 2)
            Config::g_secondaryKey = binding;
        else if (g_waitingWheelKeySlot == 3)
            Config::g_altConfigKey = binding;
        else
            Config::g_toggleKey = binding;

        // ========================================================
        // SALVA NO INI
        // ========================================================

        Config::SaveConfig();

        // ========================================================
        // FINALIZA CAPTURA
        // ========================================================

        g_waitingWheelKey = false;
        g_waitingWheelKeySlot = 0;

        Logger::GetSingleton().Print(
            "SETTINGS | Wheel Key changed to {} | VK={}",
            Config::KeyToString(
                binding
            ),
            binding
        );

        return true;
    }

    static void CheckRadialMenuAutoClose()
    {
        auto* ui = RE::UI::GetSingleton();

        if (!ui)
            return;

        // ========================================================
        // VERIFICA SE ALGUM MENU DO WHEEL ESTÁ ABERTO
        // ========================================================

        const bool settingsOpen =
            SettingsMenu::WheelSettingsMenu::IsOpen();

        const bool inventoryOpen =
            ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME);

        const bool magicOpen =
            ui->IsMenuOpen(RE::MagicMenu::MENU_NAME);

        if (inventoryOpen || magicOpen)
            return;

        if (!g_showWindow && !settingsOpen)
            return;

        // ========================================================
        // MENUS QUE FECHAM O RADIAL E O SETTINGS
        // ========================================================

        static constexpr const char* blockedMenus[] =
        {
            //
            //verificar qual desses menus ta fechando o radial quando inventory
            //ou magic menu está aberto. No momento uma soluçao rapida é
            //verificar esses menus antes de chamar esses v
            //

            "TweenMenu",
            "Main Menu",
            "Loading Menu",
            "Fader Menu",
            "Console",
            "Journal Menu",
            "MapMenu",
            "StatsMenu",
            "Sleep/Wait Menu",
            "Dialogue Menu",
            "BarterMenu",
            "ContainerMenu",
            "Crafting Menu",
            "Book Menu",
            "Lockpicking Menu",
            "MessageBoxMenu",
            "RaceSex Menu",
            "Training Menu",
            "LevelUp Menu",
            "FavoritesMenu",
            "GiftMenu",
            "Credits Menu"
        };

        // ========================================================
        // VERIFICA MENUS INCOMPATÍVEIS
        // ========================================================

        for (const char* menuName : blockedMenus)
        {
            if (!ui->IsMenuOpen(menuName))
                continue;

            Logger::GetSingleton().Print(
                "RADIAL AUTO CLOSE | Menu opened: {}",
                menuName
            );

            // ====================================================
            // FECHA SETTINGS
            // ====================================================

            if (settingsOpen)
            {
                SettingsMenu::WheelSettingsMenu::Close();
            }

            // ====================================================
            // FECHA RADIAL
            // ====================================================

            if (g_showWindow)
            {
                CloseRadialMenu();
            }

            // ====================================================
            // LIMPA ESTADOS DE TRAVA
            // ====================================================

            //g_radialToggleLocked = false;

            ResetRadialLockedOpen();

            return;
        }
    }

    void HandleGPressed()
    {   
        // ============================================================
        // RADIAL TRAVADO - SEGUNDO PRESSIONAMENTO DE G
        // ============================================================

        if (g_showWindow &&
            g_radialMode == RadialMode::Gameplay &&
            g_radialToggleLocked)
        {
            Logger::GetSingleton().Print(
                "RADIAL LOCK | Closing locked radial"
            );

            // Desativa o estado temporário do toggle.
            g_radialToggleLocked = false;

            // A soltura física deste segundo pressionamento
            // não pode executar o fechamento novamente.
            g_ignoreNextGRelease = true;

            // Fecha diretamente pela rotina original do gameplay.
            // Não chama HandleGReleased(), pois essa função
            // interpreta a soltura de um radial travado.
            HandleGameplayKeyReleased();

            return;
        }

        // ============================================================
        // SETTINGS ABERTO -> FECHA COM UM CLIQUE
        // ============================================================

        if (SettingsMenu::WheelSettingsMenu::IsOpen())
        {
            // Sobre um item do Settings, a tecla do radial abre o editor do
            // gesto em vez de fechar o menu.
            if (!g_quickDrawEditorOpen && BeginQuickDrawEditor())
                return;

            // Enquanto o editor está aberto, a mesma tecla não fecha o
            // Settings nem descarta o desenho em andamento.
            if (g_quickDrawEditorOpen)
                return;

            if (TrackEditor::IsOpen())
            {
                TrackEditor::Cancel();
                return;
            }
            Logger::GetSingleton().Print(
                "SETTINGS CLOSE | toggle pressed"
            );

            SettingsMenu::WheelSettingsMenu::Close();

            return;
        }

        // ============================================================
        // FUNCIONAMENTO NORMAL
        // ============================================================

        if (IsInventoryOpen())
        {
            HandleInventoryKeyPressed();
        }
        else
        {
            // Uma nova abertura começa sempre com o estado
            // temporário de toggle desativado.
            //
            // As configurações individuais de trava
            // permanecem intactas.

            g_radialToggleLocked = false;

            HandleGameplayKeyPressed();
        }
    }

    

    void HandleGReleased()
    {
        // ========================================================
        // CANCELA A CARGA DO SETTINGS AO SOLTAR O TOGGLE
        // ========================================================

        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
        {
            ResetSettingsScrollCharge();
        }
        
        // ============================================================
        // IGNORA A SOLTURA DO SEGUNDO PRESSIONAMENTO
        // ============================================================

        if (g_ignoreNextGRelease)
        {
            g_ignoreNextGRelease = false;

            Logger::GetSingleton().Print(
                "RADIAL LOCK | Ignoring release after toggle close"
            );

            return;
        }

        // ============================================================
        // SETTINGS ABERTO
        // ============================================================

        if (SettingsMenu::WheelSettingsMenu::IsOpen())
        {
            return;
        }

        // ============================================================
        // GAMEPLAY
        // ============================================================

        if (g_radialMode == RadialMode::Gameplay)
        {
            // Soltar a tecla depois de desenhar decide a seleção por gesto.
            // Se não houver correspondência suficiente, fecha como o fluxo
            // normal, sem equipar um item acidentalmente.
            if (g_quickDrawGameplayActive)
            {
                const bool matched = FinishQuickDrawGameplay();
                if (matched)
                    UseSelectedRadialItem();
                g_quickDrawSelectionOverride = nullptr;
                g_quickDrawSelectionSide = RadialSide::None;
                CloseRadialMenu();
                return;
            }

            // Se o radial atual está travado, a primeira soltura
            // de G não deve equipar o item nem fechar o menu.

            if (g_showWindow && IsCurrentRadialLocked())
            {
                g_radialToggleLocked = true;

                Logger::GetSingleton().Print(
                    "RADIAL LOCK | Waiting for next G press"
                );

                return;
            }

            // Radial desbloqueado: funcionamento original.
            g_radialToggleLocked = false;

            HandleGameplayKeyReleased();

            return;
        }

        // ============================================================
        // INVENTÁRIO
        // ============================================================

        if (g_radialMode == RadialMode::Inventory)
        {
            HandleInventoryKeyReleased();
        }
    }

    ImVec2 GetRadialMousePosition()
    {
        return ImVec2(
            g_radialOrigin.x + g_radialVector.x,
            g_radialOrigin.y + g_radialVector.y
        );
    }

    bool IsWeaponItem(RE::TESForm* form)
    {
        return form && form->As<RE::TESObjectWEAP>() != nullptr;
    }

    bool IsArmorItem(RE::TESForm* form)
    {
        return form && form->As<RE::TESObjectARMO>() != nullptr;
    }

    bool IsShield(RE::TESObjectARMO* armor)
    {
        if (!armor)
            return false;

        // Escudos ocupam o slot Shield
        return armor->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kShield);
    }

    bool IsLockpick(RE::TESForm* form)
    {
        if (!form)
            return false;

        auto* misc = form->As<RE::TESObjectMISC>();

        if (!misc)
            return false;

        // Lockpick vanilla FormID
        constexpr RE::FormID LOCKPICK_ID = 0x0000000A;

        return misc->GetFormID() == LOCKPICK_ID;
    }

    bool UseLockpickOnCrosshair(RE::TESObjectREFR* target)
    {
        if (!target)
            return false;

        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!player)
            return false;

        // ============================================================
        // PEGA O LOCK
        // ============================================================

        RE::REFR_LOCK* lock = target->GetLock();

        if (!lock)
            return false;

        // Já está desbloqueado
        if (!lock->IsLocked())
            return false;

        // ============================================================
        // VERIFICA SE É PORTA OU CONTAINER
        // ============================================================

        auto* baseObject = target->GetBaseObject();

        if (!baseObject)
            return false;

        const bool isDoor = baseObject->As<RE::TESObjectDOOR>() != nullptr;
        const bool isContainer = baseObject->As<RE::TESObjectCONT>() != nullptr;

        if (!isDoor && !isContainer)
            return false;

        // ============================================================
        // DESBLOQUEIA
        // ============================================================

        lock->SetLocked(false);

        return true;
    }

    // ============================================================
    // NOVO: item de lockpick selecionado no radial.
    // Antes essa função existia mas nunca era chamada -
    // era um "buraco" real no módulo de equipar itens.
    //
    // BUGFIX: além de nunca ser chamada, mexer no lock (SetLocked)
    // a partir da thread de render é o mesmo tipo de problema da
    // leitura de livros. Despachamos para a thread principal também.
    // ============================================================
    bool UseLockpickFromRadial()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return false;

        // Validação rápida: garante que existe um alvo trancado antes de
        // disparar a rotina nativa de ativação da gazua.
        auto* crosshair = RE::CrosshairPickData::GetSingleton();
        if (!crosshair)
            return false;

        RE::TESObjectREFR* target = crosshair->target[0].get().get();
        if (!target)
            return false;

        auto* lock = target->GetLock();
        if (!lock || !lock->IsLocked())
            return false;

        auto* baseObject = target->GetBaseObject();
        if (!baseObject)
            return false;

        const bool isDoor = baseObject->As<RE::TESObjectDOOR>() != nullptr;
        const bool isContainer = baseObject->As<RE::TESObjectCONT>() != nullptr;
        if (!isDoor && !isContainer)
            return false;

        // Não fazemos mais lock->SetLocked(false). Isso pulava completamente
        // o sistema vanilla de lockpicking. ActivatePickRef() entrega o alvo
        // para a rotina nativa do jogo.
        auto doPick = [player]()
        {
            player->ActivatePickRef();
        };

        if (auto* taskInterface = SKSE::GetTaskInterface())
        {
            taskInterface->AddTask(doPick);
        }
        else
        {
            doPick();
        }

        return true;
    }

    bool IsBowOrCrossbow(RE::TESObjectWEAP* weapon)
    {
        if (!weapon)
            return false;

        const auto type = weapon->GetWeaponType();

        return type == RE::WEAPON_TYPE::kBow || type == RE::WEAPON_TYPE::kCrossbow;
    }

    // Armas que ocupam o conjunto inteiro de mãos. Não podem ser tratadas
    // como a simples arma "da outra mão" durante uma troca one-handed.
    static bool IsTwoHandedWeapon(RE::TESObjectWEAP* weapon)
    {
        if (!weapon)
            return false;

        const auto type = weapon->GetWeaponType();
        return type == RE::WEAPON_TYPE::kTwoHandSword ||
            type == RE::WEAPON_TYPE::kTwoHandAxe ||
            type == RE::WEAPON_TYPE::kBow ||
            type == RE::WEAPON_TYPE::kCrossbow;
    }

    bool IsArmorEquipped(
        RE::Actor* actor,
        RE::TESObjectARMO* armor,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        if (!actor || !armor)
            return false;

        const auto inventory = actor->GetInventory();

        const auto it = inventory.find(armor);

        if (it == inventory.end())
            return false;

        const auto& [count, entryData] = it->second;

        if (!entryData)
            return false;

        // ============================================================
        // SEM UNIQUE ID
        //
        // Mantém o comportamento original.
        // ============================================================

        if (!hasUniqueID)
        {
            return entryData->IsWorn();
        }

        // ============================================================
        // COM UNIQUE ID
        //
        // Verifica a instância específica.
        // ============================================================

        if (!entryData->extraLists)
            return false;

        for (auto* extra : *entryData->extraLists)
        {
            if (!extra)
                continue;

            auto* unique =
                extra->GetByType<RE::ExtraUniqueID>();

            if (!unique)
                continue;

            if (unique->uniqueID != uniqueID)
                continue;

            // Encontramos a instância solicitada.
            return extra->HasType<RE::ExtraWorn>() ||
                extra->HasType<RE::ExtraWornLeft>();
        }

        return false;
    }

    bool UseBook(RE::TESObjectBOOK* book)
    {
        if (!book)
            return false;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return false;

        // Grimório: ensina magia e depois remove o livro
        if (book->TeachesSpell())
        {
            if (auto* taskInterface = SKSE::GetTaskInterface())
            {
                taskInterface->AddTask([book, player]()
                {
                    if (book->Read(player))
                    {
                        player->RemoveItem(
                            book,
                            1,
                            RE::ITEM_REMOVE_REASON::kRemove,
                            nullptr,
                            nullptr
                        );
                    }
                });
            }
            return true;
        }

        // Livro comum: abre a interface de leitura book->Read(player) –
        // BookMenu::OpenMenuFromBaseForm(book) – esta função estática 
        // envia a mensagem para a UI, abrindo o livro sem precisar de
        // uma referência no mundo, ideal para livros no inventário.
        // O jogo cuida de toda a exibição, incluindo efeitos de perk
        // (caso o livro os tenha).

        RE::BookMenu::OpenMenuFromBaseForm(book);

        return true;
    }

    static ImVec2 GetSideOverflowPosition(
        const ImVec2& center,
        bool leftSide,
        int overflowIndex,
        int overflowCount)
    {
        if (Config::g_customRadial && Track::HasValidSavedLayout())
            return Track::OverflowPosition(center, GetSideRadialRadius(),
                overflowIndex, overflowCount, leftSide);
        return OverflowMechanism::OverflowPosition(
            OverflowMechanism::Style::ConcentricRings,
            static_cast<RadialShape::Style>(std::clamp(
                Config::g_radialShape, 0, RadialShape::Count() - 1)), center,
            GetSideRadialRadius(), overflowIndex, overflowCount, leftSide);
    }

    // ============================================================
    // NOVO: consumo de poções / venenos / comida (AlchemyItem).
    // Faltava esse ramo explícito no módulo de equipar; sem ele o
    // item caía no fallback genérico de TESBoundObject, que funciona
    // por acaso (o motor consome poções ao "equipar"), mas não
    // tratava poção-arremessável / veneno em arma corretamente.
    // ============================================================
    bool UseAlchemyItem(RE::AlchemyItem* alchemyItem)
    {
        if (!alchemyItem)
            return false;

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* equipManager = RE::ActorEquipManager::GetSingleton();

        if (!player || !equipManager)
            return false;

        RE::Actor* actor = static_cast<RE::Actor*>(player);

        if (alchemyItem->IsPoison())
        {
            // Veneno: aplica na arma equipada em vez de beber.
            equipManager->EquipObject(
                actor,
                alchemyItem,
                nullptr,
                1,
                nullptr,
                false,
                false,
                false,
                false);

            return true;
        }

        // Poção / comida: equipar consome automaticamente.
        equipManager->EquipObject(
            actor,
            alchemyItem,
            nullptr,
            1,
            nullptr,
            true,
            false,
            true,
            true);

        return true;
    }

    // ============================================================
    // NOVO: pergaminhos (ScrollItem). Antes caíam no fallback
    // genérico sem escolher a mão correta, igual às magias.
    // ============================================================
    bool UseScrollItem(RE::ScrollItem* scroll, bool leftSide)
    {
        if (!scroll)
            return false;

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* equipManager = RE::ActorEquipManager::GetSingleton();

        if (!player || !equipManager)
            return false;

        RE::Actor* actor = static_cast<RE::Actor*>(player);

        const RE::BGSEquipSlot* handSlot = nullptr;

        if (leftSide)
        {
            constexpr RE::FormID LEFT_HAND_SLOT = 0x13F42;
            handSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(LEFT_HAND_SLOT);
        }
        else
        {
            constexpr RE::FormID RIGHT_HAND_SLOT = 0x13F43;
            handSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(RIGHT_HAND_SLOT);
        }

        equipManager->EquipObject(
            actor,
            scroll,
            nullptr,
            1,
            handSlot,
            true,
            false,
            true,
            true);

        return true;
    }

    // ============================================================
    // NOVO: quantidade de um item no inventário do ator.
    // ============================================================
    std::int32_t GetInventoryItemCount(RE::Actor* actor, RE::TESBoundObject* object)
    {
        if (!actor || !object)
            return 0;

        const auto inventory = actor->GetInventory();

        const auto it = inventory.find(object);

        if (it == inventory.end())
            return 0;

        // it->second é um std::pair<std::int32_t, std::unique_ptr<InventoryEntryData>>
        return it->second.first;
    }

        
    static bool GetSettingsPreviewItem(
        RadialItem& result)
    {
        result = {};

        // ========================================================
        // PRIORIDADE: ITEM ARRASTADO
        // ========================================================

        if (g_settingsDrag.active &&
            g_settingsDrag.form)
        {
            result.form =
                g_settingsDrag.form;

            result.uniqueID =
                g_settingsDrag.uniqueID;

            result.hasUniqueID =
                g_settingsDrag.hasUniqueID;

            result.valid = true;

            return true;
        }

        // ========================================================
        // SELEÇÃO TEMPORÁRIA DO SETTINGS
        // ========================================================

        if (!g_settingsPreviewSelectionActive)
            return false;

        if (!g_settingsPreviewSelection.form)
            return false;

        result = g_settingsPreviewSelection;

        return true;
    }

    static ImVec2 GetSideRadialAnimatedPosition(
        RadialItemAnimation& anim,
        const ImVec2& target,
        const ImVec2& center,
        bool leftSide,
        float deltaTime)
    {
        // ============================================================
        // INVENTÁRIO:
        // sem animação, igual ao comportamento antigo
        // ============================================================

        if (g_radialMode != RadialMode::Gameplay)
        {
            anim.currentPos = target;
            anim.previousPos = target;
            anim.velocity = ImVec2(0.0f, 0.0f);
            anim.posInitialized = true;

            const float dx = target.x - center.x;
            const float dy = target.y - center.y;

            anim.sideAngle = std::atan2(dy, dx);
            anim.sideRadius = std::sqrt(dx * dx + dy * dy);
            anim.sidePolarInitialized = true;

            return target;
        }

        // ============================================================
        // TARGET EM COORDENADAS POLARES
        // ============================================================

        const float targetDX =
            target.x - center.x;

        const float targetDY =
            target.y - center.y;

        float targetAngle =
            std::atan2(targetDY, targetDX);

        const float targetRadius =
            std::sqrt(
                targetDX * targetDX +
                targetDY * targetDY
            );

        // ============================================================
        // PRIMEIRA VEZ
        // ============================================================

        if (!anim.sidePolarInitialized)
        {
            // Se o item já possuía posição visual por causa da animação
            // principal, começa exatamente dali.
            if (anim.posInitialized)
            {
                const float currentDX =
                    anim.currentPos.x - center.x;

                const float currentDY =
                    anim.currentPos.y - center.y;

                anim.sideAngle =
                    std::atan2(
                        currentDY,
                        currentDX
                    );

                anim.sideRadius =
                    std::sqrt(
                        currentDX * currentDX +
                        currentDY * currentDY
                    );
            }
            else
            {
                anim.sideAngle =
                    targetAngle;

                anim.sideRadius =
                    targetRadius;

                anim.currentPos =
                    target;

                anim.posInitialized =
                    true;
            }

            anim.sidePolarInitialized = true;
        }

        deltaTime =
            std::clamp(
                deltaTime,
                0.0f,
                1.0f / 30.0f
            );

        // ============================================================
        // DESENROLA O ÂNGULO
        //
        // Faz o target representar a volta mais curta a partir
        // do ângulo atual.
        // ============================================================

        float angleDelta =
            targetAngle - anim.sideAngle;

        while (angleDelta > PI)
            angleDelta -= 2.0f * PI;

        while (angleDelta < -PI)
            angleDelta += 2.0f * PI;

        targetAngle =
            anim.sideAngle + angleDelta;

        // ============================================================
        // SMOOTH SEM MOLA / SEM OVERSHOOT
        // ============================================================

        const float angleSpeed = anim.settingsDropSettling ? 6.0f : 12.0f;
        const float radiusSpeed = anim.settingsDropSettling ? 6.0f : 10.0f;

        const float angleT =
            1.0f -
            std::exp(-angleSpeed * deltaTime);

        const float radiusT =
            1.0f -
            std::exp(-radiusSpeed * deltaTime);

        anim.sideAngle +=
            (targetAngle - anim.sideAngle) *
            angleT;

        anim.sideRadius +=
            (targetRadius - anim.sideRadius) *
            radiusT;

        // ============================================================
        // VOLTA PARA X/Y
        // ============================================================

        anim.previousPos =
            anim.currentPos;

        anim.currentPos = ImVec2(
            center.x +
                std::cos(anim.sideAngle) *
                anim.sideRadius,

            center.y +
                std::sin(anim.sideAngle) *
                anim.sideRadius
        );

        if (anim.settingsDropSettling)
        {
            const float remainingAngle = std::abs(targetAngle - anim.sideAngle);
            const float remainingRadius = std::abs(targetRadius - anim.sideRadius);
            if (remainingAngle < 0.004f && remainingRadius < 0.5f)
                anim.settingsDropSettling = false;
        }

        return anim.currentPos;
    }

    // ============================================================
    // NOVO: remove dos radiais qualquer item que tenha acabado no
    // inventário (quantidade <= 0). Feitiços e gritos (que não têm
    // contagem de inventário) nunca são removidos por essa função.
    //
    // Corrige o pedido: "se item acabou no inventário, ele some do
    // radial".
    // ============================================================
    void PruneMissingRadialItems()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();

        if (!player)
            return;

        RE::Actor* actor = static_cast<RE::Actor*>(player);

        auto pruneList = [&](std::vector<RadialItem>& items)
        {
            items.erase(
                std::remove_if(
                    items.begin(),
                    items.end(),
                    [&](const RadialItem& item)
                    {
                        if (!item.form)
                            return true;

                        // Spell normal não tem quantidade de inventário.
                        // Mantém no radial.
                        if (item.form->As<RE::SpellItem>() != nullptr &&
                            item.form->As<RE::ScrollItem>() == nullptr)
                        {
                            return false;
                        }

                        auto* bound = item.form->As<RE::TESBoundObject>();

                        if (!bound)
                            return false;

                        return GetInventoryItemCount(actor, bound) <= 0;
                    }
                ),
                items.end()
            );

            for (int i = 0; i < static_cast<int>(items.size()); ++i)
            {
                items[i].slot = i;
            }
        };

        pruneList(g_topItems);
        pruneList(g_bottomItems);
        pruneList(g_sideItems);
    }

    // ============================================================
    // equipa arma normal (não-arco) vinda de um dos lados
    // do radial, lógica de dois níveis:
    //
    //  - Já equipada NESTE lado         -> desequipa (toggle off).
    //  - Já equipada NO OUTRO lado:
    //      - 2+ cópias no inventário    -> equipa também neste lado
    //                                       (dual wield da mesma arma).
    //      - só 1 cópia no inventário   -> desequipa do outro lado e
    //                                       equipa neste lado (move).
    //  - Não equipada em nenhum lado    -> equipa normalmente.
    //
    // ============================================================
        
    static RE::ExtraDataList* GetEquippedWeaponExtraList(
        RE::Actor* actor,
        bool leftHand,
        std::uint16_t uniqueID = 0,
        bool hasUniqueID = false)
    {
        if (!actor)
            return nullptr;

        auto* weapon =
            actor->GetEquippedObject(leftHand);

        if (!weapon)
            return nullptr;

        auto* entry =
            actor->GetEquippedEntryData(leftHand);

        if (!entry || !entry->extraLists)
            return nullptr;

        for (auto* extra : *entry->extraLists)
        {
            if (!extra)
                continue;

            const bool worn =
                leftHand
                    ? extra->HasType<RE::ExtraWornLeft>()
                    : extra->HasType<RE::ExtraWorn>();

            if (!worn)
                continue;

            if (!hasUniqueID)
                return extra;

            auto* unique =
                extra->GetByType<RE::ExtraUniqueID>();

            if (unique &&
                unique->uniqueID == uniqueID)
            {
                return extra;
            }
        }

        return nullptr;
    }

    static bool EquipWeaponInHand(
        RE::Actor* actor,
        RE::ActorEquipManager* manager,
        RE::TESObjectWEAP* weapon,
        bool leftHand)
    {
        if (!actor || !manager || !weapon)
            return false;

        constexpr RE::FormID LEFT_SLOT  = 0x13F43;
        constexpr RE::FormID RIGHT_SLOT = 0x13F42;

        auto* slot =
            RE::TESForm::LookupByID<RE::BGSEquipSlot>(
                leftHand ? LEFT_SLOT : RIGHT_SLOT
            );

        if (!slot)
            return false;

        // Procura uma instância que não esteja equipada.
        RE::ExtraDataList* extra = nullptr;

        const auto inventory = actor->GetInventory();

        const auto it = inventory.find(weapon);

        if (it == inventory.end())
            return false;

        auto* entry = it->second.second.get();

        if (entry && entry->extraLists)
        {
            for (auto* list : *entry->extraLists)
            {
                if (!list)
                    continue;

                // Não reutiliza uma instância equipada.
                if (list->HasType<RE::ExtraWorn>() ||
                    list->HasType<RE::ExtraWornLeft>())
                {
                    continue;
                }

                extra = list;
                break;
            }
        }

        manager->EquipObject(
            actor,
            weapon,
            extra,
            1,
            slot,
            false,  // a_queueEquip
            false,  // a_forceEquip
            true,   // a_playSounds
            true    // a_applyNow
        );

        return true;
    }

    
    //static void LogWeaponState(
    //    RE::Actor* actor,
    //    const char* operation,
    //    RE::TESObjectWEAP* weapon,
    //    bool leftSide)
    //{
    //    if (!actor)
    //        return;

    //    constexpr RE::FormID LEFT_SLOT  = 0x13F43;
    //    constexpr RE::FormID RIGHT_SLOT = 0x13F42;

    //    auto* left =
    //        actor->GetEquippedObject(true);

    //    auto* right =
    //        actor->GetEquippedObject(false);

    //    auto* requestedSlot =
    //        RE::TESForm::LookupByID<RE::BGSEquipSlot>(
    //            leftSide ? LEFT_SLOT : RIGHT_SLOT
    //        );

    //    auto* defaultSlot =
    //        weapon ? weapon->GetEquipSlot() : nullptr;

    //    auto* leftExtra =
    //        GetEquippedWeaponExtraList(
    //            actor,
    //            true,
    //            uniqueID,
    //            hasUniqueID
    //        );

    //    auto* rightExtra =
    //        GetEquippedWeaponExtraList(
    //            actor,
    //            false,
    //            uniqueID,
    //            hasUniqueID
    //        );

    //    const int count =
    //        weapon
    //            ? GetInventoryItemCount(actor, weapon)
    //            : 0;

    //    Logger::GetSingleton().Print(
    //        "WEAPON DEBUG [{}] | "
    //        "requested={} | "
    //        "weapon={:08X} | "
    //        "type={} | "
    //        "count={} | "
    //        "defaultSlot={:08X} | "
    //        "requestedSlot={:08X} | "
    //        "left={:08X} | "
    //        "right={:08X} | "
    //        "leftExtra={} | "
    //        "rightExtra={} | "
    //        "pending={}",
    //        operation,
    //        leftSide ? "LEFT" : "RIGHT",
    //        weapon ? weapon->GetFormID() : 0,
    //        weapon
    //            ? static_cast<int>(weapon->GetWeaponType())
    //            : -1,
    //        count,
    //        defaultSlot ? defaultSlot->GetFormID() : 0,
    //        requestedSlot ? requestedSlot->GetFormID() : 0,
    //        left ? left->GetFormID() : 0,
    //        right ? right->GetFormID() : 0,
    //        leftExtra ? "FOUND" : "NULL",
    //        rightExtra ? "FOUND" : "NULL",
    //        g_pendingWeaponSwitch.active
    //    );
    //}   


    static bool UnequipWeaponFromHand(
        RE::Actor* actor,
        RE::ActorEquipManager* manager,
        RE::TESObjectWEAP* weapon,
        bool leftHand)
    {
        if (!actor || !manager || !weapon)
            return false;

        auto* equipped =
            actor->GetEquippedObject(leftHand);

        if (equipped != weapon)
            return false;

        constexpr RE::FormID LEFT_SLOT  = 0x13F43;
        constexpr RE::FormID RIGHT_SLOT = 0x13F42;

        auto* slot =
            RE::TESForm::LookupByID<RE::BGSEquipSlot>(
                leftHand ? LEFT_SLOT : RIGHT_SLOT
            );

        if (!slot)
            return false;

        auto* extra =
            GetEquippedWeaponExtraList(
                actor,
                leftHand
            );

        //LogWeaponState(
        //    actor,
        //    "BEFORE UNEQUIP",
        //    weapon,
        //    leftHand
        //);

        manager->UnequipObject(
            actor,
            weapon,
            extra,
            1,
            slot,
            false,
            false,
            true,
            true,
            nullptr
        );

        //LogWeaponState(
        //    actor,
        //    "AFTER UNEQUIP",
        //    weapon,
        //    leftHand
        //);

        return true;
    }
        
    void Menu::UpdatePendingWeaponSwitch()
    {
        if (!g_pendingWeaponSwitch.active)
            return;

        auto& pending = g_pendingWeaponSwitch;

        auto actorPtr = pending.actor.get();

        if (!actorPtr)
        {
            pending = {};
            return;
        }

        auto* actor = actorPtr.get();

        auto* weapon =
            RE::TESForm::LookupByID<RE::TESObjectWEAP>(
                pending.weaponID
            );

        auto* manager =
            RE::ActorEquipManager::GetSingleton();

        if (!weapon || !manager)
        {
            pending = {};
            return;
        }

        constexpr RE::FormID LEFT_SLOT  = 0x13F43;
        constexpr RE::FormID RIGHT_SLOT = 0x13F42;

        constexpr float TIMEOUT_SECONDS = 3.0f;

        const auto now =
            std::chrono::steady_clock::now();

        const float elapsed =
            std::chrono::duration<float>(
                now - pending.startTime
            ).count();

        // ============================================================
        // ESTADO ATUAL
        // ============================================================

        auto* left =
            actor->GetEquippedObject(true);

        auto* right =
            actor->GetEquippedObject(false);

        auto* sourceWeapon =
            pending.sourceLeft ? left : right;

        auto* targetWeapon =
            pending.targetLeft ? left : right;

        // ============================================================
        // 1. DESEQUIPAMENTO SIMPLES
        // ============================================================

        if (pending.unequipOnly)
        {
            // A arma saiu da mão solicitada.
            if (sourceWeapon != weapon)
            {
                pending = {};
                return;
            }

            // Aguarda o desequipamento terminar.
            if (elapsed >= TIMEOUT_SECONDS)
            {
                pending = {};
            }

            return;
        }

        // ============================================================
        // 2. TRANSFERÊNCIA ENTRE MÃOS
        // ============================================================

        // A arma já está na mão desejada
        // e não está mais na mão original.
        if (targetWeapon == weapon &&
            sourceWeapon != weapon)
        {
            pending = {};
            return;
        }

        // Ainda está na mão original.
        // Não equipa outra cópia.
        if (sourceWeapon == weapon)
        {
            if (elapsed >= TIMEOUT_SECONDS)
            {
                pending = {};
            }

            return;
        }

        // ============================================================
        // 3. ARMA SAIU DA MÃO ORIGINAL
        // ============================================================

        if (elapsed >= TIMEOUT_SECONDS)
        {
            pending = {};
            return;
        }

        auto* targetSlot =
            RE::TESForm::LookupByID<RE::BGSEquipSlot>(
                pending.targetLeft
                    ? LEFT_SLOT
                    : RIGHT_SLOT
            );

        if (!targetSlot)
        {
            pending = {};
            return;
        }

        // Equipa na mão solicitada.
        EquipWeaponInHand(
            actor,
            manager,
            weapon,
            pending.targetLeft
        );

        pending = {};
    }

    //Se GetEquippedEntryData() ainda não permitir identificar a instância,
    // verificamos o inventário para descobrir se a instância solicitada
    //está marcada como equipada na mão correta.
    static bool MatchesWeaponInstance(
        RE::Actor* actor,
        RE::TESObjectWEAP* weapon,
        bool leftHand,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        if (!actor || !weapon)
            return false;

        // Primeiro confirma o FormID da mão.
        if (actor->GetEquippedObject(leftHand) != weapon)
            return false;

        // Item sem identidade específica.
        if (!hasUniqueID)
            return true;

        // ==========================================
        // 1. PROCURA NOS DADOS DA MÃO EQUIPADA
        // ==========================================

        auto* equippedExtra =
            GetEquippedWeaponExtraList(
                actor,
                leftHand,
                uniqueID,
                true
            );

        if (equippedExtra)
            return true;

        // ==========================================
        // 2. FALLBACK: INVENTÁRIO
        // ==========================================

        auto* changes =
            actor->GetInventoryChanges();

        if (!changes || !changes->entryList)
            return false;

        for (auto* entry : *changes->entryList)
        {
            if (!entry || entry->object != weapon)
                continue;

            if (!entry->extraLists)
                return false;

            for (auto* extra : *entry->extraLists)
            {
                if (!extra)
                    continue;

                auto* unique =
                    extra->GetByType<RE::ExtraUniqueID>();

                if (!unique)
                    continue;

                if (unique->uniqueID != uniqueID)
                    continue;

                // Confirma que esta instância está
                // equipada na mão solicitada.

                const bool worn =
                    leftHand
                        ? extra->HasType<RE::ExtraWornLeft>()
                        : extra->HasType<RE::ExtraWorn>();

                return worn;
            }

            return false;
        }

        return false;
    }

    //O ExtraUniqueID::baseID é um identificador de origem
    //associado à instância. Ele não deve ser tratado
    //como uma garantia de que o proprietário atual é o ator.
    static RE::ExtraDataList* GetWeaponExtraForEquip(
        RE::Actor* actor,
        RE::TESObjectWEAP* weapon,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        if (!actor || !weapon)
            return nullptr;

        auto* changes =
            actor->GetInventoryChanges();

        if (!changes || !changes->entryList)
            return nullptr;

        for (auto* entry : *changes->entryList)
        {
            if (!entry || entry->object != weapon)
                continue;

            if (!entry->extraLists)
                return nullptr;

            for (auto* extra : *entry->extraLists)
            {
                if (!extra)
                    continue;

                // ==========================================
                // INSTÂNCIA ESPECÍFICA
                // ==========================================

                if (hasUniqueID)
                {
                    auto* unique =
                        extra->GetByType<RE::ExtraUniqueID>();

                    if (!unique)
                        continue;

                    if (unique->uniqueID != uniqueID)
                        continue;

                    return extra;
                }

                // ==========================================
                // ITEM SEM UNIQUE ID
                // ==========================================

                if (extra->HasType<RE::ExtraWorn>() ||
                    extra->HasType<RE::ExtraWornLeft>())
                {
                    continue;
                }

                return extra;
            }

            return nullptr;
        }

        return nullptr;
    }

            
    //procura a instância diretamente no inventário,
    //verifica a mão pelo marcador ExtraWorn ou ExtraWornLeft
    //e solicita o desequipamento uma única vez.
    //O retorno indica que a solicitação foi enviada à engine.
    //A confirmação de que a arma realmente saiu da mão será feita pelo Update.

        static bool UnequipExactWeapon(
        RE::Actor* actor,
        RE::ActorEquipManager* manager,
        RE::TESObjectWEAP* weapon,
        bool leftHand,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        if (!actor || !manager || !weapon)
            return false;

        if (actor->GetEquippedObject(leftHand) != weapon)
            return false;

        constexpr RE::FormID LEFT_SLOT  = 0x13F43;
        constexpr RE::FormID RIGHT_SLOT = 0x13F42;

        auto* slot =
            RE::TESForm::LookupByID<RE::BGSEquipSlot>(
                leftHand ? LEFT_SLOT : RIGHT_SLOT
            );

        if (!slot)
            return false;

        RE::ExtraDataList* extra = nullptr;

        // ============================================================
        // IDENTIFICA A INSTÂNCIA EXATA NO INVENTÁRIO
        // ============================================================

        if (hasUniqueID)
        {
            extra = GetWeaponExtraForEquip(
                actor,
                weapon,
                uniqueID,
                true
            );

            if (!extra)
            {
                spdlog::warn(
                    "UNEQUIP | instance missing | uniqueID={}",
                    uniqueID
                );

                return false;
            }
        }
        else
        {
            extra = GetEquippedWeaponExtraList(
                actor,
                leftHand
            );
        }

        // ============================================================
        // CONFIRMA O MARCADOR DA MÃO
        // ============================================================

        if (extra)
        {
            const bool wornRight =
                extra->HasType<RE::ExtraWorn>();

            const bool wornLeft =
                extra->HasType<RE::ExtraWornLeft>();

            const bool wornHere =
                leftHand ? wornLeft : wornRight;

            const bool wornOpposite =
                leftHand ? wornRight : wornLeft;

            // Não desequipa uma instância diferente.
            if (!wornHere)
            {
                spdlog::warn(
                    "UNEQUIP | wrong hand | uniqueID={} | left={} | right={}",
                    uniqueID,
                    wornLeft,
                    wornRight
                );

                return false;
            }

            // Estado inconsistente: uma instância marcada
            // como equipada simultaneamente nas duas mãos.
            if (wornOpposite)
            {
                spdlog::warn(
                    "UNEQUIP | conflicting worn flags | uniqueID={}",
                    uniqueID
                );

                return false;
            }
        }
        else if (hasUniqueID)
        {
            return false;
        }

        // ============================================================
        // SOLICITA O DESEQUIPAMENTO
        // ============================================================

        spdlog::info(
            "UNEQUIP REQUEST | side={} | form={:08X} | uniqueID={}",
            leftHand ? "LEFT" : "RIGHT",
            weapon->GetFormID(),
            uniqueID
        );

        const bool engineResult =
            manager->UnequipObject(
                actor,
                weapon,
                extra,
                1,
                slot,
                false,  // queueEquip
                true,   // forceEquip
                true,   // playSounds
                true,   // applyNow
                nullptr
            );

        spdlog::info(
            "UNEQUIP SENT | side={} | uniqueID={} | engineResult={}",
            leftHand ? "LEFT" : "RIGHT",
            uniqueID,
            engineResult
        );

        // A chamada foi enviada.
        // A confirmação real será feita no Update.
        return true;
    }

     
    static bool WaitForWeaponSheathe(
        RE::Actor* actor,
        PendingNormalWeaponEquip& pending)
    {
        if (!actor)
            return false;

        const auto now =
            std::chrono::steady_clock::now();

        const auto weaponState =
            actor->AsActorState()->GetWeaponState();

        // ============================================================
        // ARMA COMPLETAMENTE GUARDADA
        // ============================================================

        if (weaponState == RE::WEAPON_STATE::kSheathed)
        {
            if (pending.waitingForSheathe)
            {
                pending.waitingForSheathe = false;
                pending.phaseTime = now;
                pending.attempts = 0;

                spdlog::info(
                    "WEAPON SHEATHE COMPLETE | form={:08X} | uniqueID={}",
                    pending.weaponID,
                    pending.uniqueID
                );
            }

            return true;
        }

        // ============================================================
        // SOLICITA GUARDAR AS ARMAS UMA ÚNICA VEZ
        // ============================================================

        if (!pending.waitingForSheathe)
        {
            pending.waitingForSheathe = true;
            pending.phaseTime = now;

            actor->DrawWeaponMagicHands(false);

            spdlog::info(
                "WEAPON SHEATHE REQUEST | form={:08X} | uniqueID={} | state={}",
                pending.weaponID,
                pending.uniqueID,
                static_cast<int>(weaponState)
            );
        }

        // Aguarda a animação terminar.
        return false;
    }

        
    static bool RestoreWeaponDrawState(
        RE::Actor* actor,
        PendingNormalWeaponEquip& pending)
    {
        if (!actor)
            return false;

        // O personagem estava com as armas guardadas.
        // Não precisamos sacar novamente.
        if (!pending.wasWeaponDrawn)
            return true;

        auto* actorState = actor->AsActorState();

        if (!actorState)
            return false;

        // As armas já estão sacadas.
        if (actorState->IsWeaponDrawn())
            return true;

        // Solicita sacar uma única vez.
        if (!pending.redrawRequested)
        {
            pending.redrawRequested = true;

            actor->DrawWeaponMagicHands(true);

            spdlog::info(
                "WEAPON REDRAW REQUEST | form={:08X} | uniqueID={}",
                pending.weaponID,
                pending.uniqueID
            );
        }

        // Aguarda a animação de sacar terminar.
        return actorState->GetWeaponState() ==
            RE::WEAPON_STATE::kDrawn;
    }

    //esta versão preserva o comportamento de UnequipExactWeapon()
    //e MatchesWeaponInstance(). O bool retornado por UnequipObject()
    //não é tratado como confirmação definitiva; o código verifica
    //novamente a identidade da arma equipada nos frames seguintes   
    //a fase 4 também é executada quando a transferência falha ou
    //atinge o limite de tempo. Isso corrige o caminho em que
    //o código anterior encerrava a operação com pending = {} antes de
    //solicitar que o personagem sacasse as armas novamente.    
    void UpdatePendingNormalWeaponEquip()
    {
        auto& pending = g_pendingNormalWeaponEquip;

        if (!pending.active)
            return;

        auto actorPtr = pending.actor.get();

        if (!actorPtr)
        {
            pending = {};
            return;
        }

        auto* actor = actorPtr.get();

        auto* manager =
            RE::ActorEquipManager::GetSingleton();

        auto* weapon =
            RE::TESForm::LookupByID<RE::TESObjectWEAP>(
                pending.weaponID
            );

        if (!manager || !weapon)
        {
            pending = {};
            return;
        }

        constexpr RE::FormID LEFT_SLOT  = 0x13F43;
        constexpr RE::FormID RIGHT_SLOT = 0x13F42;

        auto* targetSlot =
            RE::TESForm::LookupByID<RE::BGSEquipSlot>(
                pending.targetLeft ? LEFT_SLOT : RIGHT_SLOT
            );

        auto* otherSlot =
            RE::TESForm::LookupByID<RE::BGSEquipSlot>(
                pending.targetLeft ? RIGHT_SLOT : LEFT_SLOT
            );

        if (!targetSlot || !otherSlot)
        {
            pending = {};
            return;
        }

        const auto now =
            std::chrono::steady_clock::now();

        const float elapsed =
            std::chrono::duration<float>(
                now - pending.startTime
            ).count();

        const float phaseElapsed =
            std::chrono::duration<float>(
                now - pending.phaseTime
            ).count();

        // ============================================================
        // FINALIZAÇÃO
        // ============================================================

        auto finish = [&]()
        {
            pending.phase = 4;
            pending.phaseTime = now;
            pending.attempts = 0;
            pending.redrawRequested = false;
        };

        // Não abandona a operação antes de tentar restaurar
        // o estado original das armas.
        if (pending.phase != 4 && elapsed >= 14.0f)
        {
            spdlog::warn(
                "WEAPON TIMEOUT | phase={} | uniqueID={}",
                pending.phase,
                pending.uniqueID
            );

            finish();
            return;
        }

        // ============================================================
        // IDENTIFICAÇÃO DA INSTÂNCIA
        //
        // Retornos:
        //  1 = instância confirmada na mão
        //  0 = instância não está naquela mão
        // -1 = identidade temporariamente indisponível
        // ============================================================

        auto getInstanceState =
            [&](bool leftHand) -> int
        {
            auto* equipped =
                actor->GetEquippedObject(leftHand);

            if (equipped != weapon)
                return 0;

            // Item sem identificação individual.
            if (!pending.hasUniqueID)
                return 1;

            // Obtém a instância diretamente do inventário.
            auto* selectedExtra =
                GetWeaponExtraForEquip(
                    actor,
                    weapon,
                    pending.uniqueID,
                    true
                );

            if (!selectedExtra)
                return -1;

            const bool wornHere =
                leftHand
                    ? selectedExtra->HasType<RE::ExtraWornLeft>()
                    : selectedExtra->HasType<RE::ExtraWorn>();

            const bool wornOpposite =
                leftHand
                    ? selectedExtra->HasType<RE::ExtraWorn>()
                    : selectedExtra->HasType<RE::ExtraWornLeft>();

            // Não confirma uma identidade contraditória.
            if (wornHere && wornOpposite)
                return -1;

            if (wornHere)
                return 1;

            // A mão contém uma arma do mesmo FormID.
            // Confirma se é outra instância identificada.
            auto* equippedExtra =
                GetEquippedWeaponExtraList(
                    actor,
                    leftHand
                );

            if (!equippedExtra)
                return -1;

            auto* equippedUnique =
                equippedExtra->GetByType<RE::ExtraUniqueID>();

            if (!equippedUnique)
                return -1;

            if (equippedUnique->uniqueID ==
                pending.uniqueID)
            {
                // O ID corresponde, mas o marcador da mão
                // ainda não está consistente.
                return -1;
            }

            // Existe outra instância equipada nesta mão.
            return 0;
        };

        const int targetState =
            getInstanceState(pending.targetLeft);

        const int otherState =
            getInstanceState(!pending.targetLeft);

        const bool onTarget =
            targetState == 1;

        const bool onOther =
            otherState == 1;

        // ============================================================
        // PHASE -1
        //
        // LIBERA BLOQUEADOR DE MÃO
        //
        // Arcos/bestas/duas-mãos ocupam ambas as mãos e escudos ocupam a
        // mão esquerda. A engine pode escolher a mão oposta se receber uma
        // solicitação one-handed sem que esse bloqueador seja removido antes.
        // ============================================================

        if (pending.phase == -1)
        {
            auto* blocker = RE::TESForm::LookupByID<RE::TESBoundObject>(
                pending.blockerFormID);

            if (!blocker)
            {
                spdlog::warn("WEAPON BLOCKER | form missing={:08X}",
                    pending.blockerFormID);
                finish();
                return;
            }

            const bool stillBlocking = pending.blockerIsTwoHanded
                ? actor->GetEquippedObject(true) == blocker ||
                    actor->GetEquippedObject(false) == blocker
                : actor->GetEquippedObject(pending.blockerLeft) == blocker;

            if (!stillBlocking)
            {
                pending.phase = pending.resumePhaseAfterBlocker;
                pending.phaseTime = now;
                pending.attempts = 0;
                pending.waitingForSheathe = false;
                spdlog::info("WEAPON BLOCKER RELEASED | form={:08X} | nextPhase={}",
                    pending.blockerFormID, pending.phase);
                return;
            }

            if (pending.attempts == 0)
            {
                // Preserva o comportamento seguro do equipador atual: espera
                // a transição de sacar/guardar terminar antes de trocar algo
                // que ocupa uma mão.
                if (!WaitForWeaponSheathe(actor, pending))
                    return;

                auto* blockerExtra = pending.blockerIsTwoHanded
                    ? nullptr
                    : GetEquippedWeaponExtraList(actor, pending.blockerLeft);
                const auto* blockerSlot = pending.blockerIsTwoHanded
                    ? nullptr
                    : targetSlot;

                pending.attempts = 1;
                pending.phaseTime = now;

                const bool sent = manager->UnequipObject(
                    actor,
                    blocker,
                    blockerExtra,
                    1,
                    blockerSlot,
                    false,
                    true,
                    true,
                    true,
                    nullptr);

                spdlog::info(
                    "WEAPON BLOCKER UNEQUIP | form={:08X} | type={} | sent={}",
                    pending.blockerFormID,
                    pending.blockerIsTwoHanded ? "two-handed" : "shield",
                    sent);
                return;
            }

            if (phaseElapsed >= 2.0f)
            {
                spdlog::warn("WEAPON BLOCKER TIMEOUT | form={:08X}",
                    pending.blockerFormID);
                finish();
            }

            return;
        }

        // ============================================================
        // PHASE 0
        //
        // DESEQUIPA A INSTÂNCIA DA MÃO SOLICITADA
        // ============================================================

        if (pending.phase == 0)
        {
            if (!pending.unequipOnly)
            {
                finish();
                return;
            }

            // A instância já saiu da mão solicitada.
            if (targetState == 0)
            {
                spdlog::info(
                    "WEAPON UNEQUIP COMPLETE | side={} | uniqueID={}",
                    pending.targetLeft ? "LEFT" : "RIGHT",
                    pending.uniqueID
                );

                finish();
                return;
            }

            // Não tenta desequipar enquanto a identidade
            // da instância estiver indisponível.
            if (targetState == -1)
            {
                if (phaseElapsed >= 2.0f)
                {
                    spdlog::warn(
                        "WEAPON UNEQUIP | identity unavailable | uniqueID={}",
                        pending.uniqueID
                    );

                    finish();
                }

                return;
            }

            // --------------------------------------------------------
            // AGUARDA A ARMA SER GUARDADA
            // --------------------------------------------------------

            if (pending.attempts == 0)
            {
                if (!WaitForWeaponSheathe(actor, pending))
                    return;

                // Evita continuar durante outra transição.
                if (actor->AsActorState()->GetWeaponState() !=
                    RE::WEAPON_STATE::kSheathed)
                {
                    return;
                }

                pending.attempts = 1;
                pending.phaseTime = now;

                const bool sent =
                    UnequipExactWeapon(
                        actor,
                        manager,
                        weapon,
                        pending.targetLeft,
                        pending.uniqueID,
                        pending.hasUniqueID
                    );

                spdlog::info(
                    "WEAPON UNEQUIP SENT | side={} | uniqueID={} | sent={}",
                    pending.targetLeft ? "LEFT" : "RIGHT",
                    pending.uniqueID,
                    sent
                );

                if (!sent)
                {
                    finish();
                }

                return;
            }

            // --------------------------------------------------------
            // AGUARDA A CONCLUSÃO
            //
            // Não repete UnequipObject automaticamente.
            // --------------------------------------------------------

            if (phaseElapsed >= 2.0f)
            {
                spdlog::warn(
                    "WEAPON UNEQUIP NOT CONFIRMED | side={} | uniqueID={}",
                    pending.targetLeft ? "LEFT" : "RIGHT",
                    pending.uniqueID
                );

                finish();
            }

            return;
        }

        // ============================================================
        // PHASE 1
        //
        // TRANSFERÊNCIA:
        // RETIRA A INSTÂNCIA DA MÃO DE ORIGEM
        // ============================================================

        if (pending.phase == 1)
        {
            // Instância já chegou ao destino.
            if (onTarget && otherState == 0)
            {
                pending.phase = 3;
                pending.phaseTime = now;
                pending.attempts = 0;
                return;
            }

            // --------------------------------------------------------
            // INSTÂNCIA SAIU DA ORIGEM
            // --------------------------------------------------------

            if (otherState == 0)
            {
                // Aguarda o estado de origem estabilizar.
                if (pending.attempts > 0 &&
                    phaseElapsed < 0.35f)
                {
                    return;
                }

                pending.phase = 2;
                pending.phaseTime = now;
                pending.attempts = 0;

                spdlog::info(
                    "WEAPON TRANSFER | source released | uniqueID={}",
                    pending.uniqueID
                );

                return;
            }

            // --------------------------------------------------------
            // IDENTIDADE DA ORIGEM INDISPONÍVEL
            // --------------------------------------------------------

            if (otherState == -1)
            {
                if (phaseElapsed >= 2.0f)
                {
                    spdlog::warn(
                        "WEAPON TRANSFER | source identity unavailable | uniqueID={}",
                        pending.uniqueID
                    );

                    finish();
                }

                return;
            }

            // --------------------------------------------------------
            // AGUARDA GUARDAR AS ARMAS
            // --------------------------------------------------------

            if (pending.attempts == 0)
            {
                if (!WaitForWeaponSheathe(actor, pending))
                    return;

                if (actor->AsActorState()->GetWeaponState() !=
                    RE::WEAPON_STATE::kSheathed)
                {
                    return;
                }

                pending.attempts = 1;
                pending.phaseTime = now;

                const bool sent =
                    UnequipExactWeapon(
                        actor,
                        manager,
                        weapon,
                        !pending.targetLeft,
                        pending.uniqueID,
                        pending.hasUniqueID
                    );

                spdlog::info(
                    "WEAPON TRANSFER UNEQUIP | from={} | uniqueID={} | sent={}",
                    pending.targetLeft ? "RIGHT" : "LEFT",
                    pending.uniqueID,
                    sent
                );

                if (!sent)
                {
                    finish();
                }

                return;
            }

            // --------------------------------------------------------
            // AGUARDA A INSTÂNCIA SAIR DA ORIGEM
            // --------------------------------------------------------

            if (phaseElapsed >= 2.0f)
            {
                spdlog::warn(
                    "WEAPON TRANSFER FAILED | source not released | uniqueID={}",
                    pending.uniqueID
                );

                finish();
            }

            return;
        }

        // ============================================================
        // PHASE 2
        //
        // EQUIPA A INSTÂNCIA NA MÃO SOLICITADA
        // ============================================================

        if (pending.phase == 2)
        {
            // Equipamento já confirmado.
            if (onTarget && otherState == 0)
            {
                pending.phase = 3;
                pending.phaseTime = now;
                pending.attempts = 0;
                return;
            }

            // A instância ainda está na outra mão.
            // Isso pode ocorrer logo após remover um arco/arma de duas mãos:
            // o próprio Skyrim autoequipa a única arma disponível antes de
            // chegarmos a esta fase. Não é uma falha; reaproveitamos a
            // transferência normal para levá-la à mão originalmente pedida.
            if (onOther)
            {
                pending.phase = 1;
                pending.phaseTime = now;
                pending.attempts = 0;
                pending.waitingForSheathe = false;

                spdlog::info(
                    "WEAPON AUTOEQUIP TRANSFER | from={} | to={} | uniqueID={}",
                    pending.targetLeft ? "RIGHT" : "LEFT",
                    pending.targetLeft ? "LEFT" : "RIGHT",
                    pending.uniqueID);
                return;
            }

            // Não equipa durante uma identidade incerta.
            if (targetState == -1 ||
                otherState == -1)
            {
                if (phaseElapsed >= 2.0f)
                {
                    spdlog::warn(
                        "WEAPON EQUIP | identity unavailable | uniqueID={}",
                        pending.uniqueID
                    );

                    finish();
                }

                return;
            }

            // --------------------------------------------------------
            // ENVIA A SOLICITAÇÃO UMA ÚNICA VEZ
            // --------------------------------------------------------

            if (pending.attempts == 0)
            {
                auto* selectedExtra =
                    GetWeaponExtraForEquip(
                        actor,
                        weapon,
                        pending.uniqueID,
                        pending.hasUniqueID
                    );

                if (pending.hasUniqueID &&
                    !selectedExtra)
                {
                    spdlog::warn(
                        "WEAPON EQUIP | instance missing | uniqueID={}",
                        pending.uniqueID
                    );

                    finish();
                    return;
                }

                // Confirma novamente os marcadores da instância.
                if (selectedExtra)
                {
                    const bool wornRight =
                        selectedExtra->HasType<RE::ExtraWorn>();

                    const bool wornLeft =
                        selectedExtra->HasType<RE::ExtraWornLeft>();

                    // Não equipa uma instância já marcada
                    // como equipada em alguma mão.
                    if (wornRight || wornLeft)
                    {
                        spdlog::warn(
                            "WEAPON EQUIP | instance already worn | uniqueID={} | left={} | right={}",
                            pending.uniqueID,
                            wornLeft,
                            wornRight
                        );

                        finish();
                        return;
                    }
                }

                pending.attempts = 1;
                pending.phaseTime = now;

                spdlog::info(
                    "WEAPON EQUIP REQUEST | side={} | uniqueID={}",
                    pending.targetLeft ? "LEFT" : "RIGHT",
                    pending.uniqueID
                );

                manager->EquipObject(
                    actor,
                    weapon,
                    selectedExtra,
                    1,
                    targetSlot,
                    false,
                    false,
                    true,
                    false
                );

                return;
            }

            // --------------------------------------------------------
            // AGUARDA O EQUIPAMENTO
            // --------------------------------------------------------

            if (phaseElapsed >= 2.0f)
            {
                spdlog::warn(
                    "WEAPON EQUIP NOT CONFIRMED | side={} | uniqueID={}",
                    pending.targetLeft ? "LEFT" : "RIGHT",
                    pending.uniqueID
                );

                finish();
            }

            return;
        }

        // ============================================================
        // PHASE 3
        //
        // CONFIRMA O EQUIPAMENTO E VERIFICA A OUTRA MÃO
        // ============================================================

        if (pending.phase == 3)
        {
            if (phaseElapsed < 0.35f)
                return;

            if (!onTarget || otherState != 0)
            {
                if (phaseElapsed >= 2.0f)
                {
                    spdlog::warn(
                        "WEAPON CONFIRM FAILED | uniqueID={}",
                        pending.uniqueID
                    );

                    finish();
                }

                return;
            }

            spdlog::info(
                "WEAPON EQUIP CONFIRMED | side={} | uniqueID={}",
                pending.targetLeft ? "LEFT" : "RIGHT",
                pending.uniqueID
            );

            // --------------------------------------------------------
            // VERIFICA A ARMA ORIGINAL DA OUTRA MÃO
            // --------------------------------------------------------

            if (!pending.previousWeaponWasEquipped ||
                pending.previousWeaponID == 0)
            {
                finish();
                return;
            }

            auto* previousWeapon =
                RE::TESForm::LookupByID<RE::TESObjectWEAP>(
                    pending.previousWeaponID
                );

            if (!previousWeapon)
            {
                finish();
                return;
            }

            // A arma anterior continua equipada.
            if (MatchesWeaponInstance(
                    actor,
                    previousWeapon,
                    !pending.targetLeft,
                    pending.previousUniqueID,
                    pending.previousHasUniqueID
                ))
            {
                finish();
                return;
            }

            // Não sobrescreve outra arma.
            if (actor->GetEquippedObject(
                    !pending.targetLeft))
            {
                finish();
                return;
            }

            // Não restaura uma arma que saiu do inventário.
            if (GetInventoryItemCount(
                    actor,
                    previousWeapon
                ) <= 0)
            {
                finish();
                return;
            }

            auto* previousExtra =
                GetWeaponExtraForEquip(
                    actor,
                    previousWeapon,
                    pending.previousUniqueID,
                    pending.previousHasUniqueID
                );

            if (pending.previousHasUniqueID &&
                !previousExtra)
            {
                finish();
                return;
            }

            // Não equipa a mesma instância nas duas mãos.
            if (MatchesWeaponInstance(
                    actor,
                    previousWeapon,
                    pending.targetLeft,
                    pending.previousUniqueID,
                    pending.previousHasUniqueID
                ))
            {
                finish();
                return;
            }

            spdlog::info(
                "WEAPON RESTORE REQUEST | side={} | uniqueID={}",
                pending.targetLeft ? "RIGHT" : "LEFT",
                pending.previousUniqueID
            );

            manager->EquipObject(
                actor,
                previousWeapon,
                previousExtra,
                1,
                otherSlot,
                false,
                false,
                true,
                false
            );

            pending.phase = 5;
            pending.phaseTime = now;

            return;
        }

        // ============================================================
        // PHASE 5
        //
        // AGUARDA RESTAURAR A ARMA DA OUTRA MÃO
        // ============================================================

        if (pending.phase == 5)
        {
            auto* previousWeapon =
                RE::TESForm::LookupByID<RE::TESObjectWEAP>(
                    pending.previousWeaponID
                );

            if (!previousWeapon)
            {
                finish();
                return;
            }

            if (MatchesWeaponInstance(
                    actor,
                    previousWeapon,
                    !pending.targetLeft,
                    pending.previousUniqueID,
                    pending.previousHasUniqueID
                ))
            {
                spdlog::info(
                    "WEAPON RESTORE COMPLETE | uniqueID={}",
                    pending.previousUniqueID
                );

                finish();
                return;
            }

            // Não repete EquipObject enquanto a engine
            // ainda pode estar processando a solicitação.
            if (phaseElapsed >= 2.0f)
            {
                spdlog::warn(
                    "WEAPON RESTORE NOT CONFIRMED | uniqueID={}",
                    pending.previousUniqueID
                );

                finish();
            }

            return;
        }

            
        // ============================================================
        // PHASE 4
        //
        // RESTAURA O ESTADO ORIGINAL DE COMBATE
        //
        // FUNCIONA TAMBÉM COM AS DUAS MÃOS VAZIAS.
        // ============================================================

        if (pending.phase == 4)
        {
            auto* state = actor->AsActorState();

            if (!state)
            {
                pending = {};
                return;
            }

            // ========================================================
            // O PERSONAGEM ESTAVA COM AS ARMAS GUARDADAS
            // ========================================================

            if (!pending.wasWeaponDrawn)
            {
                pending = {};
                return;
            }

            const auto weaponState =
                state->GetWeaponState();

            // ========================================================
            // JÁ ESTÁ EM MODO DE COMBATE
            // ========================================================

            if (weaponState == RE::WEAPON_STATE::kDrawn)
            {
                spdlog::info(
                    "WEAPON OPERATION COMPLETE | combat mode restored"
                );

                pending = {};
                return;
            }

            // ========================================================
            // AGUARDA A OPERAÇÃO DE EQUIPAMENTO ESTABILIZAR
            // ========================================================

            if (!pending.redrawRequested)
            {
                if (phaseElapsed < 0.25f)
                    return;

                pending.redrawRequested = true;
                pending.phaseTime = now;

                // Solicita sacar as armas novamente.
                //
                // Se não houver armas equipadas, solicita ao Skyrim
                // entrar em modo de combate com as mãos livres.

                actor->DrawWeaponMagicHands(true);

                spdlog::info(
                    "WEAPON REDRAW REQUEST | uniqueID={} | leftForm={:08X} | rightForm={:08X}",
                    pending.uniqueID,
                    actor->GetEquippedObject(true)
                        ? actor->GetEquippedObject(true)->GetFormID()
                        : 0,
                    actor->GetEquippedObject(false)
                        ? actor->GetEquippedObject(false)->GetFormID()
                        : 0
                );

                return;
            }

            // ========================================================
            // AGUARDA O SKYRIM ENTRAR EM MODO DE COMBATE
            // ========================================================

            if (weaponState == RE::WEAPON_STATE::kDrawn)
            {
                pending = {};
                return;
            }

            // ========================================================
            // TIMEOUT
            // ========================================================

            if (phaseElapsed >= 3.0f)
            {
                spdlog::warn(
                    "WEAPON REDRAW TIMEOUT | state={} | uniqueID={}",
                    static_cast<int>(weaponState),
                    pending.uniqueID
                );

                pending = {};
            }

            return;
        }

        // ============================================================
        // FASE INVÁLIDA
        // ============================================================

        spdlog::warn(
            "WEAPON INVALID PHASE | phase={}",
            pending.phase
        );

        finish();
    }

    void UpdateWeaponStateDebug()
    {
        static RE::FormID lastLeft = 0;
        static RE::FormID lastRight = 0;

        static bool initialized = false;

        auto* actor =
            RE::PlayerCharacter::GetSingleton();

        if (!actor)
        {
            initialized = false;
            return;
        }

        auto* left =
            actor->GetEquippedObject(true);

        auto* right =
            actor->GetEquippedObject(false);

        const RE::FormID leftID =
            left ? left->GetFormID() : 0;

        const RE::FormID rightID =
            right ? right->GetFormID() : 0;

        if (!initialized)
        {
            initialized = true;

            lastLeft = leftID;
            lastRight = rightID;

            Logger::GetSingleton().Print(
                "WEAPON MONITOR INIT | "
                "left={:08X} | right={:08X}",
                leftID,
                rightID
            );

            return;
        }

        if (leftID == lastLeft &&
            rightID == lastRight)
        {
            return;
        }

        Logger::GetSingleton().Print(
            "WEAPON STATE CHANGED | "
            "LEFT {:08X} -> {:08X} | "
            "RIGHT {:08X} -> {:08X}",
            lastLeft,
            leftID,
            lastRight,
            rightID
        );

        lastLeft = leftID;
        lastRight = rightID;
    }

    static RE::ExtraDataList* FindWeaponExtraByUniqueID(
        RE::Actor* actor,
        RE::TESObjectWEAP* weapon,
        std::uint16_t uniqueID)
    {
        if (!actor || !weapon)
            return nullptr;

        const auto inventory = actor->GetInventory();

        const auto it = inventory.find(weapon);

        if (it == inventory.end())
            return nullptr;

        auto* entry = it->second.second.get();

        if (!entry || !entry->extraLists)
            return nullptr;

        for (auto* extra : *entry->extraLists)
        {
            if (!extra)
                continue;

            auto* unique =
                extra->GetByType<RE::ExtraUniqueID>();

            if (!unique)
                continue;

            if (unique->uniqueID == uniqueID)
                return extra;
        }

        return nullptr;
    }


    static bool GetEquippedWeaponIdentity(
        RE::Actor* actor,
        bool leftHand,
        RE::FormID& formID,
        std::uint16_t& uniqueID,
        bool& hasUniqueID)
    {
        formID = 0;
        uniqueID = 0;
        hasUniqueID = false;

        if (!actor)
            return false;

        auto* weapon =
            actor->GetEquippedObject(leftHand);

        if (!weapon)
            return false;

        formID = weapon->GetFormID();

        auto* extra =
            GetEquippedWeaponExtraList(
                actor,
                leftHand
            );

        if (extra)
        {
            auto* unique =
                extra->GetByType<RE::ExtraUniqueID>();

            if (unique)
            {
                uniqueID = unique->uniqueID;
                hasUniqueID = true;
            }
        }

        return true;
    }

    
    void UseNormalWeapon(
        RE::Actor* actor,
        RE::ActorEquipManager* equipManager,
        RE::TESObjectWEAP* weapon,
        bool leftSide,
        std::uint16_t uniqueID = 0,
        bool hasUniqueID = false)
    {
        if (!actor || !equipManager || !weapon)
            return;

        constexpr RE::FormID LEFT_SLOT  = 0x13F43;
        constexpr RE::FormID RIGHT_SLOT = 0x13F42;

        auto* targetSlot =
            RE::TESForm::LookupByID<RE::BGSEquipSlot>(
                leftSide ? LEFT_SLOT : RIGHT_SLOT
            );

        if (!targetSlot)
            return;

        // ============================================================
        // 1. EVITA OPERAÇÕES SIMULTÂNEAS
        // ============================================================

        if (g_pendingNormalWeaponEquip.active)
        {
            spdlog::info(
                "WEAPON EQUIP | pending operation active"
            );

            return;
        }

        // ============================================================
        // 2. VERIFICA INVENTÁRIO
        // ============================================================

        const int count =
            GetInventoryItemCount(
                actor,
                weapon
            );

        if (count <= 0)
            return;

        // ============================================================
        // 3. LOCALIZA A INSTÂNCIA SELECIONADA
        // ============================================================

        auto* selectedExtra =
            GetWeaponExtraForEquip(
                actor,
                weapon,
                uniqueID,
                hasUniqueID
            );

        // Nunca substitui silenciosamente uma instância
        // específica por outra arma com o mesmo FormID.

        if (hasUniqueID && !selectedExtra)
        {
            spdlog::warn(
                "WEAPON EQUIP | instance not found | form={:08X} | uniqueID={}",
                weapon->GetFormID(),
                uniqueID
            );

            return;
        }

        // ============================================================
        // 4. VERIFICA EM QUAL MÃO ESTÁ A INSTÂNCIA
        // ============================================================

        const bool onTarget =
            MatchesWeaponInstance(
                actor,
                weapon,
                leftSide,
                uniqueID,
                hasUniqueID
            );

        const bool onOther =
            MatchesWeaponInstance(
                actor,
                weapon,
                !leftSide,
                uniqueID,
                hasUniqueID
            );

        spdlog::info(
            "WEAPON EQUIP | form={:08X} | uniqueID={} | target={} | onTarget={} | onOther={} | count={}",
            weapon->GetFormID(),
            uniqueID,
            leftSide ? "LEFT" : "RIGHT",
            onTarget,
            onOther,
            count
        );

        // ============================================================
        // 5. PREPARA OPERAÇÃO PENDENTE
        // ============================================================

        PendingNormalWeaponEquip pending{};

        // ============================================================
        // SALVA O ESTADO ORIGINAL DAS ARMAS
        // ============================================================

        auto* actorState = actor->AsActorState();

        if (actorState)
        {
            pending.wasWeaponDrawn =
                actorState->IsWeaponDrawn();
        }

        pending.redrawRequested = false;

        //Continua

        pending.active = true;

        pending.actor =
            actor->GetHandle();

        pending.weaponID =
            weapon->GetFormID();

        pending.uniqueID =
            uniqueID;

        pending.hasUniqueID =
            hasUniqueID;

        pending.targetLeft =
            leftSide;

        pending.startTime =
            std::chrono::steady_clock::now();

        pending.phaseTime =
            pending.startTime;

        pending.phase = 0;
        pending.attempts = 0;

        // ============================================================
        // BLOQUEADORES DO SLOT
        //
        // Mantém todo o resolvedor atual de armas/unique IDs, mas abre uma
        // etapa anterior quando uma peça incompatível ocupa a mão solicitada.
        // ============================================================

        const auto assignTwoHandedBlocker = [&](RE::TESForm* equipped,
                                                 bool equippedLeft) {
            auto* equippedWeapon = equipped
                ? equipped->As<RE::TESObjectWEAP>()
                : nullptr;
            if (!IsTwoHandedWeapon(equippedWeapon))
                return false;

            pending.blockerFormID = equippedWeapon->GetFormID();
            pending.blockerIsTwoHanded = true;
            pending.blockerLeft = equippedLeft;
            return true;
        };

        const bool blockedByTwoHanded =
            assignTwoHandedBlocker(actor->GetEquippedObject(true), true) ||
            assignTwoHandedBlocker(actor->GetEquippedObject(false), false);

        if (!blockedByTwoHanded)
        {
            auto* targetEquipped = actor->GetEquippedObject(leftSide);
            auto* targetArmor = targetEquipped
                ? targetEquipped->As<RE::TESObjectARMO>()
                : nullptr;
            if (IsShield(targetArmor))
            {
                pending.blockerFormID = targetArmor->GetFormID();
                pending.blockerIsTwoHanded = false;
                pending.blockerLeft = leftSide;
            }
        }

        if (pending.blockerFormID != 0)
        {
            // Se a própria arma já estiver na outra mão, ela ainda precisa
            // passar pela transferência normal após liberar o bloqueador.
            pending.resumePhaseAfterBlocker = onOther ? 1 : 2;
            pending.phase = -1;
            pending.unequipOnly = false;
            g_pendingNormalWeaponEquip = pending;

            spdlog::info(
                "WEAPON BLOCKER DETECTED | form={:08X} | type={} | target={} | resumePhase={}",
                pending.blockerFormID,
                pending.blockerIsTwoHanded ? "two-handed" : "shield",
                leftSide ? "LEFT" : "RIGHT",
                pending.resumePhaseAfterBlocker);
            return;
        }

        // ============================================================
        // 6. MESMA INSTÂNCIA NA MÃO SOLICITADA
        //
        // Selecionar novamente = desequipar.
        // ============================================================

        if (onTarget)
        {
            pending.unequipOnly = true;

            g_pendingNormalWeaponEquip = pending;

            spdlog::info(
                "WEAPON ACTION | UNEQUIP | side={} | uniqueID={}",
                leftSide ? "LEFT" : "RIGHT",
                uniqueID
            );

            return;
        }

        // ============================================================
        // 7. MESMA INSTÂNCIA NA OUTRA MÃO
        //
        // Primeiro desequipa.
        // Depois equipa na mão solicitada.
        // ============================================================

        if (onOther)
        {
            pending.unequipOnly = false;

            // A fase 1 aguarda a arma sair da mão oposta.
            pending.phase = 1;

            g_pendingNormalWeaponEquip = pending;

            spdlog::info(
                "WEAPON ACTION | TRANSFER | from={} | to={} | uniqueID={}",
                leftSide ? "RIGHT" : "LEFT",
                leftSide ? "LEFT" : "RIGHT",
                uniqueID
            );

            return;
        }

        // ============================================================
        // 8. NOVA ARMA OU OUTRA INSTÂNCIA
        //
        // Registra a arma original da outra mão.
        // ============================================================

        auto* otherEquipped = actor->GetEquippedObject(!leftSide);
        auto* otherWeapon = otherEquipped
            ? otherEquipped->As<RE::TESObjectWEAP>()
            : nullptr;

        // Uma arma de duas mãos não é uma "arma da outra mão" a restaurar.
        // Ela já teria sido tratada como bloqueador acima; esta defesa evita
        // que alguma transição tardia a recoloque após uma troca one-handed.
        if (!IsTwoHandedWeapon(otherWeapon))
        {
            GetEquippedWeaponIdentity(
                actor,
                !leftSide,
                pending.previousWeaponID,
                pending.previousUniqueID,
                pending.previousHasUniqueID
            );
        }

        pending.previousWeaponWasEquipped =
            pending.previousWeaponID != 0;

        pending.unequipOnly = false;

        // Fase 2: equipa a instância selecionada.
        pending.phase = 2;

        g_pendingNormalWeaponEquip = pending;

        spdlog::info(
            "WEAPON ACTION | EQUIP | side={} | form={:08X} | uniqueID={}",
            leftSide ? "LEFT" : "RIGHT",
            weapon->GetFormID(),
            uniqueID
        );
    }
    
    
    static bool IsPhysicalHandItem(RE::TESForm* form)
    {
        if (!form)
            return false;
        if (form->As<RE::TESObjectWEAP>())
            return true;
        return IsShield(form->As<RE::TESObjectARMO>());
    }

    static RE::ExtraDataList* GetEquippedShieldExtraList(
        RE::Actor* actor,
        RE::TESObjectARMO* shield)
    {
        if (!actor || !shield)
            return nullptr;

        const auto inventory = actor->GetInventory();
        const auto it = inventory.find(shield);
        if (it == inventory.end() || !it->second.second)
            return nullptr;

        auto* entry = it->second.second.get();
        if (!entry || !entry->extraLists)
            return nullptr;

        for (auto* extra : *entry->extraLists)
        {
            if (extra && extra->HasType<RE::ExtraWornLeft>())
                return extra;
        }
        return nullptr;
    }

    static void RequestClearPhysicalHand(
        RE::Actor* actor,
        RE::ActorEquipManager* manager,
        bool leftHand)
    {
        if (!actor || !manager)
            return;

        auto* form = actor->GetEquippedObject(leftHand);
        if (!IsPhysicalHandItem(form))
            return;

        constexpr RE::FormID LEFT_SLOT = 0x13F43;
        constexpr RE::FormID RIGHT_SLOT = 0x13F42;
        const auto* slot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(
            leftHand ? LEFT_SLOT : RIGHT_SLOT);
        if (!slot)
            return;

        if (auto* weapon = form->As<RE::TESObjectWEAP>())
        {
            // Sem slot: para armas de uma mão, informar LEFT/RIGHT faz o
            // Skyrim tentar preservar a arma transferindo-a para a outra mão.
            // Aqui queremos removê-la por completo antes do arco/besta.
            manager->UnequipObject(
                actor, weapon, GetEquippedWeaponExtraList(actor, leftHand), 1,
                nullptr, false, false, true, true, nullptr);
            return;
        }

        if (auto* shield = form->As<RE::TESObjectARMO>(); IsShield(shield))
        {
            manager->UnequipObject(
                actor, shield, GetEquippedShieldExtraList(actor, shield), 1,
                slot, false, false, true, true, nullptr);
        }
    }

    static bool ArePhysicalHandsClear(RE::Actor* actor)
    {
        return actor &&
            !IsPhysicalHandItem(actor->GetEquippedObject(true)) &&
            !IsPhysicalHandItem(actor->GetEquippedObject(false));
    }

    void UpdatePendingTwoHandedWeaponEquip()
    {
        auto& pending = g_pendingTwoHandedWeaponEquip;
        if (pending.stage == TwoHandedEquipStage::None)
            return;

        auto actorPtr = pending.actor.get();
        auto* actor = actorPtr ? actorPtr.get() : nullptr;
        auto* manager = RE::ActorEquipManager::GetSingleton();
        auto* weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(pending.weaponID);
        if (!actor || !manager || !weapon)
        {
            pending = {};
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const float elapsed = std::chrono::duration<float>(now - pending.startTime).count();
        const float stageElapsed = std::chrono::duration<float>(now - pending.stageTime).count();
        if (elapsed >= 8.0f)
        {
            spdlog::warn("TWO-HANDED EQUIP TIMEOUT | form={:08X}", pending.weaponID);
            pending = {};
            return;
        }

        if (pending.stage == TwoHandedEquipStage::WaitingForSheathe)
        {
            if (actor->IsWeaponDrawn())
            {
                if (!pending.sheatheRequested)
                {
                    pending.sheatheRequested = true;
                    actor->DrawWeaponMagicHands(false);
                    spdlog::info("TWO-HANDED SHEATHE REQUEST | form={:08X}", pending.weaponID);
                }
                return;
            }

            // Ao equipar arco/besta, o próprio EquipObject substitui a arma
            // atual. Tentar limpar primeiro uma espada na mão esquerda faz o
            // Skyrim migrá-la visualmente para a direita.
            pending.stage = pending.unequipOnly
                ? TwoHandedEquipStage::WaitingForBowUnequip
                : TwoHandedEquipStage::WaitingForEquip;
            pending.stageTime = now;
            return;
        }

        if (pending.stage == TwoHandedEquipStage::WaitingForBowUnequip)
        {
            if (!pending.equipRequested)
            {
                pending.equipRequested = true;
                pending.stageTime = now;
                manager->UnequipObject(actor, weapon, nullptr, 1, nullptr,
                    false, false, true, true, nullptr);
                spdlog::info("TWO-HANDED UNEQUIP REQUEST | form={:08X}", pending.weaponID);
                return;
            }

            if (actor->GetEquippedObject(false) == weapon ||
                actor->GetEquippedObject(true) == weapon)
                return;

            // O Skyrim pode restaurar a arma anterior um frame depois de
            // remover uma arma de duas mãos. Como o arco já está guardado,
            // limpamos esse retorno sem o flash de uma arma sacada.
            pending.stage = TwoHandedEquipStage::WaitingForHandsClear;
            pending.clearRequested = false;
            pending.stageTime = now;
            return;
        }

        if (pending.stage == TwoHandedEquipStage::WaitingForHandsClear)
        {
            const bool handsClear = ArePhysicalHandsClear(actor);

            // Durante o desligamento do arco, o Skyrim pode tentar restaurar
            // o último equipamento por alguns frames. Mantemos tudo guardado
            // e removemos somente o que ele realmente recolocar.
            if (!handsClear)
            {
                if (actor->IsWeaponDrawn())
                    actor->DrawWeaponMagicHands(false);

                // O Skyrim pode mover uma arma da mão esquerda para a direita
                // depois do primeiro UnequipObject. Revalida em intervalos
                // curtos até as duas mãos físicas estarem realmente vazias.
                if (!pending.clearRequested || stageElapsed >= 0.12f)
                {
                    auto* left = actor->GetEquippedObject(true);
                    auto* right = actor->GetEquippedObject(false);
                    // Uma arma de duas mãos pode ser reportada nas duas mãos;
                    // nesse caso uma única solicitação sem slot é suficiente.
                    if (left && left == right && left->As<RE::TESObjectWEAP>())
                    {
                        manager->UnequipObject(
                            actor, left->As<RE::TESObjectWEAP>(), nullptr, 1,
                            nullptr, false, false, true, true, nullptr);
                    }
                    else
                    {
                        RequestClearPhysicalHand(actor, manager, true);
                        RequestClearPhysicalHand(actor, manager, false);
                    }
                    pending.clearRequested = true;
                    pending.stageTime = now;
                    spdlog::info("TWO-HANDED CLEAR RESTORED HANDS | form={:08X} | left={:08X} | right={:08X}",
                        pending.weaponID,
                        left ? left->GetFormID() : 0,
                        right ? right->GetFormID() : 0);
                }
                return;
            }

            if (pending.unequipOnly)
            {
                // Mantém uma janela curta livre de itens físicos. Isso pega
                // a restauração tardia sem mostrar uma espada sacada.
                if (stageElapsed < 0.35f)
                    return;

                if (pending.wasWeaponDrawn)
                {
                    pending.stage = TwoHandedEquipStage::WaitingForFinalState;
                    pending.finalStateRequested = false;
                    pending.stageTime = now;
                    return;
                }

                pending = {};
                return;
            }

            pending.stage = TwoHandedEquipStage::WaitingForEquip;
            pending.stageTime = now;
            return;
        }

        if (pending.stage == TwoHandedEquipStage::WaitingForEquip)
        {
            if (!pending.equipRequested)
            {
                pending.equipRequested = true;
                pending.stageTime = now;
                manager->EquipObject(actor, weapon, nullptr, 1, nullptr,
                    false, false, true, false);
                spdlog::info("TWO-HANDED EQUIP REQUEST | form={:08X}", pending.weaponID);
                return;
            }

            if (actor->GetEquippedObject(false) != weapon)
                return;

            pending.stage = TwoHandedEquipStage::WaitingForFinalState;
            pending.stageTime = now;
            return;
        }

        // Preserva o estado de sacar/guardar que existia antes do fluxo.
        if (pending.stage == TwoHandedEquipStage::WaitingForFinalState)
        {
            const bool isDrawn = actor->IsWeaponDrawn();
            if (pending.wasWeaponDrawn == isDrawn)
            {
                pending = {};
                return;
            }

            if (!pending.finalStateRequested && stageElapsed >= 0.20f)
            {
                pending.finalStateRequested = true;
                pending.stageTime = now;
                actor->DrawWeaponMagicHands(pending.wasWeaponDrawn);
                return;
            }
        }
    }

    static void RequestTwoHandedWeaponEquip(
        RE::Actor* actor,
        RE::TESObjectWEAP* weapon)
    {
        if (!actor || !weapon || g_pendingTwoHandedWeaponEquip.stage != TwoHandedEquipStage::None)
            return;

        // Uma seleção de arco substitui uma troca one-handed ainda em curso.
        // Isso impede que uma solicitação antiga restaure a espada depois.
        g_pendingNormalWeaponEquip = {};

        auto& pending = g_pendingTwoHandedWeaponEquip;
        pending = {};
        pending.stage = TwoHandedEquipStage::WaitingForSheathe;
        pending.actor = actor->GetHandle();
        pending.weaponID = weapon->GetFormID();
        pending.wasWeaponDrawn = actor->IsWeaponDrawn();
        pending.startTime = std::chrono::steady_clock::now();
        pending.stageTime = pending.startTime;

        spdlog::info("TWO-HANDED EQUIP START | form={:08X} | drawn={}",
            pending.weaponID, pending.wasWeaponDrawn);
    }

    static void RequestTwoHandedWeaponUnequip(
        RE::Actor* actor,
        RE::TESObjectWEAP* weapon)
    {
        if (!actor || !weapon ||
            g_pendingTwoHandedWeaponEquip.stage != TwoHandedEquipStage::None)
            return;

        // Um desequipamento do arco também cancela uma troca one-handed
        // ainda pendente, para que ela não reapareça após o arco sair.
        g_pendingNormalWeaponEquip = {};

        auto& pending = g_pendingTwoHandedWeaponEquip;
        pending = {};
        pending.stage = TwoHandedEquipStage::WaitingForSheathe;
        pending.actor = actor->GetHandle();
        pending.weaponID = weapon->GetFormID();
        pending.unequipOnly = true;
        pending.wasWeaponDrawn = actor->IsWeaponDrawn();
        pending.startTime = std::chrono::steady_clock::now();
        pending.stageTime = pending.startTime;

        spdlog::info("TWO-HANDED UNEQUIP START | form={:08X} | drawn={}",
            pending.weaponID, pending.wasWeaponDrawn);
    }

    static bool UnequipSpellFromHand(
        RE::Actor* actor,
        RE::SpellItem* spell,
        bool leftSide);

    static void RequestSpellHandUnequip(
        RE::Actor* actor,
        RE::SpellItem* spell,
        bool leftSide)
    {
        if (!actor || !spell)
            return;

        g_pendingSpellHandRefresh = {};

        const auto otherHandSlot = leftSide
            ? RE::Actor::SlotTypes::kRightHand
            : RE::Actor::SlotTypes::kLeftHand;
        auto& actorData = actor->GetActorRuntimeData();

        // Com somente a magia esquerda equipada, usa o nativo Papyrus com a
        // fonte esquerda explícita. Este teste mantém o desequipamento por
        // mão, sem DeselectSpell nem sinais artificiais do animation graph.
        if (leftSide && actorData.selectedSpells[otherHandSlot] == nullptr)
        {
            // O weapon state global não identifica de forma confiável uma
            // magia isolada à esquerda. Forçamos o ciclo das mãos para este
            // caso, inclusive quando o ator já reporta Sheathed.
            auto& pending = g_pendingSpellHandRefresh;
            pending = {};
            pending.stage = SpellHandRefreshStage::WaitingForSheathe;
            pending.actor = actor->GetHandle();
            pending.spellID = spell->GetFormID();
            pending.leftSide = true;
            pending.wasWeaponDrawn = true;
            pending.stageTime = std::chrono::steady_clock::now();

            actor->DrawWeaponMagicHands(false);
            spdlog::info("SPELL UNEQUIP | forced sheathe before sole left-hand Papyrus unequip | form={:08X}",
                spell->GetFormID());
            return;
        }

        // A mão direita (e casos com magia na outra mão) conserva o caminho
        // nativo já validado. A captura permanece ativa para diagnóstico.
        if (actorData.selectedSpells[otherHandSlot] == nullptr)
            BeginSpellUnequipAnimationCapture(actor);

        if (!UnequipSpellFromHand(actor, spell, leftSide))
        {
            spdlog::warn("SPELL UNEQUIP | failed to dispatch Actor.UnequipSpell | form={:08X}",
                spell->GetFormID());
        }
    }

    static bool UnequipSpellFromHand(
        RE::Actor* actor,
        RE::SpellItem* spell,
        bool leftSide)
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!actor || !spell || !vm)
            return false;

        auto* handlePolicy = vm->GetObjectHandlePolicy();
        if (!handlePolicy)
            return false;

        const auto actorHandle =
            handlePolicy->GetHandleForObject(actor->GetFormType(), actor);
        if (actorHandle == handlePolicy->EmptyHandle())
            return false;

        // Actor.UnequipSpell recebe a mão explicitamente (0 = esquerda,
        // 1 = direita). Diferente de DeselectSpell, ele não remove a mesma
        // magia da outra mão nem deixa o graph de magia sem a transição nativa.
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result;
        const auto source = static_cast<std::uint32_t>(leftSide ? 0 : 1);
        return vm->DispatchMethodCall(
            actorHandle,
            RE::BSFixedString("Actor"),
            RE::BSFixedString("UnequipSpell"),
            RE::MakeFunctionArguments(
                static_cast<RE::SpellItem*>(spell),
                static_cast<std::uint32_t>(source)),
            result);
    }

    static bool IsMagicHandTransitionActive(RE::Actor* actor)
    {
        if (!actor)
            return false;

        // Esses sinais existem nos graphs de personagem que usam a transição
        // padrão de equipar/guardar. Caso um graph customizado não os exponha,
        // a confirmação do slot vazio abaixo continua sendo o fallback.
        bool equipping = false;
        bool unequipping = false;
        const bool hasEquippingState = actor->GetGraphVariableBool(
            RE::BSFixedString("IsEquipping"), equipping);
        const bool hasUnequippingState = actor->GetGraphVariableBool(
            RE::BSFixedString("IsUnequipping"), unequipping);

        return (hasEquippingState && equipping) ||
               (hasUnequippingState && unequipping);
    }

    void UpdatePendingSpellHandRefresh()
    {
        auto& pending = g_pendingSpellHandRefresh;
        if (pending.stage == SpellHandRefreshStage::None)
            return;

        auto actorPtr = pending.actor.get();
        auto* actor = actorPtr ? actorPtr.get() : nullptr;
        auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(pending.spellID);
        if (!actor || !spell)
        {
            pending = {};
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const float elapsed = std::chrono::duration<float>(now - pending.stageTime).count();

        if (pending.stage == SpellHandRefreshStage::WaitingForSheathe)
        {
            // A magia esquerda isolada pode reportar Sheathed antes mesmo do
            // graph processar o DrawWeaponMagicHands(false). Aguardamos uma
            // janela fixa em vez de confiar nesse estado global.
            constexpr float forcedSheatheDelay = 0.35f;
            if (elapsed < forcedSheatheDelay)
                return;

            // Usa a API nativa do Actor com a mão alvo. DeselectSpell é
            // global por magia e podia remover ambas as mãos sem concluir a
            // transição de animação da mão que acabou vazia.
            if (!UnequipSpellFromHand(actor, spell, pending.leftSide))
            {
                spdlog::warn("SPELL UNEQUIP | failed to dispatch Actor.UnequipSpell | form={:08X}",
                    pending.spellID);
                pending = {};
                return;
            }

            // Sem uma magia na outra mão, o Skyrim precisa de um frame curto
            // para consolidar as mãos vazias antes do redraw.
            pending.spellChangeApplied = true;
            pending.stage = SpellHandRefreshStage::WaitingForRedraw;
            pending.stageTime = now;
            return;
        }

        if (pending.stage == SpellHandRefreshStage::WaitingForRedraw)
        {
            // Este estágio também é usado quando o personagem já estava com
            // as mãos guardadas: nesse caso não há animação para redesenhar.
            if (!pending.spellChangeApplied)
            {
                if (!UnequipSpellFromHand(actor, spell, pending.leftSide))
                {
                    spdlog::warn("SPELL UNEQUIP | failed to dispatch Actor.UnequipSpell | form={:08X}",
                        pending.spellID);
                    pending = {};
                    return;
                }
                pending.spellChangeApplied = true;
                pending.stageTime = now;
                return;
            }

            if (!pending.wasWeaponDrawn)
            {
                pending = {};
                return;
            }

            if (!pending.redrawRequested)
            {
                const auto handSlot = pending.leftSide
                    ? RE::Actor::SlotTypes::kLeftHand
                    : RE::Actor::SlotTypes::kRightHand;
                const bool stillEquipped =
                    actor->GetActorRuntimeData().selectedSpells[handSlot] == spell;

                // Não usamos mais um atraso fixo. A transição só prossegue
                // quando o Papyrus limpou o slot e o graph está estável por
                // alguns frames consecutivos, evitando sacar no meio do
                // guardar e provocar o flick visual.
                const bool safeToRedraw =
                    !stillEquipped &&
                    !actor->IsWeaponDrawn() &&
                    !IsMagicHandTransitionActive(actor);
                if (!safeToRedraw)
                {
                    pending.stableEmptyHandFrames = 0;
                    if (elapsed >= 2.0f)
                    {
                        spdlog::warn("SPELL UNEQUIP | hand did not reach a stable empty state | form={:08X}",
                            pending.spellID);
                        pending = {};
                    }
                    return;
                }

                constexpr std::uint8_t stableFramesRequired = 4;
                if (++pending.stableEmptyHandFrames < stableFramesRequired)
                    return;

                pending.redrawRequested = true;
                pending.stageTime = now;
                actor->DrawWeaponMagicHands(true);
                return;
            }

            if (!actor->IsWeaponDrawn() && elapsed < 2.0f)
                return;
        }

        pending = {};
    }

    void UpdatePendingWeaponAction()
    {
        auto& pending = g_pendingWeaponAction;

        if (pending.stage == WeaponActionStage::None)
            return;

        auto actorPtr = pending.actor.get();

        if (!actorPtr)
        {
            pending = {};
            return;
        }

        auto* actor = actorPtr.get();

        auto* manager =
            RE::ActorEquipManager::GetSingleton();

        auto* weapon =
            RE::TESForm::LookupByID<RE::TESObjectWEAP>(
                pending.weaponID
            );

        if (!manager || !weapon)
        {
            pending = {};
            return;
        }

        const auto now =
            std::chrono::steady_clock::now();

        const float totalElapsed =
            std::chrono::duration<float>(
                now - pending.startTime
            ).count();

        const float stageElapsed =
            std::chrono::duration<float>(
                now - pending.stageTime
            ).count();

        constexpr float SHEATHE_DELAY = 0.25f;
        constexpr float EQUIP_DELAY = 0.35f;
        constexpr float TIMEOUT = 5.0f;

        // ============================================================
        // TIMEOUT GERAL
        // ============================================================

        if (totalElapsed >= TIMEOUT)
        {
            Logger::GetSingleton().Print(
                "WEAPON ACTION | TIMEOUT"
            );

            pending = {};
            return;
        }

        // ============================================================
        // 1. AGUARDANDO GUARDAR AS ARMAS
        // ============================================================

        if (pending.stage ==
            WeaponActionStage::WaitingForSheathe)
        {
            // Ainda está sacando ou guardando.
            if (actor->IsWeaponDrawn())
                return;

            // Aguarda um pequeno intervalo adicional
            // para o estado de animação estabilizar.
            if (stageElapsed < SHEATHE_DELAY)
                return;

            Logger::GetSingleton().Print(
                "WEAPON ACTION | SHEATHE COMPLETE"
            );

            // Executa sua lógica original.
            UseNormalWeapon(
                actor,
                manager,
                weapon,
                pending.leftSide
            );

            pending.stage =
                WeaponActionStage::WaitingForEquip;

            pending.stageTime = now;

            return;
        }

        // ============================================================
        // 2. AGUARDANDO EQUIPAMENTO
        // ============================================================

        if (pending.stage ==
            WeaponActionStage::WaitingForEquip)
        {
            // A transferência de uma única cópia
            // ainda está em andamento.
            if (g_pendingWeaponSwitch.active)
                return;

            // Aguarda a operação de equipamento
            // terminar de processar.
            if (stageElapsed < EQUIP_DELAY)
                return;

            Logger::GetSingleton().Print(
                "WEAPON ACTION | EQUIP COMPLETE"
            );

            const bool redraw = pending.wasDrawn;

            pending = {};

            // Restaura o estado de combate original.
            if (redraw && !actor->IsWeaponDrawn())
            {
                actor->DrawWeaponMagicHands(true);
            }
        }
    }


    static void RequestNormalWeapon(
        RE::Actor* actor,
        RE::ActorEquipManager* manager,
        RE::TESObjectWEAP* weapon,
        bool leftSide)
    {
        if (!actor || !manager || !weapon)
            return;

        // Impede operações simultâneas.
        if (g_pendingWeaponAction.stage !=
            WeaponActionStage::None)
        {
            return;
        }

        if (g_pendingWeaponSwitch.active)
            return;

        // Se as armas estão guardadas,
        // utiliza o sistema original.
        if (!actor->IsWeaponDrawn())
        {
            UseNormalWeapon(
                actor,
                manager,
                weapon,
                leftSide
            );

            return;
        }

        // ============================================================
        // ARMAS SACADAS
        // ============================================================

        g_pendingWeaponAction = {};

        auto& pending = g_pendingWeaponAction;

        pending.stage =
            WeaponActionStage::WaitingForSheathe;

        pending.actor = actor->GetHandle();

        pending.weaponID = weapon->GetFormID();

        pending.leftSide = leftSide;
        pending.wasDrawn = true;

        pending.startTime =
            std::chrono::steady_clock::now();

        pending.stageTime = pending.startTime;

        // Solicita que o personagem guarde as armas.
        //actor->AsActorState()->actorState1.weaponState =
        //    RE::WEAPON_STATE::kSheathing;

        actor->DrawWeaponMagicHands(false);

        Logger::GetSingleton().Print(
            "WEAPON ACTION | SHEATHE REQUESTED"
        );
    }

    static RE::ExtraDataList* GetArmorExtraByUniqueID(
        RE::Actor* actor,
        RE::TESObjectARMO* armor,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        if (!actor || !armor || !hasUniqueID)
            return nullptr;

        const auto inventory = actor->GetInventory();

        const auto it = inventory.find(armor);

        if (it == inventory.end())
            return nullptr;

        const auto& [count, entryData] = it->second;

        if (!entryData || !entryData->extraLists)
            return nullptr;

        for (auto* extra : *entryData->extraLists)
        {
            if (!extra)
                continue;

            auto* unique =
                extra->GetByType<RE::ExtraUniqueID>();

            if (!unique)
                continue;

            if (unique->uniqueID == uniqueID)
            {
                return extra;
            }
        }

        return nullptr;
    }

    void UseSelectedRadialItem()
    {
        if (g_radialSide == RadialSide::None && !g_quickDrawSelectionOverride)
            return;

        ImVec2 mousePos = GetRadialMousePosition();

        // O Quick Draw já encontrou a instância exata; isso evita depender
        // da posição final do cursor no radial correspondente.
        RadialItem* selected = g_quickDrawSelectionOverride;

        // ============================================================
        // DESCOBRE QUAL ITEM ESTÁ SELECIONADO
        // ============================================================

        if (!selected) switch (g_radialSide)
        {
        case RadialSide::Left:
        case RadialSide::Right:
        {
            const int totalItems = static_cast<int>(g_sideItems.size());

            if (totalItems <= 0)
                break;

            const int visibleCount = std::min(totalItems, GetSideVisibleLimit());

            const WheelLayout wheelLayout = GetWheelLayout();
            const ImVec2 sideCenter =
                (g_radialSide == RadialSide::Left)
                ? wheelLayout.leftRadial
                : wheelLayout.rightRadial;

            const int visibleIndex = GetSideRadialItem(
                mousePos,
                sideCenter,
                g_radialSide == RadialSide::Left,
                visibleCount,
                MENU_INNER_RADIUS);

            if (visibleIndex >= 0)
            {
                //const int actualIndex = g_sideScrollOffset + visibleIndex;

                const int actualIndex =
                    GetSideActualIndex(visibleIndex);


                if (actualIndex >= 0 && actualIndex < static_cast<int>(g_sideItems.size()))
                {
                    selected = &g_sideItems[actualIndex];
                }
            }

            break;
        }

        case RadialSide::Top:
        {
            const int totalItems =
                static_cast<int>(g_topItems.size());

            if (totalItems <= 0)
                break;

            // Nenhuma seleção ativa.
            if (!g_topHasSelection)
                break;

            const int actualIndex =
                WrapIndex(
                    g_topSelectedIndex,
                    totalItems
                );

            selected =
                &g_topItems[actualIndex];

            break;
        }

        case RadialSide::Bottom:
        {
            const int totalItems =
                static_cast<int>(g_bottomItems.size());

            if (totalItems <= 0)
                break;

            // Nenhuma seleção ativa.
            if (!g_bottomHasSelection)
                break;

            const int actualIndex =
                WrapIndex(
                    g_bottomSelectedIndex,
                    totalItems
                );

            selected =
                &g_bottomItems[actualIndex];

            break;
        }

        default:
            return;
        }

        // ============================================================
        // VALIDA
        // ============================================================

        if (!selected || !selected->valid || !selected->form)
        {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* equipManager = RE::ActorEquipManager::GetSingleton();

        if (!player || !equipManager)
            return;

        RE::Actor* actor = static_cast<RE::Actor*>(player);

        RE::TESForm* form = selected->form;

        const bool quickDrawSelection = g_quickDrawSelectionOverride != nullptr;
        
        const RadialSide actionSide = quickDrawSelection
            ? g_quickDrawSelectionSide
            : g_radialSide;
        
        // O gesto do Quick Draw escolhe explicitamente a mão pelo botão que
        // iniciou o traço, independentemente do radial que continha o item.
        bool leftSide = quickDrawSelection
            ? g_quickDrawGameplayEquipLeft
            : actionSide == RadialSide::Left;

        const bool topBottomSelection =
            actionSide == RadialSide::Top || actionSide == RadialSide::Bottom;
        if (topBottomSelection && !quickDrawSelection)
        {
            // O clique tem a maior prioridade e ignora tanto o estado das
            // mãos quanto o sentido do scroll.
            if (g_topBottomHandOverride != TopBottomHandOverride::None)
            {
                leftSide = g_topBottomHandOverride == TopBottomHandOverride::Left;
            }
            else
            {
                // Sem clique, o sentido do scroll decide sempre a mão:
                // baixo usa a esquerda; cima, a direita. Não há exceção
                // baseada em uma mão já ocupada ou vazia.
                leftSide = g_topBottomLastScrollEquipLeft;
            }
        }

        // ============================================================
        // LOCKPICKS (faltava esse ramo por completo)
        // ============================================================

        if (IsLockpick(form))
        {
            UseLockpickFromRadial();
            return;
        }

        // ============================================================
        // LIVROS
        // ============================================================

        if (auto* book = form->As<RE::TESObjectBOOK>())
        {
            UseBook(book);
            return;
        }

        // ============================================================
        // PERGAMINHOS (faltava ramo dedicado; caía no genérico)
        // ============================================================

        if (auto* scroll = form->As<RE::ScrollItem>())
        {
            UseScrollItem(scroll, leftSide);
            return;
        }

        // ============================================================
        // POÇÕES / VENENOS / COMIDA (faltava ramo dedicado)
        // ============================================================

        if (auto* alchemy = form->As<RE::AlchemyItem>())
        {
            UseAlchemyItem(alchemy);
            return;
        }

        // ============================================================
        // MAGIA
        // ============================================================

        if (auto* spell = form->As<RE::SpellItem>())
        {
            // Usa o MESMO mapeamento dos radiais laterais usado por
            // UseNormalWeapon(): esquerdo -> 0x13F43, direito -> 0x13F42.
            // Isso evita equipar a magia na mão oposta à indicada pelo radial.
            constexpr RE::FormID SLOT_FOR_LEFT_RADIAL  = 0x13F43;
            constexpr RE::FormID SLOT_FOR_RIGHT_RADIAL = 0x13F42;

            const RE::FormID slotID =
                leftSide ? SLOT_FOR_LEFT_RADIAL : SLOT_FOR_RIGHT_RADIAL;

            const RE::BGSEquipSlot* spellSlot =
                RE::TESForm::LookupByID<RE::BGSEquipSlot>(slotID);

            if (!spellSlot)
                return;

            // EquipSpell chama uma função relocada do engine; como este
            // caminho nasce do draw/render hook, despachamos a operação para
            // a thread principal, igual ao tratamento já usado para livros.
            // UnequipSpell recebe a mão alvo e preserva a outra mão quando
            // ambas usam a mesma magia.
            const auto handSlot = leftSide
                ? RE::Actor::SlotTypes::kLeftHand
                : RE::Actor::SlotTypes::kRightHand;
            auto equipSpell = [actor, spell, spellSlot, handSlot, leftSide]()
            {
                auto* manager = RE::ActorEquipManager::GetSingleton();
                if (manager)
                {
                    auto& actorData = actor->GetActorRuntimeData();
                    const bool alreadyEquipped =
                        actorData.selectedSpells[handSlot] == spell;
                    if (alreadyEquipped)
                    {
                        RequestSpellHandUnequip(actor, spell, leftSide);
                    }
                    else
                        manager->EquipSpell(actor, spell, spellSlot);
                }
            };

            if (auto* taskInterface = SKSE::GetTaskInterface())
            {
                taskInterface->AddTask(equipSpell);
            }
            else
            {
                equipSpell();
            }

            return;
        }

        // ============================================================
        // GRITO
        // ============================================================

        if (auto* shout = form->As<RE::TESShout>())
        {
            auto equipShout = [actor, shout]()
            {
                auto* manager = RE::ActorEquipManager::GetSingleton();
                if (manager)
                {
                    manager->EquipShout(actor, shout);
                }
            };

            if (auto* taskInterface = SKSE::GetTaskInterface())
            {
                taskInterface->AddTask(equipShout);
            }
            else
            {
                equipShout();
            }

            return;
        }

        // ============================================================
        // ARMADURAS / ESCUDOS / ANÉIS / AMULETOS
        // ============================================================

        if (auto* armor = form->As<RE::TESObjectARMO>())
        {
            // ========================================================
            // IDENTIFICA A INSTÂNCIA
            // ========================================================

            auto* extra = GetArmorExtraByUniqueID(
                actor,
                armor,
                selected->uniqueID,
                selected->hasUniqueID
            );

            // Se o radial identifica uma instância específica,
            // não devemos equipar outra por engano.

            if (selected->hasUniqueID && !extra)
                return;

            // ========================================================
            // ESCUDO
            // ========================================================

            if (IsShield(armor))
            {
                constexpr RE::FormID LEFT_HAND_SLOT = 0x13F43;

                auto* leftHandSlot =
                    RE::TESForm::LookupByID<RE::BGSEquipSlot>(
                        LEFT_HAND_SLOT
                    );

                if (!leftHandSlot)
                    return;

                // Verifica se esta instância específica está equipada.

                const bool equipped = IsArmorEquipped(
                    actor,
                    armor,
                    selected->uniqueID,
                    selected->hasUniqueID
                );

                if (equipped)
                {
                    // Desequipa somente o escudo selecionado.

                    equipManager->UnequipObject(
                        actor,
                        armor,
                        extra,
                        1,
                        leftHandSlot,
                        true,
                        false,
                        true,
                        true
                    );
                }
                else
                {
                    // Equipa o escudo na mão esquerda.

                    equipManager->EquipObject(
                        actor,
                        armor,
                        extra,
                        1,
                        leftHandSlot,
                        true,
                        false,
                        true,
                        true
                    );
                }

                return;
            }

            // ========================================================
            // ARMADURA NORMAL / ANÉIS / AMULETOS
            //
            // Não utilizamos slots LEFT/RIGHT.
            // ========================================================

            const bool equipped = IsArmorEquipped(
                actor,
                armor,
                selected->uniqueID,
                selected->hasUniqueID
            );

            if (equipped)
            {
                equipManager->UnequipObject(
                    actor,
                    armor,
                    extra,
                    1,
                    nullptr,
                    true,
                    false,
                    true,
                    true
                );
            }
            else
            {
                equipManager->EquipObject(
                    actor,
                    armor,
                    extra,
                    1,
                    nullptr,
                    true,
                    false,
                    true,
                    true
                );
            }

            return;
        }

        // ============================================================
        // ARMAS
        // ============================================================

        if (auto* weapon = form->As<RE::TESObjectWEAP>())
        {
            // --------------------------------------------------------
            // BOW / CROSSBOW
            // --------------------------------------------------------

            if (IsBowOrCrossbow(weapon))
            {
                const bool equipped = actor->GetEquippedObject(false) == weapon;

                if (equipped)
                {
                    RequestTwoHandedWeaponUnequip(actor, weapon);
                }
                else
                {
                    RequestTwoHandedWeaponEquip(actor, weapon);
                }

                return;
            }

            // --------------------------------------------------------
            // ARMAS NORMAIS
            //
            // BUGFIX: a lógica antiga usava sempre 1 slot fixo por lado
            // e um simples "equipado? desequipa : equipa", o que causava:
            //   - troca de mãos invertida (esquerda equipava na direita);
            //   - "equipar infinitamente" sem nunca desequipar quando
            //     clicado repetidamente no mesmo lado;
            //   - clicar no lado oposto ao já equipado não fazia nada
            //     quando só havia 1 cópia da arma.
            // UseNormalWeapon() resolve os três problemas.
            // --------------------------------------------------------

            UseNormalWeapon(
                actor,
                equipManager,
                weapon,
                leftSide,
                selected->uniqueID,
                selected->hasUniqueID
            );

            return;
        }

        // ============================================================
        // MUNIÇÃO
        // ============================================================

        if (auto* ammo = form->As<RE::TESAmmo>())
        {
            RE::TESAmmo* currentAmmo = nullptr;

            auto* process = actor->GetActorRuntimeData().currentProcess;

            if (process)
            {
                auto* ammoEntry = process->GetCurrentAmmo();

                Logger::GetSingleton().Print(
                    "AMMO ENTRY: {}",
                    ammoEntry ? "VALID" : "NULL");

                if (ammoEntry)
                {
                    auto* ammoObject = ammoEntry->object;

                    Logger::GetSingleton().Print(
                        "AMMO OBJECT: {} formID={:08X}",
                        ammoObject ? "VALID" : "NULL",
                        ammoObject ? ammoObject->formID : 0);

                    if (ammoObject)
                        currentAmmo = ammoObject->As<RE::TESAmmo>();
                }
            }

            Logger::GetSingleton().Print(
                "AMMO: selected={:08X} equipped={:08X}",
                ammo->formID,
                currentAmmo ? currentAmmo->formID : 0);

            if (currentAmmo == ammo)
            {
                Logger::GetSingleton().Print(
                    "AMMO: mesma munição -> desequipando");

                equipManager->UnequipObject(
                    actor,
                    ammo,
                    nullptr,
                    1,
                    nullptr,
                    true,
                    true,
                    true,
                    true,
                    nullptr);
            }
            else
            {
                Logger::GetSingleton().Print(
                    "AMMO: equipando/trocando");

                equipManager->EquipObject(
                    actor,
                    ammo,
                    nullptr,
                    1,
                    nullptr,
                    true,
                    false,
                    true,
                    true);
            }

            return;
        }
        
        // ============================================================
        // TOCHAS E OUTRAS LUZES PORTÁTEIS
        // ============================================================

        if (auto* light = form->As<RE::TESObjectLIGH>())
        {
            constexpr RE::FormID LEFT_HAND_SLOT = 0x13F42;
            auto* leftHandSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(LEFT_HAND_SLOT);

            auto* equippedLeft = actor->GetEquippedObject(true);

            if (equippedLeft == light)
            {
                equipManager->UnequipObject(
                    actor, light, nullptr, 1, leftHandSlot, true, false, true, true);
            }
            else
            {
                equipManager->EquipObject(
                    actor, light, nullptr, 1, leftHandSlot, true, false, true, true);
            }

            return;
        }

        // ============================================================
        // OUTROS TESBoundObject (fallback genérico)
        // ============================================================

        if (auto* object = form->As<RE::TESBoundObject>())
        {
            equipManager->EquipObject(
                actor, object, nullptr, 1, nullptr, true, false, true, true);
        }
    }

    void HandleGameplayKeyReleased()
    {
        // Aqui vamos descobrir o item selecionado
        UseSelectedRadialItem();
        CloseRadialMenu();
    }

    struct SelectedInventoryItem
    {
        RE::TESForm* form = nullptr;

        std::uint16_t uniqueID = 0;
        bool hasUniqueID = false;
    };

    // ============================================================
    // GARANTE UNIQUE ID EM UMA EXTRA DATA LIST EXISTENTE
    // ============================================================

        
    bool EnsureExtraUniqueID(
        RE::Actor* actor,
        RE::ExtraDataList* extra)
    {
        if (!actor || !extra)
            return false;

        // Já possui identificação individual.
        if (extra->HasType<RE::ExtraUniqueID>())
            return true;

        auto* changes = actor->GetInventoryChanges();

        if (!changes)
            return false;

        // Solicita ao Skyrim que atribua o identificador.
        changes->SetUniqueID(
            extra,
            nullptr,
            actor
        );

        // Confirma se a operação funcionou.
        return extra->HasType<RE::ExtraUniqueID>();
    }

        
    RE::InventoryEntryData* GetRealInventoryEntry(
        RE::Actor* actor,
        RE::TESBoundObject* object)
    {
        if (!actor || !object)
            return nullptr;

        auto* changes = actor->GetInventoryChanges();

        if (!changes || !changes->entryList)
            return nullptr;

        for (auto* entry : *changes->entryList)
        {
            if (!entry)
                continue;

            if (entry->object == object)
                return entry;
        }

        return nullptr;
    }

    // ============================================================
    // GARANTE UNIQUE ID PARA UMA INSTÂNCIA
    // ============================================================

    bool EnsureItemUniqueID(
        RE::Actor* actor,
        RE::ExtraDataList* extra)
    {
        if (!actor || !extra)
            return false;

        // Já possui UniqueID.
        if (extra->HasType<RE::ExtraUniqueID>())
            return true;

        // Não atribuir um único ID a várias unidades.
        if (extra->GetCount() != 1)
            return false;

        auto* inventoryChanges =
            actor->GetInventoryChanges();

        if (!inventoryChanges)
            return false;

        const std::uint16_t nextID =
            inventoryChanges->GetNextUniqueID();

        auto* unique = new RE::ExtraUniqueID(
            actor->GetFormID(),
            nextID
        );

        extra->Add(unique);

        return extra->HasType<RE::ExtraUniqueID>();
    }

    // ============================================================
    // IDENTIFICA ARMAS E ARMADURAS DO PLAYER
    // ============================================================

    void EnsurePlayerInventoryUniqueIDs()
    {
        auto* player =
            RE::PlayerCharacter::GetSingleton();

        if (!player)
            return;

        auto* changes =
            player->GetInventoryChanges();

        if (!changes || !changes->entryList)
            return;

        // ============================================================
        // PERCORRE AS ENTRADAS REAIS DO INVENTÁRIO
        // ============================================================

        for (auto* entry : *changes->entryList)
        {
            if (!entry || !entry->object)
                continue;

            auto* object = entry->object;

            // Apenas armas e armaduras.
            if (!object->As<RE::TESObjectWEAP>() &&
                !object->As<RE::TESObjectARMO>())
            {
                continue;
            }

            if (!entry->extraLists)
                continue;

            // ========================================================
            // IDENTIFICA AS INSTÂNCIAS
            // ========================================================

            for (auto* extra : *entry->extraLists)
            {
                if (!extra)
                    continue;

                // Uma lista que representa várias unidades
                // precisa ser separada antes de receber IDs.
                if (extra->GetCount() != 1)
                    continue;

                if (extra->HasType<RE::ExtraUniqueID>())
                    continue;

                if (EnsureItemUniqueID(player, extra))
                {
                    auto* unique =
                        extra->GetByType<RE::ExtraUniqueID>();

                    if (unique)
                    {
                        spdlog::info(
                            "UNIQUE ID CREATED | form={:08X} | uniqueID={} | count={}",
                            object->GetFormID(),
                            unique->uniqueID,
                            extra->GetCount()
                        );
                    }
                }
            }
        }
    }

    SelectedInventoryItem GetSelectedInventoryItemData()
    {
        SelectedInventoryItem result{};

        auto* ui = RE::UI::GetSingleton();

        if (!ui)
            return result;

        // ============================================================
        // INVENTÁRIO
        // ============================================================

        if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME))
        {
            auto inventoryMenu =
                ui->GetMenu<RE::InventoryMenu>();

            if (!inventoryMenu)
                return result;

            auto& data = inventoryMenu->GetRuntimeData();

            if (!data.itemList)
                return result;

            auto* selected =
                data.itemList->GetSelectedItem();

            if (!selected || !selected->data.objDesc)
                return result;

            auto* entry = selected->data.objDesc;

            result.form = entry->object;

            if (!result.form)
                return result;

            // Apenas armas e armaduras precisam de UniqueID.
            if (!result.form->As<RE::TESObjectWEAP>() &&
                !result.form->As<RE::TESObjectARMO>())
            {
                return result;
            }

            // ============================================================
            // LOG: ITEM SELECIONADO
            // ============================================================

            spdlog::info(
                "INVENTORY SELECTED | form={:08X} | name={} | selectedCount={} | hasExtraLists={}",
                result.form->GetFormID(),
                result.form->GetName(),
                selected->data.GetCount(),
                entry->extraLists != nullptr
            );

            if (!entry->extraLists)
            {
                spdlog::info(
                    "INVENTORY RESULT | form={:08X} | NO EXTRA LISTS",
                    result.form->GetFormID()
                );

                return result;
            }

            // ============================================================
            // IDENTIFICAÇÃO DA INSTÂNCIA
            // ============================================================

            RE::ExtraDataList* candidate = nullptr;

            int matchingInstances = 0;
            int totalExtraLists = 0;

            for (auto* extra : *entry->extraLists)
            {
                if (!extra)
                    continue;

                ++totalExtraLists;

                auto* unique =
                    extra->GetByType<RE::ExtraUniqueID>();

                // ========================================================
                // LOG: CADA INSTÂNCIA
                // ========================================================

                spdlog::info(
                    "INVENTORY EXTRA | form={:08X} | uniqueID={} | hasUnique={} | count={} | enchanted={} | worn={} | wornLeft={}",
                    result.form->GetFormID(),
                    unique ? unique->uniqueID : 0,
                    unique != nullptr,
                    extra->GetCount(),
                    extra->HasType<RE::ExtraEnchantment>(),
                    extra->HasType<RE::ExtraWorn>(),
                    extra->HasType<RE::ExtraWornLeft>()
                );

                if (!unique)
                    continue;

                // Uma lista pode representar várias unidades.
                if (extra->GetCount() != 1)
                    continue;

                candidate = extra;

                ++matchingInstances;

                // Não interrompemos o loop para conseguir
                // registrar todas as listas no log.
            }

            // ============================================================
            // LOG: RESULTADO DA BUSCA
            // ============================================================

            spdlog::info(
                "INVENTORY SCAN | form={:08X} | totalExtraLists={} | matchingInstances={} | selectedCount={}",
                result.form->GetFormID(),
                totalExtraLists,
                matchingInstances,
                selected->data.GetCount()
            );

            // Não escolhemos uma instância arbitrariamente.
            if (matchingInstances != 1)
            {
                spdlog::info(
                    "INVENTORY RESULT | form={:08X} | AMBIGUOUS OR NO UNIQUE ID",
                    result.form->GetFormID()
                );

                return result;
            }

            if (selected->data.GetCount() != 1)
            {
                spdlog::info(
                    "INVENTORY RESULT | form={:08X} | SELECTED COUNT != 1",
                    result.form->GetFormID()
                );

                return result;
            }

            auto* unique =
                candidate->GetByType<RE::ExtraUniqueID>();

            if (!unique)
                return result;

            result.uniqueID = unique->uniqueID;
            result.hasUniqueID = true;

            // ============================================================
            // LOG: IDENTIFICAÇÃO CONCLUÍDA
            // ============================================================

            spdlog::info(
                "INVENTORY RESULT | form={:08X} | uniqueID={} | hasUniqueID={}",
                result.form->GetFormID(),
                result.uniqueID,
                result.hasUniqueID
            );

            return result;
        }

        // ============================================================
        // MENU DE MAGIAS
        // ============================================================

        if (ui->IsMenuOpen(RE::MagicMenu::MENU_NAME))
        {
            auto magicMenu =
                ui->GetMenu<RE::MagicMenu>();

            if (!magicMenu)
                return result;

            auto& data = magicMenu->GetRuntimeData();

            if (!data.itemList)
                return result;

            auto* selected =
                data.itemList->GetSelectedItem();

            if (selected && selected->data.baseForm)
            {
                result.form =
                    selected->data.baseForm;
            }
        }

        return result;
    }

    RE::TESForm* GetSelectedInventoryItem()
    {
        auto* ui = RE::UI::GetSingleton();
        if (!ui)
            return nullptr;

        if (ui->IsItemMenuOpen())
        {
            auto inventoryMenu = ui->GetMenu<RE::InventoryMenu>();

            if (inventoryMenu)
            {
                auto& data = inventoryMenu->GetRuntimeData();

                if (data.itemList)
                {
                    auto* selected = data.itemList->GetSelectedItem();

                    if (selected && selected->data.objDesc)
                    {
                        return selected->data.objDesc->object;
                    }
                }
            }
        }

        // ============================================================
        // NOVO: MENU DE MAGIAS
        //
        // Antes só líamos o InventoryMenu, então feitiços e gritos
        // selecionados no Menu de Magias nunca eram detectados pelo
        // radial (só funcionava puxando do inventário normal de itens).
        //
        // OBS: o MagicMenu reaproveita o mesmo componente de lista
        // (RE::ItemList) do InventoryMenu. Se a sua build de
        // CommonLibSSE nomear esse campo de forma diferente (algumas
        // forks usam "spellList"), ajuste apenas a linha "data.itemList"
        // logo abaixo.
        // ============================================================

        if (ui->IsMenuOpen(RE::MagicMenu::MENU_NAME))
        {
            auto magicMenu = ui->GetMenu<RE::MagicMenu>();

            if (magicMenu)
            {
                auto& data = magicMenu->GetRuntimeData();

                if (data.itemList)
                {
                    auto* selected = data.itemList->GetSelectedItem();

                    // MagicItemData armazena o ponteiro direto em baseForm:
                    if (selected && selected->data.baseForm)
                    {
                        return selected->data.baseForm;
                    }
                }
            }
        }

        return nullptr;
    }

    ImVec2 GetSkyrimMousePos()
    {
        auto ui = RE::UI::GetSingleton();
        if (!ui) {
            return ImGui::GetIO().MousePos;
        }

        // Tenta pegar o menu de inventário ou contêiner
        auto menu = ui->GetMenu(RE::InventoryMenu::MENU_NAME);
        if (!menu) {
            menu = ui->GetMenu(RE::ContainerMenu::MENU_NAME);
        }
        if (!menu) {
            menu = ui->GetMenu(RE::MagicMenu::MENU_NAME);
        }

        if (menu && menu->uiMovie) {
            auto movie = menu->uiMovie.get();
            RE::GFxValue cursorX, cursorY;
            bool found = false;

            // 1. Tenta o caminho padrão do Skyrim Vanilla
            if (movie->GetVariable(&cursorX, "_root.Cursor._x") &&
                movie->GetVariable(&cursorY, "_root.Cursor._y"))
            {
                found = true;
            }
            // 2. Tenta o caminho do SkyUI (dentro da estrutura do Menu)
            else if (movie->GetVariable(&cursorX, "_root.Menu_mc.cursor._x") &&
                    movie->GetVariable(&cursorY, "_root.Menu_mc.cursor._y"))
            {
                found = true;
            }
            // 3. Tenta pegar a coordenada global do mouse mantida pela Engine de Input do Scaleform
            else if (movie->GetVariable(&cursorX, "_xmouse") &&
                    movie->GetVariable(&cursorY, "_ymouse"))
            {
                found = true;
            }

            if (found && cursorX.IsNumber() && cursorY.IsNumber()) {
                // Pega a largura/altura nativa do Stage do Flash (padrão SkyUI é 1280x720)
                RE::GFxValue stageW, stageH;
                float sw = 1280.0f;
                float sh = 720.0f;

                if (movie->GetVariable(&stageW, "stage.stageWidth") && stageW.IsNumber()) {
                    sw = static_cast<float>(stageW.GetNumber());
                }
                if (movie->GetVariable(&stageH, "stage.stageHeight") && stageH.IsNumber()) {
                    sh = static_cast<float>(stageH.GetNumber());
                }

                // O retângulo visível informa qual trecho do Stage foi
                // projetado no viewport. Em proporções diferentes de 16:9 ele
                // pode começar antes de zero (ultrawide) ou se estender no eixo
                // Y (telas mais quadradas). A conversão antiga aplicava esse
                // excesso novamente e afastava o indicador do cursor.
                const RE::GRectF visible = movie->GetVisibleFrameRect();
                const float visibleWidth = visible.right - visible.left;
                const float visibleHeight = visible.bottom - visible.top;
                if (std::isfinite(visibleWidth) && std::isfinite(visibleHeight) &&
                    visibleWidth > 0.001f && visibleHeight > 0.001f)
                {
                    const float normalizedX =
                        (static_cast<float>(cursorX.GetNumber()) - visible.left) /
                        visibleWidth;
                    const float normalizedY =
                        (static_cast<float>(cursorY.GetNumber()) - visible.top) /
                        visibleHeight;
                    const ImVec2 realSize = Resolution::GetRealSize();
                    return Resolution::ToVirtual(ImVec2(
                        normalizedX * realSize.x,
                        normalizedY * realSize.y));
                }

                // Fallback para filmes sem um visible frame válido.
                return ImVec2(
                    static_cast<float>(cursorX.GetNumber()) / sw * 1920.0f,
                    static_cast<float>(cursorY.GetNumber()) / sh * 1080.0f);
            }
        }

        // Se falhar em todos os MovieViews, aí sim usa o ImGui como fallback
        return ImGui::GetIO().MousePos;
    }


    constexpr float FAST_DRAG_ZONE_RADIUS = 72.0f;

    static void InitializeInventoryDragPosition()
    {
        const ImVec2 mouse = GetSkyrimMousePos();
        g_fastDragZoneActive = false;
        g_fastDragReturningToCursor = false;
        g_fastDragLastMousePosition = mouse;

        if (Config::g_fastInventoryDrag && g_hasLastInventoryRadialPosition)
        {
            g_inventoryDraggedPosition = g_lastInventoryRadialPosition;
            g_fastDragZoneCenter = g_lastInventoryRadialPosition;
            g_fastDragZoneActive = true;
            return;
        }

        g_inventoryDraggedPosition = mouse;
    }

    void UpdateInventoryDragPointerFromSkyrimMouse()
    {
        const ImVec2 mouse = GetSkyrimMousePos();

        if (!Config::g_fastInventoryDrag)
        {
            g_fastDragZoneActive = false;
            g_fastDragReturningToCursor = false;
            g_inventoryDraggedPosition = mouse;
            return;
        }

        if (g_fastDragZoneActive)
        {
            const ImVec2 delta(
                mouse.x - g_fastDragLastMousePosition.x,
                mouse.y - g_fastDragLastMousePosition.y);
            const ImVec2 screen = Resolution::GetVirtualSize();
            g_inventoryDraggedPosition.x = std::clamp(
                g_inventoryDraggedPosition.x + delta.x, 0.0f, screen.x);
            g_inventoryDraggedPosition.y = std::clamp(
                g_inventoryDraggedPosition.y + delta.y, 0.0f, screen.y);
            g_fastDragLastMousePosition = mouse;

            const float dx = g_inventoryDraggedPosition.x - g_fastDragZoneCenter.x;
            const float dy = g_inventoryDraggedPosition.y - g_fastDragZoneCenter.y;
            if (dx * dx + dy * dy >= FAST_DRAG_ZONE_RADIUS * FAST_DRAG_ZONE_RADIUS)
            {
                g_fastDragZoneActive = false;
                g_fastDragReturningToCursor = true;
            }
            return;
        }

        // A transição é atualizada por frame em UpdateInventoryDrag().
        // Não a substitua por um salto no próximo evento de mouse.
        if (!g_fastDragReturningToCursor)
            g_inventoryDraggedPosition = mouse;
    }

    void OpenInventoryRadial()
    {
        Slowtime::End();
        SetGameplayBlurApplied(false);
        g_showWindow = true;
        g_radialMode = RadialMode::Inventory;

        g_inventoryItemReleased = false;
        g_inventoryItemWasPlaced = false;
        g_inventoryOverflowMorphActive = true;

        //g_draggedInventoryItem = GetSelectedInventoryItem();
        const auto selected =
            GetSelectedInventoryItemData();

        g_draggedInventoryItem = selected.form;

        g_draggedUniqueID = selected.uniqueID;

        g_draggedHasUniqueID = selected.hasUniqueID;

        if (!g_draggedInventoryItem)
        {
            g_inventoryDragMode = InventoryDragMode::None;
            return;
        }

        ImGuiIO& io = ImGui::GetIO();

        InitializeInventoryDragPosition();

        //POINT pt{};
        //GetCursorPos(&pt);
        //g_inventoryLastPhysicalMousePos = pt;

        g_inventoryItemJustGrabbed = true;

        g_inventoryDragMode = InventoryDragMode::KeyDrag;
    }

    void UpdateInventoryDrag()
    {
        ImGuiIO& io = ImGui::GetIO();

        // ============================================================
        // KEY DRAG
        // ============================================================

        if (g_inventoryDragMode == InventoryDragMode::KeyDrag)
        {
            if (g_fastDragReturningToCursor)
            {
                const ImVec2 target = GetSkyrimMousePos();
                const float factor = 1.0f - std::exp(-18.0f *
                    std::clamp(io.DeltaTime, 0.0f, 1.0f / 30.0f));
                g_inventoryDraggedPosition.x +=
                    (target.x - g_inventoryDraggedPosition.x) * factor;
                g_inventoryDraggedPosition.y +=
                    (target.y - g_inventoryDraggedPosition.y) * factor;
                const float dx = target.x - g_inventoryDraggedPosition.x;
                const float dy = target.y - g_inventoryDraggedPosition.y;
                if (dx * dx + dy * dy < 1.0f)
                {
                    g_inventoryDraggedPosition = target;
                    g_fastDragReturningToCursor = false;
                }
            }
            if (g_inventoryItemJustGrabbed)
                g_inventoryItemJustGrabbed = false;

            return;
        }

        // ============================================================
        // SNAP ANIMATION
        // ============================================================

        if (g_inventoryDragMode == InventoryDragMode::SnapAnimating)
        {
            g_inventorySnapProgress += io.DeltaTime / INVENTORY_SNAP_DURATION;

            float t = std::clamp(g_inventorySnapProgress, 0.0f, 1.0f);

            // Smoothstep
            t = t * t * (3.0f - 2.0f * t);

            g_inventoryDraggedPosition = ImVec2(
                g_inventorySnapStart.x + (g_inventorySnapEnd.x - g_inventorySnapStart.x) * t,
                g_inventorySnapStart.y + (g_inventorySnapEnd.y - g_inventorySnapStart.y) * t
            );

            if (g_inventorySnapProgress >= 1.0f)
            {
                g_inventorySnapProgress = 1.0f;

                // O item já foi colocado no vector antes da animação começar.
                g_inventorySnapAnimating = false;

                g_inventoryDragMode = InventoryDragMode::None;

                CloseInventoryRadial();
            }
        }
    }

    float GetDropProximityAlpha(
        const ImVec2& itemPos,
        const ImVec2& menuCenter,
        float activationRadius)
    {
        float dx = itemPos.x - menuCenter.x;
        float dy = itemPos.y - menuCenter.y;

        float distance = std::sqrt(dx * dx + dy * dy);

        float t = 1.0f - std::clamp(distance / activationRadius, 0.0f, 1.0f);

        return t;
    }

    ImVec2 GetSideSlotPosition(
        const ImVec2& center,
        int slot,
        bool leftSide)
    {
        const int slotCount = GetSideVisibleLimit();
        const float radius = GetSideRadialRadius();

        if (slot < 0 || slot >= slotCount)
            return center;

        float angle =
            (-PI / 2.0f) +
            (2.0f * PI * static_cast<float>(slot) / static_cast<float>(slotCount));

        if (leftSide)
            angle = PI - angle;

        return ImVec2(
            center.x + std::cos(angle) * radius,
            center.y + std::sin(angle) * radius
        );
    }

    void HandleInventoryKeyReleased()
    {
        if (!g_showWindow)
            return;

        if (g_radialMode != RadialMode::Inventory)
            return;

        RadialDropTarget target = GetInventoryDropTarget();

        // ============================================================
        // SOLTOU EM UM RADIAL
        // ============================================================

        if (target.valid)
        {
            PlaceDraggedItem(target);
            return;
        }

        // ============================================================
        // SOLTOU FORA
        // ============================================================

        CloseInventoryRadial();
    }

    
    bool IsItemInRadials(
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID)
    {
        if (!form)
            return false;

        auto contains = [&](const std::vector<RadialItem>& items)
        {
            return std::any_of(
                items.begin(),
                items.end(),
                [&](const RadialItem& item)
                {
                    if (item.form != form)
                        return false;

                    // Itens identificados individualmente.
                    if (hasUniqueID)
                    {
                        return item.hasUniqueID &&
                            item.uniqueID == uniqueID;
                    }

                    // Itens sem identificação individual.
                    return !item.hasUniqueID;
                }
            );
        };

        return contains(g_sideItems) ||
            contains(g_topItems) ||
            contains(g_bottomItems);
    }

    void HandleInventoryKeyPressed()
    {
        if (g_showWindow)
            return;

        // ============================================================
        // 1. GARANTE UNIQUE ID DOS ITENS
        // ============================================================

        EnsurePlayerInventoryUniqueIDs();

        // ============================================================
        // 2. OBTÉM O ITEM SELECIONADO
        // ============================================================

        SelectedInventoryItem selectedItem =
            GetSelectedInventoryItemData();

        if (!selectedItem.form)
            return;

        // ============================================================
        // 3. ITEM JÁ ESTÁ NO RADIAL?
        // ============================================================

        if (RemoveItemFromRadials(
            selectedItem.form,
            selectedItem.uniqueID,
            selectedItem.hasUniqueID))
        {
            return;
        }

        // ============================================================
        // 4. NOVO ITEM
        // ============================================================

        g_draggedInventoryItem =
            selectedItem.form;

        g_draggedInventoryUniqueID =
            selectedItem.uniqueID;

        g_draggedInventoryHasUniqueID =
            selectedItem.hasUniqueID;

        // ============================================================
        // 5. DEBUG DA IDENTIFICAÇÃO
        // ============================================================

        spdlog::info(
            "RADIAL SELECT | form={:08X} | uniqueID={} | hasUniqueID={}",
            selectedItem.form->GetFormID(),
            selectedItem.uniqueID,
            selectedItem.hasUniqueID
        );

        // ============================================================
        // 6. PREPARA O DRAG
        // ============================================================

        g_inventoryItemWasPlaced = false;
        g_inventoryItemReleased = false;
        g_inventoryItemJustGrabbed = true;
        g_inventoryOverflowMorphActive = true;

        InitializeInventoryDragPosition();

        // ============================================================
        // 7. ABRE O RADIAL
        // ============================================================

        g_radialMode =
            RadialMode::Inventory;

        g_inventoryDragMode =
            InventoryDragMode::KeyDrag;

        g_showWindow = true;
    }
    
    void DrawStar(ImDrawList* draw_list, ImVec2 center, float radius_outer, float radius_inner, ImU32 color) {
        ImVec2 points[10];
        for (int i = 0; i < 10; i++) {
            float r = (i % 2 == 0) ? radius_outer : radius_inner;
            float angle = i * (PI / 5.0f) - (PI / 2.0f); // Começa no topo
            points[i] = ImVec2(center.x + r * cosf(angle), center.y + r * sinf(angle));
        }

        // Desenha triângulos individuais do centro para cada par de pontos consecutivos
        for (int i = 0; i < 10; i++) {
            int next = (i + 1) % 10;
            draw_list->AddTriangleFilled(center, points[i], points[next], color);
        }
    }

    static void DrawGoldenCircle(
        ImDrawList* draw,
        const ImVec2& center,
        float radius,
        float alpha)
    {
        if (!draw)
            return;

        const ImU32 gold =
            IM_COL32(238, 221, 130, 180);

        const ImU32 color =
            FadeColor(gold, alpha);

        // Círculo externo vazio
        draw->AddCircle(
            center,
            radius,
            color,
            12,
            2.0f
        );

        // Ponto central preenchido
        draw->AddCircleFilled(
            center,
            2.5f,
            color,
            32
        );
    }

    static void DrawGoldenRing3D(
        ImDrawList* draw,
        const ImVec2& center,
        float radius,
        float alpha,
        float yaw,
        float pitch)
    {
        if (!draw)
            return;

        // Projeção ortográfica de um círculo 3D. Uma rotação real preserva
        // sempre um eixo com o diâmetro inteiro; só o eixo menor fecha até
        // virar linha ao ficar de perfil. Isso impede o anel de encolher
        // para um ponto quando há movimento em X e Y ao mesmo tempo.
        const float normalX = std::sin(yaw) * std::cos(pitch);
        const float normalY = std::sin(pitch);
        const float normalZ = std::cos(yaw) * std::cos(pitch);
        const float minorScale = std::max(0.035f, std::abs(normalZ));
        const float minorAngle = std::atan2(normalY, normalX);
        const float majorAngle = minorAngle + 1.57079632679f;
        const float cosMajor = std::cos(majorAngle);
        const float sinMajor = std::sin(majorAngle);
        const float cosMinor = std::cos(minorAngle);
        const float sinMinor = std::sin(minorAngle);
        const ImU32 color = FadeColor(IM_COL32(238, 221, 130, 180), alpha);

        constexpr int segments = 32;
        ImVec2 points[segments];
        for (int i = 0; i < segments; ++i)
        {
            const float angle = 6.28318530718f *
                static_cast<float>(i) / static_cast<float>(segments);
            const float major = std::cos(angle) * radius;
            const float minor = std::sin(angle) * radius * minorScale;
            points[i] = ImVec2(
                center.x + major * cosMajor + minor * cosMinor,
                center.y + major * sinMajor + minor * sinMinor);
        }
        draw->AddPolyline(points, segments, color, ImDrawFlags_Closed, 2.0f);

        // O núcleo não faz parte do anel: permanece legível e estável.
        draw->AddCircleFilled(center, 2.5f, color, 32);
    }


    void RenderInventoryItemOverlay()
    {
        auto* ui = RE::UI::GetSingleton();

        if (!ui)
            return;

        if (!ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) &&
            !ui->IsMenuOpen(RE::MagicMenu::MENU_NAME))
        {
            return;
        }

        // ============================================================
        // OBTÉM A INSTÂNCIA SELECIONADA
        // ============================================================

        SelectedInventoryItem selectedItem =
            GetSelectedInventoryItemData();

        if (!selectedItem.form)
            return;

        // ============================================================
        // VERIFICA SE ESTA INSTÂNCIA ESTÁ NO RADIAL
        // ============================================================

        const bool isAssignedToRadial =
            IsItemInRadials(
                selectedItem.form,
                selectedItem.uniqueID,
                selectedItem.hasUniqueID
            );

        if (!isAssignedToRadial)
            return;

        // ============================================================
        // OVERLAY
        // ============================================================

        ImGui::SetNextWindowPos(ImVec2(0, 0));

        ImGui::SetNextWindowSize(
            ImGui::GetIO().DisplaySize
        );

        ImGui::Begin(
            "SkyrimUIOverlay",
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBackground
        );

        ImDrawList* drawList =
            ImGui::GetForegroundDrawList();

        const ImVec2 mousePos = GetSkyrimMousePos();

        // Animação exclusiva deste indicador. Ela não reutiliza cache,
        // velocidade ou estado dos itens dos radiais.
        static bool indicatorInitialized = false;
        static ImVec2 previousMouse{};
        static ImVec2 motionOffset{};
        static float ringYaw = 0.0f;
        static float ringPitch = 0.0f;
        static float ringYawVelocity = 0.0f;
        static float ringPitchVelocity = 0.0f;
        const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f);
        if (!indicatorInitialized)
        {
            previousMouse = mousePos;
            indicatorInitialized = true;
        }
        const ImVec2 mouseDelta(
            mousePos.x - previousMouse.x,
            mousePos.y - previousMouse.y);
        previousMouse = mousePos;

        // Inércia muito sutil do anel. Movimentos rápidos acrescentam mais
        // força, enquanto o amortecimento deixa o giro terminar devagar.
        ringYawVelocity = std::clamp(ringYawVelocity + mouseDelta.x * 0.006f,
            -0.055f, 0.055f);
        ringPitchVelocity = std::clamp(ringPitchVelocity + mouseDelta.y * 0.006f,
            -0.055f, 0.055f);
        ringYaw += ringYawVelocity;
        ringPitch += ringPitchVelocity;
        const float ringDamping = std::exp(-2.2f * dt);
        ringYawVelocity *= ringDamping;
        ringPitchVelocity *= ringDamping;
        const ImVec2 motionTarget(
            std::clamp(-mouseDelta.x * 0.10f, -4.0f, 4.0f),
            std::clamp(-mouseDelta.y * 0.10f, -4.0f, 4.0f));
        const float motionFactor = 1.0f - std::exp(-5.0f * dt);
        motionOffset.x += (motionTarget.x - motionOffset.x) * motionFactor;
        motionOffset.y += (motionTarget.y - motionOffset.y) * motionFactor;
        const float time = static_cast<float>(ImGui::GetTime());
        const ImVec2 floatOffset(
            std::cos(time * 0.75f) * 1.5f,
            std::sin(time * 1.05f) * 2.5f);

        const ImVec2 indicatorPos(
            // Âncora fixa do indicador em relação ao cursor do Skyrim.
            // Altere estes dois valores para ajustar manualmente a posição.
            mousePos.x + 60.0f + floatOffset.x + motionOffset.x,
            mousePos.y + 62.0f + floatOffset.y + motionOffset.y);

        // ============================================================
        // ESTRELA
        // ============================================================

        //DrawStar(
        //    drawList,
        //    ImVec2(
        //        textPos.x + 35.0f,
        //        textPos.y + 10.0f
        //    ),
        //    15.0f,
        //    6.0f,
        //    IM_COL32(255, 215, 0, 255)
        //);

        DrawGoldenRing3D(
            drawList,
            indicatorPos,
            12.0f,
            1.0f,
            ringYaw,
            ringPitch
        );

        ImGui::End();
    }

    void CheckGKey()
    {
        const bool currentG = (GetAsyncKeyState('G') & 0x8000) != 0;

        const bool pressed = currentG && !g_lastGState;
        const bool released = !currentG && g_lastGState;

        if (pressed)
        {
            if (IsInventoryOpen())
            {
                HandleInventoryKeyPressed();
            }
            else
            {
                HandleGameplayKeyPressed();
            }
        }

        if (released)
        {
            if (g_radialMode == RadialMode::Gameplay)
            {
                HandleGameplayKeyReleased();
            }
            else
            {
                HandleInventoryKeyReleased();
            }
        }

        g_lastGState = currentG;
    }

    //----------------------------------------------------------------------------------------------------
    //
    //                                   MENU
    //
    //----------------------------------------------------------------------------------------------------

    // Função utilitária para aplicar a transparência do Fade nas cores do ImDrawList
    ImU32 FadeColor(ImU32 color, float alpha)
    {
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
        c.w *= alpha;
        return ImGui::ColorConvertFloat4ToU32(c);
    }

    // Interpolação linear de cor (usada nas transições de hover para
    // dar uma sensação de "aquecimento" no item selecionado).
    inline ImU32 LerpColor(ImU32 a, ImU32 b, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);

        ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
        ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);

        ImVec4 result(
            ca.x + (cb.x - ca.x) * t,
            ca.y + (cb.y - ca.y) * t,
            ca.z + (cb.z - ca.z) * t,
            ca.w + (cb.w - ca.w) * t
        );

        return ImGui::ColorConvertFloat4ToU32(result);
    }

    ImVec2 ClampPointToDistance(
        const ImVec2& origin,
        const ImVec2& target,
        float maxDistance)
    {
        float dx = target.x - origin.x;
        float dy = target.y - origin.y;

        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance <= maxDistance)
            return target;

        if (distance <= 0.001f)
            return origin;

        float scale = maxDistance / distance;

        return ImVec2(
            origin.x + dx * scale,
            origin.y + dy * scale
        );
    }

    float GetTopLineLimit(
        const ImVec2& center,
        const ImVec2& mouse)
    {
        float dx = mouse.x - center.x;
        float dy = mouse.y - center.y;

        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < 0.001f)
            return 0.0f;

        // Distância onde a linha encontra aproximadamente a região da lua.
        constexpr float topBoundary = 70.0f;

        return topBoundary;
    }

    float GetMenuLineLength()
    {
        switch (g_radialSide)
        {
        case RadialSide::Left:
        case RadialSide::Right:
            return std::max(RADIAL_DEADZONE, GetSideRadialRadius() - 30.0f);

        case RadialSide::Top:
        case RadialSide::Bottom:
            return 55.0f;

        default:
            return RADIAL_DEADZONE;
        }
    }

    RadialSide GetRadialDirection(
        const ImVec2& mouse,
        const ImVec2& center)
    {
        float dx = mouse.x - center.x;
        float dy = mouse.y - center.y;

        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < RADIAL_DEADZONE)
            return RadialSide::None;

        // Ângulo em relação ao eixo vertical.
        float angleFromVertical =
            std::atan2(std::abs(dx), std::abs(dy)) * 180.0f / 3.14159265f;

        // ============================================================
        // FAIXA VERTICAL ±20°
        // ============================================================

        if (angleFromVertical <= 20.0f)
        {
            if (dy < 0.0f)
                return RadialSide::Top;
            else
                return RadialSide::Bottom;
        }

        // ============================================================
        // ESQUERDA / DIREITA
        // ============================================================

        if (dx < 0.0f)
            return RadialSide::Left;

        return RadialSide::Right;
    }

    static QuickDrawKey MakeQuickDrawKey(const RadialItem& item)
    {
        return {
            item.form ? item.form->GetFormID() : 0,
            item.uniqueID,
            item.hasUniqueID
        };
    }

    static bool SameQuickDrawKey(const QuickDrawKey& a, const QuickDrawKey& b)
    {
        return a.formID == b.formID &&
            a.hasUniqueID == b.hasUniqueID &&
            (!a.hasUniqueID || a.uniqueID == b.uniqueID);
    }

    static std::vector<ImVec2>* FindQuickDrawStroke(const RadialItem& item)
    {
        const QuickDrawKey key = MakeQuickDrawKey(item);
        if (!key.formID)
            return nullptr;
        for (auto& gesture : g_quickDrawGestures)
        {
            if (SameQuickDrawKey(gesture.key, key))
                return &gesture.points;
        }
        return nullptr;
    }

    static void StoreQuickDrawStroke(const RadialItem& item,
        const std::vector<ImVec2>& points)
    {
        const QuickDrawKey key = MakeQuickDrawKey(item);
        if (!key.formID)
            return;

        auto it = std::find_if(g_quickDrawGestures.begin(),
            g_quickDrawGestures.end(), [&](const QuickDrawGesture& gesture) {
                return SameQuickDrawKey(gesture.key, key);
            });

        // Uma linha vazia significa que o usuário removeu o desenho.
        if (points.empty())
        {
            if (it != g_quickDrawGestures.end())
                g_quickDrawGestures.erase(it);
            return;
        }

        if (it == g_quickDrawGestures.end())
        {
            g_quickDrawGestures.push_back({ key, points });
        }
        else
        {
            it->points = points;
        }
    }

    static void AddQuickDrawPoint(std::vector<ImVec2>& points,
        const ImVec2& position, const ImVec2& center, float radius)
    {
        if (radius <= 0.0f)
            return;

        ImVec2 point((position.x - center.x) / radius,
            (position.y - center.y) / radius);
        const float length = std::sqrt(point.x * point.x + point.y * point.y);
        if (length > 1.0f)
        {
            point.x /= length;
            point.y /= length;
        }

        // Filtro leve: remove tremor de alta frequência, mas conserva curvas
        // e cantos do gesto. Como os dois lados (editor/gameplay) usam a
        // mesma coleta, a comparação continua consistente.
        if (!points.empty())
        {
            const ImVec2& previous = points.back();
            constexpr float kSmoothing = 0.68f;
            point.x = previous.x + (point.x - previous.x) * kSmoothing;
            point.y = previous.y + (point.y - previous.y) * kSmoothing;

            // Evita gravar centenas de pontos iguais quando o mouse fica parado.
            const float dx = point.x - previous.x;
            const float dy = point.y - previous.y;
            if (dx * dx + dy * dy < 0.00015f)
                return;
        }
        points.push_back(point);
    }

    static std::vector<ImVec2> ResampleQuickDraw(const std::vector<ImVec2>& input,
        std::size_t count)
    {
        if (input.empty() || count == 0)
            return {};
        if (input.size() == 1)
            return std::vector<ImVec2>(count, input.front());

        std::vector<float> accumulated(input.size(), 0.0f);
        for (std::size_t i = 1; i < input.size(); ++i)
        {
            const float dx = input[i].x - input[i - 1].x;
            const float dy = input[i].y - input[i - 1].y;
            accumulated[i] = accumulated[i - 1] + std::sqrt(dx * dx + dy * dy);
        }
        const float total = accumulated.back();
        if (total <= 0.0001f)
            return std::vector<ImVec2>(count, input.front());

        std::vector<ImVec2> result;
        result.reserve(count);
        for (std::size_t sample = 0; sample < count; ++sample)
        {
            const float distance = total * static_cast<float>(sample) /
                static_cast<float>(count - 1);
            std::size_t segment = 1;
            while (segment < accumulated.size() && accumulated[segment] < distance)
                ++segment;
            if (segment >= accumulated.size())
            {
                result.push_back(input.back());
                continue;
            }
            const float begin = accumulated[segment - 1];
            const float span = std::max(0.0001f, accumulated[segment] - begin);
            const float t = std::clamp((distance - begin) / span, 0.0f, 1.0f);
            result.emplace_back(
                input[segment - 1].x + (input[segment].x - input[segment - 1].x) * t,
                input[segment - 1].y + (input[segment].y - input[segment - 1].y) * t);
        }
        return result;
    }

    static float QuickDrawSimilarity(const std::vector<ImVec2>& a,
        const std::vector<ImVec2>& b)
    {
        // Pontos isolados não são gestos úteis: exigimos uma linha real.
        if (a.size() < 2 || b.size() < 2)
            return 0.0f;
        constexpr std::size_t samples = 32;
        auto normalize = [](std::vector<ImVec2> points) {
            ImVec2 center{};
            for (const auto& point : points)
            {
                center.x += point.x;
                center.y += point.y;
            }
            center.x /= static_cast<float>(points.size());
            center.y /= static_cast<float>(points.size());
            float scale = 0.0f;
            for (auto& point : points)
            {
                point.x -= center.x;
                point.y -= center.y;
                scale = std::max(scale, std::sqrt(point.x * point.x + point.y * point.y));
            }
            scale = std::max(scale, 0.04f);
            for (auto& point : points)
            {
                point.x /= scale;
                point.y /= scale;
            }
            return points;
        };
        const auto lhs = normalize(ResampleQuickDraw(a, samples));
        const auto rhs = normalize(ResampleQuickDraw(b, samples));
        auto score = [&](bool reversed) {
            float distance = 0.0f;
            for (std::size_t i = 0; i < samples; ++i)
            {
                const ImVec2& point = rhs[reversed ? samples - 1 - i : i];
                const float dx = lhs[i].x - point.x;
                const float dy = lhs[i].y - point.y;
                distance += std::sqrt(dx * dx + dy * dy);
            }
            return std::clamp(1.0f - (distance / samples) / 1.50f, 0.0f, 1.0f);
        };
        // Mantém o gesto tolerante à direção em que o jogador o desenhou.
        return std::max(score(false), score(true));
    }

    static void DrawQuickDrawStroke(ImDrawList* draw,
        const std::vector<ImVec2>& points, const ImVec2& center,
        float radius, ImU32 color, float thickness)
    {
        if (!draw || points.size() < 2)
            return;
        for (std::size_t i = 1; i < points.size(); ++i)
        {
            const ImVec2 a(center.x + points[i - 1].x * radius,
                center.y + points[i - 1].y * radius);
            const ImVec2 b(center.x + points[i].x * radius,
                center.y + points[i].y * radius);
            draw->AddLine(a, b, color, thickness);
        }
    }

    static bool PointInQuickDrawRect(const ImVec2& point,
        const ImVec2& min, const ImVec2& max)
    {
        return point.x >= min.x && point.x <= max.x &&
            point.y >= min.y && point.y <= max.y;
    }

    static bool FinishQuickDrawGameplay()
    {
        if (!g_quickDrawGameplayActive)
            return false;

        RadialItem* bestItem = nullptr;
        RadialSide bestSide = RadialSide::None;
        float bestScore = QUICK_DRAW_MATCH_THRESHOLD;
        const auto testItems = [&](std::vector<RadialItem>& items, RadialSide side) {
            for (auto& item : items)
            {
                if (!item.valid || !item.form)
                    continue;
                const auto* stored = FindQuickDrawStroke(item);
                if (!stored || stored->empty())
                    continue;
                const float score = QuickDrawSimilarity(g_quickDrawGameplayStroke, *stored);
                if (score >= bestScore)
                {
                    bestScore = score;
                    bestItem = &item;
                    bestSide = side;
                }
            }
        };

        testItems(g_sideItems, RadialSide::Left);
        testItems(g_topItems, RadialSide::Top);
        testItems(g_bottomItems, RadialSide::Bottom);

        g_quickDrawGameplayActive = false;
        g_quickDrawGameplayDrawing = false;
        g_quickDrawGameplayExpandT = 0.0f;
        g_quickDrawGameplayStroke.clear();

        if (!bestItem)
            return false;

        g_quickDrawSelectionOverride = bestItem;
        g_quickDrawSelectionSide = bestSide;
        spdlog::info("QUICK DRAW | matched {:08X} score={:.2f}",
            bestItem->form->GetFormID(), bestScore);
        return true;
    }

    static void CancelQuickDrawGameplay()
    {
        if (!g_quickDrawGameplayActive)
            return;

        // Encostar no limite expandido é um cancelamento explícito: não
        // aguardamos a soltura da tecla principal e não avaliamos o gesto.
        g_quickDrawGameplayActive = false;
        g_quickDrawGameplayDrawing = false;
        g_quickDrawGameplayExpandT = 0.0f;
        g_quickDrawGameplayStroke.clear();
        g_quickDrawSelectionOverride = nullptr;
        g_quickDrawSelectionSide = RadialSide::None;
        g_quickDrawGameplayEquipLeft = false;
        CloseRadialMenu();
    }

    bool BeginQuickDrawEditor()
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen() ||
            !g_settingsHoveredItem || g_settingsDrag.active)
            return false;

        g_quickDrawEditorItem = {};
        g_quickDrawEditorItem.form = g_settingsHoveredItem;
        g_quickDrawEditorItem.uniqueID = g_settingsHoveredUniqueID;
        g_quickDrawEditorItem.hasUniqueID = g_settingsHoveredHasUniqueID;
        g_quickDrawEditorItem.valid = true;
        g_quickDrawEditorStroke.clear();
        if (const auto* stored = FindQuickDrawStroke(g_quickDrawEditorItem))
            g_quickDrawEditorStroke = *stored;
        g_quickDrawEditorDrawing = false;
        g_quickDrawEditorOpen = true;

        // Todo gesto começa exatamente no centro do círculo. Reposicionamos
        // também o cursor real para que o primeiro delta físico coincida com
        // o desenho exibido, inclusive em ultrawide.
        const ImVec2 center = Resolution::GetVirtualCenter();
        g_settingsMousePos = center;
        ImGui::GetIO().MousePos = center;
        if (HWND hwnd = GetForegroundWindow())
        {
            RECT rect{};
            if (GetWindowRect(hwnd, &rect))
                SetCursorPos((rect.left + rect.right) / 2,
                    (rect.top + rect.bottom) / 2);
        }

        // Mantém o preview 3D atual apontando para a instância exata.
        g_settingsPreviewSelection = g_quickDrawEditorItem;
        g_settingsPreviewSelectionActive = true;
        g_settingsPreviewLastHoverTime = ImGui::GetTime();

        spdlog::info("QUICK DRAW | editor opened for {:08X}",
            g_quickDrawEditorItem.form->GetFormID());
        return true;
    }

    bool IsQuickDrawEditorOpen()
    {
        return g_quickDrawEditorOpen;
    }

    static void DrawQuickDrawEditor(float alpha)
    {
        if (!g_quickDrawEditorOpen)
            return;

        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        
        const ImVec2 center(screen.x * 0.5f, screen.y * 0.5f);
        
        auto* draw = ImGui::GetForegroundDrawList();
        
        if (!draw)
            return;

        // A coleta dos pontos ocorre no render para acompanhar a posição
        // física do cursor do Skyrim mesmo entre eventos de mouse.
        if (g_quickDrawEditorDrawing)
            AddQuickDrawPoint(g_quickDrawEditorStroke, g_settingsMousePos,
                center, QUICK_DRAW_EDITOR_RADIUS);

        draw->AddCircle(center, QUICK_DRAW_EDITOR_RADIUS,
            FadeColor(IM_COL32(245, 245, 245, 205), alpha), 96, 1.8f);
        
        DrawQuickDrawStroke(draw, g_quickDrawEditorStroke, center,
            QUICK_DRAW_EDITOR_RADIUS,
            FadeColor(IM_COL32(255, 255, 255, 240), alpha), 5.0f);

        constexpr ImVec2 buttonSize(110.0f, 40.0f);
        
        const float buttonsY = center.y + QUICK_DRAW_EDITOR_RADIUS + 28.0f;
        
        g_quickDrawEditorResetMin = ImVec2(center.x - buttonSize.x - 8.0f, buttonsY);
        g_quickDrawEditorResetMax = ImVec2(center.x - 8.0f, buttonsY + buttonSize.y);
        g_quickDrawEditorOkMin = ImVec2(center.x + 8.0f, buttonsY);
        g_quickDrawEditorOkMax = ImVec2(center.x + buttonSize.x + 8.0f, buttonsY + buttonSize.y);

        const auto drawButton = [&](const ImVec2& min, const ImVec2& max,
            const char* label) {
            const bool hovered = PointInQuickDrawRect(g_settingsMousePos, min, max);
        
            draw->AddRectFilled(min, max, hovered
                ? FadeColor(IM_COL32(235, 235, 235, 160), alpha)
                : FadeColor(IM_COL32(42, 42, 42, 205), alpha), 3.0f);
        
            //draw->AddRect(min, max, FadeColor(hovered
            //    ? IM_COL32(215, 195, 150, 255)
            //    : IM_COL32(190, 190, 190, 190), alpha), 3.0f, 0, 1.0f);
        
            const ImVec2 text = ImGui::CalcTextSize(label);
        
            draw->AddText(ImVec2((min.x + max.x - text.x) * 0.5f,
                (min.y + max.y - text.y) * 0.5f),
                FadeColor(hovered ? IM_COL32(25, 25, 25, 255) :
                    IM_COL32(240, 240, 240, 255), alpha), label);
        };

        drawButton(g_quickDrawEditorResetMin, g_quickDrawEditorResetMax,
            Language::Get("reset").c_str());
        
        drawButton(g_quickDrawEditorOkMin, g_quickDrawEditorOkMax,
            Language::Get("ok").c_str());

        // O editor usa sempre a posição central, independentemente do layout
        // configurado para o preview comum do Settings.
        ItemPreview::SetHudPosition(center);
        
        ItemPreview::SetSizeScale(1.0f);
        
        ItemPreview::SilentPreviewMenu::Open();
        
        ItemPreview::Show(g_quickDrawEditorItem.form,
            g_quickDrawEditorItem.uniqueID,
            g_quickDrawEditorItem.hasUniqueID);
        
        DrawSettingsCursor();
    }

    bool HandleQuickDrawEditorMouseButton(int button, bool pressed)
    {
        if (!g_quickDrawEditorOpen)
            return false;

        // O editor é modal: evita que ambos os cliques executem comandos
        // do Settings enquanto uma linha está sendo criada.
        if (button != 0)
            return true;

        if (!pressed)
        {
            g_quickDrawEditorDrawing = false;
            return true;
        }

        if (PointInQuickDrawRect(g_settingsMousePos,
                g_quickDrawEditorResetMin, g_quickDrawEditorResetMax))
        {
            g_quickDrawEditorStroke.clear();
            StoreQuickDrawStroke(g_quickDrawEditorItem, {});
            return true;
        }
        if (PointInQuickDrawRect(g_settingsMousePos,
                g_quickDrawEditorOkMin, g_quickDrawEditorOkMax))
        {
            StoreQuickDrawStroke(g_quickDrawEditorItem, g_quickDrawEditorStroke);
            g_quickDrawEditorOpen = false;
            g_quickDrawEditorDrawing = false;
            return true;
        }

        // Cada novo clique inicia uma única linha e substitui a anterior.
        g_quickDrawEditorStroke.clear();
        g_quickDrawEditorDrawing = true;
        const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f,
            ImGui::GetIO().DisplaySize.y * 0.5f);
        g_settingsMousePos = center;
        ImGui::GetIO().MousePos = center;
        if (HWND hwnd = GetForegroundWindow())
        {
            RECT rect{};
            if (GetWindowRect(hwnd, &rect))
                SetCursorPos((rect.left + rect.right) / 2,
                    (rect.top + rect.bottom) / 2);
        }
        // A primeira amostra é o centro mesmo que o evento do clique tenha
        // chegado antes de o Windows confirmar o reposicionamento do cursor.
        g_quickDrawEditorStroke.emplace_back(0.0f, 0.0f);
        return true;
    }

    bool BeginQuickDrawGameplayStroke(int button, bool pressed)
    {
        if (g_radialMode != RadialMode::Gameplay || !g_showWindow ||
            g_radialLocked || g_radialSide != RadialSide::None ||
            (button != 0 && button != 1))
            return false;

        if (!pressed)
        {
            g_quickDrawGameplayDrawing = false;
            return g_quickDrawGameplayActive;
        }

        g_quickDrawGameplayActive = true;
        g_quickDrawGameplayDrawing = true;
        g_quickDrawGameplayEquipLeft = button == 0;
        g_quickDrawGameplayExpandT = 0.0f;
        g_quickDrawGameplayStroke.clear();
        const ImVec2 center = Resolution::GetVirtualCenter();
        AddQuickDrawPoint(g_quickDrawGameplayStroke, GetRadialMousePosition(),
            center, QUICK_DRAW_EDITOR_RADIUS);
        return true;
    }

    bool HandleTopBottomGameplayMouseButton(int button, bool pressed)
    {
        if (!pressed || (button != 0 && button != 1) ||
            g_radialMode != RadialMode::Gameplay || !g_showWindow ||
            (g_radialSide != RadialSide::Top &&
                g_radialSide != RadialSide::Bottom))
        {
            return false;
        }

        const bool hasSelection = g_radialSide == RadialSide::Top
            ? g_topHasSelection
            : g_bottomHasSelection;
        if (!hasSelection)
            return false;

        // Clique explícito escolhe a mão e conclui o radial imediatamente,
        // mesmo se a tecla de ativação continuar pressionada.
        g_topBottomHandOverride = button == 0
            ? TopBottomHandOverride::Left
            : TopBottomHandOverride::Right;
        UseSelectedRadialItem();
        g_topBottomHandOverride = TopBottomHandOverride::None;

        // A soltura posterior da tecla não pode selecionar o item novamente.
        g_ignoreNextGRelease = true;
        CloseRadialMenu();
        return true;
    }

    bool IsQuickDrawGameplayActive()
    {
        return g_quickDrawGameplayActive;
    }

    void SaveQuickDraw(SKSE::SerializationInterface* serialization)
    {
        if (!serialization)
            return;

        const std::uint32_t count = static_cast<std::uint32_t>(
            std::min<std::size_t>(g_quickDrawGestures.size(), 4096));
        if (!serialization->WriteRecordData(&count, sizeof(count)))
            return;

        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto& gesture = g_quickDrawGestures[i];
            const std::uint8_t hasUnique = gesture.key.hasUniqueID ? 1 : 0;
            const std::uint16_t pointCount = static_cast<std::uint16_t>(
                std::min<std::size_t>(gesture.points.size(), 2048));
            if (!serialization->WriteRecordData(&gesture.key.formID, sizeof(RE::FormID)) ||
                !serialization->WriteRecordData(&gesture.key.uniqueID, sizeof(std::uint16_t)) ||
                !serialization->WriteRecordData(&hasUnique, sizeof(hasUnique)) ||
                !serialization->WriteRecordData(&pointCount, sizeof(pointCount)))
                return;
            for (std::uint16_t point = 0; point < pointCount; ++point)
            {
                if (!serialization->WriteRecordData(&gesture.points[point], sizeof(ImVec2)))
                    return;
            }
        }
    }

    bool LoadQuickDraw(SKSE::SerializationInterface* serialization,
        std::uint32_t version, std::uint32_t)
    {
        g_quickDrawGestures.clear();
        if (!serialization || version != 1)
            return false;

        std::uint32_t count = 0;
        if (serialization->ReadRecordData(&count, sizeof(count)) != sizeof(count) ||
            count > 4096)
            return false;

        g_quickDrawGestures.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            RE::FormID oldFormID = 0;
            std::uint16_t uniqueID = 0;
            std::uint8_t hasUnique = 0;
            std::uint16_t pointCount = 0;
            if (serialization->ReadRecordData(&oldFormID, sizeof(oldFormID)) != sizeof(oldFormID) ||
                serialization->ReadRecordData(&uniqueID, sizeof(uniqueID)) != sizeof(uniqueID) ||
                serialization->ReadRecordData(&hasUnique, sizeof(hasUnique)) != sizeof(hasUnique) ||
                serialization->ReadRecordData(&pointCount, sizeof(pointCount)) != sizeof(pointCount) ||
                pointCount > 2048)
                return false;

            std::uint32_t formID = 0;
            if (!serialization->ResolveFormID(oldFormID, formID) || !formID)
            {
                // Ainda consumimos os pontos do registro inválido.
                ImVec2 discarded{};
                for (std::uint16_t point = 0; point < pointCount; ++point)
                    if (serialization->ReadRecordData(&discarded, sizeof(discarded)) != sizeof(discarded))
                        return false;
                continue;
            }

            QuickDrawGesture gesture{};
            gesture.key = { formID, uniqueID, hasUnique != 0 };
            gesture.points.resize(pointCount);
            for (auto& point : gesture.points)
            {
                if (serialization->ReadRecordData(&point, sizeof(point)) != sizeof(point))
                    return false;
            }
            if (!gesture.points.empty())
                g_quickDrawGestures.push_back(std::move(gesture));
        }

        spdlog::info("QUICK DRAW | loaded {} gestures", g_quickDrawGestures.size());
        return true;
    }

    void ClearQuickDraw()
    {
        g_quickDrawGestures.clear();
        g_quickDrawEditorOpen = false;
        g_quickDrawEditorDrawing = false;
        g_quickDrawGameplayActive = false;
        g_quickDrawGameplayDrawing = false;
        g_quickDrawGameplayExpandT = 0.0f;
        g_quickDrawGameplayEquipLeft = false;
        g_quickDrawSelectionOverride = nullptr;
        g_quickDrawSelectionSide = RadialSide::None;
    }

    float GetRadialEdgeMargin()
    {
        // Retorna uma margem de segurança de 20 pixels da borda do monitor
        return 20.0f;
    }

    bool g_radialRecentered = false;

    static void StartSideWrapAnimation(
        RadialItemAnimation& anim,
        const ImVec2& center,
        int direction)
    {
        const float dx =
            anim.currentPos.x - center.x;

        const float dy =
            anim.currentPos.y - center.y;

        anim.sideWrapStartAngle =
            std::atan2(dy, dx);

        anim.sideWrapStartRadius =
            std::sqrt(
                dx * dx +
                dy * dy
            );

        anim.sideWrapDirection = direction;
        anim.sideWrapT = 0.0f;
        anim.sideWrapActive = true;

        // O PRIMEIRO Draw após o scroll congelará o destino.
        anim.sideWrapTargetInitialized = false;

        anim.velocity = ImVec2(0.0f, 0.0f);
    }

    void Menu::ScrollSideRadial(int direction)
    {
        if (g_radialMode == RadialMode::Inventory &&
            g_inventoryTopologySettleRemaining > 0.0f && direction != 0)
        {
            g_pendingInventoryScroll = std::clamp(
                g_pendingInventoryScroll + (direction < 0 ? -1 : 1),
                -12, 12);
            return;
        }

        const int totalItems =
            static_cast<int>(g_sideItems.size());

        if (totalItems < 3)
        {
            g_sideScrollOffset = 0;
            return;
        }

        if (totalItems <= GetSideVisibleLimit() && Config::g_lockSideScroll)
            return;

        if (direction == 0)
            return;

        g_sideScrollDirection = direction < 0 ? -1 : 1;

        const int visibleCount =
            std::min(
                totalItems,
                GetSideVisibleLimit()
            );

        const int overflowCount =
            totalItems - visibleCount;

        // ============================================================
        // ESTADO ANTES DE ALTERAR O OFFSET
        // ============================================================

        const int oldLastOverflowIndex =
            WrapSideIndex(
                g_sideScrollOffset +
                visibleCount +
                overflowCount - 1,
                totalItems
            );

        const int oldFirstMainIndex =
            WrapSideIndex(
                g_sideScrollOffset,
                totalItems
            );

        const bool leftSide =
            g_radialSide == RadialSide::Left;

        // ============================================================
        // ALTERA IMEDIATAMENTE O OFFSET
        // ============================================================

        if (direction > 0)
        {
            ++g_sideScrollOffset;
        }
        else
        {
            --g_sideScrollOffset;
        }

        g_sideScrollOffset =
            WrapSideIndex(
                g_sideScrollOffset,
                totalItems
            );
        g_sideScrollStardustEnergy = std::min(
            3.0f, g_sideScrollStardustEnergy + 0.72f);

        // ============================================================
        // CAUDA -> PRINCIPAL
        // ============================================================

        if (direction < 0)
        {
            RadialItem& item =
                g_sideItems[oldLastOverflowIndex];

            if (item.form)
            {
                RadialItemAnimation& anim = GetOrCreateAnim(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID
                );

                //StartSideWrapAnimation(
                //    anim,
                //    g_radialOrigin,
                //    leftSide ? -1 : +1
                //);
            }
        }

        // ============================================================
        // PRINCIPAL -> CAUDA
        // ============================================================

        else
        {
            RadialItem& item =
                g_sideItems[oldFirstMainIndex];

            if (item.form)
            {
                RadialItemAnimation& anim = GetOrCreateAnim(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID
                );

                //StartSideWrapAnimation(
                //    anim,
                //    g_radialOrigin,
                //    leftSide ? +1 : -1
                //);
            }
        }
    }

    void UpdateRadialMouse()
    {
        if (!g_showWindow)
            return;

        ImGuiIO& io = ImGui::GetIO();

        const bool quickDraw = g_quickDrawGameplayActive;
        const bool vertical =
            g_radialSide == RadialSide::Top ||
            g_radialSide == RadialSide::Bottom;

        const float mouseSmooth = quickDraw
            ? 1.0f
            : vertical
            ? 0.15f
            : LayoutLerp(1.0f, 0.05f, Config::g_sideMouseSmooth);
        const float mouseSensitivity = quickDraw
            ? 1.0f
            : vertical
            ? 1.0f
            : std::clamp(Config::g_sideMouseSensitivity, 0.25f, 3.0f);
        const float maxMouseStep = quickDraw ? 1000.0f : vertical ? 1.0f : 15.0f;
        const float maxMouseStepSq = maxMouseStep * maxMouseStep;

        // ============================================================
        // RECENTER
        // ============================================================

        if (g_radialRecentered)
        {
            g_radialRecentered = false;
            g_lastMousePos = io.MousePos;
            g_radialMouseVelocity = ImVec2(0.0f, 0.0f);
            return;
        }

        // ============================================================
        // INIT
        // ============================================================

        if (!g_radialMouseInitialized)
        {
            g_lastMousePos = io.MousePos;
            g_radialMouseInitialized = true;
            g_radialMouseVelocity = ImVec2(0.0f, 0.0f);
            return;
        }

        // ============================================================
        // DELTA FÍSICO
        // ============================================================

        const ImVec2 currentMouse = io.MousePos;

        const ImVec2 delta(
            (currentMouse.x - g_lastMousePos.x) * mouseSensitivity,
            (currentMouse.y - g_lastMousePos.y) * mouseSensitivity
        );

        constexpr float deselectThreshold = 0.5f;

        if (std::abs(delta.x) > deselectThreshold ||
            std::abs(delta.y) > deselectThreshold)
        {
            if (g_radialSide == RadialSide::Top)
            {
                g_topHasSelection = false;
            }
            else if (g_radialSide == RadialSide::Bottom)
            {
                g_bottomHasSelection = false;
            }
        }

        g_lastMousePos = currentMouse;

        // ============================================================
        // SMOOTHING
        // ============================================================

        g_radialMouseVelocity.x +=
            (delta.x - g_radialMouseVelocity.x) * mouseSmooth;

        g_radialMouseVelocity.y +=
            (delta.y - g_radialMouseVelocity.y) * mouseSmooth;

        ImVec2 smoothDelta = g_radialMouseVelocity;

        // ============================================================
        // LIMITE DA VELOCIDADE
        // ============================================================

        const float smoothLenSq =
            smoothDelta.x * smoothDelta.x +
            smoothDelta.y * smoothDelta.y;

        if (smoothLenSq > maxMouseStepSq)
        {
            const float smoothLen = std::sqrt(smoothLenSq);
            const float scale = maxMouseStep / smoothLen;

            smoothDelta.x *= scale;
            smoothDelta.y *= scale;
        }

        if (std::abs(smoothDelta.x) < 0.001f &&
            std::abs(smoothDelta.y) < 0.001f)
        {
            return;
        }

        // ============================================================
        // NOVO VETOR
        // ============================================================

        ImVec2 proposedVector(
            g_radialVector.x + smoothDelta.x,
            g_radialVector.y + smoothDelta.y
        );

        // ============================================================
        // LIMITE DO RADIAL
        // ============================================================

        const float maxLength = g_quickDrawGameplayActive
            ? QUICK_DRAW_EDITOR_RADIUS
            : GetMenuLineLength();

        if (maxLength > 0.0f)
        {
            const float maxLengthSq = maxLength * maxLength;

            float proposedLenSq =
                proposedVector.x * proposedVector.x +
                proposedVector.y * proposedVector.y;

            if (proposedLenSq > maxLengthSq)
            {
                const float currentLenSq =
                    g_radialVector.x * g_radialVector.x +
                    g_radialVector.y * g_radialVector.y;

                const float dot =
                    smoothDelta.x * g_radialVector.x +
                    smoothDelta.y * g_radialVector.y;

                if (dot > 0.0f && currentLenSq >= maxLengthSq)
                {
                    if (currentLenSq > 0.000001f)
                    {
                        const float currentLen = std::sqrt(currentLenSq);
                        const float invLen = 1.0f / currentLen;

                        const ImVec2 radialDir(
                            g_radialVector.x * invLen,
                            g_radialVector.y * invLen
                        );

                        const float radialDelta =
                            smoothDelta.x * radialDir.x +
                            smoothDelta.y * radialDir.y;

                        proposedVector.x -= radialDir.x * radialDelta;
                        proposedVector.y -= radialDir.y * radialDelta;
                    }

                    proposedLenSq =
                        proposedVector.x * proposedVector.x +
                        proposedVector.y * proposedVector.y;

                    if (proposedLenSq > maxLengthSq)
                    {
                        const float proposedLen = std::sqrt(proposedLenSq);
                        const float scale = maxLength / proposedLen;

                        proposedVector.x *= scale;
                        proposedVector.y *= scale;
                    }
                }
                else
                {
                    const float proposedLen = std::sqrt(proposedLenSq);

                    if (proposedLen > 0.0f)
                    {
                        const float scale = maxLength / proposedLen;

                        proposedVector.x *= scale;
                        proposedVector.y *= scale;
                    }
                }
            }
        }

        g_radialVector = proposedVector;

        // ============================================================
        // RECENTRALIZAÇÃO FÍSICA
        // ============================================================

        const float edgeMargin = GetRadialEdgeMargin();

        const bool hitEdge =
            currentMouse.x <= edgeMargin ||
            currentMouse.x >= io.DisplaySize.x - edgeMargin ||
            currentMouse.y <= edgeMargin ||
            currentMouse.y >= io.DisplaySize.y - edgeMargin;

        if (!hitEdge)
            return;

        HWND hwnd = GetForegroundWindow();

        if (!hwnd)
            return;

        RECT rect{};

        if (!GetWindowRect(hwnd, &rect))
            return;

        const int centerX = (rect.left + rect.right) / 2;
        const int centerY = (rect.top + rect.bottom) / 2;

        SetCursorPos(centerX, centerY);

        g_radialRecentered = true;
    }

       
    static ImVec2 GetSettingsFloatingPosition(
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID,
        const ImVec2& originalPosition,
        float time)
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return originalPosition;

        if (!form)
            return originalPosition;

        // Cada item possui uma fase diferente.
        const float phase =
            static_cast<float>(
                form->GetFormID() % 1000
            ) * 0.013f;

        constexpr float amplitude = 2.5f;

        return ImVec2(
            originalPosition.x +
                std::sin(time * 0.75f + phase) *
                amplitude,

            originalPosition.y +
                std::cos(time * 0.90f + phase * 1.3f) *
                amplitude
        );
    }

    static float UpdateSettingsItemHover(
        RE::TESForm* form,
        std::uint16_t uniqueID,
        bool hasUniqueID,
        float deltaTime)
    {
        if (!form)
            return 0.0f;

        RadialItemAnimation& anim =
            GetOrCreateAnim(
                form,
                uniqueID,
                hasUniqueID
            );

        const bool hovered =
            SettingsMenu::WheelSettingsMenu::IsOpen() &&

            SameSettingsItem(
                form,
                uniqueID,
                hasUniqueID,
                g_settingsHoveredItem,
                g_settingsHoveredUniqueID,
                g_settingsHoveredHasUniqueID
            ) &&

            !g_settingsDrag.active;

        const float target = hovered ? 1.0f : 0.0f;

        const float factor =
            1.0f - std::exp(
                -14.0f * std::clamp(deltaTime, 0.0f, 0.05f)
            );

        anim.settingsHoverT +=
            (target - anim.settingsHoverT) * factor;

        return anim.settingsHoverT;
    }

        
    // ============================================================
    // DESENHA QUANTIDADE DO ITEM
    //
    // O badge permanece sobreposto ao item normalmente.
    // Quando hovered, afasta-se na direção oposta ao centro
    // do radial.
    // ============================================================

    //se quiser mais colado ou mais afastado

    //ajusta :

    //const float baseOffset = itemRadius + badgeRadius + 3.0f;
    //const float hoverExtraOffset = 16.0f;

    enum class QuantityBadgeDirection
    {
        AutoToCenter,
        Up,
        Down
    };

        
    static std::string FormatItemQuantity(int quantity)
    {
        if (quantity < 10000)
            return std::to_string(quantity);

        // ========================================================
        // CONVERSÃO PARA MILHARES
        // ========================================================

        // Arredonda para uma casa decimal.
        //const int tenths = (quantity + 50) / 100;
        //Nao arredonda pra cima
        const int tenths = quantity / 100;

        const int thousands = tenths / 10;
        const int decimal = tenths % 10;

        std::string result =
            std::to_string(thousands);

        // Só adiciona a casa decimal quando necessária.
        if (decimal != 0)
        {
            result += ",";
            result += std::to_string(decimal);
        }

        result += "k";

        return result;
    }

    static void DrawRadialItemQuantity(
        ImDrawList* draw,
        const ImVec2& itemPos,
        const ImVec2& radialCenter,
        float itemRadius,
        int quantity,
        float hoverT,
        float alpha,
        QuantityBadgeDirection directionMode = QuantityBadgeDirection::AutoToCenter)
    {
        if (!draw || quantity <= 1)
            return;

        // ========================================================
        // DIREÇÃO
        // ========================================================

        float dx = 0.0f;
        float dy = 0.0f;

        switch (directionMode)
        {
        case QuantityBadgeDirection::Up:
            dx = 0.0f;
            dy = -1.0f;
            break;

        case QuantityBadgeDirection::Down:
            dx = 0.0f;
            dy = 1.0f;
            break;

        case QuantityBadgeDirection::AutoToCenter:
        default:
        {
            dx = radialCenter.x - itemPos.x;
            dy = radialCenter.y - itemPos.y;

            const float distance =
                std::sqrt(dx * dx + dy * dy);

            if (distance > 0.001f)
            {
                dx /= distance;
                dy /= distance;
            }
            else
            {
                dx = 0.0f;
                dy = 1.0f;
            }
            break;
        }
        }

        // ========================================================
        // HOVER SUAVE
        // ========================================================

        hoverT =
            std::clamp(hoverT, 0.0f, 1.0f);

        const float smoothHover =
            hoverT * hoverT *
            (3.0f - 2.0f * hoverT);

        // ========================================================
        // TAMANHO
        // ========================================================

        //constexpr float badgeRadius = 13.5f;
        // ========================================================
        // TAMANHO DINÂMICO CONFORME A QUANTIDADE
        // ========================================================

        const float fontScale = std::clamp(Config::g_fontSizeScale, 1.0f, 2.5f);
        const float badgeRadius = (
            quantity >= 10000 ? 18.0f :
            quantity >= 1000 ? 15.5f :
            quantity >= 100  ? 13.5f :
                            11.5f) * fontScale;

        // ========================================================
        // POSIÇÃO
        // ========================================================

        const float baseOffset =
            itemRadius + badgeRadius + 3.0f;

        const float hoverExtraOffset =
            16.0f;

        const float finalOffset =
            baseOffset +
            hoverExtraOffset * smoothHover;

        const ImVec2 badgePos(
            itemPos.x + dx * finalOffset,
            itemPos.y + dy * finalOffset
        );

        // ========================================================
        // VISUAL
        // ========================================================

        const int bgAlpha =
            static_cast<int>(127.0f * alpha);

        const int borderAlpha =
            static_cast<int>(
                (120.0f + 110.0f * smoothHover) * alpha
            );

        draw->AddCircleFilled(
            badgePos,
            badgeRadius,
            IM_COL32(20, 20, 25, bgAlpha),
            32
        );

        //draw->AddCircle(
        //    badgePos,
        //    badgeRadius,
        //    IM_COL32(255, 255, 255, borderAlpha),
        //    32,
        //    1.5f + 0.7f * smoothHover
        //);

        //quantidade de itens sem formataçao (raw)
        const std::string text =
            std::to_string(quantity);

        //quantidade de itens com formataçao em "k"
        //const std::string text =
        //    FormatItemQuantity(quantity);

        const ImVec2 textSize =
            ImGui::CalcTextSize(text.c_str());

        // ImFont performs CPU-side glyph clipping before Resolution can
        // expand ImGui's root 1920x1080 clip rect. Give quantity text the
        // complete physical viewport in virtual coordinates up front.
        draw->PushClipRect(
            Resolution::ToVirtual(ImVec2(0.0f, 0.0f)),
            Resolution::ToVirtual(Resolution::GetRealSize()),
            false);
        
        draw->AddText(
            ImVec2(
                badgePos.x - textSize.x * 0.5f,
                badgePos.y - textSize.y * 0.5f
            ),
            FadeColor(
                IM_COL32(255, 255, 255, 235),
                alpha
            ),
            text.c_str()
        );
        
        draw->PopClipRect();
    }

    // ============================================================
    // INDICADOR DE QUICK DRAW
    //
    // No WheelSettings, itens que têm um gesto salvo recebem um ponto no
    // lado oposto ao contador de quantidade. Assim o marcador acompanha a
    // leitura visual do slot sem disputar espaço com o número interno.
    // ============================================================
    static void DrawQuickDrawIndicator(
        ImDrawList* draw,
        const RadialItem& item,
        const ImVec2& itemPos,
        const ImVec2& radialCenter,
        float itemRadius,
        int quantity,
        float alpha,
        float hoverT,
        bool isOverflow,
        QuantityBadgeDirection directionMode = QuantityBadgeDirection::AutoToCenter)
    {
        // Cada abertura do WheelSettings inicia uma nova sessão de órbita.
        // O valor é compartilhado pelos itens apenas como semente; velocidade,
        // fase e sentido continuam únicos para cada um.
        static bool wasSettingsOpen = false;
        
        static std::uint32_t orbitSession = 0;
        
        const bool settingsOpen = SettingsMenu::WheelSettingsMenu::IsOpen();
        
        if (settingsOpen && !wasSettingsOpen)
            ++orbitSession;
        
        wasSettingsOpen = settingsOpen;

        if (!draw ||
            !settingsOpen ||
            alpha <= 0.001f)
            return;

        const auto* stroke = FindQuickDrawStroke(item);
        
        if (!stroke || stroke->empty())
            return;

        // A órbita é local ao próprio item. O radial de origem/destino não
        // participa da direção inicial: isso evita aceleração e teleporte ao
        // arrastar entre Side, Top e Bottom.
        (void)radialCenter;
        (void)directionMode;

        (void)quantity;

        // A distância é medida a partir da borda do slot, nunca da fonte do
        // contador. Assim alterar Font Size não desloca a órbita. Em zero a
        // bolinha é tangente à borda; em 100 há ~76 px virtuais de espaço.
        
        hoverT = std::clamp(hoverT, 0.0f, 1.0f);
        
        const float smoothHover = hoverT * hoverT * (3.0f - 2.0f * hoverT);
        
        // Pequena flutuação própria: ela se soma ao deslocamento do item,
        // por isso o marcador não parece preso à animação do slot.
        const float time = static_cast<float>(ImGui::GetTime());
        
        const std::uint32_t seed =
            (item.form ? item.form->GetFormID() : 0u) ^
            (static_cast<std::uint32_t>(item.uniqueID) << 16);
        
        const float phase = static_cast<float>(seed % 628u) * 0.01f;

        // Mistura a sessão atual à identidade do item. Assim o sentido e a
        // velocidade mudam a cada abertura, mas permanecem estáveis enquanto
        // o menu está aberto.
        std::uint32_t orbitSeed = seed ^
            (orbitSession * 0x9E3779B9u + 0x7F4A7C15u);
        
        orbitSeed ^= orbitSeed >> 16;
        orbitSeed *= 0x7FEB352Du;
        orbitSeed ^= orbitSeed >> 15;
        
        const float orbitVariation =
            static_cast<float>(orbitSeed & 0xFFFFu) / 65535.0f;
        
        const float orbitSpeed = 0.36f * (0.60f + 0.80f * orbitVariation);
        
        const float orbitDirection = (orbitSeed & 0x10000u) ? 1.0f : -1.0f;
        
        const float orbitAngle = orbitDirection *
            (time * orbitSpeed +
                static_cast<float>((orbitSeed >> 17) % 628u) * 0.01f);

        
        const float floatScale = isOverflow ? 0.80f : 1.0f;
        const float pointRadius = 3.6f * floatScale;
        constexpr float kMaximumDrawMarkGap = 76.0f;
        const float edgeGap = std::clamp(Config::g_drawMarkDistance, 0.0f, 100.0f) /
            100.0f * kMaximumDrawMarkGap;
        const float offset = itemRadius + pointRadius + edgeGap +
            16.0f * smoothHover;
        
        const ImVec2 ownFloat(
            std::sin(time * 1.25f + phase) * 1.35f * floatScale,
            std::cos(time * 1.05f + phase * 1.37f) * 1.65f * floatScale);
        
        // Vetor inicial aleatório por item/sessão, sempre local ao ícone.
        // `orbitAngle` já inclui a fase aleatória e a rotação contínua.
        const float baseX = offset;
        const float baseY = 0.0f;
        
        const float orbitCos = std::cos(orbitAngle);
        
        const float orbitSin = std::sin(orbitAngle);
        
        const ImVec2 markerPos(
            itemPos.x + baseX * orbitCos - baseY * orbitSin + ownFloat.x,
            itemPos.y + baseX * orbitSin + baseY * orbitCos + ownFloat.y);

        // Pulso lento e individual: cada item parte de uma fase diferente,
        // variando suavemente entre 20% e sua opacidade normal.
        const float pulse = 0.20f + 0.80f *
            (0.5f + 0.5f * std::sin(time * 0.72f + phase * 1.91f));

        // Mesmo desenho do cursor do WheelSettings, 10% menor. Excedentes
        // recebem mais 20% de redução e metade da opacidade dos principais.
        const float indicatorAlpha =
            alpha * (isOverflow ? 0.50f : 0.80f) * pulse;

        // Histórico real da posição do marcador, mantido desativado para uma
        // futura iteração de cauda baseada no movimento real do item.
#if 0
        // A cauda não é mais uma
        // projeção matemática do arco: cada segmento ocupa uma posição onde
        // a bolinha realmente esteve. Isso conserva a órbita parada e cria
        // curvas naturais quando o item é arrastado.
        struct TrailPoint
        {
            ImVec2 position{};
            float time = 0.0f;
        };
        struct IndicatorTrail
        {
            QuickDrawKey key{};
            ImVec2 radialCenter{};
            std::vector<TrailPoint> points;
            float lastSeenTime = 0.0f;
            int lastSampleFrame = -1;
        };
        static std::vector<IndicatorTrail> indicatorTrails;
        static std::uint32_t trailSession = 0;
        if (trailSession != orbitSession)
        {
            indicatorTrails.clear();
            trailSession = orbitSession;
        }

        const QuickDrawKey quickDrawKey = MakeQuickDrawKey(item);
        const auto sameCenter = [&](const ImVec2& a, const ImVec2& b) {
            const float x = a.x - b.x;
            const float y = a.y - b.y;
            return x * x + y * y < 1.0f;
        };
        auto trailIt = std::find_if(indicatorTrails.begin(), indicatorTrails.end(),
            [&](const IndicatorTrail& trail) {
                return SameQuickDrawKey(trail.key, quickDrawKey) &&
                    sameCenter(trail.radialCenter, radialCenter);
            });
        if (trailIt == indicatorTrails.end())
        {
            IndicatorTrail trail{};
            trail.key = quickDrawKey;
            trail.radialCenter = radialCenter;

            // Ao soltar em outro radial, reaproveita e translada o histórico
            // mais recente da mesma instância. Assim a cauda chega ao slot
            // novo sem teletransportar ou perder a curva do arrasto.
            auto donor = std::max_element(indicatorTrails.begin(), indicatorTrails.end(),
                [&](const IndicatorTrail& a, const IndicatorTrail& b) {
                    const bool aMatch = SameQuickDrawKey(a.key, quickDrawKey);
                    const bool bMatch = SameQuickDrawKey(b.key, quickDrawKey);
                    if (aMatch != bMatch)
                        return !aMatch;
                    return a.lastSeenTime < b.lastSeenTime;
                });
            if (donor != indicatorTrails.end() &&
                SameQuickDrawKey(donor->key, quickDrawKey) &&
                !donor->points.empty() && time - donor->lastSeenTime < 0.25f)
            {
                const ImVec2 delta(
                    markerPos.x - donor->points.front().position.x,
                    markerPos.y - donor->points.front().position.y);
                trail.points = donor->points;
                for (auto& point : trail.points)
                {
                    point.position.x += delta.x;
                    point.position.y += delta.y;
                }
            }
            else
            {
                trail.points.push_back({ markerPos, time });
            }
            indicatorTrails.push_back(std::move(trail));
            trailIt = std::prev(indicatorTrails.end());
        }

        IndicatorTrail& trail = *trailIt;
        trail.lastSeenTime = time;
        const int frame = ImGui::GetFrameCount();
        if (trail.lastSampleFrame != frame)
        {
            const bool needsSample = trail.points.empty() ||
                std::hypot(markerPos.x - trail.points.front().position.x,
                    markerPos.y - trail.points.front().position.y) > 0.35f;
            if (needsSample)
                trail.points.insert(trail.points.begin(), { markerPos, time });
            trail.lastSampleFrame = frame;
        }
        constexpr std::size_t maxTrailPoints = 720;
        if (trail.points.size() > maxTrailPoints)
            trail.points.resize(maxTrailPoints);

        // Mantém o comprimento-base/variação que já estavam configurados.
        const float tailVariation = 0.50f +
            static_cast<float>((orbitSeed >> 1) & 0xFFFFu) / 65535.0f;
        const float tailLength = 100.0f * tailVariation * floatScale;
        ImVec2 tailPrevious = markerPos;
        float tailDistance = 0.0f;
        for (const TrailPoint& point : trail.points)
        {
            const float dx = point.position.x - tailPrevious.x;
            const float dy = point.position.y - tailPrevious.y;
            const float segmentLength = std::sqrt(dx * dx + dy * dy);
            if (segmentLength <= 0.001f)
                continue;

            const float remaining = tailLength - tailDistance;
            if (remaining <= 0.001f)
                break;
            const float usedLength = std::min(segmentLength, remaining);
            const float segmentT = usedLength / segmentLength;
            const ImVec2 tailPoint(
                tailPrevious.x + dx * segmentT,
                tailPrevious.y + dy * segmentT);
            const float progress = std::clamp(
                tailDistance / std::max(tailLength, 0.001f), 0.0f, 1.0f);
            const float tailAlpha = indicatorAlpha * 0.70f *
                std::pow(1.0f - progress, 1.35f);
            if (tailAlpha > 0.001f)
            {
                draw->AddLine(
                    tailPrevious,
                    tailPoint,
                    FadeColor(IM_COL32(255, 255, 255, 255), tailAlpha),
                    1.25f * floatScale);
            }
            tailPrevious = tailPoint;
            tailDistance += usedLength;
        }
#endif

        // Cauda circular fixa: usa apenas a órbita atual do marcador. O
        // hover altera o offset/radius inteiro, portanto bolinha e cauda
        // crescem juntas sem qualquer influência do movimento de drag.
        const float fixedTailVariation = 0.50f +
            static_cast<float>((orbitSeed >> 1) & 0xFFFFu) / 65535.0f;
        const float fixedTailLength = 100.0f * fixedTailVariation * floatScale;
        const float fixedOrbitRadius = std::max(
            1.0f, std::sqrt(baseX * baseX + baseY * baseY));
        const int fixedTailSegments = std::max(
            16, static_cast<int>(std::ceil(fixedTailLength / 4.0f)));
        ImVec2 fixedTailPrevious = markerPos;
        for (int segment = 1; segment <= fixedTailSegments; ++segment)
        {
            const float progress = static_cast<float>(segment) /
                static_cast<float>(fixedTailSegments);
            const float angleBack = (fixedTailLength * progress) /
                fixedOrbitRadius;
            const float tailAngle = orbitAngle - orbitDirection * angleBack;
            const float tailCos = std::cos(tailAngle);
            const float tailSin = std::sin(tailAngle);
            const ImVec2 fixedTailPoint(
                itemPos.x + baseX * tailCos - baseY * tailSin + ownFloat.x,
                itemPos.y + baseX * tailSin + baseY * tailCos + ownFloat.y);
            const float tailAlpha = indicatorAlpha * 0.70f *
                std::pow(1.0f - progress, 1.35f);
            if (tailAlpha > 0.001f)
            {
                draw->AddLine(
                    fixedTailPrevious,
                    fixedTailPoint,
                    FadeColor(IM_COL32(255, 255, 255, 255), tailAlpha),
                    1.25f * floatScale);
            }
            fixedTailPrevious = fixedTailPoint;
        }
        
        draw->AddCircleFilled(
            ImVec2(markerPos.x + 1.35f, markerPos.y + 1.35f),
            4.5f * floatScale,
            FadeColor(IM_COL32(0, 0, 0, 160), indicatorAlpha),
            16);
        
        draw->AddCircleFilled(
            markerPos,
            pointRadius,
            FadeColor(IM_COL32(255, 255, 255, 255), indicatorAlpha),
            16);
        
        draw->AddCircle(
            markerPos,
            5.4f * floatScale,
            FadeColor(IM_COL32(255, 255, 255, 100), indicatorAlpha),
            24,
            1.0f);
    }

    static void DrawSettingsDragQuickDrawIndicator(
        ImDrawList* draw,
        const ImVec2& position,
        float radius)
    {
        if (!g_settingsDrag.active || !g_settingsDrag.form)
            return;

        const WheelLayout layout = GetWheelLayout();
        const ImVec2 sourceCenter =
            g_settingsDrag.sourceSide == RadialSide::Top ? layout.topRadial :
            g_settingsDrag.sourceSide == RadialSide::Bottom ? layout.bottomRadial :
            g_settingsDrag.sourceSide == RadialSide::Right ? layout.rightRadial :
            layout.leftRadial;
        const ItemInfo::Data info = ItemInfo::Get(
            g_settingsDrag.item.form,
            g_settingsDrag.item.uniqueID,
            g_settingsDrag.item.hasUniqueID);

        DrawQuickDrawIndicator(
            draw,
            g_settingsDrag.item,
            position,
            sourceCenter,
            radius,
            info.instanceQuantity,
            1.0f,
            0.0f,
            false,
            QuantityBadgeDirection::AutoToCenter);
    }

    
    static std::string GetRadialItemDisplayName(
        const RadialItem& item)
    {
        if (!item.form)
            return {};

        // ============================================================
        // NOME PADRÃO
        // ============================================================

        const char* baseName =
            item.form->GetName();

        std::string result =
            baseName ? baseName : "";

        // ============================================================
        // APENAS ARMAS E ARMADURAS
        // ============================================================

        auto* object =
            item.form->As<RE::TESBoundObject>();

        if (!object)
            return result;

        if (!item.form->As<RE::TESObjectWEAP>() &&
            !item.form->As<RE::TESObjectARMO>())
        {
            return result;
        }

        // ============================================================
        // PROCURA A INSTÂNCIA NO INVENTÁRIO REAL
        // ============================================================

        auto* player =
            RE::PlayerCharacter::GetSingleton();

        if (!player)
            return result;

        auto* changes =
            player->GetInventoryChanges();

        if (!changes || !changes->entryList)
            return result;

        for (auto* entry : *changes->entryList)
        {
            if (!entry || !entry->object)
                continue;

            if (entry->object != object)
                continue;

            if (!entry->extraLists)
                continue;

            // ========================================================
            // LOCALIZA A INSTÂNCIA PELO UNIQUE ID
            // ========================================================

            for (auto* extra : *entry->extraLists)
            {
                if (!extra)
                    continue;

                if (item.hasUniqueID)
                {
                    auto* unique =
                        extra->GetByType<RE::ExtraUniqueID>();

                    if (!unique)
                        continue;

                    if (unique->uniqueID != item.uniqueID)
                        continue;

                    if (unique->baseID != player->GetFormID())
                        continue;
                }
                else
                {
                    // Sem UniqueID, não podemos escolher
                    // arbitrariamente outra instância.
                    continue;
                }

                // ============================================================
                // NOME DA INSTÂNCIA
                // ============================================================

                const char* displayName =
                    extra->GetDisplayName(object);

                std::string finalName =
                    (displayName && displayName[0] != '\0')
                    ? displayName
                    : result;

                // ============================================================
                // ENCANTAMENTO INDIVIDUAL
                // ============================================================

                auto* enchantData =
                    extra->GetByType<RE::ExtraEnchantment>();

                if (enchantData && enchantData->enchantment)
                {
                    const char* enchantName =
                        enchantData->enchantment->GetName();

                    if (enchantName && enchantName[0] != '\0')
                    {
                        // Evita adicionar o nome do encantamento duas vezes.
                        if (finalName.find(enchantName) == std::string::npos)
                        {
                            finalName += " - ";
                            finalName += enchantName;
                        }
                    }
                }

                return finalName;
            }
        }

        return result;
    }


        
    static void DrawSettingsPreviewQuantity(
        const RadialItem& item,
        float alpha,
        Config::ItemPreviewProfile profile = Config::ItemPreviewProfile::Menu)
    {
        if (!item.form)
            return;

        // ========================================================
        // QUANTIDADE DA INSTÂNCIA
        // ========================================================

        const ItemInfo::Data& info =
            ItemInfo::GetPreviewInfo(
                item.form,
                item.uniqueID,
                item.hasUniqueID,
                GetRadialItemDisplayName(item)
            );

        const int quantity =
            info.instanceQuantity;

        // Só aparece quando existe mais de uma unidade.
        if (quantity <= 1)
            return;

        ImDrawList* draw =
            ImGui::GetForegroundDrawList();

        const WheelLayout wheelLayout = GetWheelLayout();
        const auto& preview = Config::GetItemPreviewLayoutConst(profile);

        // ========================================================
        // POSIÇÃO DO PREVIEW
        // ========================================================

        const ImVec2 previewCenter(
            LayoutLerp(wheelLayout.min.x, wheelLayout.max.x, preview.itemPositionX),
            LayoutLerp(wheelLayout.min.y, wheelLayout.max.y, preview.itemPositionY)
        );

        // ========================================================
        // POSIÇÃO FIXA DA QUANTIDADE
        //
        // Fica abaixo e à direita do preview.
        // Os offsets são proporcionais à tela.
        // ========================================================

        const ImVec2 originalPosition(
            previewCenter.x + (wheelLayout.max.x - wheelLayout.min.x) * 0.060f *
                (preview.itemSize * 0.01f),
            previewCenter.y + (wheelLayout.max.y - wheelLayout.min.y) * 0.11f *
                (preview.itemSize * 0.01f)
        );

        // ========================================================
        // ANIMAÇÃO DE FLUTUAÇÃO
        // ========================================================

        //const ImVec2 quantityPos =
        //    GetSettingsFloatingPosition(
        //        item.form,
        //        item.uniqueID,
        //        item.hasUniqueID,
        //        originalPosition,
        //        static_cast<float>(
        //            ImGui::GetTime()
        //        )
        //    );

        // ========================================================
        // FLUTUAÇÃO NO SETTINGS E GAMEPLAY
        // ========================================================

        ImVec2 quantityPos = originalPosition;

        if (g_radialMode == RadialMode::Gameplay ||
            SettingsMenu::WheelSettingsMenu::IsOpen())
        {
            // A função original só anima no Settings.
            // Aqui aplicamos a mesma flutuação nos dois modos.

            const float time =
                static_cast<float>(ImGui::GetTime());

            const float phase =
                static_cast<float>(
                    item.form->GetFormID() % 1000
                ) * 0.013f;

            constexpr float amplitude = 2.5f;

            quantityPos.x +=
                std::sin(time * 0.75f + phase) *
                amplitude;

            quantityPos.y +=
                std::cos(time * 0.90f + phase * 1.3f) *
                amplitude;
        }

        // ========================================================
        // TAMANHO DINÂMICO
        //
        // Aproximadamente 2,5x o tamanho da
        // bolinha original dos itens.
        // ========================================================


        const float fontScale = std::clamp(Config::g_fontSizeScale, 1.0f, 2.5f);
        const float badgeRadius = (
            quantity >= 10000 ? 20.0f :
            quantity >= 1000 ? 18.5f :
            quantity >= 100  ? 15.5f :
                            13.5f) * fontScale;

        const float badgeFinalRadius =
            badgeRadius * 1.3f;

        // ========================================================
        // FUNDO CINZA ESCURO
        // ========================================================

        draw->AddCircleFilled(
            quantityPos,
            badgeFinalRadius,
            FadeColor(
                IM_COL32(20, 20, 25, 100),
                alpha
            ),
            48
        );

        // ========================================================
        // BORDA SUAVE
        // ========================================================

        //draw->AddCircle(
        //    quantityPos,
        //    badgeRadius,
        //    FadeColor(
        //        IM_COL32(255, 255, 255, 110),
        //        alpha
        //    ),
        //    48,
        //    1.5f
        //);

        // ========================================================
        // TEXTO DA QUANTIDADE
        // ========================================================

        const std::string quantityText =
            FormatItemQuantity(quantity);

        ImFont* font =
            ImGui::GetFont();

        if (!font)
            return;

        // Aumenta também o tamanho do texto.
        const float fontSize =
            ImGui::GetFontSize() * 1.20f;

        const ImVec2 textSize =
            font->CalcTextSizeA(
                fontSize,
                FLT_MAX,
                0.0f,
                quantityText.c_str()
            );

        const ImVec2 textPos(
            quantityPos.x - textSize.x * 0.5f,
            quantityPos.y - textSize.y * 0.5f
        );

        // Sombra.
        draw->AddText(
            font,
            fontSize,
            ImVec2(
                textPos.x + 1.0f,
                textPos.y + 1.0f
            ),
            FadeColor(
                IM_COL32(0, 0, 0, 190),
                alpha
            ),
            quantityText.c_str()
        );

        // Texto original.
        draw->AddText(
            font,
            fontSize,
            textPos,
            FadeColor(
                IM_COL32(255, 255, 255, 235),
                alpha
            ),
            quantityText.c_str()
        );
    }



    // ============================================================
    static ImVec4 GetGameplayIconColor(const RadialItem& item)
    {
        const auto color = [](std::uint32_t packed) {
            return ImVec4(((packed >> 16) & 0xFF) / 255.0f,
                ((packed >> 8) & 0xFF) / 255.0f,
                (packed & 0xFF) / 255.0f, 1.0f);
        };
        const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);

        if (!item.form)
            return white;

        const ItemIcon::Type iconType = ItemIcon::ResolveType(item.form);
        if (Config::g_coloredPotions)
        {
            switch (iconType)
            {
            case ItemIcon::Type::PotionHealth: return color(Config::g_potionColors[0]);
            case ItemIcon::Type::PotionStamina: return color(Config::g_potionColors[1]);
            case ItemIcon::Type::PotionMagicka: return color(Config::g_potionColors[2]);
            case ItemIcon::Type::PotionPoison: return color(Config::g_potionColors[3]);
            case ItemIcon::Type::PotionFire: return color(Config::g_potionColors[4]);
            case ItemIcon::Type::PotionFrost: return color(Config::g_potionColors[5]);
            case ItemIcon::Type::PotionShock: return color(Config::g_potionColors[6]);
            default: break;
            }
        }

        const auto schoolColor = [&](RE::ActorValue school) {
            switch (school)
            {
            case RE::ActorValue::kAlteration: return color(Config::g_schoolColors[0]);
            case RE::ActorValue::kConjuration: return color(Config::g_schoolColors[1]);
            case RE::ActorValue::kDestruction: return color(Config::g_schoolColors[2]);
            case RE::ActorValue::kIllusion: return color(Config::g_schoolColors[3]);
            case RE::ActorValue::kRestoration: return color(Config::g_schoolColors[4]);
            default: return white;
            }
        };

        if (Config::g_coloredMagicSchools)
        {
            switch (iconType)
            {
            case ItemIcon::Type::MagicFire: return color(Config::g_magicElementColors[0]);
            case ItemIcon::Type::MagicFrost: return color(Config::g_magicElementColors[1]);
            case ItemIcon::Type::MagicShock: return color(Config::g_magicElementColors[2]);
            default: break;
            }

            if (const auto* spell = item.form->As<RE::SpellItem>())
                return schoolColor(spell->GetAssociatedSkill());
        }

        if (Config::g_coloredItemEnchants &&
            (item.form->As<RE::TESObjectWEAP>() || item.form->As<RE::TESObjectARMO>()))
        {
            const auto info = ItemInfo::Get(item.form, item.uniqueID, item.hasUniqueID);
            if (info.isEnchanted)
            {
                for (const auto& effect : info.enchantment.effects)
                {
                    std::string name = effect.name;
                    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });
                    if (name.find("fire") != std::string::npos) return color(Config::g_enchantColors[0]);
                    if (name.find("frost") != std::string::npos || name.find("ice") != std::string::npos) return color(Config::g_enchantColors[1]);
                    if (name.find("shock") != std::string::npos || name.find("lightning") != std::string::npos) return color(Config::g_enchantColors[2]);
                    if (name.find("poison") != std::string::npos) return color(Config::g_enchantColors[3]);
                }
                for (const auto& effect : info.enchantment.effects)
                {
                    const ImVec4 tint = schoolColor(effect.associatedSkill);
                    if (tint.x != 1.0f || tint.y != 1.0f || tint.z != 1.0f)
                        return tint;
                }
                return color(Config::g_enchantColors[4]);
            }
        }

        return white;
    }

    static ImU32 MakeGameplayIconColor(const RadialItem& item, int alpha,
        float brightness, bool applyBaseOpacity, RadialSide side)
    {
        const ImVec4 tint = GetGameplayIconColor(item);
        
        const ItemVisualStyle style = GetItemVisualStyle(side);
        
        const float baseR = static_cast<float>((style.iconColor >> 16) & 0xFF) / 255.0f;
        
        const float baseG = static_cast<float>((style.iconColor >> 8) & 0xFF) / 255.0f;
        
        const float baseB = static_cast<float>(style.iconColor & 0xFF) / 255.0f;
        
        if (applyBaseOpacity)
            alpha = static_cast<int>(alpha *
                std::clamp(style.iconOpacity * 0.01f, 0.0f, 1.0f));
        
        return IM_COL32(
            static_cast<int>(std::clamp(tint.x * baseR * brightness, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(tint.y * baseG * brightness, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(tint.z * baseB * brightness, 0.0f, 1.0f) * 255.0f),
            alpha);
    }

    // Após remover um item no WheelSettings, o número/distribuição de slots
    // custom muda de uma vez. Esta transição é deliberadamente cartesiana e
    // curta: ela acomoda os vizinhos suavemente sem deixar o novo alvo ser
    // interpretado como uma ordem de percorrer todo o circuito.
    static ImVec2 GetSettingsCircuitSettlingPosition(
        RadialItemAnimation& anim, const ImVec2& target, float deltaTime)
    {
        if (!anim.posInitialized)
        {
            anim.currentPos = target;
            anim.previousPos = target;
            anim.posInitialized = true;
            anim.settingsDropSettling = false;
            
            return target;
        }

        const float factor = 1.0f - std::exp(-7.5f *
            std::clamp(deltaTime, 0.0f, 1.0f / 30.0f));
        
        anim.previousPos = anim.currentPos;
        
        anim.currentPos.x += (target.x - anim.currentPos.x) * factor;
        anim.currentPos.y += (target.y - anim.currentPos.y) * factor;
        
        const float dx = target.x - anim.currentPos.x;
        const float dy = target.y - anim.currentPos.y;
        
        if (dx * dx + dy * dy < 0.25f)
        {
            anim.currentPos = target;
            anim.previousPos = target;
            anim.settingsDropSettling = false;
        }
        
        return anim.currentPos;
    }

    static ImVec2 GetSettingsCustomDropEntryPosition(
        RadialItemAnimation& anim, const ImVec2& target, float deltaTime)
    {
        if (!anim.settingsCustomDropEntering)
            return target;

        // A distância do TrackMovement já foi resolvida para o novo slot.
        // Interpolamos somente a apresentação de entrada, sem alterar a
        // posição lógica do circuito nem reintroduzir o slot de origem.
        constexpr float kEntryDuration = 0.20f;
        
        anim.settingsCustomDropProgress = std::min(
            1.0f,
            anim.settingsCustomDropProgress +
                std::clamp(deltaTime, 0.0f, 1.0f / 30.0f) / kEntryDuration);
        
        const float t = anim.settingsCustomDropProgress;
        
        const float smoothT = t * t * (3.0f - 2.0f * t);
        
        const ImVec2 position(
            anim.settingsCustomDropStart.x +
                (target.x - anim.settingsCustomDropStart.x) * smoothT,
            anim.settingsCustomDropStart.y +
                (target.y - anim.settingsCustomDropStart.y) * smoothT);

        if (t >= 1.0f)
        {
            anim.settingsCustomDropEntering = false;
            anim.settingsDropSettling = false;
        }
        
        return position;
    }

    static ImVec2 GetInventoryDropSettlingPosition(
        RadialItemAnimation& anim, const ImVec2& target, float deltaTime)
    {
        if (!anim.inventoryDropSettling)
            return target;

        // Termina antes do snap (0,20 s), para nenhum estado de acomodação
        // sobreviver ao fechamento automático do radial de inventário.
        constexpr float duration = 0.18f;
        
        anim.inventoryDropProgress = std::min(1.0f,
            anim.inventoryDropProgress +
                std::clamp(deltaTime, 0.0f, 1.0f / 30.0f) / duration);
        
        const float t = anim.inventoryDropProgress;
        
        const float smooth = t * t * (3.0f - 2.0f * t);
        
        const ImVec2 result(
            anim.inventoryDropStart.x +
                (target.x - anim.inventoryDropStart.x) * smooth,
            anim.inventoryDropStart.y +
                (target.y - anim.inventoryDropStart.y) * smooth);

        anim.previousPos = anim.currentPos;
        anim.currentPos = result;
        anim.posInitialized = true;

        if (anim.inventoryDropProgress >= 1.0f)
        {
            anim.inventoryDropSettling = false;
            anim.inventoryDropProgress = 0.0f;
            anim.previousPos = target;
            anim.currentPos = target;
        }
        
        return anim.currentPos;
    }

    static ImVec2 GetInventorySideSettlingPosition(
        RadialItemAnimation& anim, const ImVec2& target,
        float deltaTime, bool leftSide)
    {
        const std::size_t side = leftSide ? 0u : 1u;

        // Left e Right exibem a mesma lista. Cada apresentação precisa manter
        // sua própria posição visual; currentPos pertence ao item e não pode
        // alternar entre centros opostos duas vezes no mesmo frame.
        if (!anim.inventorySideInitialized[side])
        {
            anim.inventorySidePosition[side] = target;
            anim.inventorySideStart[side] = target;
            anim.inventorySideProgress[side] = 0.0f;
            anim.inventorySideSettling[side] = false;
            anim.inventorySideInitialized[side] = true;
            return target;
        }

        if (!anim.inventorySideSettling[side])
        {
            anim.inventorySidePosition[side] = target;
            return target;
        }

        constexpr float duration = 0.18f;
        
        anim.inventorySideProgress[side] = std::min(
            1.0f,
            anim.inventorySideProgress[side] +
                std::clamp(deltaTime, 0.0f, 1.0f / 30.0f) / duration);

        const float t = anim.inventorySideProgress[side];
        
        const float smooth = t * t * (3.0f - 2.0f * t);
        
        const ImVec2& start = anim.inventorySideStart[side];
        
        ImVec2 result(
            start.x + (target.x - start.x) * smooth,
            start.y + (target.y - start.y) * smooth);

        anim.inventorySidePosition[side] = result;
        
        if (anim.inventorySideProgress[side] >= 1.0f)
        {
            anim.inventorySideSettling[side] = false;
            anim.inventorySideProgress[side] = 0.0f;
            anim.inventorySidePosition[side] = target;
            result = target;
        }
        
        return result;
    }

    static ImVec2 GetInventoryCircuitPosition(
        RadialItemAnimation& anim, const ImVec2& target,
        const ImVec2& center, float radius, bool leftSide,
        int rotationDirection, float deltaTime,
        RadialAnimation::Style style, RadialShape::Style radialShape,
        float targetCircuitT)
    {
        const std::size_t side = leftSide ? 0u : 1u;
        // Uma mudança de quantidade não é um passo de scroll. Se ambos
        // acontecerem antes do próximo frame, não reutilize a direção antiga
        // para obrigar uma volta longa; o primeiro alvo novo usa a menor rota.
        const int effectiveDirection = anim.inventorySideSettling[side]
            ? 0
            : rotationDirection;
        ImVec2 current = anim.inventorySideInitialized[side]
            ? anim.inventorySidePosition[side]
            : target;

        ImVec2 result = TrackMovement::UpdateCircuit(
            anim.inventoryCircuitAnimation[side], current, target,
            center, radius, leftSide, effectiveDirection, deltaTime,
            style, radialShape, targetCircuitT);

        anim.inventorySidePosition[side] = result;
        anim.inventorySideStart[side] = result;
        anim.inventorySideProgress[side] = 0.0f;
        anim.inventorySideSettling[side] = false;
        anim.inventorySideInitialized[side] = true;
        return result;
    }

    // MENU
    // ============================================================

    void DrawTopRadialMenu(const ImVec2& center, int totalInventoryItems, float alpha)
    {
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImGuiIO& io = ImGui::GetIO();

        
        const float deltaTime = io.DeltaTime > 0.0f ? io.DeltaTime : (1.0f / 60.0f);

        const ItemVisualStyle style = GetItemVisualStyle(RadialSide::Top);
        const int visibleLimit = std::clamp(static_cast<int>(std::lround(style.quantity)), 3, 50);
        const float itemRadius = 34.0f *
            (style.generalSize * 0.01f) * (style.slotSize * 0.01f);

        int totalItems = std::max(totalInventoryItems, 0);

        // ============================================================
        // QUANTOS ITENS FICAM VISÍVEIS DE UMA VEZ, E SCROLL MAGNÉTICO
        // (só entra em ação quando totalItems > TOPBOTTOM_VISIBLE_SLOTS)
        // ============================================================

        const int visibleCount = std::min(std::max(totalItems, 1), visibleLimit);

        g_topSelectedIndex =
            WrapIndex(
                g_topSelectedIndex,
                totalItems
            );

        // ============================================================
        // ANIMAÇÃO DO SCROLL
        // ============================================================

        constexpr float scrollSpeed = 12.0f;

        g_topScrollAnim +=
            (0.0f - g_topScrollAnim) *
            (1.0f - std::exp(
                -scrollSpeed * deltaTime
            ));

        if (std::abs(g_topScrollAnim) < 0.001f)
        {
            g_topScrollAnim = 0.0f;
        }

        // ============================================================
        // DESENHO DA CURVA-GUIA (LUA SUPERIOR)
        // ============================================================

        ImVec2 previous;
        for (int i = 0; i <= 60; i++)
        {
            float t = static_cast<float>(i) / 60.0f;
            ImVec2 current = GetTopBottomCurvePoint(center, t, true);

            if (i > 0)
            {
                float segT = (static_cast<float>(i) - 0.5f) / 60.0f;
                float edgeAlphaFactor = 4.0f * segT * (1.0f - segT);
                float segmentAlpha = alpha * edgeAlphaFactor;

                draw->AddLine(
                    previous,
                    current,
                    FadeColor(IM_COL32(255, 255, 255, static_cast<int>(
                        100.0f * std::clamp(style.lineOpacity * 0.01f, 0.0f, 1.0f))), segmentAlpha),
                    2.0f
                );
            }
            previous = current;
        }

        if (totalItems <= 0)
            return;

        // ------------------------------------------------------------
        // DETECÇÃO DE HOVER (baseada nos itens VISÍVEIS, não numa
        // régua absoluta de 13 posições - corrige dessincronia com
        // a seleção real do item)
        // ------------------------------------------------------------

        //int selectedVisibleIndex = GetTopRadialItem(
        //    GetRadialMousePosition(), center, visibleCount, MENU_INNER_RADIUSTOPBOTTOM
        //);
        //int mouseVisibleIndex =
        //    GetTopRadialItem(
        //        radialMouse,
        //        center,
        //        visibleCount,
        //        MENU_INNER_RADIUSTOPBOTTOM
        //    );
        // ============================================================
        // RENDERIZAÇÃO: itens distribuídos igualmente pelo arco,
        // sempre centralizados independente de quantos existem.
        // ============================================================
        
        const bool scrollingList =
            totalItems > visibleLimit;

        const int centerSlot =
            visibleCount / 2;

        ImDrawListSplitter topItemLayers;
        topItemLayers.Split(draw, 2);

        for (int visibleIndex = 0;
            visibleIndex < visibleCount;
            ++visibleIndex)
        {
            int realItemIndex = 0;

            if (scrollingList)
            {
                // ========================================================
                // LISTA GRANDE
                //
                // O item selecionado SEMPRE ocupa o centro.
                // Os vizinhos são obtidos circularmente.
                // ========================================================

                const int relativeIndex =
                    visibleIndex - centerSlot;

                realItemIndex =
                    WrapIndex(
                        g_topSelectedIndex + relativeIndex,
                        totalItems
                    );
            }
            else
            {
                // ========================================================
                // LISTA PEQUENA
                //
                // Os itens não se movem.
                // Só a seleção anda.
                // ========================================================

                realItemIndex =
                    visibleIndex;
            }

            if (realItemIndex < 0 || realItemIndex >= totalItems)
                continue;

            //const float t = GetTopBottomItemT(visibleIndex, visibleCount);
            // ============================================================
            // DISTRIBUIÇÃO QUE CRESCE A PARTIR DO CENTRO
            // ============================================================

            float visualIndex =
                static_cast<float>(visibleIndex);

            if (scrollingList)
            {
                visualIndex += g_topScrollAnim;
            }

            const float fullSpacing =
                1.0f /
                static_cast<float>(
                    visibleLimit - 1
                );

            const float centerIndex =
                (static_cast<float>(visibleCount) - 1.0f) *
                0.5f;

            const float t =
                0.5f +
                (visualIndex - centerIndex) *
                fullSpacing;
            
            const ImVec2 targetPos = GetTopBottomCurvePoint(center, t, true);

            // Fade suave nas pontas do arco (só relevante quando o
            // menu está cheio/rolando; com poucos itens não corta nada).
            float alphaFactor = 1.0f;
            const float fadeRange = 0.15f;

            if (visibleCount > 1)
            {
                if (t < fadeRange)
                {
                    alphaFactor = (t - (-0.05f)) / (fadeRange - (-0.05f));
                }
                else if (t > (1.0f - fadeRange))
                {
                    alphaFactor = ((1.05f) - t) / (fadeRange + 0.05f);
                }
            }

            alphaFactor = std::clamp(alphaFactor, 0.0f, 1.0f);

            if (alphaFactor <= 0.001f)
                continue;

            const float gameplayItemOpacity =
                g_radialMode == RadialMode::Gameplay
                    ? style.opacity * 0.01f : 1.0f;

            bool hovered = false;

            if (scrollingList)
            {
                hovered =
                    g_topHasSelection &&
                    (visibleIndex == centerSlot);
            }
            else
            {
                hovered =
                    g_topHasSelection &&
                    (realItemIndex == g_topSelectedIndex);
            }

            if (SettingsMenu::WheelSettingsMenu::IsOpen())
            {
                hovered = false;
            }

            if (g_radialMode == RadialMode::Gameplay &&
                g_radialSide == RadialSide::Top &&
                hovered)
            {
                SetGameplayDescriptionItem(
                    g_topItems[realItemIndex]
                );
            }

            RE::TESForm* form = g_topItems[realItemIndex].form;

            //RadialItemAnimation& anim = GetOrCreateAnim(form);

            // NOVO: posição interpolada -> resolve o "teletransporte"
            // quando a lista passa de 1 para 2, 3... itens.
            //ImVec2 itemPos = GetRadialItemAnimatedPosition(anim, targetPos, deltaTime);

            RadialItemAnimation& anim = GetOrCreateAnim(
                form,
                g_topItems[realItemIndex].uniqueID,
                g_topItems[realItemIndex].hasUniqueID
            );

            // ============================================================
            // ITEM NOVO NASCE NO CENTRO DA LUA
            // ============================================================

            if (!anim.posInitialized && g_radialMode == RadialMode::Gameplay)
            {
                const ImVec2 spawnPos =
                    GetTopBottomCurvePoint(center, 0.5f, true);

                anim.currentPos = spawnPos;
                anim.previousPos = spawnPos;
                anim.velocity = ImVec2(0.0f, 0.0f);
                anim.posInitialized = true;
            }

            //ImVec2 itemPos =
            //    GetRadialItemAnimatedPosition(
            //        anim,
            //        targetPos,
            //        deltaTime
            //    );

            ImVec2 itemPos;

            if (scrollingList &&
                std::abs(g_topScrollAnim) > 0.001f)
            {
                itemPos = targetPos;

                anim.currentPos = targetPos;
                anim.previousPos = targetPos;
                anim.velocity = ImVec2(0.0f, 0.0f);
                anim.posInitialized = true;
            }
            else if (g_radialMode == RadialMode::Inventory &&
                anim.inventoryDropSettling)
            {
                itemPos = GetInventoryDropSettlingPosition(
                    anim, targetPos, deltaTime);
            }
            else
            {
                itemPos =
                    GetRadialItemAnimatedPosition(
                        anim,
                        targetPos,
                        deltaTime
                    );
            }

            //float hoverT = StepHoverAnimation(anim, hovered, deltaTime);
            const bool allowHover =
                g_radialMode != RadialMode::Inventory ||
                SettingsMenu::WheelSettingsMenu::IsOpen();

            float hoverT =
                StepHoverAnimation(
                    anim,
                    allowHover && hovered,
                    deltaTime
                );

            float radius = itemRadius + hoverT * 8.0f;

            // ============================================================
            // SETTINGS - REGISTRA ITEM DO RADIAL SUPERIOR
            // ============================================================

            if (SettingsMenu::WheelSettingsMenu::IsOpen())
            {
                const RadialItem& item = g_topItems[realItemIndex];

                // 1. Flutuação
                itemPos = GetSettingsFloatingPosition(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID,
                    itemPos,
                    static_cast<float>(ImGui::GetTime())
                );

                // 2. Repulsão
                itemPos = GetSettingsRepulsionPosition(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID,
                    itemPos,
                    ImGui::GetIO().DeltaTime
                );

                // 3. Registra a posição final para hover e drag
                RegisterSettingsItem(
                    item.form,
                    RadialSide::Top,
                    realItemIndex,
                    itemPos,
                    radius
                );

                // 4. Hover
                hoverT = UpdateSettingsItemHover(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID,
                    ImGui::GetIO().DeltaTime
                );
            }

            EmitItemStardust(anim, itemPos, deltaTime, RadialSide::Top);

            const RadialItem& topLayerItem = g_topItems[realItemIndex];
            const bool topSettingsHovered = SettingsMenu::WheelSettingsMenu::IsOpen() &&
                SameSettingsItem(topLayerItem.form, topLayerItem.uniqueID,
                    topLayerItem.hasUniqueID, g_settingsHoveredItem,
                    g_settingsHoveredUniqueID, g_settingsHoveredHasUniqueID) &&
                !g_settingsDrag.active;
            topItemLayers.SetCurrentChannel(draw,
                hovered || topSettingsHovered ? 1 : 0);
            int bgAlpha = static_cast<int>(235 * alphaFactor);
            const float configuredBorderAlpha = 255.0f *
                std::clamp(style.borderOpacity * 0.01f, 0.0f, 1.0f);
            int borderAlpha = static_cast<int>((configuredBorderAlpha +
                (255.0f - configuredBorderAlpha) * hoverT) * alphaFactor);
            int textAlpha = static_cast<int>(255 * alphaFactor);
            int glowBgAlpha = static_cast<int>(25 * alphaFactor * hoverT);
            int glowOutAlpha = static_cast<int>(160 * alphaFactor * hoverT);

            if (hoverT > 0.001f)
            {
                draw->AddCircleFilled(
                    itemPos,
                    radius + 15.0f * hoverT,
                    FadeColor(IM_COL32(255, 255, 255, glowBgAlpha), alpha * gameplayItemOpacity)
                );

                draw->AddCircle(
                    itemPos,
                    radius + 8.0f * hoverT,
                    FadeColor(IM_COL32(255, 255, 255, glowOutAlpha), alpha * gameplayItemOpacity),
                    48,
                    2.0f
                );
            }

            const RadialItem& equippedItem =
                g_topItems[realItemIndex];

            DrawEquippedItemBackground(
                draw,
                equippedItem.form,
                equippedItem.uniqueID,
                equippedItem.hasUniqueID,
                itemPos,
                radius,
                alpha * alphaFactor * gameplayItemOpacity,
                RadialSide::Top
            );

            draw->AddCircle(
                itemPos,
                radius,
                FadeColor(IM_COL32(
                    (style.borderColor >> 16) & 0xFF,
                    (style.borderColor >> 8) & 0xFF,
                    style.borderColor & 0xFF,
                    borderAlpha),
                    alpha * gameplayItemOpacity),
                48,
                1.5f + hoverT * 1.5f
            );

            // ============================================================
            // ÍCONE DO ITEM
            // ============================================================

            auto* icon = ItemIcon::Get(form);

            if (icon)
            {
                const float iconRadius =
                    radius * 0.53f * (style.iconSize /
                        std::max(style.slotSize, 1.0f));

                const ImVec2 iconMin(
                    itemPos.x - iconRadius,
                    itemPos.y - iconRadius
                );

                const ImVec2 iconMax(
                    itemPos.x + iconRadius,
                    itemPos.y + iconRadius
                );

                draw->AddImage(
                    reinterpret_cast<ImTextureID>(icon),
                    iconMin,
                    iconMax,
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f),
                    FadeColor(
                        MakeGameplayIconColor(
                            equippedItem,
                            static_cast<int>(255 * alphaFactor), 1.0f, true,
                            RadialSide::Top
                        ),
                        alpha * gameplayItemOpacity
                    )
                );
            }

            // ============================================================
            // QUANTIDADE DO ITEM
            // ============================================================


            const RadialItem& item =
                g_topItems[realItemIndex];

            const auto itemInfo =
                ItemInfo::Get(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID
                );

            if (g_radialMode != RadialMode::Gameplay || Config::g_showItemQuantity)
                DrawRadialItemQuantity(
                    draw,
                    itemPos,
                    center,
                    radius,
                    itemInfo.instanceQuantity,
                    hoverT,
                    alpha * alphaFactor * gameplayItemOpacity,
                    QuantityBadgeDirection::Up
                );

            DrawQuickDrawIndicator(
                draw,
                item,
                itemPos,
                center,
                radius,
                itemInfo.instanceQuantity,
                alpha * alphaFactor * gameplayItemOpacity,
                hoverT,
                false,
                QuantityBadgeDirection::Up);

        }
        topItemLayers.Merge(draw);
    }

    void DrawBottomRadialMenu(const ImVec2& center, int totalInventoryItems, float alpha)
    {
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImGuiIO& io = ImGui::GetIO();

        const float deltaTime = io.DeltaTime > 0.0f ? io.DeltaTime : (1.0f / 60.0f);

        const ItemVisualStyle style = GetItemVisualStyle(RadialSide::Bottom);
        const int visibleLimit = std::clamp(static_cast<int>(std::lround(style.quantity)), 3, 50);
        const float itemRadius = 34.0f *
            (style.generalSize * 0.01f) * (style.slotSize * 0.01f);

        int totalItems = std::max(totalInventoryItems, 0);

        const int visibleCount = std::min(std::max(totalItems, 1), visibleLimit);

        //UpdateAndDrawRadialParticles(
        //    ImGui::GetForegroundDrawList(),
        //    deltaTime,
        //    alpha
        //);

        // ============================================================
        // SCROLL SUAVE (MAGNÉTICO) - só entra em ação com overflow
        // ============================================================

        g_bottomSelectedIndex =
            WrapIndex(
                g_bottomSelectedIndex,
                totalItems
            );

        // ============================================================
        // ANIMAÇÃO DO SCROLL
        // ============================================================

        constexpr float scrollSpeed = 12.0f;

        g_bottomScrollAnim +=
            (0.0f - g_bottomScrollAnim) *
            (1.0f - std::exp(
                -scrollSpeed * deltaTime
            ));

        if (std::abs(g_bottomScrollAnim) < 0.001f)
        {
            g_bottomScrollAnim = 0.0f;
        }

        // ============================================================
        // DESENHO DA CURVA-GUIA (LUA INFERIOR)
        // ============================================================

        ImVec2 previous;
        for (int i = 0; i <= 60; i++)
        {
            float t = static_cast<float>(i) / 60.0f;
            ImVec2 current = GetTopBottomCurvePoint(center, t, false);

            if (i > 0)
            {
                float segT = (static_cast<float>(i) - 0.5f) / 60.0f;
                float edgeAlphaFactor = 4.0f * segT * (1.0f - segT);
                float segmentAlpha = alpha * edgeAlphaFactor;

                draw->AddLine(
                    previous,
                    current,
                    FadeColor(IM_COL32(255, 255, 255, static_cast<int>(
                        100.0f * std::clamp(style.lineOpacity * 0.01f, 0.0f, 1.0f))), segmentAlpha),
                    2.0f
                );
            }
            previous = current;
        }

        if (totalItems <= 0)
            return;

        // ------------------------------------------------------------
        // DETECÇÃO DE HOVER (com base nos itens visíveis)
        // ------------------------------------------------------------

        //int selectedVisibleIndex = GetBottomRadialItem(
        //    GetRadialMousePosition(), center, visibleCount, MENU_INNER_RADIUSTOPBOTTOM
        //);

        // ============================================================
        // RENDERIZAÇÃO: itens sempre igualmente distribuídos e
        // centralizados, independente da quantidade.
        // ============================================================

        const bool scrollingList =
            totalItems > visibleLimit;

        const int centerSlot =
            visibleCount / 2;

        ImDrawListSplitter bottomItemLayers;
        bottomItemLayers.Split(draw, 2);

        for (int visibleIndex = 0;
            visibleIndex < visibleCount;
            ++visibleIndex)
        {
            int realItemIndex = 0;

            if (scrollingList)
            {
                // ========================================================
                // LISTA GRANDE
                //
                // O item selecionado SEMPRE ocupa o centro.
                // Os vizinhos são obtidos circularmente.
                // ========================================================

                const int relativeIndex =
                    visibleIndex - centerSlot;

                realItemIndex =
                    WrapIndex(
                        g_bottomSelectedIndex + relativeIndex,
                        totalItems
                    );
            }
            else
            {
                // ========================================================
                // LISTA PEQUENA
                //
                // Os itens não se movem.
                // Só a seleção anda.
                // ========================================================

                realItemIndex =
                    visibleIndex;
            }

            if (realItemIndex < 0 || realItemIndex >= totalItems)
                continue;

            //const float t = GetTopBottomItemT(visibleIndex, visibleCount);
            // ============================================================
            // DISTRIBUIÇÃO QUE CRESCE A PARTIR DO CENTRO
            // ============================================================

            float visualIndex =
                static_cast<float>(visibleIndex);

            if (scrollingList)
            {
                visualIndex += g_bottomScrollAnim;
            }

            const float fullSpacing =
                1.0f /
                static_cast<float>(
                    visibleLimit - 1
                );

            const float centerIndex =
                (static_cast<float>(visibleCount) - 1.0f) *
                0.5f;

            const float t =
                0.5f +
                (visualIndex - centerIndex) *
                fullSpacing;
            
            const ImVec2 targetPos = GetTopBottomCurvePoint(center, t, false);

            float alphaFactor = 1.0f;
            const float fadeRange = 0.15f;

            if (visibleCount > 1)
            {
                if (t < fadeRange)
                {
                    alphaFactor = (t - (-0.05f)) / (fadeRange - (-0.05f));
                }
                else if (t > (1.0f - fadeRange))
                {
                    alphaFactor = ((1.05f) - t) / (fadeRange + 0.05f);
                }
            }

            alphaFactor = std::clamp(alphaFactor, 0.0f, 1.0f);

            if (alphaFactor <= 0.001f)
                continue;

            const float gameplayItemOpacity =
                g_radialMode == RadialMode::Gameplay
                    ? style.opacity * 0.01f : 1.0f;

            bool hovered = false;

            if (scrollingList)
            {
                hovered =
                    g_bottomHasSelection &&
                    (visibleIndex == centerSlot);
            }
            else
            {
                hovered =
                    g_bottomHasSelection &&
                    (realItemIndex == g_bottomSelectedIndex);
            }

            if (SettingsMenu::WheelSettingsMenu::IsOpen())
            {
                hovered = false;
            }

            //envia para descript apos calcular hovered

            if (g_radialMode == RadialMode::Gameplay &&
                g_radialSide == RadialSide::Bottom &&
                hovered)
            {
                SetGameplayDescriptionItem(
                    g_bottomItems[realItemIndex]
                );
            }

            RE::TESForm* form = g_bottomItems[realItemIndex].form;

            RadialItemAnimation& anim = GetOrCreateAnim(
                form,
                g_bottomItems[realItemIndex].uniqueID,
                g_bottomItems[realItemIndex].hasUniqueID
            );

            
            if (!anim.posInitialized && g_radialMode == RadialMode::Gameplay)
            {
                const ImVec2 spawnPos =
                    GetTopBottomCurvePoint(center, 0.5f, false);

                anim.currentPos = spawnPos;
                anim.previousPos = spawnPos;
                anim.velocity = ImVec2(0.0f, 0.0f);
                anim.posInitialized = true;
            }

            //ImVec2 itemPos = GetRadialItemAnimatedPosition(anim, targetPos, deltaTime);
            ImVec2 itemPos;

            if (scrollingList &&
                std::abs(g_bottomScrollAnim) > 0.001f)
            {
                itemPos = targetPos;

                anim.currentPos = targetPos;
                anim.previousPos = targetPos;
                anim.velocity = ImVec2(0.0f, 0.0f);
                anim.posInitialized = true;
            }
            else if (g_radialMode == RadialMode::Inventory &&
                anim.inventoryDropSettling)
            {
                itemPos = GetInventoryDropSettlingPosition(
                    anim, targetPos, deltaTime);
            }
            else
            {
                itemPos =
                    GetRadialItemAnimatedPosition(
                        anim,
                        targetPos,
                        deltaTime
                    );
            }
            //if (g_radialMode == RadialMode::Inventory)
            //{
            //    EmitRadialParticles(
            //        itemPos,
            //        anim.velocity,
            //        deltaTime
            //    );
            //}

            //float hoverT = StepHoverAnimation(anim, hovered, deltaTime);
            const bool allowHover =
                g_radialMode != RadialMode::Inventory ||
                SettingsMenu::WheelSettingsMenu::IsOpen();

            float hoverT =
                StepHoverAnimation(
                    anim,
                    allowHover && hovered,
                    deltaTime
                );
                
            float radius = itemRadius + hoverT * 8.0f;

            // ============================================================
            // SETTINGS - REGISTRA ITEM DO RADIAL INFERIOR
            // ============================================================

            if (SettingsMenu::WheelSettingsMenu::IsOpen())
            {
                const RadialItem& item = g_bottomItems[realItemIndex];

                // 1. Flutuação
                itemPos = GetSettingsFloatingPosition(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID,
                    itemPos,
                    static_cast<float>(ImGui::GetTime())
                );

                // 2. Repulsão
                itemPos = GetSettingsRepulsionPosition(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID,
                    itemPos,
                    ImGui::GetIO().DeltaTime
                );

                // 3. Registra a posição final para hover e drag
                RegisterSettingsItem(
                    item.form,
                    RadialSide::Bottom,
                    realItemIndex,
                    itemPos,
                    radius
                );

                // 4. Hover
                hoverT = UpdateSettingsItemHover(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID,
                    ImGui::GetIO().DeltaTime
                );
            }
            
            EmitItemStardust(anim, itemPos, deltaTime, RadialSide::Bottom);

            const RadialItem& bottomLayerItem = g_bottomItems[realItemIndex];
            const bool bottomSettingsHovered = SettingsMenu::WheelSettingsMenu::IsOpen() &&
                SameSettingsItem(bottomLayerItem.form, bottomLayerItem.uniqueID,
                    bottomLayerItem.hasUniqueID, g_settingsHoveredItem,
                    g_settingsHoveredUniqueID, g_settingsHoveredHasUniqueID) &&
                !g_settingsDrag.active;
            bottomItemLayers.SetCurrentChannel(draw,
                hovered || bottomSettingsHovered ? 1 : 0);
            int bgAlpha = static_cast<int>(235 * alphaFactor);
            const float configuredBorderAlpha = 255.0f *
                std::clamp(style.borderOpacity * 0.01f, 0.0f, 1.0f);
            int borderAlpha = static_cast<int>((configuredBorderAlpha +
                (255.0f - configuredBorderAlpha) * hoverT) * alphaFactor);
            int textAlpha = static_cast<int>(255 * alphaFactor);
            int glowBgAlpha = static_cast<int>(25 * alphaFactor * hoverT);
            int glowOutAlpha = static_cast<int>(160 * alphaFactor * hoverT);

            if (hoverT > 0.001f)
            {
                draw->AddCircleFilled(
                    itemPos,
                    radius + 15.0f * hoverT,
                    FadeColor(IM_COL32(255, 255, 255, glowBgAlpha), alpha * gameplayItemOpacity)
                );

                draw->AddCircle(
                    itemPos,
                    radius + 8.0f * hoverT,
                    FadeColor(IM_COL32(255, 255, 255, glowOutAlpha), alpha * gameplayItemOpacity),
                    48,
                    2.0f
                );
            }

            const RadialItem& equippedItem =
                g_bottomItems[realItemIndex];

            DrawEquippedItemBackground(
                draw,
                equippedItem.form,
                equippedItem.uniqueID,
                equippedItem.hasUniqueID,
                itemPos,
                radius,
                alpha * alphaFactor * gameplayItemOpacity,
                RadialSide::Bottom
            );

            draw->AddCircle(
                itemPos,
                radius,
                FadeColor(IM_COL32(
                    (style.borderColor >> 16) & 0xFF,
                    (style.borderColor >> 8) & 0xFF,
                    style.borderColor & 0xFF,
                    borderAlpha),
                    alpha * gameplayItemOpacity),
                48,
                1.5f + hoverT * 1.5f
            );

            // ============================================================
            // ÍCONE DO ITEM
            // ============================================================

            auto* icon = ItemIcon::Get(form);

            if (icon)
            {
                const float iconRadius =
                    radius * 0.53f * (style.iconSize /
                        std::max(style.slotSize, 1.0f));

                const ImVec2 iconMin(
                    itemPos.x - iconRadius,
                    itemPos.y - iconRadius
                );

                const ImVec2 iconMax(
                    itemPos.x + iconRadius,
                    itemPos.y + iconRadius
                );

                draw->AddImage(
                    reinterpret_cast<ImTextureID>(icon),
                    iconMin,
                    iconMax,
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f),
                    FadeColor(
                        MakeGameplayIconColor(
                            g_bottomItems[realItemIndex],
                            static_cast<int>(255 * alphaFactor), 1.0f, true,
                            RadialSide::Bottom
                        ),
                        alpha * gameplayItemOpacity
                    )
                );
            }

            // ============================================================
            // QUANTIDADE DO ITEM
            // ============================================================

            const RadialItem& item =
                g_bottomItems[realItemIndex];

            const auto itemInfo =
                ItemInfo::Get(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID
                );

            if (g_radialMode != RadialMode::Gameplay || Config::g_showItemQuantity)
                DrawRadialItemQuantity(
                    draw,
                    itemPos,
                    center,
                    radius,
                    itemInfo.instanceQuantity,
                    hoverT,
                    alpha * alphaFactor * gameplayItemOpacity,
                    QuantityBadgeDirection::Down
                );

            DrawQuickDrawIndicator(
                draw,
                item,
                itemPos,
                center,
                radius,
                itemInfo.instanceQuantity,
                alpha * alphaFactor * gameplayItemOpacity,
                hoverT,
                false,
                QuantityBadgeDirection::Down);


        }
        bottomItemLayers.Merge(draw);
    }

     

    void UpdateSideRadialScroll()
    {
        // O SKSE input sink é a única fonte de notches. Ler MouseWheel aqui
        // também criava um segundo movimento direto em alguns setups, pulando
        // slots sem informar a animação. A fila do WheelSettings é processada
        // em ProcessSettingsEditor(), depois que a topologia estabiliza.
        if (g_showWindow &&
            (g_radialSide == RadialSide::Left ||
             g_radialSide == RadialSide::Right) &&
            g_sideItems.size() < 3)
        {
            g_sideScrollOffset = 0;
        }
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
        {
            g_settingsSideScrollQueue.clear();
            g_settingsSideScrollAnimationDirection = 0;
        }
    }

        
    static ImVec2 ApplyOverflowHoverRepulsion(
        RadialItemAnimation& anim,
        const ImVec2& position,
        bool isHoveredItem,
        float deltaTime)
    {
        deltaTime = std::clamp(
            deltaTime,
            0.0f,
            0.05f
        );

        constexpr float repulsionRadius = 85.0f;
        constexpr float repulsionForce = 20.0f;
        constexpr float animationSpeed = 14.0f;

        ImVec2 targetOffset(0.0f, 0.0f);

        // ========================================================
        // REPULSÃO
        // ========================================================

        const bool inventoryDragRepulsion = IsInventoryDragVisualActive();
        if ((inventoryDragRepulsion || g_hoveredOverflowIndex >= 0) &&
            (!isHoveredItem || inventoryDragRepulsion))
        {
            const ImVec2 repulsionOrigin = inventoryDragRepulsion
                ? g_inventoryDraggedPosition
                : g_hoveredOverflowPosition;
            const float dx =
                position.x - repulsionOrigin.x;

            const float dy =
                position.y - repulsionOrigin.y;

            const float distanceSq =
                dx * dx + dy * dy;

            const float radiusSq =
                repulsionRadius * repulsionRadius;

            if (distanceSq > 0.001f &&
                distanceSq < radiusSq)
            {
                const float distance =
                    std::sqrt(distanceSq);

                // 1 próximo do item, 0 fora da área.
                float influence =
                    1.0f - distance / repulsionRadius;

                // Suaviza a distribuição da força.
                influence =
                    influence * influence *
                    (3.0f - 2.0f * influence);

                const float force =
                    repulsionForce *
                    influence *
                    g_overflowRepulsionStrength;

                targetOffset.x =
                    (dx / distance) * force;

                targetOffset.y =
                    (dy / distance) * force;
            }
        }

        // ========================================================
        // ANIMAÇÃO SUAVE
        // ========================================================

        const float factor =
            1.0f - std::exp(
                -animationSpeed * deltaTime
            );

        anim.overflowRepulsion.x +=
            (targetOffset.x -
                anim.overflowRepulsion.x) * factor;

        anim.overflowRepulsion.y +=
            (targetOffset.y -
                anim.overflowRepulsion.y) * factor;

        // ========================================================
        // POSIÇÃO VISUAL
        // ========================================================

        return ImVec2(
            position.x + anim.overflowRepulsion.x,
            position.y + anim.overflowRepulsion.y
        );
    }

    void DrawRadialMenu(
        const ImVec2& center,
        bool leftSide,
        float alpha)
    {
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImGuiIO& io = ImGui::GetIO();
        const float deltaTime = io.DeltaTime > 0.0f ? io.DeltaTime : (1.0f / 60.0f);

        const int totalItems = static_cast<int>(g_sideItems.size());

        // No gameplay, radial vazio não existe visualmente. No Inventory e
        // no Settings, o círculo permanece como área visual/de drop.
        if (totalItems <= 0 &&
            g_radialMode != RadialMode::Inventory &&
            !SettingsMenu::WheelSettingsMenu::IsOpen())
            return;

        const int maxVisible = GetSideVisibleLimit();

        int visibleCount = std::min(totalItems, maxVisible);

        // No WheelSettings a instância arrastada é removida da lista assim
        // que o drag começa. No Legacy, se ela veio da camada principal, a
        // compactação imediata promovia o primeiro excedente para o último
        // slot principal até o drop. Mantemos a fronteira visual anterior
        // durante o drag: os itens restantes se acomodam, mas nenhum
        // excedente muda de camada só porque há uma peça no cursor.
        const bool preserveLegacyOverflowBoundary =
            SettingsMenu::WheelSettingsMenu::IsOpen() &&
            g_settingsDrag.active &&
            !Config::g_customRadial &&
            (g_settingsDrag.sourceSide == RadialSide::Left ||
                g_settingsDrag.sourceSide == RadialSide::Right);
        const int preDragTotal = totalItems + 1;
        const int preDragVisibleCount = std::min(preDragTotal, maxVisible);
        const int sourceVisualOrdinal = preDragTotal > 0
            ? WrapSideIndex(
                g_settingsDrag.sourceIndex -
                    g_settingsDrag.sideScrollOffsetBeforeRemoval,
                preDragTotal)
            : -1;
        if (preserveLegacyOverflowBoundary &&
            preDragTotal > maxVisible)
        {
            if (g_settingsDrag.sourceIndex >= 0 &&
                sourceVisualOrdinal >= 0 &&
                sourceVisualOrdinal < preDragVisibleCount &&
                visibleCount > 0)
            {
                --visibleCount;
            }
        }

        //const int maxOffset = std::max(0, totalItems - visibleCount);
        //g_sideScrollOffset = std::clamp(g_sideScrollOffset, 0, maxOffset);

        

        const float radius = GetSideRadialRadius();
        const float itemRadius = 34.0f *
            (Config::g_generalItemSize * 0.01f) * (Config::g_slotSize * 0.01f);

        // ============================================================
        // CÍRCULO EXTERNO
        // ============================================================

        const auto radialShape = static_cast<RadialShape::Style>(std::clamp(
            Config::g_radialShape, 0, RadialShape::Count() - 1));
        const auto overflowMechanism = OverflowMechanism::Style::ConcentricRings;
        const bool customTrack = Config::g_customRadial && Track::HasValidSavedLayout();
        // Sem excedentes o circuito externo não participa da rotação. O
        // trilho custom continua desenhado/configurado, mas os itens fecham
        // sua volta exclusivamente no radial principal.
        const bool customCircuitMovement = customTrack && totalItems > visibleCount;
        std::vector<Track::CircuitSlot> customMainSlots;
        std::vector<Track::CircuitSlot> customOverflowSlots;
        if (customTrack)
        {
            const auto slots = Track::CircuitSlots(center, radius, leftSide,
                visibleCount, std::max(0, totalItems - visibleCount), radialShape);
            for (const auto& slot : slots)
                (slot.main ? customMainSlots : customOverflowSlots).push_back(slot);
        }
        if (customTrack)
        {
            const auto drawCustomRing = [&](float ringRadius, float baseOpacity,
                float thickness) {
                constexpr int segments = 192;
                const float rotation = Track::RadialRotation();
                const float opacity = Track::RadialLineOpacity() * 0.01f;
                for (int segment = 0; segment < segments; ++segment)
                {
                    const float a0 = 2.0f * PI * static_cast<float>(segment) / segments;
                    const float a1 = 2.0f * PI * static_cast<float>(segment + 1) / segments;
                    // Quando não há overflow, as aberturas editadas para os
                    // terminais são fechadas visualmente junto com o circuito.
                    const float visibility = customCircuitMovement
                        ? std::min(Track::RadialLineVisibility(a0),
                            Track::RadialLineVisibility(a1))
                        : 1.0f;
                    const int lineAlpha = static_cast<int>(
                        baseOpacity * opacity * visibility);
                    if (lineAlpha <= 0) continue;
                    const float directionAngle0 = leftSide
                        ? a0 + rotation : PI - (a0 + rotation);
                    const float directionAngle1 = leftSide
                        ? a1 + rotation : PI - (a1 + rotation);
                    const ImVec2 p0 = RadialShape::PositionAtAngles(
                        radialShape, center, ringRadius, a0, directionAngle0);
                    const ImVec2 p1 = RadialShape::PositionAtAngles(
                        radialShape, center, ringRadius, a1, directionAngle1);
                    draw->AddLine(p0, p1,
                        FadeColor(IM_COL32(255, 255, 255, lineAlpha), alpha), thickness);
                }
            };
            drawCustomRing(radius + 12.0f, 45.0f, 2.0f);
            drawCustomRing(radius, 110.0f, 1.5f);
            for (const auto& guide : Track::GuidePaths(center, radius, leftSide))
            {
                for (std::size_t i = 1; i < guide.size(); ++i)
                {
                    const ImVec2 middle((guide[i - 1].x + guide[i].x) * 0.5f,
                        (guide[i - 1].y + guide[i].y) * 0.5f);
                    const float visibility = Track::OverflowLineVisibility(
                        middle, center, radius);
                    const int lineAlpha = static_cast<int>(255.0f *
                        Track::LineOpacity() * 0.01f * visibility);
                    if (lineAlpha <= 0) continue;
                    draw->AddLine(guide[i - 1], guide[i],
                        FadeColor(IM_COL32(255, 255, 255, lineAlpha), alpha), 1.5f);
                }
            }
        }
        else
        {
            const auto outerOutline = RadialShape::Outline(
                radialShape, center, radius + 12.0f, leftSide);
            const auto innerOutline = RadialShape::Outline(
                radialShape, center, radius, leftSide);
            draw->AddPolyline(outerOutline.data(), static_cast<int>(outerOutline.size()),
                FadeColor(IM_COL32(255, 255, 255, 45), alpha), ImDrawFlags_Closed, 2.0f);
            draw->AddPolyline(innerOutline.data(), static_cast<int>(innerOutline.size()),
                FadeColor(IM_COL32(255, 255, 255, 110), alpha), ImDrawFlags_Closed, 1.5f);
            for (const auto& guide : OverflowMechanism::GuidePaths(
                overflowMechanism, center, radius, leftSide))
            {
                draw->AddPolyline(guide.data(), static_cast<int>(guide.size()),
                    FadeColor(IM_COL32(255, 255, 255, 55), alpha), ImDrawFlags_Closed, 1.5f);
            }
        }
        
        // No inventário, mesmo vazio, o radial continua existindo
        // como área de drop.
        // No gameplay, vazio não deve mostrar nada.
        if (totalItems <= 0)
            return;
        

                
        // ============================================================
        // IDENTIFICA O ITEM EXCEDENTE HOVERED antes dos itens principais
        // ============================================================

        g_hoveredOverflowIndex = -1;

        const bool settingsOpen =
            SettingsMenu::WheelSettingsMenu::IsOpen();

        const ImVec2 hoverMouse =
            settingsOpen
                ? g_settingsMousePos
                : IsInventoryDragVisualActive()
                    ? g_inventoryDraggedPosition
                    : GetRadialMousePosition();

        float closestDistanceSq = 18.0f * 18.0f;

        if (totalItems > maxVisible)
        {
            const int overflowCount =
                totalItems - visibleCount;

            for (int i = 0; i < overflowCount; ++i)
            {
                const int actualIndex =
                    WrapSideIndex(
                        g_sideScrollOffset +
                        (customTrack && i < static_cast<int>(customOverflowSlots.size())
                            ? customOverflowSlots[static_cast<std::size_t>(i)].ordinal
                            : visibleCount + i),
                        totalItems
                    );

                const RadialItem& item =
                    g_sideItems[actualIndex];

                if (!item.form)
                    continue;

                RadialItemAnimation& anim =
                    GetOrCreateAnim(
                        item.form,
                        item.uniqueID,
                        item.hasUniqueID
                    );

                // Utiliza a posição animada do frame anterior.
                // Não atualiza a animação duas vezes no mesmo frame.

                ImVec2 position =
                    anim.posInitialized
                        ? anim.currentPos
                        : customTrack && i < static_cast<int>(customOverflowSlots.size())
                            ? customOverflowSlots[static_cast<std::size_t>(i)].position
                            : GetSideOverflowPosition(
                                center, leftSide, i, overflowCount);

                position.x += anim.overflowRepulsion.x;
                position.y += anim.overflowRepulsion.y;

                const float dx =
                    hoverMouse.x - position.x;

                const float dy =
                    hoverMouse.y - position.y;

                const float distanceSq =
                    dx * dx + dy * dy;

                if (distanceSq < closestDistanceSq)
                {
                    closestDistanceSq = distanceSq;

                    g_hoveredOverflowIndex =
                        actualIndex;

                    g_hoveredOverflowPosition =
                        position;
                }
            }
        }

        // ============================================================
        // ANIMAÇÃO GLOBAL DA FORÇA DE REPULSÃO
        // ============================================================

        const float targetStrength =
            (IsInventoryDragVisualActive() || g_hoveredOverflowIndex >= 0)
                ? 1.0f
                : 0.0f;

        const float repulsionFactor =
            1.0f - std::exp(
                -12.0f *
                std::clamp(deltaTime, 0.0f, 0.05f)
            );

        g_overflowRepulsionStrength +=
            (targetStrength -
                g_overflowRepulsionStrength) *
            repulsionFactor;



        // ============================================================
        // ITENS PRINCIPAIS
        // ============================================================

        ImVec2 mouse = settingsOpen
            ? g_settingsMousePos
            : GetRadialMousePosition();

        const int selectedVisibleIndex =
            GetSideRadialItem(mouse, center, leftSide, visibleCount, MENU_INNER_RADIUS);

        // Keep the hovered gameplay item in a dedicated final layer.  This
        // preserves the compact radial geometry while guaranteeing that the
        // selected circle, icon, border and quantity badge remain readable
        // when neighboring slots overlap it.
        ImDrawListSplitter gameplayItemLayers;
        gameplayItemLayers.Split(draw, 2);

        for (int i = 0; i < visibleCount; ++i)
        {
            const int actualIndex =
                WrapSideIndex(
                    g_sideScrollOffset +
                    (customTrack && i < static_cast<int>(customMainSlots.size())
                        ? customMainSlots[static_cast<std::size_t>(i)].ordinal : i),
                    totalItems
                );

            RadialItem& item = g_sideItems[actualIndex];

            // ========================================================
            // POSIÇÃO DINÂMICA
            // ========================================================

            const ImVec2 rawPos = customTrack && i < static_cast<int>(customMainSlots.size())
                ? customMainSlots[static_cast<std::size_t>(i)].position
                : RadialShape::MainPosition(
                    radialShape, center, radius, i, visibleCount, leftSide);

            const bool hovered =
                !SettingsMenu::WheelSettingsMenu::IsOpen() &&
                g_radialMode == RadialMode::Gameplay &&
                selectedVisibleIndex == i;
            gameplayItemLayers.SetCurrentChannel(draw, hovered ? 1 : 0);

            // ========================================================
            // ITEM SELECIONADO é enviado para o descriptor após 
            // extrairmos ele pelo hovered
            // ========================================================

            if (g_radialMode == RadialMode::Gameplay &&
                hovered)
            {
                SetGameplayDescriptionItem(item);
            }
            
            // ========================================================
            // TRANSIÇÃO SUAVE DE HOVER E DE POSIÇÃO
            // ========================================================
            
            RadialItemAnimation& anim = GetOrCreateAnim(
                item.form,
                item.uniqueID,
                item.hasUniqueID
            );

            anim.sidePolarInitialized = false;

            const bool wasSideWrapping = anim.sideWrapActive;

            ImVec2 itemPos;
            const bool preserveMainVisualDuringSettling =
                settingsOpen && anim.settingsDropSettling;

            if (g_radialMode == RadialMode::Inventory)
            {
                itemPos = customCircuitMovement
                    ? GetInventoryCircuitPosition(
                        anim, rawPos, center, radius, leftSide,
                        g_sideScrollDirection, deltaTime,
                        static_cast<RadialAnimation::Style>(std::clamp(
                            Config::g_radialAnimation, 0, RadialAnimation::Count() - 1)),
                        radialShape,
                        i < static_cast<int>(customMainSlots.size())
                            ? customMainSlots[static_cast<std::size_t>(i)].circuitT
                            : -1.0f)
                    : GetInventorySideSettlingPosition(
                        anim, rawPos, deltaTime, leftSide);
            }
            else if (settingsOpen || g_radialMode == RadialMode::Gameplay)
            {
                if (customCircuitMovement)
                {
                    const bool resizingCustomRadial = settingsOpen &&
                        g_activeLayoutSlider == LayoutSlider::RadialStretch;
                    if (resizingCustomRadial)
                    {
                        // Alterar o raio muda a geometria, não a ordem do
                        // circuito. Descarta a distância do raio anterior e
                        // acompanha o novo slot sem disparar uma volta.
                        anim.customTrackAnimation = {};
                        anim.settingsDropSettling = true;
                    }
                    itemPos = resizingCustomRadial
                        ? GetSettingsCircuitSettlingPosition(anim, rawPos, deltaTime)
                        : TrackMovement::UpdateCircuit(
                            anim.customTrackAnimation,
                            anim.posInitialized ? anim.currentPos : rawPos,
                            rawPos, center, radius, leftSide,
                            settingsOpen && anim.settingsDropSettling
                                ? 0 : g_sideScrollDirection,
                            deltaTime,
                            static_cast<RadialAnimation::Style>(std::clamp(
                                Config::g_radialAnimation, 0, RadialAnimation::Count() - 1)),
                            radialShape,
                            i < static_cast<int>(customMainSlots.size())
                                ? customMainSlots[static_cast<std::size_t>(i)].circuitT
                                : -1.0f);
                    if (settingsOpen && !resizingCustomRadial)
                    {
                        itemPos = GetSettingsCustomDropEntryPosition(
                            anim, itemPos, deltaTime);
                    }
                    if (settingsOpen && anim.settingsDropSettling &&
                        !anim.settingsCustomDropEntering &&
                        anim.customTrackAnimation.progress >= 1.0f)
                    {
                        anim.settingsDropSettling = false;
                    }
                }
                else
                {
                    const ImVec2 routedPos = OverflowMechanism::RouteTransition(
                        anim.overflowMechanismAnimation,
                        anim.posInitialized ? anim.currentPos : rawPos, rawPos,
                        center, radius, leftSide, OverflowMechanism::Layer::Main,
                        i, totalItems, deltaTime, overflowMechanism);
                    itemPos = RadialAnimation::Update(
                        anim.gameplayRadialAnimation, routedPos, center, leftSide,
                        RadialAnimation::Layer::Main, deltaTime,
                        static_cast<RadialAnimation::Style>(std::clamp(
                            Config::g_radialAnimation, 0, RadialAnimation::Count() - 1)),
                        // A fila do WheelSettings já aplica cada notch ao
                        // offset. Forçar também um arco dirigido no Legacy
                        // fazia um único notch parecer vários passos.
                        0);
                }
                anim.previousPos = anim.currentPos;
                anim.currentPos = itemPos;
                anim.posInitialized = true;
            }
            else if (anim.sideWrapActive)
            {
                itemPos =
                    GetSideWrapAnimatedPosition(
                        anim,
                        rawPos,
                        center,
                        deltaTime
                    );
            }
            else
            {
                itemPos =
                    GetRadialItemAnimatedPosition(
                        anim,
                        rawPos,
                        deltaTime
                    );
            }

            // ============================================================
            // REPULSÃO DOS ITENS PRINCIPAIS
            // ============================================================

            if (!(g_radialMode == RadialMode::Gameplay && customTrack))
                itemPos = ApplyOverflowHoverRepulsion(
                    anim, itemPos, false, deltaTime);

            EmitItemStardust(anim, itemPos, deltaTime,
                leftSide ? RadialSide::Left : RadialSide::Right);

            // ============================================================
            // SIDE WRAP - BOLINHA -> ITEM
            // ============================================================

            float wrapAlpha = 0.9f;

            float wrapDotAlpha = 0.0f;
            float wrapItemAlpha = 0.8f;

            if (wasSideWrapping && anim.sideWrapActive)
            {
                const float t =
                    std::clamp(anim.sideWrapT, 0.0f, 1.0f);

                // Até 35% do caminho: só bolinha branca.
                // De 35% até 65%: bolinha vira o item.
                constexpr float morphStart = 0.65f;
                constexpr float morphEnd   = 0.80f;

                if (t <= morphStart)
                {
                    wrapDotAlpha  = 1.0f;
                    wrapItemAlpha = 0.0f;
                }
                else if (t < morphEnd)
                {
                    float p =
                        (t - morphStart) /
                        (morphEnd - morphStart);

                    p = std::clamp(p, 0.0f, 1.0f);

                    // Smoothstep
                    p = p * p * (3.0f - 2.0f * p);

                    wrapDotAlpha  = 1.0f - p;
                    wrapItemAlpha = p;
                }
                else
                {
                    wrapDotAlpha  = 0.0f;
                    wrapItemAlpha = 1.0f;
                }
            }

            const float itemDrawAlpha =
                alpha * wrapItemAlpha *
                (g_radialMode == RadialMode::Gameplay
                    ? Config::g_itemOpacity * 0.01f : 0.8f);

            
            //ANIMAÇAO DE RADIAL POLAR
            //ImVec2 itemPos =
            //    GetSideRadialAnimatedPosition(
            //        anim,
            //        rawPos,
            //        center,
            //        deltaTime
            //    );

            //float hoverT = StepHoverAnimation(anim, hovered, deltaTime);
            //ITEM HOVERED COM MODO INVENTORY BLOQUEADO
            const bool allowHover =
                g_radialMode != RadialMode::Inventory ||
                SettingsMenu::WheelSettingsMenu::IsOpen();

            float hoverT =
                StepHoverAnimation(
                    anim,
                    allowHover && hovered,
                    deltaTime
                );

            //const float currentRadius = itemRadius + hoverT * 8.0f;
                const bool customGameplayVisual = customTrack &&
                    g_radialMode == RadialMode::Gameplay;
                // No circuito custom, o tamanho acompanha a parte física do
                // trilho onde o item realmente está, não apenas o slot que
                // acabou de ser atribuído. Isso impede que scroll alternado
                // acumule itens de overflow com aparência de item principal.
                const float radialSizeTarget = customCircuitMovement &&
                    !preserveMainVisualDuringSettling &&
                    g_radialMode != RadialMode::Inventory
                    ? anim.customTrackAnimation.mainBlend
                    : 1.0f;
            anim.radialSizeT +=
                (radialSizeTarget - anim.radialSizeT) *
                std::min(1.0f, deltaTime * 10.0f);

            const float baseAnimatedRadius =
                12.0f +
                (itemRadius - 12.0f) * anim.radialSizeT;

            const float currentRadius =
                baseAnimatedRadius +
                hoverT * 8.0f;

            // ============================================================
            // SETTINGS - REGISTRA ITEM PRINCIPAL DO RADIAL LATERAL
            // ============================================================

            if (SettingsMenu::WheelSettingsMenu::IsOpen())
            {
                // 1. Flutuação
                itemPos = GetSettingsFloatingPosition(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID,
                    itemPos,
                    static_cast<float>(ImGui::GetTime())
                );

                // 2. Repulsão do item arrastado
                itemPos = GetSettingsRepulsionPosition(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID,
                    itemPos,
                    ImGui::GetIO().DeltaTime
                );

                // 3. Registra a posição final para hover e drag
                RegisterSettingsItem(
                    item.form,
                    leftSide
                        ? RadialSide::Left
                        : RadialSide::Right,
                    actualIndex,
                    itemPos,
                    currentRadius
                );

                // 4. Hover
                hoverT = UpdateSettingsItemHover(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID,
                    deltaTime
                );

                const bool settingsHoveredLayer = SameSettingsItem(
                    item.form, item.uniqueID, item.hasUniqueID,
                    g_settingsHoveredItem, g_settingsHoveredUniqueID,
                    g_settingsHoveredHasUniqueID) && !g_settingsDrag.active;
                gameplayItemLayers.SetCurrentChannel(
                    draw, settingsHoveredLayer ? 1 : 0);
            }

            if (wrapDotAlpha > 0.001f)
            {
                draw->AddCircleFilled(
                    itemPos,
                    12.0f,
                    FadeColor(
                        IM_COL32(
                            255,
                            255,
                            255,
                            static_cast<int>(35.0f * wrapDotAlpha)
                        ),
                        alpha
                    )
                );
            }

            // ========================================================
            // GLOW (fade contínuo)
            // ========================================================

            if (hoverT > 0.001f)
            {
                draw->AddCircleFilled(
                    itemPos,
                    currentRadius + 14.0f * hoverT,
                    FadeColor(IM_COL32(255, 255, 255, static_cast<int>(25 * hoverT)), itemDrawAlpha));

                draw->AddCircle(
                    itemPos,
                    currentRadius + 8.0f * hoverT,
                    FadeColor(IM_COL32(255, 255, 255, static_cast<int>(150 * hoverT)), itemDrawAlpha),
                    48,
                    2.0f);
            }

            // ========================================================
            // ITEM - FUNDO
            // ========================================================

            if (customGameplayVisual)
            {
                draw->AddCircleFilled(itemPos, currentRadius,
                    FadeColor(IM_COL32(255, 255, 255, 35),
                        itemDrawAlpha * (1.0f - anim.radialSizeT)), 48);
                if (anim.radialSizeT > 0.001f)
                    DrawEquippedItemBackground(draw, item.form, item.uniqueID,
                        item.hasUniqueID, itemPos, currentRadius,
                        itemDrawAlpha * anim.radialSizeT);
            }
            else
                DrawEquippedItemBackground(
                    draw, item.form, item.uniqueID, item.hasUniqueID,
                    itemPos, currentRadius, itemDrawAlpha);

            auto* icon =
                ItemIcon::Get(item.form);

            if (icon)
            {
                const float iconRadius = customGameplayVisual
                    ? 5.5f + (currentRadius * 0.53f * (Config::g_iconSize /
                        std::max(Config::g_slotSize, 1.0f)) - 5.5f) * anim.radialSizeT
                    : currentRadius * 0.53f * (Config::g_iconSize /
                        std::max(Config::g_slotSize, 1.0f));

                const ImVec2 iconMin(
                    itemPos.x - iconRadius,
                    itemPos.y - iconRadius
                );

                const ImVec2 iconMax(
                    itemPos.x + iconRadius,
                    itemPos.y + iconRadius
                );

                draw->AddImage(
                    reinterpret_cast<ImTextureID>(icon),
                    iconMin,
                    iconMax,
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f),
                    FadeColor(
                        MakeGameplayIconColor(item, static_cast<int>(
                            255.0f * (customGameplayVisual
                                ? 0.25f + 0.75f * anim.radialSizeT : 1.0f))),
                        itemDrawAlpha
                    )
                );
            }

            // ========================================================
            // BORDA - DESENHA POR ÚLTIMO
            // ========================================================

            const float configuredBorderAlpha = 255.0f *
                std::clamp(Config::g_itemBorderOpacity * 0.01f, 0.0f, 1.0f);
            const int borderAlpha = static_cast<int>(configuredBorderAlpha +
                (255.0f - configuredBorderAlpha) * hoverT);

            draw->AddCircle(
                itemPos,
                currentRadius,
                FadeColor(
                    IM_COL32(
                        (Config::g_itemBorderColor >> 16) & 0xFF,
                        (Config::g_itemBorderColor >> 8) & 0xFF,
                        Config::g_itemBorderColor & 0xFF,
                        borderAlpha
                    ),
                    itemDrawAlpha
                ),
                48,
                1.5f + hoverT * 1.5f
            );

            // ============================================================
            // QUANTIDADE DO ITEM
            // ============================================================

            const ItemInfo::Data itemInfo =
                ItemInfo::Get(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID
                );

            if (g_radialMode != RadialMode::Gameplay || Config::g_showItemQuantity)
                DrawRadialItemQuantity(
                    draw,
                    itemPos,
                    center,
                    currentRadius,
                    itemInfo.instanceQuantity,
                    hoverT,
                    itemDrawAlpha * (customGameplayVisual ? anim.radialSizeT : 1.0f),
                    QuantityBadgeDirection::AutoToCenter
                );

            DrawQuickDrawIndicator(
                draw,
                item,
                itemPos,
                center,
                currentRadius,
                itemInfo.instanceQuantity,
                itemDrawAlpha * (customGameplayVisual ? anim.radialSizeT : 1.0f),
                hoverT,
                false,
                QuantityBadgeDirection::AutoToCenter);

        }

        // ============================================================
        // ITENS EXCEDENTES
        // ============================================================

        gameplayItemLayers.SetCurrentChannel(draw, 0);

        if (totalItems > maxVisible)
        {
            const float overflowItemRadius = 12.0f *
                std::clamp(Config::g_overflowSize * 0.01f, 0.25f, 2.0f);

            const int overflowCount =
                totalItems - visibleCount;

            // SEM LIMITE.
            // Todos os itens excedentes são desenhados.
            for (int i = 0; i < overflowCount; ++i)
            {
                const int actualIndex =
                    WrapSideIndex(
                        g_sideScrollOffset +
                        (customTrack && i < static_cast<int>(customOverflowSlots.size())
                            ? customOverflowSlots[static_cast<std::size_t>(i)].ordinal
                            : visibleCount + i),
                        totalItems
                    );

                RadialItem& item =
                    g_sideItems[actualIndex];

                // ========================================================
                // NOVA POSIÇÃO DO OVERFLOW
                //
                // Não depende mais da quantidade de excedentes.
                // Cada item ocupa um passo fixo ao redor do radial.
                //
                // Quando completa uma volta, GetSideOverflowPosition()
                // automaticamente começa uma camada externa.
                // ========================================================

                const ImVec2 rawOverflowPos = customTrack &&
                    i < static_cast<int>(customOverflowSlots.size())
                    ? customOverflowSlots[static_cast<std::size_t>(i)].position
                    : GetSideOverflowPosition(center, leftSide, i, overflowCount);

                // ========================================================
                // ANIMAÇÃO
                // ========================================================

                RadialItemAnimation& anim = GetOrCreateAnim(
                    item.form,
                    item.uniqueID,
                    item.hasUniqueID
                );

                //ImVec2 overflowPos =
                //    GetRadialItemAnimatedPosition(
                //        anim,
                //        rawOverflowPos,
                //        deltaTime
                //    );
                
                //ImVec2 overflowPos =
                //    GetSideRadialAnimatedPosition(
                //        anim,
                //        rawOverflowPos,
                //        center,
                //        leftSide,
                //        deltaTime
                //    );
                const bool wasSideWrapping = anim.sideWrapActive;

                ImVec2 overflowPos;
                const bool preserveOverflowVisualDuringSettling =
                    settingsOpen && anim.settingsDropSettling;

                if (g_radialMode == RadialMode::Inventory)
                {
                    overflowPos = customCircuitMovement
                        ? GetInventoryCircuitPosition(
                            anim, rawOverflowPos, center, radius, leftSide,
                            g_sideScrollDirection, deltaTime,
                            static_cast<RadialAnimation::Style>(std::clamp(
                                Config::g_radialAnimation, 0, RadialAnimation::Count() - 1)),
                            radialShape,
                            i < static_cast<int>(customOverflowSlots.size())
                                ? customOverflowSlots[static_cast<std::size_t>(i)].circuitT
                                : -1.0f)
                        : GetInventorySideSettlingPosition(
                            anim, rawOverflowPos, deltaTime, leftSide);
                }
                else if (settingsOpen || g_radialMode == RadialMode::Gameplay)
                {
                    if (customCircuitMovement)
                    {
                        const bool resizingCustomRadial = settingsOpen &&
                            g_activeLayoutSlider == LayoutSlider::RadialStretch;
                        if (resizingCustomRadial)
                        {
                            anim.customTrackAnimation = {};
                            anim.settingsDropSettling = true;
                        }
                        overflowPos = resizingCustomRadial
                            ? GetSettingsCircuitSettlingPosition(anim, rawOverflowPos, deltaTime)
                            : TrackMovement::UpdateCircuit(
                                anim.customTrackAnimation,
                                anim.posInitialized ? anim.currentPos : rawOverflowPos,
                                rawOverflowPos, center, radius, leftSide,
                                settingsOpen && anim.settingsDropSettling
                                    ? 0 : g_sideScrollDirection,
                                deltaTime,
                                static_cast<RadialAnimation::Style>(std::clamp(
                                    Config::g_radialAnimation, 0, RadialAnimation::Count() - 1)),
                                radialShape,
                                i < static_cast<int>(customOverflowSlots.size())
                                    ? customOverflowSlots[static_cast<std::size_t>(i)].circuitT
                                    : -1.0f);
                        if (settingsOpen && !resizingCustomRadial)
                        {
                            overflowPos = GetSettingsCustomDropEntryPosition(
                                anim, overflowPos, deltaTime);
                        }
                        if (settingsOpen && anim.settingsDropSettling &&
                            !anim.settingsCustomDropEntering &&
                            anim.customTrackAnimation.progress >= 1.0f)
                        {
                            anim.settingsDropSettling = false;
                        }
                    }
                    else
                    {
                        const ImVec2 routedOverflowPos = OverflowMechanism::RouteTransition(
                            anim.overflowMechanismAnimation,
                            anim.posInitialized ? anim.currentPos : rawOverflowPos,
                            rawOverflowPos, center, radius, leftSide,
                            OverflowMechanism::Layer::Overflow,
                            visibleCount + i, totalItems,
                            deltaTime, overflowMechanism);
                        overflowPos = RadialAnimation::Update(
                            anim.gameplayRadialAnimation, routedOverflowPos, center, leftSide,
                            RadialAnimation::Layer::Overflow, deltaTime,
                            static_cast<RadialAnimation::Style>(std::clamp(
                                Config::g_radialAnimation, 0, RadialAnimation::Count() - 1)),
                            // Idem para a camada excedente do Legacy: o
                            // target normal resolve um passo; não o
                            // reinterpretamos como uma rotação completa.
                            0);
                    }
                    anim.previousPos = anim.currentPos;
                    anim.currentPos = overflowPos;
                    anim.posInitialized = true;
                }
                else if (anim.sideWrapActive)
                {
                    overflowPos =
                        GetSideWrapAnimatedPosition(
                            anim,
                            rawOverflowPos,
                            center,
                            deltaTime
                        );
                    
                }
                else
                {
                    overflowPos =
                        GetSideRadialAnimatedPosition(
                            anim,
                            rawOverflowPos,
                            center,
                            leftSide,
                            deltaTime
                        );
                }

                                
                // ============================================================
                // HOVER INDIVIDUAL DO EXCEDENTE
                // ============================================================

                const bool overflowHovered =
                    actualIndex == g_hoveredOverflowIndex;

                const float hoverTarget =
                    settingsOpen && overflowHovered ? 1.0f : 0.0f;

                const float hoverFactor =
                    1.0f - std::exp(
                        -12.0f *
                        std::clamp(deltaTime, 0.0f, 0.05f)
                    );

                anim.overflowHoverT +=
                    (hoverTarget - anim.overflowHoverT) *
                    hoverFactor;

                // ============================================================
                // REPULSÃO DOS EXCEDENTES VIZINHOS
                // ============================================================

                if (!(g_radialMode == RadialMode::Gameplay && customTrack))
                    overflowPos = ApplyOverflowHoverRepulsion(
                        anim, overflowPos, overflowHovered, deltaTime);

                // Um item pode cruzar de principal para excedente durante o
                // giro. Mantém o rastro contínuo nessa fronteira também.
                EmitItemStardust(anim, overflowPos, deltaTime,
                    leftSide ? RadialSide::Left : RadialSide::Right);

                // ============================================================
                // TRANSIÇAO ALPHA DOS ITENS DURANTE TROCA DE PRINCIPAL><EXCEDENTE
                // ============================================================

                float wrapAlpha = 1.0f;

                if (wasSideWrapping)
                {
                    const float t =
                        std::clamp(anim.sideWrapT, 0.0f, 1.0f);

                    constexpr float fadeOutEnd = 0.10f;
                    constexpr float minAlpha = 0.05f;

                    if (t < fadeOutEnd)
                    {
                        const float p = t / fadeOutEnd;

                        wrapAlpha =
                            1.0f +
                            (minAlpha - 1.0f) * p;
                    }
                    else
                    {
                        float p =
                            (t - fadeOutEnd) /
                            (1.0f - fadeOutEnd);

                        p = std::clamp(p, 0.0f, 1.0f);
                        p = p * p * (3.0f - 2.0f * p);

                        wrapAlpha =
                            minAlpha +
                            (1.0f - minAlpha) * p;
                    }
                }

                float itemDrawAlpha =
                    alpha * wrapAlpha *
                    (g_radialMode == RadialMode::Gameplay
                        ? Config::g_overflowOpacity * 0.01f : 1.0f);
                    
                // Remove suavemente qualquer hover que o item tinha
                // quando estava no radial principal.
                StepHoverAnimation(
                    anim,
                    false,
                    deltaTime
                );

                // ========================================================
                // TAMANHO (SEM HOVER)
                // Principal -> overflow: 34 -> 12
                // ========================================================

                // A mesma origem física usada no arco principal: durante a
                // transição o tamanho acompanha o percurso, sem sobrar grande
                // no overflow quando a direção do scroll muda rapidamente.
                const float radialSizeTarget = customCircuitMovement &&
                    !preserveOverflowVisualDuringSettling &&
                    g_radialMode != RadialMode::Inventory
                    ? anim.customTrackAnimation.mainBlend
                    : 0.0f;
                anim.radialSizeT +=
                    (radialSizeTarget - anim.radialSizeT) *
                    std::min(
                        1.0f,
                        deltaTime * 10.0f
                    );

                const bool settingsDragVisualActive =
                    settingsOpen && g_settingsDrag.active;
                const bool dragMorphContext =
                    g_radialMode == RadialMode::Inventory || settingsOpen;
                // O morph de drag serve apenas para apresentar todos os
                // excedentes como itens enquanto o usuário segura um item.
                // Ele não pode substituir a transição física normal do
                // circuito (principal <-> excedente) no WheelSettings.
                const bool activeDragMorph =
                    IsInventoryDragVisualActive() || settingsDragVisualActive;
                if (dragMorphContext)
                {
                    const float morphTarget = activeDragMorph ? 1.0f : 0.0f;
                    const float morphSpeed = morphTarget > 0.5f
                        ? 11.0f
                        : 2.8f + static_cast<float>(i % 8) * 0.55f;
                    const float morphFactor = 1.0f - std::exp(
                        -morphSpeed * std::clamp(deltaTime, 0.0f, 0.05f));
                    anim.inventoryOverflowMorph +=
                        (morphTarget - anim.inventoryOverflowMorph) * morphFactor;
                }
                else
                {
                    anim.inventoryOverflowMorph = 0.0f;
                }
                const float visualRadialSizeT =
                    activeDragMorph
                    ? std::clamp(anim.inventoryOverflowMorph, 0.0f, 1.0f)
                    : std::clamp(anim.radialSizeT, 0.0f, 1.0f);
                constexpr float dragOverflowMainScale = 0.80f;
                const float morphItemRadius = activeDragMorph
                    ? itemRadius * dragOverflowMainScale
                    : itemRadius;
                const float animatedRadius =
                    overflowItemRadius +
                    (morphItemRadius - overflowItemRadius) *
                    visualRadialSizeT;

                // ============================================================
                // TAMANHO COM HOVER
                // ============================================================

                // No WheelSettings, o hover transforma a bolinha excedente
                // até o mesmo tamanho visual de um item principal.
                const float finalOverflowRadius =
                    animatedRadius +
                    ((activeDragMorph ? morphItemRadius : itemRadius) - animatedRadius) *
                    anim.overflowHoverT;
                
                // ============================================================
                // SETTINGS - REGISTRA ITEM EXCEDENTE
                // ============================================================

                if (SettingsMenu::WheelSettingsMenu::IsOpen())
                {
                    overflowPos = GetSettingsFloatingPosition(
                        item.form,
                        item.uniqueID,
                        item.hasUniqueID,
                        overflowPos,
                        static_cast<float>(ImGui::GetTime())
                    );

                    RegisterSettingsItem(
                        item.form,
                        leftSide
                            ? RadialSide::Left
                            : RadialSide::Right,
                        actualIndex,
                        overflowPos,
                        animatedRadius
                    );
                }

                // ============================================================
                // HOVER INDIVIDUAL DA BOLINHA EXCEDENTE
                // ============================================================

                const bool settingsOpen =
                    SettingsMenu::WheelSettingsMenu::IsOpen();

                const float dx =
                    g_settingsMousePos.x - overflowPos.x;

                const float dy =
                    g_settingsMousePos.y - overflowPos.y;

                constexpr float hoverRadius = 12.0f;

                const bool hovered =
                    settingsOpen &&
                    dx * dx + dy * dy <=
                        hoverRadius * hoverRadius;

                // Cada item possui sua própria animação.
                float& hoverT =
                    g_settingsOverflowHoverCache[item.form];

                const float dt =
                    std::clamp(
                        ImGui::GetIO().DeltaTime,
                        0.0f,
                        0.05f
                    );

                const float factor =
                    1.0f - std::exp(-12.0f * dt);

                hoverT +=
                    ((hovered ? 1.0f : 0.0f) - hoverT)
                    * factor;

                // Opacidade original: 35.
                // Hover máximo: 255.
                const float configuredOverflowAlpha = 255.0f *
                    std::clamp(Config::g_overflowBackgroundOpacity * 0.01f, 0.0f, 1.0f);
                const int overflowAlpha = static_cast<int>(configuredOverflowAlpha +
                    (settingsOpen ? (255.0f - configuredOverflowAlpha) * 0.20f * hoverT : 0.0f));
                // ========================================================
                // FUNDO
                // ========================================================

                //draw->AddCircleFilled(
                //    overflowPos,
                //    animatedRadius,
                //    FadeColor(
                //        IM_COL32(255, 255, 255, overflowAlpha),
                //        itemDrawAlpha
                //    )
                //);
                // ============================================================
                // TRANSIÇÃO VISUAL: ITEM -> BOLINHA EXCEDENTE
                // ============================================================

                const float transitionT =
                    std::clamp(
                        visualRadialSizeT,
                        0.0f,
                        1.0f
                    );

                // 0 = bolinha branca
                // 1 = item completo

                const float transitionFactor =
                    transitionT * transitionT *
                    (3.0f - 2.0f * transitionT);

                // Enquanto o excedente assume o visual de item, sua opacidade
                // geral acompanha exatamente a do item principal. No Inventory
                // o item principal usa o alpha integral; no WheelSettings ele
                // usa a opacidade configurada do radial.
                if (transitionFactor > 0.001f)
                {
                    const float overflowOpacity = settingsOpen
                        ? Config::g_overflowOpacity * 0.01f : 1.0f;
                    // Usa exatamente a mesma opacidade externa aplicada no
                    // renderizador do item principal. Assim a passagem de
                    // um renderizador para o outro não cria um pico de alpha.
                    const float mainOpacity =
                        g_radialMode == RadialMode::Gameplay
                        ? Config::g_itemOpacity * 0.01f
                        : 0.8f;
                    itemDrawAlpha = alpha * wrapAlpha *
                        (overflowOpacity +
                            (mainOpacity - overflowOpacity) * transitionFactor);
                }

                // ============================================================
                // COR DO FUNDO
                // ============================================================

                // Cor original da bolinha excedente.
                const ImVec4 overflowColor(
                    static_cast<float>((Config::g_overflowBackgroundColor >> 16) & 0xFF) / 255.0f,
                    static_cast<float>((Config::g_overflowBackgroundColor >> 8) & 0xFF) / 255.0f,
                    static_cast<float>(Config::g_overflowBackgroundColor & 0xFF) / 255.0f,
                    static_cast<float>(overflowAlpha) / 255.0f
                );

                // Fundo original do item.
                const ImVec4 itemColor(
                    static_cast<float>((Config::g_itemBackgroundColor >> 16) & 0xFF) / 255.0f,
                    static_cast<float>((Config::g_itemBackgroundColor >> 8) & 0xFF) / 255.0f,
                    static_cast<float>(Config::g_itemBackgroundColor & 0xFF) / 255.0f,
                    std::clamp(Config::g_itemBackgroundOpacity * 0.01f, 0.0f, 1.0f)
                );

                // Transição suave entre os fundos.
                const float t = transitionFactor;

                const ImVec4 finalColor(
                    overflowColor.x + (itemColor.x - overflowColor.x) * t,
                    overflowColor.y + (itemColor.y - overflowColor.y) * t,
                    overflowColor.z + (itemColor.z - overflowColor.z) * t,
                    overflowColor.w + (itemColor.w - overflowColor.w) * t
                );

                draw->AddCircleFilled(
                    overflowPos,
                    finalOverflowRadius,
                    FadeColor(
                        ImGui::ColorConvertFloat4ToU32(finalColor),
                        itemDrawAlpha
                    ),
                    48
                );

                // A camada final é exatamente a mesma usada pelo item
                // principal. Ela é apenas visual e não toca posição, índice,
                // rota ou qualquer estado do circuito.
                if (transitionFactor > 0.001f)
                {
                    DrawEquippedItemBackground(
                        draw, item.form, item.uniqueID, item.hasUniqueID,
                        overflowPos, finalOverflowRadius,
                        itemDrawAlpha * transitionFactor);
                }

                                
                // ============================================================
                // ÍCONE DO ITEM - VISÍVEL TAMBÉM NO EXCEDENTE
                // ============================================================

                auto* icon = ItemIcon::Get(item.form);

                if (icon && (g_radialMode != RadialMode::Gameplay ||
                    Config::g_showOverflowIcon))
                {
                    // ========================================================
                    // TAMANHO com HOVER
                    //
                    // Excedente: ícone pequeno.
                    // Principal: ícone acompanha o tamanho original.
                    // ========================================================

                    const float overflowIconRadius = 5.5f *
                        std::clamp(Config::g_overflowSize * 0.01f, 0.25f, 2.0f);

                    const float mainIconRadius =
                        itemRadius * 0.53f * (Config::g_iconSize /
                            std::max(Config::g_slotSize, 1.0f));

                    const float baseIconRadius =
                        overflowIconRadius +
                        (mainIconRadius - overflowIconRadius) *
                        transitionFactor;

                    // No WheelSettings, o ícone acompanha exatamente a
                    // transformação da bolinha excedente até a escala visual
                    // usada pelo item principal.
                    const float fullItemIconRadius =
                        (activeDragMorph ? morphItemRadius : itemRadius) * 0.53f;
                    const float iconRadius = std::min(fullItemIconRadius,
                        baseIconRadius +
                            (fullItemIconRadius - baseIconRadius) *
                            std::clamp(anim.overflowHoverT, 0.0f, 1.0f));

                    // ========================================================
                    // OPACIDADE com hover
                    //
                    // 0.0 = excedente: opacidade mínima.
                    // 1.0 = item principal: opacidade original.
                    // ========================================================

                    // ============================================================
                    // ÍCONE - FADE BRANCO -> PRETO
                    // ============================================================

                    constexpr float minIconAlpha = 0.25f;

                    const float baseIconAlpha =
                        minIconAlpha +
                        (1.0f - minIconAlpha) * transitionFactor;

                    // Hover de 0 a 1.
                    const float hoverT =
                        std::clamp(anim.overflowHoverT, 0.0f, 1.0f);

                    // Branco -> Preto
                    const int iconBrightness =
                        static_cast<int>(
                            255.0f * (1.0f - hoverT)
                        );

                    // Opacidade normal -> 80% no hover.
                    const float iconAlpha =
                        baseIconAlpha +
                        (0.80f - baseIconAlpha) * hoverT;

                    const float iconOpacityScale =
                        127.0f + 128.0f * transitionFactor;
                    const int iconOpacity = static_cast<int>(
                        iconOpacityScale * std::clamp(iconAlpha, 0.0f, 1.0f));

                    // ========================================================
                    // DESENHA O ÍCONE
                    // ========================================================

                    // Somente o ícone fica com 50% de opacidade
                    // durante a transição para o excedente.
                    const float iconFade =
                        wasSideWrapping ? 0.50f : 1.0f;

                    draw->AddImage(
                        reinterpret_cast<ImTextureID>(icon),

                        ImVec2(
                            overflowPos.x - iconRadius,
                            overflowPos.y - iconRadius
                        ),

                        ImVec2(
                            overflowPos.x + iconRadius,
                            overflowPos.y + iconRadius
                        ),

                        ImVec2(0.0f, 0.0f),
                        ImVec2(1.0f, 1.0f),

                        FadeColor(
                            MakeGameplayIconColor(item, iconOpacity,
                                // A transição também deve respeitar o slider
                                // Base Icon Opacity. Antes ela o ignorava e
                                // só o item já acomodado voltava a 50%.
                                iconBrightness / 255.0f, true),
                            itemDrawAlpha * iconFade
                        )
                    );
                }


                // ========================================================
                // BORDA
                // ========================================================

                draw->AddCircle(
                    overflowPos,
                    finalOverflowRadius,
                    FadeColor(
                        IM_COL32(
                            (Config::g_overflowBorderColor >> 16) & 0xFF,
                            (Config::g_overflowBorderColor >> 8) & 0xFF,
                            Config::g_overflowBorderColor & 0xFF,
                            static_cast<int>(255.0f * std::clamp(
                                Config::g_overflowBorderOpacity * 0.01f, 0.0f, 1.0f))),
                        itemDrawAlpha
                    ),
                    16,
                    1.0f
                );

                if (transitionFactor > 0.001f)
                {
                    draw->AddCircle(
                        overflowPos,
                        finalOverflowRadius,
                        FadeColor(
                            IM_COL32(
                                (Config::g_itemBorderColor >> 16) & 0xFF,
                                (Config::g_itemBorderColor >> 8) & 0xFF,
                                Config::g_itemBorderColor & 0xFF,
                                static_cast<int>(255.0f * std::clamp(
                                    Config::g_itemBorderOpacity * 0.01f,
                                    0.0f, 1.0f))),
                            itemDrawAlpha * transitionFactor),
                        48, 1.5f);

                    const ItemInfo::Data itemInfo = ItemInfo::Get(
                        item.form, item.uniqueID, item.hasUniqueID);
                    DrawRadialItemQuantity(
                        draw, overflowPos, center, finalOverflowRadius,
                        itemInfo.instanceQuantity, 0.0f,
                        itemDrawAlpha * transitionFactor,
                        QuantityBadgeDirection::AutoToCenter);
                }

                const ItemInfo::Data itemInfo = ItemInfo::Get(
                    item.form, item.uniqueID, item.hasUniqueID);
                DrawQuickDrawIndicator(
                    draw,
                    item,
                    overflowPos,
                    center,
                    finalOverflowRadius,
                    itemInfo.instanceQuantity,
                    itemDrawAlpha,
                    hoverT,
                    true,
                    QuantityBadgeDirection::AutoToCenter);
            }
        }

        gameplayItemLayers.Merge(draw);

        // ============================================================
        // CENTRO
        // ============================================================

        ImVec2 previewCenter{};
        float previewMaskRadius = 0.0f;
        const bool hasPreviewMask = GetVisibleItemPreviewMask(
            previewCenter, previewMaskRadius);
        const float previewDx = center.x - previewCenter.x;
        const float previewDy = center.y - previewCenter.y;
        const bool previewCoversCenter = hasPreviewMask &&
            previewDx * previewDx + previewDy * previewDy <=
                previewMaskRadius * previewMaskRadius;
        if (!previewCoversCenter)
        {
            draw->AddCircleFilled(center, 4.0f, FadeColor(IM_COL32(15, 15, 20, 240), alpha));
            draw->AddCircle(center, 6.0f, FadeColor(IM_COL32(255, 255, 255, 130), alpha), 32, 1.5f);
        }
    }

    void DrawDraggedInventoryItem()
    {
        if (!g_draggedInventoryItem)
            return;

        if (g_inventoryDragMode == InventoryDragMode::None)
            return;

        ImDrawList* draw = ImGui::GetForegroundDrawList();

        const ImVec2 pos = g_inventoryDraggedPosition;

        constexpr float radius = 34.0f;

        // Fast Drag mantém o item dentro da última zona válida até ele tocar
        // a borda. O anel é só uma referência visual e fica atrás do item.
        if (Config::g_fastInventoryDrag && g_fastDragZoneActive)
        {
            draw->AddCircle(
                g_fastDragZoneCenter,
                FAST_DRAG_ZONE_RADIUS,
                FadeColor(IM_COL32(255, 255, 255, 165), g_globalAlpha),
                64,
                1.35f);
        }

        //draw->AddCircleFilled(
        //    pos, radius, FadeColor(IM_COL32(20, 20, 25, 235), g_globalAlpha));
        DrawEquippedItemBackground(
            draw,
            g_draggedInventoryItem,
            g_draggedInventoryUniqueID,
            g_draggedInventoryHasUniqueID,
            pos,
            radius,
            g_globalAlpha
        );

        // O item arrastado também precisa passar pelo mesmo resolvedor de
        // cores dos slots do radial. Assim, poções, escolas de magia e
        // encantamentos mantêm a sua cor configurada durante o drag.
        RadialItem draggedItem{};
        draggedItem.form = g_draggedInventoryItem;
        draggedItem.uniqueID = g_draggedInventoryUniqueID;
        draggedItem.hasUniqueID = g_draggedInventoryHasUniqueID;

        if (g_draggedInventoryItem)
        {
            auto* icon =
                ItemIcon::Get(g_draggedInventoryItem);

            if (icon)
            {
                const float iconRadius =
                    radius * 0.53f;

                const ImVec2 iconMin(
                    g_inventoryDraggedPosition.x - iconRadius,
                    g_inventoryDraggedPosition.y - iconRadius
                );

                const ImVec2 iconMax(
                    g_inventoryDraggedPosition.x + iconRadius,
                    g_inventoryDraggedPosition.y + iconRadius
                );

                draw->AddImage(
                    reinterpret_cast<ImTextureID>(icon),
                    iconMin,
                    iconMax,
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f),
                    FadeColor(
                        MakeGameplayIconColor(
                            draggedItem,
                            255,
                            1.0f,
                            true,
                            RadialSide::Left
                        ),
                        g_globalAlpha
                    )
                );
            }
        }

        draw->AddCircle(
            pos, radius, FadeColor(IM_COL32(255, 255, 255, 220), g_globalAlpha), 48, 2.5f);

        //const char* name = g_draggedInventoryItem->GetName();
        // ============================================================
        // IDENTIFICA A INSTÂNCIA ARRASTADA
        // ============================================================

        // ============================================================
        // OBTÉM O NOME DA INSTÂNCIA
        // ============================================================

        //const std::string itemName =
        //    GetRadialItemDisplayName(draggedItem);

        //const char* name = itemName.c_str();

        //if (!name || !name[0])
        //    name = Language::Get("item").c_str();

        //ImVec2 textSize = ImGui::CalcTextSize(name);

        //draw->AddText(
        //    ImVec2(pos.x - textSize.x * 0.5f, pos.y + radius + 8.0f),
        //    FadeColor(IM_COL32(255, 255, 255, 255), g_globalAlpha),
        //    name
        //);
    }

    void DrawInventoryRadialMenu()
    {
        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 screen = io.DisplaySize;

        const WheelLayout layout = GetWheelLayout();
        const ImVec2 rightCenter = layout.rightRadial;
        const ImVec2 leftCenter = layout.leftRadial;
        const ImVec2 topCenter = layout.topRadial;
        const ImVec2 bottomCenter = layout.bottomRadial;

        const ImVec2 itemPos = g_inventoryDraggedPosition;

        constexpr float activationRadius = 230.0f;

        float rightProximity = GetDropProximityAlpha(itemPos, rightCenter, activationRadius);
        float leftProximity = GetDropProximityAlpha(itemPos, leftCenter, activationRadius);
        float topProximity = GetDropProximityAlpha(itemPos, topCenter, activationRadius);
        float bottomProximity = GetDropProximityAlpha(itemPos, bottomCenter, activationRadius);

        constexpr float baseAlpha = INVENTORY_MENU_ALPHA;

        float rightAlpha = baseAlpha + (1.0f - baseAlpha) * rightProximity;
        float leftAlpha = baseAlpha + (1.0f - baseAlpha) * leftProximity;
        float topAlpha = baseAlpha + (1.0f - baseAlpha) * topProximity;
        float bottomAlpha = baseAlpha + (1.0f - baseAlpha) * bottomProximity;

        DrawRadialMenu(rightCenter, false, rightAlpha);
        DrawRadialMenu(leftCenter, true, leftAlpha);

        DrawTopRadialMenu(topCenter, static_cast<int>(g_topItems.size()), topAlpha);
        DrawBottomRadialMenu(bottomCenter, static_cast<int>(g_bottomItems.size()), bottomAlpha);
    }


    RE::TESForm* GetRandomInventoryItem()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nullptr;

        auto inventory = player->GetInventory();
        if (inventory.empty())
            return nullptr;

        // Pega o N-esimo item de forma "aleatoria" simples (sem <random> completo)
        // usando o tick count como semente basica.
        size_t index = static_cast<size_t>(GetTickCount64()) % inventory.size();

        size_t i = 0;
        for (auto& [object, entry] : inventory)
        {
            if (i == index)
            {
                return object; // object ja eh RE::TESBoundObject*, que herda de TESForm
            }
            ++i;
        }

        return nullptr;
    }

    template <class T>
    static RE::TESForm* GetFirstNamedPreviewForm()
    {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data)
            return nullptr;

        for (auto* form : data->GetFormArray<T>())
        {
            if (form && form->GetName() && form->GetName()[0] != '\0')
                return form;
        }
        return nullptr;
    }

    static RE::TESForm* GetCategoryPreviewForm(
        Config::ItemPreviewCategory category)
    {
        static std::array<RE::TESForm*,
            static_cast<std::size_t>(Config::ItemPreviewCategory::Count)> cache{};
        const auto index = static_cast<std::size_t>(category);
        if (index >= cache.size())
            return nullptr;
        if (cache[index])
            return cache[index];

        RE::TESForm* result = nullptr;
        switch (category)
        {
        case Config::ItemPreviewCategory::Spell:
            result = RE::TESForm::LookupByID<RE::SpellItem>(0x00012FCC);
            if (!result) result = GetFirstNamedPreviewForm<RE::SpellItem>();
            break;
        case Config::ItemPreviewCategory::Weapon:
            result = RE::TESForm::LookupByID<RE::TESObjectWEAP>(0x00012EB7);
            if (!result) result = GetFirstNamedPreviewForm<RE::TESObjectWEAP>();
            break;
        case Config::ItemPreviewCategory::Potion:
            result = RE::TESForm::LookupByID<RE::AlchemyItem>(0x0003EADD);
            if (!result) result = GetFirstNamedPreviewForm<RE::AlchemyItem>();
            break;
        case Config::ItemPreviewCategory::Armor:
            result = RE::TESForm::LookupByID<RE::TESObjectARMO>(0x00012E49);
            if (!result) result = GetFirstNamedPreviewForm<RE::TESObjectARMO>();
            break;
        case Config::ItemPreviewCategory::Ammo:
            result = RE::TESForm::LookupByID<RE::TESAmmo>(0x0001397D);
            if (!result) result = GetFirstNamedPreviewForm<RE::TESAmmo>();
            break;
        case Config::ItemPreviewCategory::Book:
            result = GetFirstNamedPreviewForm<RE::TESObjectBOOK>();
            break;
        case Config::ItemPreviewCategory::Misc:
            result = GetFirstNamedPreviewForm<RE::TESObjectMISC>();
            break;
        case Config::ItemPreviewCategory::Key:
            result = GetFirstNamedPreviewForm<RE::TESKey>();
            break;
        case Config::ItemPreviewCategory::SoulGem:
            result = GetFirstNamedPreviewForm<RE::TESSoulGem>();
            break;
        case Config::ItemPreviewCategory::Ingredient:
            result = GetFirstNamedPreviewForm<RE::IngredientItem>();
            break;
        case Config::ItemPreviewCategory::Scroll:
            result = GetFirstNamedPreviewForm<RE::ScrollItem>();
            break;
        default:
            break;
        }

        cache[index] = result;
        return result;
    }

    static bool GetLayoutPreviewItem(RadialItem& item, bool forceTestItem,
        std::optional<Config::ItemPreviewCategory> category = std::nullopt)
    {
        // Os controles de posição/tamanho precisam usar sempre o mesmo
        // modelo de teste. Alternar entre o item previamente selecionado e o
        // fallback fazia o Inventory3DManager recarregar modelos com bounds
        // diferentes no meio do drag, deslocando visualmente o preview.
        static RE::TESForm* fallback = nullptr;
        if (forceTestItem)
        {
            RE::TESForm* previewForm = category
                ? GetCategoryPreviewForm(*category)
                : nullptr;
            if (!previewForm)
            {
                if (!fallback)
                    fallback = GetRandomInventoryItem();
                previewForm = fallback;
            }
            item = {};
            item.form = previewForm;
            item.valid = previewForm != nullptr;
            return item.valid;
        }

        return GetSettingsPreviewItem(item) && item.form;
    }

    void DrawSelectedItemName(
        const RadialItem& item,
        float alpha = 1.0f,
        float verticalPosition = 0.72f,
        bool useConfiguredPosition = false,
        bool useConfiguredOpacity = false,
        Config::ItemPreviewProfile profile = Config::ItemPreviewProfile::Menu)
    {
        // ============================================================
        // NOME DA INSTÂNCIA
        // ============================================================

        const std::string name = item.form
            ? GetRadialItemDisplayName(item)
            : Language::Get("item_name");

        if (name.empty())
            return;

        ImDrawList* draw =
            ImGui::GetForegroundDrawList();

        ImGuiIO& io =
            ImGui::GetIO();

        const ImVec2 nameSize =
            ImGui::CalcTextSize(name.c_str());

        const WheelLayout wheelLayout = GetWheelLayout();
        ImVec2 textPosition{};
        if (useConfiguredPosition)
        {
            const auto& preview = Config::GetItemPreviewLayoutConst(profile);
            const float targetX = LayoutLerp(
                wheelLayout.min.x + 10.0f,
                wheelLayout.max.x - 10.0f,
                preview.itemNamePositionX);
            const float targetY = LayoutLerp(
                wheelLayout.min.y + 10.0f,
                wheelLayout.max.y - 10.0f,
                preview.itemNamePositionY);
            textPosition.x = std::clamp(targetX - nameSize.x * 0.5f,
                wheelLayout.min.x + 10.0f,
                std::max(wheelLayout.min.x + 10.0f, wheelLayout.max.x - nameSize.x - 10.0f));
            textPosition.y = std::clamp(targetY,
                wheelLayout.min.y + 10.0f,
                std::max(wheelLayout.min.y + 10.0f, wheelLayout.max.y - nameSize.y - 10.0f));
        }
        else
        {
            textPosition = ImVec2(
                io.DisplaySize.x * 0.50f - nameSize.x * 0.5f,
                io.DisplaySize.y * verticalPosition);
        }

        // ============================================================
        // FADE SUAVE DO NOME
        // ============================================================

        static float s_infoAlpha = 0.0f;

        static RE::TESForm* s_lastForm = nullptr;

        static std::uint16_t s_lastUniqueID = 0;

        static bool s_lastHasUniqueID = false;

        // ============================================================
        // IDENTIFICA MUDANÇA DE INSTÂNCIA
        // ============================================================

        if (s_lastForm != item.form ||
            s_lastUniqueID != item.uniqueID ||
            s_lastHasUniqueID != item.hasUniqueID)
        {
            s_lastForm = item.form;

            s_lastUniqueID = item.uniqueID;

            s_lastHasUniqueID = item.hasUniqueID;

            s_infoAlpha = 0.0f;
        }

        // ============================================================
        // ANIMAÇÃO
        // ============================================================

        const float dt =
            io.DeltaTime > 0.0f
            ? io.DeltaTime
            : (1.0f / 60.0f);

        s_infoAlpha +=
            (1.0f - s_infoAlpha) *
            (1.0f - std::exp(
                -ITEM_TRANSITION_SPEED * dt
            ));

        // ============================================================
        // DESENHA O NOME
        // ============================================================

        draw->PushClipRect(
            Resolution::ToVirtual(ImVec2(0.0f, 0.0f)),
            Resolution::ToVirtual(Resolution::GetRealSize()),
            false);
        draw->AddText(
            ImVec2(
                textPosition.x,
                textPosition.y
            ),
            FadeColor(
                IM_COL32(255, 255, 255, 230),
                alpha * s_infoAlpha * (useConfiguredOpacity
                    ? std::clamp(Config::GetItemPreviewLayoutConst(profile).itemNameOpacity,
                        0.0f, 100.0f) * 0.01f
                    : 1.0f)
            ),
            name.c_str()
        );
        draw->PopClipRect();
    }

    
    void DrawSelectedRadialInfo()
    {
        if (g_radialMode != RadialMode::Gameplay)
        {
            
            ItemPreview::Hide();
            ItemPreview::SilentPreviewMenu::Close();
            return;
        }

        ImVec2 mouse = GetRadialMousePosition();

        RadialItem* selected = nullptr;

        switch (g_radialSide)
        {
        case RadialSide::Left:
        case RadialSide::Right:
        {
            const int totalItems = static_cast<int>(g_sideItems.size());

            if (totalItems <= 0)
                break;

            const int visibleCount = std::min(totalItems, GetSideVisibleLimit());

            const WheelLayout wheelLayout = GetWheelLayout();
            const ImVec2 center =
                (g_radialSide == RadialSide::Left)
                    ? wheelLayout.leftRadial
                    : wheelLayout.rightRadial;

            const int visibleIndex = GetSideRadialItem(
                mouse, center, g_radialSide == RadialSide::Left, visibleCount, MENU_INNER_RADIUS);

            

            if (visibleIndex >= 0)
            {
                //const int actualIndex = g_sideScrollOffset + visibleIndex;

                const int actualIndex =
                    GetSideActualIndex(visibleIndex);

                if (actualIndex >= 0 && actualIndex < static_cast<int>(g_sideItems.size()))
                {
                    selected = &g_sideItems[actualIndex];
                }
            }

            break;
        }

        case RadialSide::Top:
        {
            const int totalItems =
                static_cast<int>(g_topItems.size());

            if (totalItems <= 0)
                break;

            // TOP: seleção exclusivamente pelo scroll
            const int actualIndex =
                WrapIndex(
                    g_topSelectedIndex,
                    totalItems
                );

            selected =
                &g_topItems[actualIndex];

            break;
        }

        case RadialSide::Bottom:
        {
            const int totalItems =
                static_cast<int>(g_bottomItems.size());

            if (totalItems <= 0)
                break;

            // BOTTOM: seleção exclusivamente pelo scroll
            const int actualIndex =
                WrapIndex(
                    g_bottomSelectedIndex,
                    totalItems
                );

            selected =
                &g_bottomItems[actualIndex];

            break;
        }

        default:
            break;
        }

        const bool noTopBottomSelection =
            (g_radialSide == RadialSide::Top && !g_topHasSelection) ||
            (g_radialSide == RadialSide::Bottom && !g_bottomHasSelection);

        if (noTopBottomSelection ||
            !selected ||
            !selected->valid ||
            !selected->form)
        {
            ItemPreview::Hide();
            return;
        }

        // ============================================================
        // POSIÇÃO DO PREVIEW = CENTRO DA TELA
        // ============================================================

        const WheelLayout previewWheelLayout = GetWheelLayout();
        const auto previewProfile = PreviewProfileForSide(g_radialSide);
        const auto& previewLayout = Config::GetItemPreviewLayoutConst(previewProfile);

        ItemPreview::SetHudPosition(
            ImVec2(
                LayoutLerp(previewWheelLayout.min.x, previewWheelLayout.max.x,
                    previewLayout.itemPositionX),
                LayoutLerp(previewWheelLayout.min.y, previewWheelLayout.max.y,
                    previewLayout.itemPositionY)
            )
        );
        ItemPreview::SetSizeScale(previewLayout.itemSize * 0.01f);

        // ============================================================
        // DIREÇÃO
        // ============================================================

        switch (g_radialSide)
        {
        case RadialSide::Left:
            ItemPreview::SetRotationDirection(-1.0f);
            break;

        case RadialSide::Right:
            ItemPreview::SetRotationDirection(1.0f);
            break;

        case RadialSide::Top:
        case RadialSide::Bottom:
        {
            // Por enquanto random simples.
            // Depois podemos randomizar somente quando selected mudar.
            static float randomDirection = 1.0f;
            static RE::FormID lastRandomForm = 0;

            if (lastRandomForm != selected->form->GetFormID())
            {
                randomDirection =
                    (std::rand() % 2 == 0) ? -1.0f : 1.0f;

                lastRandomForm = selected->form->GetFormID();
            }

            ItemPreview::SetRotationDirection(randomDirection);
            break;
        }

        default:
            break;
        }

        if (Config::g_showItemPreviewGameplay)
        {
            ItemPreview::SilentPreviewMenu::Open();
            ItemPreview::Show(
                selected->form,
                selected->uniqueID,
                selected->hasUniqueID
            );
        }
        else
        {
            ItemPreview::Hide();
            ItemPreview::SilentPreviewMenu::Close();
        }
                
        DrawSelectedItemName(
            *selected,
            g_globalAlpha,
            0.72f,
            true,
            true,
            previewProfile
        );
    }

        
    void ScrollSettingsRadial(int direction)
    {
        if (direction == 0)
            return;

        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return;

        // Scroll arrives through the SKSE input sink between render frames.
        // The saved Settings cursor is virtual, so its target layout must be
        // virtual too.
        const ImVec2 screen = Resolution::GetVirtualSize();

        const RadialSide side = GetSettingsDropSide(
            g_settingsMousePos,
            screen
        );

        // ============================================================
        // TOP
        // ============================================================

        if (side == RadialSide::Top)
        {
            const int total = static_cast<int>(g_topItems.size());

            if (total <= GetTopBottomVisibleLimit(RadialSide::Top))
                return;

            g_topSelectedIndex = WrapIndex(
                g_topSelectedIndex + direction,
                total
            );

            g_topScrollAnim += static_cast<float>(direction);

            // O índice é usado apenas para posicionar a lista.
            // Não queremos selecionar itens no Settings.
            g_topHasSelection = false;

            return;
        }

        // ============================================================
        // BOTTOM
        // ============================================================

        if (side == RadialSide::Bottom)
        {
            const int total = static_cast<int>(g_bottomItems.size());

            if (total <= GetTopBottomVisibleLimit(RadialSide::Bottom))
                return;

            g_bottomSelectedIndex = WrapIndex(
                g_bottomSelectedIndex + direction,
                total
            );

            g_bottomScrollAnim += static_cast<float>(direction);

            g_bottomHasSelection = false;

            return;
        }

        // ============================================================
        // LEFT
        // ============================================================

        if (side == RadialSide::Left)
        {
            if (g_settingsTopologySettleRemaining > 0.0f)
            {
                g_pendingSettingsScroll = std::clamp(
                    g_pendingSettingsScroll + (direction < 0 ? -1 : 1),
                    -12, 12);
                return;
            }

            const int total = static_cast<int>(g_sideItems.size());

            if (total < 3)
                return;

            if (total <= GetSideVisibleLimit() && Config::g_lockSideScroll)
                return;

            QueueSettingsSideScrollStep(direction);
        }
    }

    void Menu::ScrollTopBottomRadial(int direction)
    {
        if (direction == 0)
            return;

        // Scroll para baixo (direção negativa) seleciona a mão esquerda;
        // scroll para cima seleciona a direita.
        g_topBottomLastScrollEquipLeft = direction < 0;

        // ============================================================
        // TOP
        // ============================================================

        if (g_radialSide == RadialSide::Top)
        {
            const int totalItems =
                static_cast<int>(g_topItems.size());

            if (totalItems <= 0)
                return;

            // Estava sem hover/seleção:
            // primeiro scroll apenas restaura o último item.
            if (!g_topHasSelection)
            {
                g_topSelectedIndex =
                    WrapIndex(g_topSelectedIndex, totalItems);

                g_topHasSelection = true;
                return;
            }

            // Já estava selecionado:
            // agora sim avança/volta.
            g_topSelectedIndex =
                WrapIndex(
                    g_topSelectedIndex + direction,
                    totalItems
                );

            if (totalItems > GetTopBottomVisibleLimit(RadialSide::Top))
            {
                g_topScrollAnim +=
                    static_cast<float>(direction);
            }

            return;
        }

        // ============================================================
        // BOTTOM
        // ============================================================

        if (g_radialSide == RadialSide::Bottom)
        {
            const int totalItems =
                static_cast<int>(g_bottomItems.size());

            if (totalItems <= 0)
                return;

            // Estava sem hover/seleção:
            // primeiro scroll apenas restaura o último item.
            if (!g_bottomHasSelection)
            {
                g_bottomSelectedIndex =
                    WrapIndex(g_bottomSelectedIndex, totalItems);

                g_bottomHasSelection = true;
                return;
            }

            // Já estava selecionado:
            // agora sim avança/volta.
            g_bottomSelectedIndex =
                WrapIndex(
                    g_bottomSelectedIndex + direction,
                    totalItems
                );

            if (totalItems > GetTopBottomVisibleLimit(RadialSide::Bottom))
            {
                g_bottomScrollAnim +=
                    static_cast<float>(direction);
            }

            return;
        }
    }

    void UpdateRadialScroll()
    {
        if (!g_showWindow)
            return;

        if (g_radialMode != RadialMode::Gameplay)
            return;

        ImGuiIO& io = ImGui::GetIO();

        const float wheel = io.MouseWheel;

        if (wheel == 0.0f)
            return;

        // ============================================================
        // TOP
        // ============================================================

        if (g_radialSide == RadialSide::Top)
        {
            const int totalItems =
                static_cast<int>(g_topItems.size());

            if (totalItems <= 0)
                return;

            const int direction =
                wheel > 0.0f ? 1 : -1;

            // ============================================================
            // SCROLL AGORA É O CONTROLADOR DA SELEÇÃO
            // ============================================================

            g_topSelectionMode =
                TopBottomSelectionMode::Scroll;

            g_topSelectedIndex =
                WrapIndex(
                    g_topSelectedIndex + direction,
                    totalItems
                );

            // Lista grande:
            // selecionado permanece no centro e a lista gira.
            if (totalItems > GetTopBottomVisibleLimit(RadialSide::Top))
            {
                g_topScrollAnim +=
                    static_cast<float>(direction);
            }

            return;
        }

        // ============================================================
        // BOTTOM
        // ============================================================

        if (g_radialSide == RadialSide::Bottom)
        {
            const int totalItems =
                static_cast<int>(g_bottomItems.size());

            if (totalItems <= 0)
                return;

            const int direction =
                wheel > 0.0f ? 1 : -1;

            g_bottomSelectionMode =
                TopBottomSelectionMode::Scroll;

            g_bottomSelectedIndex =
                WrapIndex(
                    g_bottomSelectedIndex + direction,
                    totalItems
                );

            if (totalItems > GetTopBottomVisibleLimit(RadialSide::Bottom))
            {
                g_bottomScrollAnim +=
                    static_cast<float>(direction);
            }

            return;
        }

        // ============================================================
        // ESQUERDA / DIREITA
        // ============================================================

        const int totalItems = static_cast<int>(g_sideItems.size());

        if (totalItems < 3)
        {
            return;
        }

        if (totalItems <= GetSideVisibleLimit() && Config::g_lockSideScroll)
            return;

        if (wheel > 0.0f)
        {
            g_sideScrollDirection = -1;
            g_sideScrollOffset--;
        }
        else if (wheel < 0.0f)
        {
            g_sideScrollDirection = 1;
            g_sideScrollOffset++;
        }

        const int visibleCount = std::min(totalItems, GetSideVisibleLimit());
        const int maxOffset = totalItems - visibleCount;
        if (maxOffset <= 0)
            g_sideScrollOffset = WrapSideIndex(g_sideScrollOffset, totalItems);
        else
            g_sideScrollOffset = std::clamp(g_sideScrollOffset, 0, maxOffset);
    }

    bool RadialSideHasItems(RadialSide side)
    {
        switch (side)
        {
        case RadialSide::Left:
        case RadialSide::Right:
            return !g_sideItems.empty();

        case RadialSide::Top:
            return !g_topItems.empty();

        case RadialSide::Bottom:
            return !g_bottomItems.empty();

        default:
            return false;
        }
    }

    // ============================================================
    // RESET DO INDICADOR
    // ============================================================

    void ResetSettingsCharge()
    {
        g_settingsCharge = 0.0f;
        g_settingsVisualCharge = 0.0f;
        g_settingsOpening = false;
    }

    // ============================================================
    // RECEBE O SCROLL DO MOUSE
    // ============================================================

    void HandleSettingsScroll()
    {
        if (g_settingsOpening)
            return;
            
        // Apenas durante gameplay.
        if (g_radialMode != RadialMode::Gameplay)
            return;

        // Radial precisa estar aberto.
        if (!g_showWindow)
            return;

        // Apenas enquanto estamos no centro.
        if (g_radialSide != RadialSide::None)
            return;

        // Não permite carregar enquanto o Settings está aberto.
        auto* ui = RE::UI::GetSingleton();

        if (ui && ui->IsMenuOpen("WheelSetting"))
            return;

        // ============================================================
        // ACUMULA CARGA
        // ============================================================

        g_settingsCharge += SETTINGS_SCROLL_STEP;

        if (g_settingsCharge > 1.0f)
            g_settingsCharge = 1.0f;

        // ============================================================
        // ABRE SETTINGS QUANDO ATINGIR O LIMITE
        // ============================================================

        if (g_settingsCharge >= 1.0f)
        {
            // Não abre imediatamente.
            // Aguarda a animação visual chegar ao final.
            g_settingsOpening = true;

            g_settingsCharge = 1.0f;

            Logger::GetSingleton().Print(
                "SETTINGS CHARGE COMPLETE | waiting animation"
            );

            return;
        }
    }

    void UpdateSettingsCharge(float deltaTime)
    {
        deltaTime = std::clamp(
            deltaTime,
            0.0f,
            0.05f
        );

        // ============================================================
        // DECAIMENTO
        // ============================================================

        if (!g_settingsOpening)
        {
            g_settingsCharge -=
                SETTINGS_DECAY_SPEED * deltaTime;

            g_settingsCharge = std::max(
                0.0f,
                g_settingsCharge
            );
        }

        // ============================================================
        // ANIMAÇÃO SUAVE
        // ============================================================

        const float factor =
            1.0f - std::exp(
                -SETTINGS_GROW_SPEED * deltaTime
            );

        g_settingsVisualCharge +=
            (g_settingsCharge - g_settingsVisualCharge)
            * factor;

        // ============================================================
        // ESPERA A BOLINHA CHEGAR AO LIMITE
        // ============================================================

        if (!g_settingsOpening)
            return;

        if (g_settingsVisualCharge < 0.995f)
            return;

        // ============================================================
        // ABRE O SETTINGS
        // ============================================================

        g_settingsOpening = false;

        g_settingsCharge = 0.0f;
        g_settingsVisualCharge = 0.0f;

        // Abre pelo controlador do menu registrado.
        // Assim, o blur também será ativado.
        SettingsMenu::WheelSettingsMenu::Open();

        Logger::GetSingleton().Print(
            "SETTINGS SHOW MESSAGE SENT"
        );
    }

    void DrawSettingsCursor()
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return;

        const ImVec2 mouse = GetSkyrimMousePos();

        // O ponto central continua respondendo imediatamente ao cursor. O
        // anel tem uma reação própria e leve ao movimento, para sugerir uma
        // peça flutuando ao redor dele sem prejudicar a precisão do clique.
        static ImVec2 lastMouse = mouse;
        static double lastAnimationTime = -1.0;
        static float movementT = 0.0f;

        const double now = ImGui::GetTime();
        if (now != lastAnimationTime)
        {
            const float mouseDx = mouse.x - lastMouse.x;
            const float mouseDy = mouse.y - lastMouse.y;
            const float distance = std::sqrt(mouseDx * mouseDx + mouseDy * mouseDy);
            const float target = std::clamp(distance / 14.0f, 0.0f, 1.0f);
            const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f);
            const float response = 1.0f - std::exp(-10.0f * dt);
            movementT += (target - movementT) * response;
            lastMouse = mouse;
            lastAnimationTime = now;
        }

        const float time = static_cast<float>(now);
        const float ringScale =
            0.85f + 0.30f * movementT +
            std::sin(time * 1.45f) * 0.018f;
        const ImVec2 ringCenter(
            mouse.x + std::sin(time * 1.15f) * 0.55f,
            mouse.y + std::cos(time * 1.30f) * 0.55f);

        ImDrawList* draw =
            ImGui::GetForegroundDrawList();

        // Sombra
        draw->AddCircleFilled(
            ImVec2(mouse.x + 1.5f, mouse.y + 1.5f),
            5.0f,
            IM_COL32(0, 0, 0, 70),
            16
        );

        // Cursor branco
        draw->AddCircleFilled(
            mouse,
            3.0f,
            IM_COL32(255, 255, 255, 210),
            16
        );

        // Anel externo: pequeno, sem preenchimento e com 50% de opacidade.
        // A escala varia 15% entre repouso e movimento.
        draw->AddCircle(
            ringCenter,
            14.5f * ringScale,
            IM_COL32(255, 255, 255, 84),
            32,
            1.35f
        );
    }

        
    // ============================================================
    // VERIFICA ALTERAÇÃO DO ITEM DO PREVIEW
    // ============================================================

    static void UpdateSettingsSectionPreviewState()
    {
        Config::ItemPreviewProfile editedProfile{};
        PreviewLayoutField editedField{};
        Config::ItemPreviewCategory editedCategory{};
        if (GetActivePreviewControl(editedProfile, editedField) ||
            GetActivePreviewCategory(editedCategory) ||
            g_previewLayoutButtonHeld)
        {
            // O item real é removido de propósito enquanto os controles de
            // preview exibem o modelo de teste. Essa troca não representa
            // uma nova seleção e não deve recolher o painel de configurações.
            g_sectionPreviewForm = nullptr;
            g_sectionPreviewUniqueID = 0;
            g_sectionPreviewHasUniqueID = false;
            return;
        }

        RadialItem previewItem{};

        const bool hasPreview =
            GetSettingsPreviewItem(previewItem);

        RE::TESForm* currentForm =
            hasPreview
                ? previewItem.form
                : nullptr;

        const std::uint16_t currentUniqueID =
            hasPreview
                ? previewItem.uniqueID
                : 0;

        const bool currentHasUniqueID =
            hasPreview
                ? previewItem.hasUniqueID
                : false;

        // ========================================================
        // IDENTIFICA MUDANÇA DE INSTÂNCIA
        // ========================================================

        const bool changed =
            g_sectionPreviewForm != currentForm ||
            g_sectionPreviewUniqueID != currentUniqueID ||
            g_sectionPreviewHasUniqueID != currentHasUniqueID;

        if (!changed)
            return;

        // ========================================================
        // NOVO ITEM - FECHA O SELETOR
        // ========================================================

        if (SettingsMenu::WheelSettingsMenu::IsOpen() &&
            (g_settingsSectionExpanded || g_settingsSectionOpenT > 0.05f))
        {
            CloseSettingsPanelKeepSectionShortcuts();
        }
        else
        {
            CloseSettingsSectionMenu();
        }

        // ========================================================
        // ATUALIZA A IDENTIFICAÇÃO
        // ========================================================

        g_sectionPreviewForm =
            currentForm;

        g_sectionPreviewUniqueID =
            currentUniqueID;

        g_sectionPreviewHasUniqueID =
            currentHasUniqueID;
    }
        
    static void UpdateSettingsPreview()
    {
        // ============================================================
        // OBTÉM O ITEM SELECIONADO
        //
        // GetSettingsPreviewItem() já considera:
        // 1. Item sendo arrastado.
        // 2. Item hovered.
        // 3. Último item selecionado durante o timer de 15 segundos.
        // ============================================================

        RadialItem previewItem{};

        Config::ItemPreviewProfile editedProfile{};
        PreviewLayoutField editedField{};
        const bool editingPreviewLayout = GetActivePreviewControl(
            editedProfile, editedField);
        Config::ItemPreviewCategory editedCategory{};
        const bool editingPreviewCategory = GetActivePreviewCategory(
            editedCategory);
        const bool editingPreview = editingPreviewLayout || editingPreviewCategory;
        if (!GetLayoutPreviewItem(previewItem, editingPreview,
                editingPreviewCategory
                    ? std::optional<Config::ItemPreviewCategory>(editedCategory)
                    : std::nullopt) ||
            !previewItem.form)
        {
            ItemPreview::Hide();
            return;
        }

        // ============================================================
        // IDENTIFICAÇÃO DA INSTÂNCIA
        // ============================================================

        RE::TESForm* form =
            previewItem.form;

        const std::uint16_t uniqueID =
            previewItem.uniqueID;

        const bool hasUniqueID =
            previewItem.hasUniqueID;

        // ============================================================
        // POSIÇÃO DO PREVIEW
        // ============================================================

        // This click is evaluated by Input.cpp after EndFrame(), where ImGui
        // reports physical dimensions. The golden close circle is virtual.
        const WheelLayout previewWheelLayout = GetWheelLayout();
        const auto previewProfile = PreviewProfileForActiveSlider();
        const auto& previewLayout = Config::GetItemPreviewLayoutConst(previewProfile);

        ItemPreview::SetHudPosition(
            ImVec2(
                LayoutLerp(previewWheelLayout.min.x, previewWheelLayout.max.x,
                    previewLayout.itemPositionX),
                LayoutLerp(previewWheelLayout.min.y, previewWheelLayout.max.y,
                    previewLayout.itemPositionY)
            )
        );
        ItemPreview::SetSizeScale(previewLayout.itemSize * 0.01f);

        ItemPreview::SilentPreviewMenu::Open();

        // ============================================================
        // PREVIEW DA INSTÂNCIA CORRETA
        // ============================================================

        ItemPreview::Show(
            form,
            uniqueID,
            hasUniqueID
        );
    }

    
    
    static void DrawGoldenPyramid(
        ImDrawList* draw,
        const ImVec2& center,
        float size,
        float alpha)
    {
        if (!draw)
            return;

        const ImU32 color =
            FadeColor(IM_COL32(255, 215, 0, 255), alpha);

        const float h = size * 0.85f;
        const float w = size * 0.65f;

        // Vértice superior
        ImVec2 top(center.x, center.y - h * 0.55f);

        // Base da pirâmide
        ImVec2 front(center.x, center.y + h * 0.55f);
        ImVec2 left(center.x - w, center.y + h * 0.15f);
        ImVec2 right(center.x + w, center.y + h * 0.15f);
        ImVec2 back(center.x, center.y - h * 0.05f);

        constexpr float thickness = 2.0f;

        // Base
        draw->AddLine(left, back, color, thickness);
        draw->AddLine(back, right, color, thickness);
        draw->AddLine(right, front, color, thickness);
        draw->AddLine(front, left, color, thickness);

        // Arestas até o topo
        draw->AddLine(top, left, color, thickness);
        draw->AddLine(top, right, color, thickness);
        draw->AddLine(top, front, color, thickness);
        draw->AddLine(top, back, color, thickness);
    }

        
    static void DrawGoldenTriangle(
        ImDrawList* draw,
        const ImVec2& center,
        float size,
        float alpha)
    {
        if (!draw)
            return;

        const ImU32 color =
            FadeColor(IM_COL32(255, 215, 0, 255), alpha);

        const ImVec2 top(
            center.x,
            center.y - size * 0.5f
        );

        const ImVec2 left(
            center.x - size * 0.5f,
            center.y + size * 0.5f
        );

        const ImVec2 right(
            center.x + size * 0.5f,
            center.y + size * 0.5f
        );

        draw->AddTriangle(
            top,
            left,
            right,
            color,
            2.0f
        );
    }

        
    static void DrawItemInfoRow(
        ImDrawList* draw,
        const ImVec2& position,
        float width,
        const char* label,
        const std::string& value,
        float alpha)
    {
        if (!draw || !label)
            return;

        const ImU32 labelColor =
            FadeColor(
                IM_COL32(165, 160, 150, 230),
                alpha
            );

        const ImU32 valueColor =
            FadeColor(
                IM_COL32(235, 230, 215, 255),
                alpha
            );

        // ========================================================
        // LABEL
        // ========================================================

        DrawTextWithShadow(
            draw,
            position,
            labelColor,
            label,
            alpha
        );

        // ========================================================
        // VALOR ALINHADO À DIREITA
        // ========================================================

        const ImVec2 valueSize =
            ImGui::CalcTextSize(
                value.c_str()
            );

        DrawTextWithShadow(
            draw,
            ImVec2(
                position.x + width - valueSize.x,
                position.y
            ),
            valueColor,
            value.c_str(),
            alpha
        );

    }


    static std::string FormatItemStat(
        float value,
        int precision = 1)
    {
        return fmt::format(
            "{:.{}f}",
            value,
            precision
        );
    }

    struct SettingsInfoPanel
    {
        ImVec2 min;
        ImVec2 max;

        float width = 0.0f;
        float height = 0.0f;
    };

    static float GetSettingsPanelContentBottom(
        const SettingsInfoPanel& panel, float padding)
    {
        // Os três painéis compartilham o mesmo corte: pouco antes do início
        // visual do radial Bottom. O conteúdo restante fica acessível pelo
        // scrollbar próprio de cada seção.
        const float bottomRadialStart = GetWheelLayout().bottomRadial.y - 95.0f;
        return std::max(panel.min.y + 80.0f,
            std::min(panel.max.y - padding, bottomRadialStart));
    }

    static SettingsInfoPanel GetSettingsInfoPanel(
        const ImVec2& screen)
    {
        SettingsInfoPanel panel{};

        // ========================================================
        // CENTRO DO RADIAL DIREITO VIRTUAL
        //
        // Ajustar esses valores com o centro do radial direito
        // ========================================================

        const WheelLayout wheelLayout = GetWheelLayout();
        const float visibleWidth = wheelLayout.max.x - wheelLayout.min.x;
        const float visibleHeight = wheelLayout.max.y - wheelLayout.min.y;
        const ImVec2 rightRadialCenter(
            wheelLayout.max.x - visibleWidth * 0.25f,
            wheelLayout.center.y
        );

        constexpr float radialRadius = 175.0f;

        // ========================================================
        // LIMITES DO PAINEL
        //
        // ESQUERDA: borda esquerda do radial direito.
        // DIREITA: centro do radial direito.
        //
        // TOP: altura da borda superior do radial.
        // BOTTOM: altura da borda inferior do radial.
        // ========================================================

        // ========================================================
        // PAINEL DE INFORMAÇÕES - LARGURA DINÂMICA
        // ========================================================

        // The descriptor grows more gently than the text itself: two thirds
        // of the user font multiplier delta keeps it readable without making
        // a 2.5x font produce an unnecessarily huge panel.
        const float descriptorScale = 1.0f +
            (std::clamp(Config::g_fontSizeScale, 1.0f, 2.5f) - 1.0f) * (2.0f / 3.0f);

        const float baseWidth =
            radialRadius * descriptorScale;

        const float baseHeight =
            radialRadius * 2.0f * descriptorScale;

        // ============================================================
        // EXPANSÃO PROPORCIONAL
        //
        // 20% da largura total:
        // 10% para esquerda e 10% para direita.
        //
        // 20% da altura total:
        // 10% para cima e 10% para baixo.
        // ============================================================

        constexpr float expansion = 0.50f;

        // ============================================================
        // POSIÇÃO VERTICAL DO PAINEL
        //
        // 0.00f = posição original
        // 0.05f = desce 5% da altura da tela
        // 0.10f = desce 10% da altura da tela
        // -0.05f = sobe 5% da altura da tela
        // ============================================================

        constexpr float verticalOffsetPercent = 0.05f;

        const float verticalOffset = visibleHeight * verticalOffsetPercent;
            
        const float expandX =
            baseWidth * expansion * 0.5f;

        const float expandY =
            baseHeight * expansion * 0.5f;

        // ============================================================
        // RETÂNGULO ORIGINAL
        // ============================================================

        const ImVec2 originalMin(
            rightRadialCenter.x - baseWidth,
            rightRadialCenter.y - radialRadius
        );

        const ImVec2 originalMax(
            rightRadialCenter.x,
            rightRadialCenter.y + radialRadius
        );

        // ============================================================
        // RETÂNGULO EXPANDIDO
        // ============================================================

        panel.min = ImVec2(
            originalMin.x - expandX,
            originalMin.y - expandY + verticalOffset
        );

        panel.max = ImVec2(
            originalMax.x + expandX,
            originalMax.y + expandY + verticalOffset
        );

        // Uma única posição fixa é compartilhada pelo WheelSettings e pelo
        // descriptor de gameplay. O radial não reposiciona mais este painel.
        GetFixedSettingsPanelBounds(wheelLayout, panel.min, panel.max);
        g_settingsPanelTargetMinX = panel.min.x;
        g_settingsPanelAnimatedMinX = panel.min.x;
        g_settingsPanelRightX = panel.max.x;
        g_settingsPanelTopY = panel.min.y;
        g_settingsHorizontalShortfall = 0.0f;

        panel.width =
            panel.max.x - panel.min.x;

        panel.height =
            panel.max.y - panel.min.y;

        return panel;
    }

      
        
    // ============================================================
    // POSIÇÃO DO DESCRIPTOR DURANTE GAMEPLAY
    // ============================================================

    static SettingsInfoPanel GetGameplayInfoPanel()
    {
        const ImVec2 screen =
            ImGui::GetIO().DisplaySize;

        // Utiliza as mesmas dimensões e margens
        // do descriptor original do Settings.
        SettingsInfoPanel panel =
            GetSettingsInfoPanel(screen);

        // ========================================================
        // RADIAL DIREITO
        //
        // Descriptor aparece na esquerda.
        // ========================================================

        if (g_radialSide == RadialSide::Right)
        {
            const float oldMinX =
                panel.min.x;

            const float oldMaxX =
                panel.max.x;

            // Espelha o retângulo horizontalmente.
            const float mirrorX = GetWheelLayout().center.x * 2.0f;
            panel.min.x = mirrorX - oldMaxX;

            panel.max.x = mirrorX - oldMinX;
        }

        // ========================================================
        // RADIAL LEFT / TOP / BOTTOM
        //
        // Descriptor permanece à direita.
        // ========================================================

        panel.width =
            panel.max.x - panel.min.x;

        panel.height =
            panel.max.y - panel.min.y;

        return panel;
    }  

    static bool IsSettingsItemCoveredByControlPanel(const ImVec2& position, float radius)
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen() ||
            g_settingsSection == SettingsSection::ItemInfo)
        {
            return false;
        }

        const SettingsInfoPanel panel = GetSettingsInfoPanel(ImGui::GetIO().DisplaySize);
        const float nearestX = std::clamp(position.x, panel.min.x, panel.max.x);
        const float nearestY = std::clamp(position.y, panel.min.y, panel.max.y);
        const float dx = position.x - nearestX;
        const float dy = position.y - nearestY;
        return dx * dx + dy * dy <= radius * radius;
    }

    static bool IsMouseOverSettingsInfoPanel()
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return false;

        const ImVec2 screen =
            ImGui::GetIO().DisplaySize;

        const SettingsInfoPanel panel =
            GetSettingsInfoPanel(screen);

        const ImVec2 mouse =
            g_settingsMousePos;

        return
            mouse.x >= panel.min.x &&
            mouse.x <= panel.max.x &&
            mouse.y >= panel.min.y &&
            mouse.y <= panel.max.y;
    }

    bool ScrollSettingsItemInfo(int direction)
    {
        if (g_settingsSection == SettingsSection::Settings)
        {
            const SettingsInfoPanel settingsPanel =
                GetSettingsInfoPanel(ImGui::GetIO().DisplaySize);
            const bool overSettings =
                g_settingsMousePos.x >= settingsPanel.min.x &&
                g_settingsMousePos.x <= settingsPanel.max.x &&
                g_settingsMousePos.y >= settingsPanel.min.y &&
                g_settingsMousePos.y <= settingsPanel.max.y;
            if (!overSettings)
                return false;
            g_settingsPanelScroll = std::clamp(
                g_settingsPanelScroll - static_cast<float>(direction) * 55.0f,
                0.0f, g_settingsPanelMaxScroll);
            return true;
        }

        if (g_settingsSection == SettingsSection::Gameplay)
        {
            const SettingsInfoPanel gameplayPanel =
                GetSettingsInfoPanel(ImGui::GetIO().DisplaySize);
            const bool overGameplay =
                g_settingsMousePos.x >= gameplayPanel.min.x &&
                g_settingsMousePos.x <= gameplayPanel.max.x &&
                g_settingsMousePos.y >= gameplayPanel.min.y &&
                g_settingsMousePos.y <= gameplayPanel.max.y;
            if (!overGameplay) return false;
            g_gameplayPanelScroll = std::clamp(
                g_gameplayPanelScroll - static_cast<float>(direction) * 55.0f,
                0.0f, g_gameplayPanelMaxScroll);
            return true;
        }
        if (g_settingsSection == SettingsSection::Layout)
        {
            const SettingsInfoPanel layoutPanel =
                GetSettingsInfoPanel(ImGui::GetIO().DisplaySize);
            const bool overLayout =
                g_settingsMousePos.x >= layoutPanel.min.x &&
                g_settingsMousePos.x <= layoutPanel.max.x &&
                g_settingsMousePos.y >= layoutPanel.min.y &&
                g_settingsMousePos.y <= layoutPanel.max.y;
            if (!overLayout)
                return false;

            g_layoutPanelScroll = std::clamp(
                g_layoutPanelScroll - static_cast<float>(direction) * 55.0f,
                0.0f,
                g_layoutPanelMaxScroll);
            return true;
        }

        // ========================================================
        // MOUSE FORA DO PAINEL
        //
        // Deixa o scroll funcionar normalmente nos radiais.
        // ========================================================

        if (!IsMouseOverSettingsInfoPanel())
            return false;

        if (direction == 0)
            return true;

        const SettingsInfoPanel panel =
            GetSettingsInfoPanel(
                ImGui::GetIO().DisplaySize
            );

        // ========================================================
        // ALTURA MÁXIMA DO SCROLL
        // ========================================================

        const float maxScroll =
            std::max(
                0.0f,
                g_settingsInfoContentHeight -
                panel.height
            );

        // ========================================================
        // ATUALIZA SCROLL
        // ========================================================

        g_settingsInfoScroll =
            std::clamp(
                g_settingsInfoScroll -
                    static_cast<float>(direction) *
                    SETTINGS_INFO_SCROLL_STEP,

                0.0f,
                maxScroll
            );

        return true;
    }
    
        
    void DrawTextWithShadow(
        ImDrawList* draw,
        const ImVec2& position,
        ImU32 color,
        const char* text,
        float alpha)
    {
        if (!draw || !text || !text[0])
            return;

        // ========================================================
        // SOMBRA
        // ========================================================

        const ImVec2 shadowPosition(
            position.x + 1.0f,
            position.y + 1.0f
        );

        draw->AddText(
            shadowPosition,
            FadeColor(
                IM_COL32(0, 0, 0, 190),
                alpha
            ),
            text
        );

        // ========================================================
        // TEXTO ORIGINAL
        // ========================================================

        draw->AddText(
            position,
            color,
            text
        );
    }

    static float DrawWrappedItemInfoText(
        ImDrawList* draw,
        const ImVec2& position,
        float width,
        ImU32 color,
        const std::string& text,
        float alpha)
    {
        if (!draw || text.empty() || width <= 0.0f)
            return 0.0f;

        auto* font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize();
        if (!font || fontSize <= 0.0f)
            return 0.0f;

        // O overload com wrap_width preserva o font atual do painel e deixa
        // o ImGui calcular as quebras, inclusive para texto UTF-8 localizado.
        const ImVec2 size = font->CalcTextSizeA(
            fontSize,
            FLT_MAX,
            width,
            text.c_str());
        const ImU32 shadow = FadeColor(IM_COL32(0, 0, 0, 190), alpha);

        draw->AddText(
            font,
            fontSize,
            ImVec2(position.x + 1.0f, position.y + 1.0f),
            shadow,
            text.c_str(),
            nullptr,
            width);
        draw->AddText(
            font,
            fontSize,
            position,
            color,
            text.c_str(),
            nullptr,
            width);

        return size.y;
    }

    static float DrawSettingsItemInfo(
        const ItemInfo::Data& info,
        const RadialItem* radialItem,
        const ImVec2& position,
        float alpha,
        float panelWidth)
    {
        if (!info.form)
            return 0.0f;

        ImDrawList* draw =
            ImGui::GetForegroundDrawList();

        //constexpr float panelWidth = 260.0f;
        const float descriptorScale = 1.0f +
            (std::clamp(Config::g_fontSizeScale, 1.0f, 2.5f) - 1.0f) * (2.0f / 3.0f);
        const float rowHeight = 23.0f * descriptorScale;

        const ImU32 gold =
            FadeColor(
                IM_COL32(215, 195, 150, 255),
                alpha
            );

        const ImU32 white =
            FadeColor(
                IM_COL32(235, 230, 215, 255),
                alpha
            );

        const ImU32 muted =
            FadeColor(
                IM_COL32(155, 150, 140, 200),
                alpha
            );

        const ImU32 separatorColor =
            FadeColor(
                IM_COL32(180, 165, 130, 80),
                alpha
            );

        float y = position.y;

        // ========================================================
        // HEADER
        // ========================================================

        //draw->AddText(
        //    ImVec2(position.x, y),
        //    gold,
        //    "ITEM INFORMATION"
        //);

        DrawTextWithShadow(
            draw,
            ImVec2(position.x, y),
            gold,
            Language::Get("information").c_str(),
            alpha
        );

        y += 28.0f * descriptorScale;

        draw->AddLine(
            ImVec2(position.x, y),
            ImVec2(
                position.x + panelWidth,
                y
            ),
            separatorColor,
            1.0f
        );

        y += 12.0f * descriptorScale;

        // ========================================================
        // NOME DO ITEM
        // ========================================================

        draw->AddText(
            ImVec2(position.x, y),
            white,
            info.name.c_str()
        );

        y += 30.0f * descriptorScale;

        // ========================================================
        // QUICK DRAW SALVO
        //
        // abaixo do nome: antes da quantidade para
        // itens físicos e antes do cabeçalho para magias.
        // ========================================================
        if (radialItem)
        {
            if (const auto* gesture = FindQuickDrawStroke(*radialItem);
                gesture && !gesture->empty())
            {
            
                const float gestureRadius = 44.0f * descriptorScale;
            
                const ImVec2 gestureCenter(position.x + panelWidth * 0.5f,
                    y + gestureRadius + 1.0f);
            
                draw->AddCircle(gestureCenter, gestureRadius,
                    FadeColor(IM_COL32(190, 190, 190, 115), alpha), 40, 1.0f);
            
                DrawQuickDrawStroke(draw, *gesture, gestureCenter,
                    gestureRadius, FadeColor(IM_COL32(245, 245, 245, 235), alpha),
                    1.6f * descriptorScale);
            
                y += (gestureRadius * 2.0f + 13.0f * descriptorScale);
            }
        }

        // ========================================================
        // FUNÇÃO AUXILIAR DE ATRIBUTOS
        // ========================================================

        auto AddRow = [&](const char* label,
                        const std::string& value)
        {
            DrawItemInfoRow(
                draw,
                ImVec2(position.x, y),
                panelWidth,
                label,
                value,
                alpha
            );

            y += rowHeight;
        };

        // ========================================================
        // QUANTIDADE
        // ========================================================

        if (info.totalQuantity > 0)
        {
            //AddRow(
            //    "Quantity",
            //    fmt::format(
            //        "{} / {}",
            //        info.instanceQuantity,
            //        info.totalQuantity
            //    )
            //);
            // ============================================================
            // QUANTIDADE>>>> DA INSTÂNCIA <<<<<<<<<<<<<<<<<<
            // ============================================================

            if (info.instanceQuantity > 0)
            {
                AddRow(
                    Language::Get("quantity").c_str(),
                    std::to_string(
                        info.instanceQuantity
                    )
                );
            }
        }

        // ========================================================
        // TEXTO DO LIVRO
        //
        // É exibido sem título, logo após Quantity, como solicitado. O
        // texto já chega sem as tags do BookMenu e usa o mesmo font, cores e
        // sombra aplicados às informações do painel.
        // ========================================================

        if (!info.bookText.empty())
        {
            y += 8.0f * descriptorScale;
            y += DrawWrappedItemInfoText(
                draw,
                ImVec2(position.x, y),
                panelWidth,
                white,
                info.bookText,
                alpha);
            y += 12.0f * descriptorScale;
        }

        // ========================================================
        // ATRIBUTOS FÍSICOS
        // ========================================================

        if (info.hasPhysicalStats)
        {
            y += 8.0f * descriptorScale;

            draw->AddText(
                ImVec2(position.x, y),
                gold,
                Language::Get("attributes").c_str()
            );

            y += 25.0f * descriptorScale;

            if (info.form->As<RE::TESObjectWEAP>())
            {
                AddRow(
                    Language::Get("base_damage").c_str(),
                    FormatItemStat(
                        info.damage,
                        0
                    )
                );

                AddRow(
                    Language::Get("attack_speed").c_str(),
                    FormatItemStat(
                        info.attackSpeed,
                        2
                    )
                );

                AddRow(
                    Language::Get("reach").c_str(),
                    FormatItemStat(
                        info.reach,
                        2
                    )
                );

                AddRow(
                    Language::Get("stagger").c_str(),
                    FormatItemStat(
                        info.stagger,
                        2
                    )
                );

                AddRow(
                    Language::Get("critical_damage").c_str(),
                    FormatItemStat(
                        info.criticalDamage,
                        0
                    )
                );
            }

            if (info.form->As<RE::TESObjectARMO>())
            {
                AddRow(
                    Language::Get("base_armor").c_str(),
                    FormatItemStat(
                        info.armorRating,
                        0
                    )
                );
            }

            AddRow(
                Language::Get("weight").c_str(),
                FormatItemStat(
                    info.weight,
                    1
                )
            );

            AddRow(
                Language::Get("value").c_str(),
                std::to_string(info.value)
            );

            // ====================================================
            // MELHORIA DA INSTÂNCIA
            // ====================================================

            if (info.hasItemHealth)
            {
                AddRow(
                    Language::Get("item_health_multiplier").c_str(),
                    FormatItemStat(
                        info.itemHealth,
                        2
                    )
                );
            }
        }

        // ========================================================
        // MAGIAS
        // ========================================================

        if (info.form->As<RE::SpellItem>())
        {
            y += 8.0f * descriptorScale;

            draw->AddText(
                ImVec2(position.x, y),
                gold,
                Language::Get("magic").c_str()
            );

            y += 25.0f * descriptorScale;

            AddRow(
                Language::Get("school").c_str(),
                info.magicSchool
            );

            AddRow(
                Language::Get("magicka_cost").c_str(),
                FormatItemStat(
                    info.magickaCost,
                    0
                )
            );

            AddRow(
                Language::Get("charge_time").c_str(),
                FormatItemStat(
                    info.chargeTime,
                    2
                )
            );

            AddRow(
                Language::Get("range").c_str(),
                FormatItemStat(
                    info.range,
                    1
                )
            );
        }

        // ========================================================
        // ENCANTAMENTO
        // ========================================================

        if (info.isEnchanted)
        {
            y += 12.0f * descriptorScale;

            draw->AddLine(
                ImVec2(position.x, y),
                ImVec2(
                    position.x + panelWidth,
                    y
                ),
                separatorColor,
                1.0f
            );

            y += 12.0f * descriptorScale;

            draw->AddText(
                ImVec2(position.x, y),
                gold,
                Language::Get("enchantment").c_str()
            );

            y += 25.0f * descriptorScale;

            if (!info.enchantment.name.empty())
            {
                draw->AddText(
                    ImVec2(position.x, y),
                    white,
                    info.enchantment.name.c_str()
                );

                y += rowHeight;
            }
        }

        // ========================================================
        // EFEITOS
        // ========================================================

        const auto& effects =
            info.isEnchanted
                ? info.enchantment.effects
                : info.effects;

        if (!effects.empty())
        {
            y += 10.0f * descriptorScale;

            draw->AddText(
                ImVec2(position.x, y),
                gold,
                Language::Get("effects").c_str()
            );

            y += 25.0f * descriptorScale;

            for (const auto& effect : effects)
            {
                // ====================================================
                // EFEITO DESCONHECIDO
                // ====================================================

                if (!effect.known)
                {
                    draw->AddText(
                        ImVec2(position.x, y),
                        muted,
                        Language::Get("unknown_effect").c_str()
                    );

                    y += rowHeight;
                    continue;
                }

                // ====================================================
                // NOME DO EFEITO
                // ====================================================

                draw->AddText(
                    ImVec2(position.x, y),
                    white,
                    effect.name.c_str()
                );

                y += rowHeight;

                // ====================================================
                // MAGNITUDE
                // ====================================================

                if (effect.magnitude > 0.0f)
                {
                    AddRow(
                        Language::Get("magnitude").c_str(),
                        FormatItemStat(
                            effect.magnitude,
                            1
                        )
                    );
                }

                // ====================================================
                // DURAÇÃO
                // ====================================================

                if (effect.duration > 0.0f)
                {
                    AddRow(
                        Language::Get("duration").c_str(),
                        fmt::format(
                            fmt::runtime(Language::Get("duration_seconds")),
                            FormatItemStat(
                                effect.duration,
                                0
                            )
                        )
                    );
                }

                y += 5.0f * descriptorScale;
            }
        }

        // ============================================================
        // RETORNA A ALTURA TOTAL DO CONTEÚDO
        // ============================================================

        return y - position.y + 10.0f;

    }

        
    
    static void DrawSettingsItemInfoPanel(
        const ItemInfo::Data& info,
        const RadialItem* radialItem,
        float alpha,
        const SettingsInfoPanel& panel)
    {
        if (!info.form)
            return;

        ImDrawList* draw =
            ImGui::GetForegroundDrawList();

        //const ImVec2 screen =
        //    ImGui::GetIO().DisplaySize;

        //const SettingsInfoPanel panel =
        //    GetSettingsInfoPanel(screen);

        // ========================================================
        // MARGENS INTERNAS
        // ========================================================

        constexpr float paddingX = 8.0f;
        constexpr float paddingY = 5.0f;

        const float contentWidth =
            panel.width - paddingX * 2.0f;

        const float visibleHeight =
            panel.height - paddingY * 2.0f;

        if (contentWidth <= 0.0f ||
            visibleHeight <= 0.0f)
        {
            return;
        }

        const ImVec2 viewportClipMin =
            Resolution::ToVirtual(ImVec2(0.0f, 0.0f));
        const ImVec2 viewportClipMax =
            Resolution::ToVirtual(Resolution::GetRealSize());
        draw->PushClipRect(viewportClipMin, viewportClipMax, false);

        // ========================================================
        // LIMITA O SCROLL
        // ========================================================

        const float maxScroll =
            std::max(
                0.0f,
                g_settingsInfoContentHeight -
                visibleHeight
            );

        g_settingsInfoScroll =
            std::clamp(
                g_settingsInfoScroll,
                0.0f,
                maxScroll
            );

        // ========================================================
        // ÁREA VISÍVEL
        // ========================================================

        const ImVec2 clipMin(
            panel.min.x + paddingX,
            panel.min.y + paddingY
        );

        const ImVec2 clipMax(
            panel.max.x - paddingX,
            panel.max.y - paddingY
        );

        // ========================================================
        // POSIÇÃO DO CONTEÚDO COM SCROLL
        //
        // Somente o conteúdo se movimenta.
        // Os limites do painel permanecem fixos.
        // ========================================================

        const ImVec2 contentPosition(
            clipMin.x,
            clipMin.y - g_settingsInfoScroll
        );

        // ========================================================
        // ATIVA O CLIPPING
        // ========================================================

        draw->PushClipRect(
            clipMin,
            clipMax,
            true
        );

        // ========================================================
        // DESENHA AS INFORMAÇÕES
        // ========================================================

        g_settingsInfoContentHeight =
            DrawSettingsItemInfo(
                info,
                radialItem,
                contentPosition,
                alpha,
                contentWidth
            );

        // ========================================================
        // DESATIVA O CLIPPING
        // ========================================================

        draw->PopClipRect();

        // ========================================================
        // RECALCULA O LIMITE PARA O CONTEÚDO ATUAL
        // ========================================================

        const float updatedMaxScroll =
            std::max(
                0.0f,
                g_settingsInfoContentHeight -
                visibleHeight
            );

        g_settingsInfoScroll =
            std::clamp(
                g_settingsInfoScroll,
                0.0f,
                updatedMaxScroll
            );
        draw->PopClipRect();
    }

    
    // ============================================================
    // ATUALIZA O ESTADO DO PAINEL QUANDO O ITEM MUDA
    // ============================================================

    static void UpdateSettingsItemInfoSelection(
        const RadialItem& previewItem)
    {
        const bool itemChanged =
            g_settingsInfoLastForm != previewItem.form ||
            g_settingsInfoLastUniqueID != previewItem.uniqueID ||
            g_settingsInfoLastHasUniqueID != previewItem.hasUniqueID;

        if (!itemChanged)
            return;

        // ========================================================
        // NOVO ITEM - REINICIA O SCROLL
        // ========================================================

        g_settingsInfoScroll = 0.0f;

        g_settingsInfoContentHeight = 0.0f;

        // ========================================================
        // ARMAZENA A NOVA IDENTIFICAÇÃO
        // ========================================================

        g_settingsInfoLastForm =
            previewItem.form;

        g_settingsInfoLastUniqueID =
            previewItem.uniqueID;

        g_settingsInfoLastHasUniqueID =
            previewItem.hasUniqueID;
    }

        
    static void UpdateSettingsPreviewSelection()
    {
        Config::ItemPreviewProfile previewProfile{};
        PreviewLayoutField previewField{};
        Config::ItemPreviewCategory previewCategory{};
        if (GetActivePreviewControl(previewProfile, previewField) ||
            GetActivePreviewCategory(previewCategory))
        {
            // Durante a edição do layout o preview é deliberadamente um item
            // de teste, nunca a seleção persistida do radial.
            g_settingsPreviewSelection = {};
            g_settingsPreviewSelectionActive = false;
            g_settingsPreviewLastHoverTime = 0.0;
            return;
        }

        const double currentTime =
            ImGui::GetTime();

        // ========================================================
        // ITEM SENDO ARRASTADO
        //
        // O drag tem prioridade e não deve expirar.
        // ========================================================

        if (g_settingsDrag.active &&
            g_settingsDrag.form)
        {
            g_settingsPreviewSelection.form =
                g_settingsDrag.form;

            g_settingsPreviewSelection.uniqueID =
                g_settingsDrag.uniqueID;

            g_settingsPreviewSelection.hasUniqueID =
                g_settingsDrag.hasUniqueID;

            g_settingsPreviewSelection.valid = true;

            g_settingsPreviewSelectionActive = true;

            g_settingsPreviewLastHoverTime =
                currentTime;

            return;
        }

        // ========================================================
        // MOUSE SOBRE UM ITEM
        //
        // Atualiza a seleção e reinicia o timer.
        // ========================================================

        if (g_settingsHoveredItem)
        {
            g_settingsPreviewSelection.form =
                g_settingsHoveredItem;

            g_settingsPreviewSelection.uniqueID =
                g_settingsHoveredUniqueID;

            g_settingsPreviewSelection.hasUniqueID =
                g_settingsHoveredHasUniqueID;

            g_settingsPreviewSelection.valid = true;

            g_settingsPreviewSelectionActive = true;

            g_settingsPreviewLastHoverTime =
                currentTime;

            return;
        }

        // ========================================================
        // NENHUM ITEM SELECIONADO
        // ========================================================

        if (!g_settingsPreviewSelectionActive)
            return;

        // ========================================================
        // EXPIRAÇÃO APÓS 15 SEGUNDOS
        // ========================================================

        const double elapsed =
            currentTime -
            g_settingsPreviewLastHoverTime;

        if (elapsed >= SETTINGS_PREVIEW_TIMEOUT)
        {
            g_settingsPreviewSelection = {};

            g_settingsPreviewSelectionActive = false;

            g_settingsPreviewLastHoverTime = 0.0;
        }
    }

        
    bool SettingsCloseButtonClick()
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return false;

        // Click handling is outside the virtual render frame.
        const ImVec2 screen = Resolution::GetVirtualSize();

        // Mesma posição usada no DrawGoldenCircle.
        const WheelLayout wheelLayout = GetWheelLayout();
        ImVec2 buttonCenter = GetSettingsCloseButtonTarget(wheelLayout);
        if (g_settingsCloseButtonAnimatedY > 0.0f)
            buttonCenter.y = g_settingsCloseButtonAnimatedY;

        constexpr float buttonRadius = 12.0f;

        const float dx =
            g_settingsMousePos.x - buttonCenter.x;

        const float dy =
            g_settingsMousePos.y - buttonCenter.y;

        // ========================================================
        // VERIFICA SE O MOUSE ESTÁ DENTRO DO BOTÃO
        // ========================================================

        const bool hovered =
            dx * dx + dy * dy <=
            buttonRadius * buttonRadius;

        if (!hovered)
            return false;

        // ========================================================
        // FECHA O SETTINGS
        // ========================================================

        SettingsMenu::WheelSettingsMenu::Close();

        // Limpa a seleção temporária e o scroll da descrição.
        ResetSettingsItemInfo();

        // Invalida as informações do preview.
        ItemInfo::InvalidatePreviewCache();

        return true;
    }

        
    // ============================================================
    // SETTINGS - CLIQUE NO BOTÃO WHEEL KEY
    // ============================================================

    bool SettingsWheelKeyClick()
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return false;

        if (g_settingsSection != SettingsSection::Settings)
            return false;

        const ImVec2 mouse =
            g_settingsMousePos;

        const bool primaryHovered =
            mouse.x >= g_wheelKeyButtonMin.x &&
            mouse.x <= g_wheelKeyButtonMax.x &&
            mouse.y >= g_wheelKeyButtonMin.y &&
            mouse.y <= g_wheelKeyButtonMax.y;

        const bool secondaryHovered =
            mouse.x >= g_secondaryKeyButtonMin.x &&
            mouse.x <= g_secondaryKeyButtonMax.x &&
            mouse.y >= g_secondaryKeyButtonMin.y &&
            mouse.y <= g_secondaryKeyButtonMax.y;

        const bool altConfigHovered =
            mouse.x >= g_altConfigKeyButtonMin.x &&
            mouse.x <= g_altConfigKeyButtonMax.x &&
            mouse.y >= g_altConfigKeyButtonMin.y &&
            mouse.y <= g_altConfigKeyButtonMax.y;

        const bool secondaryResetHovered =
            mouse.x >= g_secondaryKeyResetHitbox.min.x &&
            mouse.x <= g_secondaryKeyResetHitbox.max.x &&
            mouse.y >= g_secondaryKeyResetHitbox.min.y &&
            mouse.y <= g_secondaryKeyResetHitbox.max.y;
        if (secondaryResetHovered)
        {
            Config::g_secondaryKey = 0;
            g_waitingWheelKey = false;
            g_waitingWheelKeySlot = 0;
            Config::SaveConfig();
            return true;
        }

        const bool altConfigResetHovered =
            mouse.x >= g_altConfigKeyResetHitbox.min.x &&
            mouse.x <= g_altConfigKeyResetHitbox.max.x &&
            mouse.y >= g_altConfigKeyResetHitbox.min.y &&
            mouse.y <= g_altConfigKeyResetHitbox.max.y;
        if (altConfigResetHovered)
        {
            Config::g_altConfigKey = 0;
            g_waitingWheelKey = false;
            g_waitingWheelKeySlot = 0;
            Config::SaveConfig();
            return true;
        }

        const float autoDx = mouse.x - g_automaticArrowButtonCenter.x;
        const float autoDy = mouse.y - g_automaticArrowButtonCenter.y;
        const bool automaticHovered =
            autoDx * autoDx + autoDy * autoDy <=
                g_automaticArrowButtonRadius * g_automaticArrowButtonRadius;

        if (automaticHovered)
        {
            Config::g_automaticArrowMenus = !Config::g_automaticArrowMenus;
            Config::SaveConfig();
            return true;
        }


        if (!primaryHovered && !secondaryHovered && !altConfigHovered)
        {
            return false;
        }

        // Começa a aguardar a próxima tecla.
        g_waitingWheelKey = true;
        g_waitingWheelKeySlot = altConfigHovered ? 3 : (secondaryHovered ? 2 : 1);

        return true;
    }
    
    // ============================================================
    // SETTINGS - CLIQUE NO gameplay desc
    // ============================================================
    bool SettingsGameplayDescriptionClick()
    {
        if (!SettingsMenu::WheelSettingsMenu::IsOpen())
            return false;

        if (g_settingsSection != SettingsSection::Gameplay &&
            g_settingsSection != SettingsSection::Layout)
            return false;

        const auto insideRect = [&](const ImVec2& min, const ImVec2& max) {
            return g_settingsMousePos.x >= min.x && g_settingsMousePos.x <= max.x &&
                g_settingsMousePos.y >= min.y && g_settingsMousePos.y <= max.y;
        };

        if (g_settingsSection == SettingsSection::Layout)
        {
            const float previewDx = g_settingsMousePos.x - g_gameplayPreviewButtonCenter.x;
            const float previewDy = g_settingsMousePos.y - g_gameplayPreviewButtonCenter.y;
            const float previewRadius = g_gameplayPreviewButtonRadius + 3.0f;
        
            if (previewDx * previewDx + previewDy * previewDy <= previewRadius * previewRadius)
            {
                Config::g_showItemPreviewGameplay = !Config::g_showItemPreviewGameplay;
                Config::SaveConfig();
                return true;
            }
        
            const float infoDx = g_settingsMousePos.x - g_gameplayDescriptionButtonCenter.x;
            const float infoDy = g_settingsMousePos.y - g_gameplayDescriptionButtonCenter.y;
            const float infoRadius = g_gameplayDescriptionButtonRadius + 3.0f;
        
            if (infoDx * infoDx + infoDy * infoDy <= infoRadius * infoRadius)
            {
                Config::g_showGameplayDescription = !Config::g_showGameplayDescription;
                Config::SaveConfig();
                return true;
            }
        
            const float quantityDx = g_settingsMousePos.x - g_showItemQuantityButtonCenter.x;
            const float quantityDy = g_settingsMousePos.y - g_showItemQuantityButtonCenter.y;
            const float quantityRadius = g_showItemQuantityButtonRadius + 3.0f;
        
            if (quantityDx * quantityDx + quantityDy * quantityDy <=
                quantityRadius * quantityRadius)
            {
                Config::g_showItemQuantity = !Config::g_showItemQuantity;
                Config::SaveConfig();
                return true;
            }
            const float overflowIconDx = g_settingsMousePos.x - g_showOverflowIconButtonCenter.x;
            const float overflowIconDy = g_settingsMousePos.y - g_showOverflowIconButtonCenter.y;
            const float overflowIconRadius = g_showOverflowIconButtonRadius + 3.0f;
        
            if (overflowIconDx * overflowIconDx + overflowIconDy * overflowIconDy <=
                overflowIconRadius * overflowIconRadius)
            {
                Config::g_showOverflowIcon = !Config::g_showOverflowIcon;
                Config::SaveConfig();
                return true;
            }
        
            const float stardustDx = g_settingsMousePos.x - g_stardustButtonCenter.x;
            const float stardustDy = g_settingsMousePos.y - g_stardustButtonCenter.y;
            const float stardustRadius = g_stardustButtonRadius + 3.0f;
        
            if (stardustDx * stardustDx + stardustDy * stardustDy <=
                stardustRadius * stardustRadius)
            {
                Config::g_stardustEnabled = !Config::g_stardustEnabled;
                if (!Config::g_stardustEnabled)
                    g_radialParticles.clear();
                Config::SaveConfig();
                return true;
            }
            return false;
        }

        if (insideRect(g_radialShapeButtonMin, g_radialShapeButtonMax))
        {
            g_radialShapeSliderDragging = true;
            return true;
        }
        if (insideRect(g_radialAnimationButtonMin, g_radialAnimationButtonMax))
        {
            g_radialAnimationSliderDragging = true;
            return true;
        }
        if (insideRect(g_trackModeButtonMin, g_trackModeButtonMax))
        {
            Config::g_customRadial = !Config::g_customRadial;
            Config::SaveConfig();
            return true;
        }
        if (insideRect(g_trackEditorButtonMin, g_trackEditorButtonMax))
        {
            TrackEditor::Open();
            return true;
        }

        if (g_languageListOpen)
        {
            for (const auto& option : g_languageOptionHitboxes)
            {
        
                if (!insideRect(option.min, option.max))
                    continue;
        
                    if (Language::SetCurrent(option.language))
                {
                    Config::g_language = Language::GetCurrentName();
                    Config::SaveConfig();
                }
        
                g_languageListOpen = false;
        
                return true;
            }
        }

        if (insideRect(g_languageButtonMin, g_languageButtonMax))
        {
        
            Language::RefreshAvailableLanguages();
            g_languageListOpen = !g_languageListOpen;
        
            return true;
        }

        std::array<bool*, 4> blurScopes{
            &Config::g_blurTop,
            &Config::g_blurCentral,
            &Config::g_blurBottom,
            &Config::g_blurDraw
        };
        for (std::size_t i = 0; i < blurScopes.size(); ++i)
        {
        
            const float dx = g_settingsMousePos.x - g_blurScopeCenters[i].x;
            const float dy = g_settingsMousePos.y - g_blurScopeCenters[i].y;
            const float radius = g_blurScopeRadius + 3.0f;
        
            if (dx * dx + dy * dy <= radius * radius)
            {
                *blurScopes[i] = !*blurScopes[i];
                Config::SaveConfig();
                return true;
            }
        }

        if (insideRect(g_slowTimeSliderMin, g_slowTimeSliderMax))
        {
            g_slowTimeSliderDragging = true;
            return true;
        }
        
        if (insideRect(g_slowTimeResetHitbox.min, g_slowTimeResetHitbox.max))
        {
        
            Config::g_slowTimeMultiplier = 0.15f;
            Config::SaveConfig();
        
            return true;
        }
        
        {
            std::array<bool*, 4> scopes{
                &Config::g_slowTimeTop,
                &Config::g_slowTimeCentral,
                &Config::g_slowTimeBottom,
                &Config::g_slowTimeDraw
            };
            for (std::size_t i = 0; i < scopes.size(); ++i)
            {
                const float dx = g_settingsMousePos.x - g_slowTimeScopeCenters[i].x;
                const float dy = g_settingsMousePos.y - g_slowTimeScopeCenters[i].y;
                const float radius = g_slowTimeScopeRadius + 3.0f;
        
                if (dx * dx + dy * dy <= radius * radius)
                {
                    *scopes[i] = !*scopes[i];
                    Config::SaveConfig();
                    return true;
                }
            }
        }

        const float fastDragDx = g_settingsMousePos.x - g_fastDragButtonCenter.x;
        
        const float fastDragDy = g_settingsMousePos.y - g_fastDragButtonCenter.y;
        
        const float fastDragRadius = g_fastDragButtonRadius + 3.0f;
        
        if (fastDragDx * fastDragDx + fastDragDy * fastDragDy <=
            fastDragRadius * fastDragRadius)
        {
        
            Config::g_fastInventoryDrag = !Config::g_fastInventoryDrag;
            Config::SaveConfig();
        
            return true;
        }



        const bool customIconsAvailable = IconCustom::HasValidConfiguration();
        
        const float reloadDx = g_settingsMousePos.x - g_customIconsReloadCenter.x;
        const float reloadDy = g_settingsMousePos.y - g_customIconsReloadCenter.y;
        const float reloadRadius = g_customIconsReloadRadius + 3.0f;
    
        if (customIconsAvailable && reloadDx * reloadDx + reloadDy * reloadDy <=
            reloadRadius * reloadRadius)
        {
    
            const bool stillValid = IconCustom::Reload();
    
            if (!stillValid)
                Config::SetCustomIconsPreference(false);
            Config::SaveConfig();
    
            return true;
        }

        std::array<bool*, 4> iconSettings{
            &Config::g_customIcons,
            &Config::g_coloredPotions,
            &Config::g_coloredMagicSchools,
            &Config::g_coloredItemEnchants
        };
    
        for (std::size_t i = 0; i < iconSettings.size(); ++i)
        {
            if (i == 0 && !customIconsAvailable)
                continue;
    
            const float iconDx = g_settingsMousePos.x - g_gameplayIconButtonCenters[i].x;
            const float iconDy = g_settingsMousePos.y - g_gameplayIconButtonCenters[i].y;
            const float radius = g_gameplayIconButtonRadius + 3.0f;
    
            if (iconDx * iconDx + iconDy * iconDy <= radius * radius)
            {
    
                if (i == 0)
                    Config::SetCustomIconsPreference(!Config::g_customIcons);
                else
                    *iconSettings[i] = !*iconSettings[i];
                Config::SaveConfig();
    
                return true;
            }
        }

        g_languageListOpen = false;
        g_radialShapeListOpen = false;
        g_radialAnimationListOpen = false;
    
        return false;
    }

    // ============================================================
    // SETTINGS - PAINEL GERAL
    // ============================================================

    static void DrawSettingsGeneralPanel(float alpha)
    {
        ImDrawList* draw =
            ImGui::GetForegroundDrawList();
        bool generalSliderResetTooltip = false;

        const ImVec2 screen =
            ImGui::GetIO().DisplaySize;

        const SettingsInfoPanel panel =
            GetSettingsInfoPanel(screen);

        // ========================================================
        // ÁREA DO PAINEL
        // ========================================================

        constexpr float padding = 10.0f;

        const float x =
            panel.min.x + padding;

        const float availableWidth =
            panel.width - padding * 2.0f;

        if (availableWidth <= 0.0f)
            return;

        const float controlScale = 1.0f +
            (std::clamp(Config::g_fontSizeScale, 1.0f, 2.5f) - 1.0f) * (2.0f / 3.0f);

        const float panelHeaderY = panel.min.y + padding;
    
        const float contentTop = panelHeaderY + 29.0f * controlScale;
    
        const float contentBottom =
            GetSettingsPanelContentBottom(panel, padding);
    
        const float y = panelHeaderY - g_settingsPanelScroll;
    
        const float scrollbarX = panel.max.x + 6.0f * controlScale;
    
        g_settingsScrollbarHitbox.min = ImVec2(
            scrollbarX - 8.0f * controlScale, contentTop);
        g_settingsScrollbarHitbox.max = ImVec2(
            scrollbarX + 8.0f * controlScale, contentBottom);
    
        if (g_settingsScrollbarDragging && g_settingsPanelMaxScroll > 0.0f)
        {
            const float normalized = std::clamp(
                (g_settingsMousePos.y - contentTop) /
                    std::max(contentBottom - contentTop, 1.0f),
                0.0f, 1.0f);
            g_settingsPanelScroll = normalized * g_settingsPanelMaxScroll;
        }
    
        draw->PushClipRect(
            ImVec2(panel.min.x, contentTop),
            ImVec2(panel.max.x, contentBottom), true);

        const ImU32 gold =
            FadeColor(
                IM_COL32(215, 195, 150, 255),
                alpha
            );

        const ImU32 white =
            FadeColor(
                IM_COL32(235, 230, 215, 255),
                alpha
            );

        // ========================================================
        // TÍTULO
        // ========================================================

        if (g_settingsSectionMorphT < 0.55f)
        {
            DrawTextWithShadow(
                draw,
                ImVec2(x, y),
                gold,
                Language::Get("settings").c_str(),
                alpha
            );
        }

        // ========================================================
        // SEPARADOR
        // ========================================================

        const float separatorY =
            panelHeaderY + 29.0f * controlScale;

        draw->AddLine(
            ImVec2(x, separatorY),
            ImVec2(
                x + availableWidth,
                separatorY
            ),
            FadeColor(
                IM_COL32(215, 195, 150, 90),
                alpha
            ),
            1.0f
        );

        const ImVec2 mouse = g_settingsMousePos;

        // ========================================================
        // LABEL - WHEEL KEY
        // ========================================================

        const float labelY = separatorY + 18.0f * controlScale - g_settingsPanelScroll;

        const char* wheelKeyLabel = Language::Get("first_key").c_str();
        const ImVec2 wheelKeyLabelSize = ImGui::CalcTextSize(wheelKeyLabel);
        DrawTextWithShadow(
            draw,
            ImVec2(
                x + (availableWidth - wheelKeyLabelSize.x) * 0.5f,
                labelY),
            white,
            wheelKeyLabel,
            alpha
        );

        // ========================================================
        // BOTÃO DA TECLA
        // ========================================================

        const float buttonY =
            labelY + 27.0f * controlScale;

        const float buttonHeight = std::max(34.0f * controlScale, ImGui::GetFontSize() + 14.0f);

        g_wheelKeyButtonMin = ImVec2(
            x,
            buttonY
        );

        g_wheelKeyButtonMax = ImVec2(
            x + availableWidth,
            buttonY + buttonHeight
        );

        const bool hovered =
            mouse.x >= g_wheelKeyButtonMin.x &&
            mouse.x <= g_wheelKeyButtonMax.x &&
            mouse.y >= g_wheelKeyButtonMin.y &&
            mouse.y <= g_wheelKeyButtonMax.y;

        // ========================================================
        // FUNDO DO BOTÃO
        // ========================================================

        draw->AddRectFilled(
            g_wheelKeyButtonMin,
            g_wheelKeyButtonMax,
            FadeColor(
                g_waitingWheelKey && g_waitingWheelKeySlot == 1
                    ? IM_COL32(85, 75, 55, 215)
                    : hovered
                        ? IM_COL32(65, 65, 70, 235)
                        : IM_COL32(20, 20, 25, 235),
                alpha
            ),
            5.0f
        );

        // ========================================================
        // BORDA
        // ========================================================

        draw->AddRect(
            g_wheelKeyButtonMin,
            g_wheelKeyButtonMax,
            FadeColor(
                g_waitingWheelKey && g_waitingWheelKeySlot == 1
                    ? IM_COL32(215, 195, 150, 235)
                    : IM_COL32(
                        255,
                        255,
                        255,
                        hovered ? 190 : 85
                    ),
                alpha
            ),
            5.0f,
            0,
            1.5f
        );

        // ========================================================
        // TEXTO DA TECLA
        // ========================================================

        const std::string buttonText =
            g_waitingWheelKey && g_waitingWheelKeySlot == 1
                ? Language::Get("waiting_key")
                : Config::KeyToString(
                    Config::g_toggleKey
                );

        const ImVec2 textSize =
            ImGui::CalcTextSize(
                buttonText.c_str()
            );

        const ImVec2 textPos(
            (
                g_wheelKeyButtonMin.x +
                g_wheelKeyButtonMax.x -
                textSize.x
            ) * 0.5f,

            (
                g_wheelKeyButtonMin.y +
                g_wheelKeyButtonMax.y -
                textSize.y
            ) * 0.5f
        );

        DrawTextWithShadow(
            draw,
            textPos,
            (g_waitingWheelKey && g_waitingWheelKeySlot == 1) ? gold : white,
            buttonText.c_str(),
            alpha
        );

        const float secondLabelY = g_wheelKeyButtonMax.y + 13.0f * controlScale;
    
        const char* secondKeyLabel = Language::Get("second_key").c_str();
    
        const ImVec2 secondKeyLabelSize = ImGui::CalcTextSize(secondKeyLabel);
    
        DrawTextWithShadow(draw,
            ImVec2(x + (availableWidth - secondKeyLabelSize.x) * 0.5f, secondLabelY),
            white, secondKeyLabel, alpha);

        const float secondButtonY = secondLabelY + 27.0f * controlScale;
        const float secondaryResetArea = 28.0f * controlScale;
    
        g_secondaryKeyButtonMin = ImVec2(x, secondButtonY);
    
        g_secondaryKeyButtonMax = ImVec2(
            x + availableWidth - secondaryResetArea, secondButtonY + buttonHeight);
    
        const ImVec2 secondaryResetCenter(
            x + availableWidth - 7.0f * controlScale,
            secondButtonY + buttonHeight * 0.5f);
    
        g_secondaryKeyResetHitbox.min = ImVec2(
            secondaryResetCenter.x - 10.0f * controlScale,
            secondaryResetCenter.y - 10.0f * controlScale);
    
        g_secondaryKeyResetHitbox.max = ImVec2(
            secondaryResetCenter.x + 10.0f * controlScale,
            secondaryResetCenter.y + 10.0f * controlScale);
    
        const bool secondaryHovered =
            mouse.x >= g_secondaryKeyButtonMin.x && mouse.x <= g_secondaryKeyButtonMax.x &&
            mouse.y >= g_secondaryKeyButtonMin.y && mouse.y <= g_secondaryKeyButtonMax.y;
    
        const bool waitingSecondary = g_waitingWheelKey && g_waitingWheelKeySlot == 2;
    
        draw->AddRectFilled(g_secondaryKeyButtonMin, g_secondaryKeyButtonMax,
            FadeColor(waitingSecondary ? IM_COL32(85, 75, 55, 215) :
                secondaryHovered ? IM_COL32(65, 65, 70, 235) : IM_COL32(20, 20, 25, 235), alpha), 5.0f);
    
        draw->AddRect(g_secondaryKeyButtonMin, g_secondaryKeyButtonMax,
            FadeColor(waitingSecondary ? IM_COL32(215, 195, 150, 235) :
                IM_COL32(255, 255, 255, secondaryHovered ? 190 : 85), alpha), 5.0f, 0, 1.5f);
    
        const std::string secondButtonText = waitingSecondary
            ? Language::Get("waiting_key")
            : Config::g_secondaryKey != 0
                ? Config::KeyToString(Config::g_secondaryKey)
                : Language::Get("none");
    
        const ImVec2 secondTextSize = ImGui::CalcTextSize(secondButtonText.c_str());
    
        DrawTextWithShadow(draw,
            ImVec2((g_secondaryKeyButtonMin.x + g_secondaryKeyButtonMax.x - secondTextSize.x) * 0.5f,
                (g_secondaryKeyButtonMin.y + g_secondaryKeyButtonMax.y - secondTextSize.y) * 0.5f),
            waitingSecondary ? gold : white, secondButtonText.c_str(), alpha);
    
        const bool secondaryResetHovered =
            mouse.x >= g_secondaryKeyResetHitbox.min.x &&
            mouse.x <= g_secondaryKeyResetHitbox.max.x &&
            mouse.y >= g_secondaryKeyResetHitbox.min.y &&
            mouse.y <= g_secondaryKeyResetHitbox.max.y;
    
        const int secondaryResetShade = secondaryResetHovered ? 245 : 65;
    
    
        generalSliderResetTooltip = generalSliderResetTooltip || secondaryResetHovered;
        
        draw->AddCircleFilled(secondaryResetCenter, 5.0f * controlScale,
            FadeColor(IM_COL32(secondaryResetShade, secondaryResetShade,
                secondaryResetShade, 255), alpha), 20);
        draw->AddCircle(secondaryResetCenter, 6.5f * controlScale,
            FadeColor(IM_COL32(135, 132, 126, secondaryResetHovered ? 220 : 120), alpha),
            20, 1.0f);

        const float altLabelY = g_secondaryKeyButtonMax.y + 13.0f * controlScale;
        
        const char* altConfigLabel = Language::Get("alt_config_key").c_str();
        
        const ImVec2 altConfigLabelSize = ImGui::CalcTextSize(altConfigLabel);
        
        DrawTextWithShadow(draw,
            ImVec2(x + (availableWidth - altConfigLabelSize.x) * 0.5f, altLabelY),
            white, altConfigLabel, alpha);
        
        const float altButtonY = altLabelY + 27.0f * controlScale;
        
        g_altConfigKeyButtonMin = ImVec2(x, altButtonY);
        g_altConfigKeyButtonMax = ImVec2(x + availableWidth - secondaryResetArea,
            altButtonY + buttonHeight);
        
        const ImVec2 altResetCenter(x + availableWidth - 7.0f * controlScale,
            altButtonY + buttonHeight * 0.5f);
        
        g_altConfigKeyResetHitbox.min = ImVec2(altResetCenter.x - 10.0f * controlScale,
            altResetCenter.y - 10.0f * controlScale);
        g_altConfigKeyResetHitbox.max = ImVec2(altResetCenter.x + 10.0f * controlScale,
            altResetCenter.y + 10.0f * controlScale);
        
        const bool altHovered = mouse.x >= g_altConfigKeyButtonMin.x &&
            mouse.x <= g_altConfigKeyButtonMax.x && mouse.y >= g_altConfigKeyButtonMin.y &&
            mouse.y <= g_altConfigKeyButtonMax.y;
        
        const bool waitingAlt = g_waitingWheelKey && g_waitingWheelKeySlot == 3;
        
        draw->AddRectFilled(g_altConfigKeyButtonMin, g_altConfigKeyButtonMax,
            FadeColor(waitingAlt ? IM_COL32(85, 75, 55, 215) :
                altHovered ? IM_COL32(65, 65, 70, 235) : IM_COL32(20, 20, 25, 235), alpha), 5.0f);
        
        draw->AddRect(g_altConfigKeyButtonMin, g_altConfigKeyButtonMax,
            FadeColor(waitingAlt ? IM_COL32(215, 195, 150, 235) :
                IM_COL32(255, 255, 255, altHovered ? 190 : 85), alpha), 5.0f, 0, 1.5f);
        
        const std::string altButtonText = waitingAlt ? Language::Get("waiting_key") :
            (Config::g_altConfigKey != 0 ? Config::KeyToString(Config::g_altConfigKey) : Language::Get("none"));
        
        const ImVec2 altTextSize = ImGui::CalcTextSize(altButtonText.c_str());
        
        DrawTextWithShadow(draw, ImVec2((g_altConfigKeyButtonMin.x + g_altConfigKeyButtonMax.x - altTextSize.x) * 0.5f,
            (g_altConfigKeyButtonMin.y + g_altConfigKeyButtonMax.y - altTextSize.y) * 0.5f),
            waitingAlt ? gold : white, altButtonText.c_str(), alpha);
        
        const bool altResetHovered = mouse.x >= g_altConfigKeyResetHitbox.min.x &&
            mouse.x <= g_altConfigKeyResetHitbox.max.x && mouse.y >= g_altConfigKeyResetHitbox.min.y &&
            mouse.y <= g_altConfigKeyResetHitbox.max.y;
        
        generalSliderResetTooltip = generalSliderResetTooltip || altResetHovered;
        
        
        const int altResetShade = altResetHovered ? 245 : 65;
        
        generalSliderResetTooltip = generalSliderResetTooltip || altResetHovered;
        
        
        draw->AddCircleFilled(altResetCenter, 5.0f * controlScale,
            FadeColor(IM_COL32(altResetShade, altResetShade, altResetShade, 255), alpha), 20);
        draw->AddCircle(altResetCenter, 6.5f * controlScale,
            FadeColor(IM_COL32(135, 132, 126, altResetHovered ? 220 : 120), alpha), 20, 1.0f);

        const float automaticOptionY =
            g_altConfigKeyButtonMax.y + 25.0f * controlScale;
        
        g_automaticArrowButtonRadius = 10.0f * controlScale;    
        g_automaticArrowButtonCenter = ImVec2(x + g_automaticArrowButtonRadius, automaticOptionY);
    
        const float autoDx = mouse.x - g_automaticArrowButtonCenter.x;
    
        const float autoDy = mouse.y - g_automaticArrowButtonCenter.y;
    
        const bool automaticHovered = autoDx * autoDx + autoDy * autoDy <=
            g_automaticArrowButtonRadius * g_automaticArrowButtonRadius;
    
        const ImU32 automaticBackground = Config::g_automaticArrowMenus
            ? IM_COL32(235, 235, 235, 140) : IM_COL32(20, 20, 25, 235);
        
        draw->AddCircleFilled(g_automaticArrowButtonCenter,
            g_automaticArrowButtonRadius, FadeColor(automaticBackground, alpha), 40);
        
        draw->AddCircle(g_automaticArrowButtonCenter,
            g_automaticArrowButtonRadius + (automaticHovered ? 2.0f : 0.0f),
            FadeColor(IM_COL32(255, 255, 255, automaticHovered ? 210 : 100), alpha), 40, 1.5f);
        
        DrawTextWithShadow(draw,
            ImVec2(g_automaticArrowButtonCenter.x + g_automaticArrowButtonRadius + 12.0f,
                automaticOptionY - ImGui::GetFontSize() * 0.5f),
            white, Language::Get("automatic_arrow_menus").c_str(), alpha);


        struct GeneralSliderData
        {
            const char* label;
            float* value;
            float minimum;
            float maximum;
            const char* format;
        };
        GeneralSliderData sliders[] = {
            { Language::Get("mouse_sensitivity").c_str(), &Config::g_sideMouseSensitivity, 0.25f, 3.0f, "{:.2f}x" },
            { Language::Get("mouse_smooth").c_str(), &Config::g_sideMouseSmooth, 0.0f, 100.0f, "{:.0f}%" },
            { Language::Get("analog_sensitivity").c_str(), &Config::g_gamepadAnalogSensitivity, 0.25f, 3.0f, "{:.2f}x" },
            { Language::Get("analog_smooth").c_str(), &Config::g_gamepadAnalogSmooth, 0.0f, 100.0f, "{:.0f}%" }
        };

        const float rowHeight = std::max(48.0f * controlScale, ImGui::GetFontSize() + 27.0f * controlScale);
        
        const float trackHeight = 13.0f * controlScale;
        
        const float resetAreaWidth = 24.0f * controlScale;
        
        const float trackWidth = availableWidth - resetAreaWidth;
        
        const float slidersTop = automaticOptionY + 34.0f * controlScale;
        
        for (std::size_t i = 0; i < std::size(sliders); ++i)
        {
        
            const float rowY = slidersTop + rowHeight * static_cast<float>(i);
        
            const float trackY = rowY + ImGui::GetFontSize() + 9.0f * controlScale;
        
            auto& hitbox = g_generalSliderHitboxes[i];
        
            hitbox.min = ImVec2(x, trackY - trackHeight * 0.5f - 4.0f * controlScale);
        
            hitbox.max = ImVec2(x + trackWidth, trackY + trackHeight * 0.5f + 4.0f * controlScale);
        
            const ImVec2 resetCenter(x + availableWidth - 6.0f * controlScale, trackY);
        
            auto& resetHitbox = g_generalResetHitboxes[i];
        
            resetHitbox.min = ImVec2(resetCenter.x - 10.0f * controlScale, resetCenter.y - 10.0f * controlScale);
        
            resetHitbox.max = ImVec2(resetCenter.x + 10.0f * controlScale, resetCenter.y + 10.0f * controlScale);

            if (g_activeGeneralSlider == static_cast<GeneralSlider>(i))
            {
                const float normalized = std::clamp(
                    (g_settingsMousePos.x - x) / std::max(trackWidth, 1.0f), 0.0f, 1.0f);
        
                *sliders[i].value = sliders[i].minimum +
                    (sliders[i].maximum - sliders[i].minimum) * normalized;
        
                std::size_t changedPreviewCategory = 0;
        
                if (GetPreviewCategorySliderInfo(
                        static_cast<LayoutSlider>(i), changedPreviewCategory))
                    ItemPreview::InvalidateSizeScale();
            }

            const bool sliderHovered =
                g_settingsMousePos.x >= hitbox.min.x && g_settingsMousePos.x <= hitbox.max.x &&
                g_settingsMousePos.y >= hitbox.min.y && g_settingsMousePos.y <= hitbox.max.y;
        
            const float normalized = std::clamp(
                (*sliders[i].value - sliders[i].minimum) /
                    std::max(sliders[i].maximum - sliders[i].minimum, 0.001f), 0.0f, 1.0f);
        
            const float fillX = x + trackWidth * normalized;

        
            DrawTextWithShadow(draw, ImVec2(x, rowY), white, sliders[i].label, alpha);
        
            const std::string sliderValue = std::vformat(sliders[i].format,
                std::make_format_args(*sliders[i].value));
        
            const ImVec2 sliderValueSize = ImGui::CalcTextSize(sliderValue.c_str());
        
            DrawTextWithShadow(draw, ImVec2(x + availableWidth - sliderValueSize.x, rowY),
                FadeColor(IM_COL32(180, 165, 130, 230), alpha), sliderValue.c_str(), alpha);

            draw->AddRectFilled(ImVec2(x, trackY - trackHeight * 0.5f),
                ImVec2(x + trackWidth, trackY + trackHeight * 0.5f),
                FadeColor(IM_COL32(28, 29, 34, 235), alpha), 4.0f);
        
            draw->AddRectFilled(ImVec2(x, trackY - trackHeight * 0.5f),
                ImVec2(fillX, trackY + trackHeight * 0.5f),
                FadeColor(sliderHovered ? IM_COL32(235, 230, 215, 225) : IM_COL32(190, 184, 170, 205), alpha),
                4.0f);
        
            draw->AddRect(ImVec2(x, trackY - trackHeight * 0.5f),
                ImVec2(x + trackWidth, trackY + trackHeight * 0.5f),
                FadeColor(IM_COL32(115, 112, 106, sliderHovered ? 210 : 135), alpha), 4.0f, 0, 1.0f);
        
            const bool resetHovered =
                g_settingsMousePos.x >= resetHitbox.min.x && g_settingsMousePos.x <= resetHitbox.max.x &&
                g_settingsMousePos.y >= resetHitbox.min.y && g_settingsMousePos.y <= resetHitbox.max.y;
            generalSliderResetTooltip = generalSliderResetTooltip || resetHovered;
        
            const int resetShade = resetHovered ? 235 : 60;
        
            draw->AddCircleFilled(resetCenter, 5.0f * controlScale,
                FadeColor(IM_COL32(resetShade, resetShade, resetShade, 255), alpha), 20);
        
            draw->AddCircle(resetCenter, 6.5f * controlScale,
                FadeColor(IM_COL32(120, 120, 120, resetHovered ? 220 : 110), alpha), 20, 1.0f);
        }

        
        const float resetSeparatorY = slidersTop +
            rowHeight * static_cast<float>(std::size(sliders)) + 8.0f * controlScale;
        
        draw->AddLine(ImVec2(x, resetSeparatorY), ImVec2(x + availableWidth, resetSeparatorY),
            FadeColor(IM_COL32(215, 195, 150, 90), alpha), 1.0f);
        
        const float resetAllRadius = 8.0f * controlScale;
        
        const ImVec2 resetAllCenter(
            x + availableWidth * 0.5f,
            resetSeparatorY + 28.0f * controlScale);
        
        g_resetAllConfigHitbox.min = ImVec2(resetAllCenter.x - 13.0f * controlScale,
            resetAllCenter.y - 13.0f * controlScale);
        g_resetAllConfigHitbox.max = ImVec2(resetAllCenter.x + 13.0f * controlScale,
            resetAllCenter.y + 13.0f * controlScale);
        
        const bool resetAllHovered =
            mouse.x >= g_resetAllConfigHitbox.min.x && mouse.x <= g_resetAllConfigHitbox.max.x &&
            mouse.y >= g_resetAllConfigHitbox.min.y && mouse.y <= g_resetAllConfigHitbox.max.y;
        
        const int resetAllShade = resetAllHovered ? 245 : 65;
        
        draw->AddCircleFilled(resetAllCenter, resetAllRadius,
            FadeColor(IM_COL32(resetAllShade, resetAllShade, resetAllShade, 255), alpha), 24);
        draw->AddCircle(resetAllCenter, resetAllRadius + 2.0f * controlScale,
            FadeColor(IM_COL32(145, 145, 145, resetAllHovered ? 230 : 120), alpha), 24, 1.2f);
        if (resetAllHovered)
        {
        
            const std::string& resetLabel = Language::Get("reset_all_configuration");
            const ImVec2 resetLabelSize = ImGui::CalcTextSize(resetLabel.c_str());
        
            DrawTextWithShadow(draw,
                ImVec2(resetAllCenter.x - resetLabelSize.x * 0.5f,
                    resetAllCenter.y + resetAllRadius + 10.0f * controlScale),
                white, resetLabel.c_str(), alpha);
        }

        draw->PopClipRect();

        
        const float totalContentHeight =
            resetAllCenter.y + resetAllRadius + 55.0f * controlScale +
            g_settingsPanelScroll - contentTop;
        
        const float visibleContentHeight = std::max(
            contentBottom - contentTop, 1.0f);
        
        g_settingsPanelMaxScroll = std::max(
            0.0f, totalContentHeight - visibleContentHeight);
        
        g_settingsPanelScroll = std::clamp(
            g_settingsPanelScroll, 0.0f, g_settingsPanelMaxScroll);

        
        draw->AddRectFilled(
            ImVec2(scrollbarX - 2.0f * controlScale, contentTop),
            ImVec2(scrollbarX + 2.0f * controlScale, contentBottom),
            FadeColor(IM_COL32(30, 31, 36, 220), alpha), 3.0f * controlScale);
        
        const float thumbHeight = std::max(
            22.0f * controlScale,
            visibleContentHeight * std::clamp(
                visibleContentHeight / std::max(totalContentHeight, 1.0f),
                0.0f, 1.0f));
        
        const float thumbTravel = std::max(
            0.0f, visibleContentHeight - thumbHeight);
        
        const float scrollT = g_settingsPanelMaxScroll > 0.0f
            ? g_settingsPanelScroll / g_settingsPanelMaxScroll : 0.0f;
        
        const bool scrollbarHovered =
            g_settingsMousePos.x >= g_settingsScrollbarHitbox.min.x &&
            g_settingsMousePos.x <= g_settingsScrollbarHitbox.max.x &&
            g_settingsMousePos.y >= g_settingsScrollbarHitbox.min.y &&
            g_settingsMousePos.y <= g_settingsScrollbarHitbox.max.y;
        
        
        draw->AddRectFilled(
            ImVec2(scrollbarX - 4.0f * controlScale,
                contentTop + thumbTravel * scrollT),
            ImVec2(scrollbarX + 4.0f * controlScale,
                contentTop + thumbTravel * scrollT + thumbHeight),
            FadeColor(scrollbarHovered || g_settingsScrollbarDragging
                ? IM_COL32(235, 230, 215, 230)
                : IM_COL32(95, 94, 98, 220), alpha),
            4.0f * controlScale);

        if (generalSliderResetTooltip)
        {
        
            const char* tooltip = Language::Get("reset_value").c_str();
            const ImVec2 textSize = ImGui::CalcTextSize(tooltip);
            const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            const float tooltipPadding = 7.0f * controlScale;
        
            ImVec2 tooltipMin(
                g_settingsMousePos.x + 14.0f * controlScale,
                g_settingsMousePos.y + 14.0f * controlScale);
        
            tooltipMin.x = std::clamp(tooltipMin.x, 4.0f,
                std::max(4.0f, displaySize.x - textSize.x - tooltipPadding * 2.0f - 4.0f));
        
            tooltipMin.y = std::clamp(tooltipMin.y, 4.0f,
                std::max(4.0f, displaySize.y - textSize.y - tooltipPadding * 2.0f - 4.0f));
        
            const ImVec2 tooltipMax(
                tooltipMin.x + textSize.x + tooltipPadding * 2.0f,
                tooltipMin.y + textSize.y + tooltipPadding * 2.0f);
        
            draw->AddRectFilled(tooltipMin, tooltipMax,
                FadeColor(IM_COL32(13, 14, 18, 245), alpha), 4.0f * controlScale);
        
            draw->AddRect(tooltipMin, tooltipMax,
                FadeColor(IM_COL32(180, 176, 165, 180), alpha), 4.0f * controlScale);
        
            DrawTextWithShadow(draw,
                ImVec2(tooltipMin.x + tooltipPadding,
                    tooltipMin.y + tooltipPadding),
                FadeColor(IM_COL32(240, 237, 226, 255), alpha),
                tooltip, alpha);
        }
    }

        
    static void DrawSettingsGameplayPanel(float alpha)
    {
        const float controlScale = 1.0f +
            (std::clamp(Config::g_fontSizeScale, 1.0f, 2.5f) - 1.0f) * (2.0f / 3.0f);

        ImDrawList* draw =
            ImGui::GetForegroundDrawList();

        const ImVec2 screen =
            ImGui::GetIO().DisplaySize;

        const SettingsInfoPanel panel =
            GetSettingsInfoPanel(screen);

        constexpr float padding = 10.0f;

        const float x =
            panel.min.x + padding;

        const float y =
            panel.min.y + padding;

        const float width =
            panel.width - padding * 2.0f;

        // O tooltip é emitido depois da barra de rolagem e do conteúdo inteiro
        // do painel. Assim ele não pode ser coberto pelo clip/slider lateral.
        bool customIconsReloadTooltip = false;

        if (width <= 0.0f)
            return;

        // ========================================================
        // TÍTULO
        // ========================================================

        if (g_settingsSectionMorphT < 0.55f)
        {
            DrawTextWithShadow(
                draw,
                ImVec2(x, y),
                FadeColor(
                    IM_COL32(215, 195, 150, 255),
                    alpha
                ),
                Language::Get("gameplay").c_str(),
                alpha
            );
        }

        // ========================================================
        // SEPARADOR
        // ========================================================

        draw->AddLine(
            ImVec2(x, y + 29.0f * controlScale),
            ImVec2(x + width, y + 29.0f * controlScale),
            FadeColor(
                IM_COL32(215, 195, 150, 90),
                alpha
            ),
            1.0f
        );

        const float gameplayContentTop = y + 40.0f * controlScale;
        
        const float gameplayContentBottom =
            GetSettingsPanelContentBottom(panel, padding);
        
        const float gameplayScrollbarX = panel.max.x + 6.0f * controlScale;
        
        g_gameplayScrollbarHitbox.min = ImVec2(
            gameplayScrollbarX - 8.0f * controlScale, gameplayContentTop);
        g_gameplayScrollbarHitbox.max = ImVec2(
            gameplayScrollbarX + 8.0f * controlScale, gameplayContentBottom);
        
        if (g_gameplayScrollbarDragging && g_gameplayPanelMaxScroll > 0.0f)
        {
            const float normalized = std::clamp(
                (g_settingsMousePos.y - gameplayContentTop) /
                    std::max(gameplayContentBottom - gameplayContentTop, 1.0f),
                0.0f, 1.0f);
            g_gameplayPanelScroll = normalized * g_gameplayPanelMaxScroll;
        }
        
        draw->PushClipRect(ImVec2(panel.min.x, gameplayContentTop),
            ImVec2(panel.max.x, gameplayContentBottom), true);

        
        const float buttonRadius = 12.0f * controlScale;
        
        const ImU32 gold = FadeColor(IM_COL32(215, 195, 150, 255), alpha);
        
        const ImU32 white = FadeColor(IM_COL32(235, 230, 215, 255), alpha);

        // Gameplay layout: Language -> Animation -> Menu.
        // Keep the controls themselves untouched; only their vertical flow changes.
        
        const float languageY = y + 45.0f * controlScale - g_gameplayPanelScroll;
        
        const float languageButtonHeight = std::max(
            34.0f * controlScale, ImGui::GetFontSize() + 14.0f);
        
        const float animationTitleY =
            languageY + languageButtonHeight + 26.0f * controlScale;
        // The three Animation controls occupy a fixed vertical sequence.  This
        // mirrors drawSelector's spacing so MENU begins after that sequence,
        // without changing any selector behavior or hitbox.
        
        const float animationContentBottomY = animationTitleY +
            251.0f * controlScale + 2.0f * ImGui::GetFontSize();

        // MENU -------------------------------------------------------
        
        const float menuTitleY = animationContentBottomY + 16.0f * controlScale;
        
        DrawTextWithShadow(draw, ImVec2(x, menuTitleY), gold,
            Language::Get("menu_section").c_str(), alpha);
        
        const ImVec2 menuTitleSize = ImGui::CalcTextSize(
            Language::Get("menu_section").c_str());
        
        draw->AddLine(
            ImVec2(x + menuTitleSize.x + 12.0f * controlScale,
                menuTitleY + ImGui::GetFontSize() * 0.5f),
            ImVec2(x + width, menuTitleY + ImGui::GetFontSize() * 0.5f),
            FadeColor(IM_COL32(215, 195, 150, 90), alpha), 1.0f);

        const float blurTitleY = menuTitleY + 31.0f * controlScale;
        
        const char* blurTitle = Language::Get("blur").c_str();
        
        DrawTextWithShadow(draw,
            ImVec2(x, blurTitleY),
            white, blurTitle, alpha);
        
        const float blurScopeY = blurTitleY + 25.0f * controlScale;
        
        g_blurScopeRadius = buttonRadius * 0.5f;
        
        const float blurScopeSpacing = 20.0f * controlScale;
        const float drawScopeGap = 24.0f * controlScale;
        
        const float blurScopeStartX = x + g_blurScopeRadius;
        
        const std::array<bool, 4> blurScopeValues{
            Config::g_blurTop, Config::g_blurCentral, Config::g_blurBottom,
            Config::g_blurDraw
        };
        
        const std::array<const char*, 4> blurScopeLabels{
            "slow_time_top", "slow_time_left_right", "slow_time_bottom", "draw"
        };
        
        const char* blurScopeTooltip = nullptr;
        
        for (std::size_t i = 0; i < g_blurScopeCenters.size(); ++i)
        {
            const float scopeX = i == 3
                ? blurScopeStartX + blurScopeSpacing * 3.0f + drawScopeGap
                : blurScopeStartX + blurScopeSpacing * static_cast<float>(i);
            const ImVec2 center(scopeX, blurScopeY);
        
            g_blurScopeCenters[i] = center;
        
            const float dx = g_settingsMousePos.x - center.x;
            const float dy = g_settingsMousePos.y - center.y;
            const bool hovered = dx * dx + dy * dy <=
                g_blurScopeRadius * g_blurScopeRadius;
        
            if (hovered)
                blurScopeTooltip = Language::Get(blurScopeLabels[i]).c_str();
        
            draw->AddCircleFilled(center, g_blurScopeRadius,
                FadeColor(blurScopeValues[i]
                    ? IM_COL32(235, 235, 235, 150)
                    : IM_COL32(20, 20, 25, 235), alpha), 32);
            draw->AddCircle(center,
                g_blurScopeRadius + (hovered ? 1.5f : 0.0f),
                FadeColor(IM_COL32(255, 255, 255, hovered ? 210 : 100), alpha),
                32, 1.2f);
        }

        const float slowOptionY = blurScopeY + 34.0f * controlScale;
        const char* slowTitle = Language::Get("slowtime").c_str();
        DrawTextWithShadow(draw,
            ImVec2(x, slowOptionY),
            white, slowTitle, alpha);

        const float scopeY = slowOptionY + 25.0f * controlScale;
        
        g_slowTimeScopeRadius = buttonRadius * 0.5f;
        
        const float scopeSpacing = 20.0f * controlScale;
        
        const float scopeStartX = x + g_slowTimeScopeRadius;
        
        const std::array<bool, 4> scopeValues{
            Config::g_slowTimeTop,
            Config::g_slowTimeCentral,
            Config::g_slowTimeBottom,
            Config::g_slowTimeDraw
        };
        
        const std::array<const char*, 4> scopeLabels{
            "slow_time_top", "slow_time_left_right", "slow_time_bottom", "draw"
        };
        
        const char* slowScopeTooltip = nullptr;
        
        const float scopeControlAlpha = 1.0f;
        
        for (std::size_t i = 0; i < g_slowTimeScopeCenters.size(); ++i)
        {
            const float scopeX = i == 3
                ? scopeStartX + scopeSpacing * 3.0f + drawScopeGap
                : scopeStartX + scopeSpacing * static_cast<float>(i);
            const ImVec2 center(scopeX, scopeY);
            g_slowTimeScopeCenters[i] = center;
        
            const float dx = g_settingsMousePos.x - center.x;
            const float dy = g_settingsMousePos.y - center.y;
            const bool hovered = dx * dx + dy * dy <=
                g_slowTimeScopeRadius * g_slowTimeScopeRadius;
        
            if (hovered)
                slowScopeTooltip = Language::Get(scopeLabels[i]).c_str();
        
            draw->AddCircleFilled(center, g_slowTimeScopeRadius,
                FadeColor(scopeValues[i]
                    ? IM_COL32(235, 235, 235, 150)
                    : IM_COL32(20, 20, 25, 235),
                    alpha * scopeControlAlpha), 32);
            draw->AddCircle(center,
                g_slowTimeScopeRadius + (hovered ? 1.5f : 0.0f),
                FadeColor(IM_COL32(255, 255, 255, hovered ? 210 : 100),
                    alpha * scopeControlAlpha), 32, 1.2f);
        }

        const float slowLabelY = slowOptionY + 55.0f * controlScale;
        const float slowTrackY = slowLabelY + 27.0f * controlScale;
        const float slowTrackInset = 5.0f * controlScale;
        const float slowTrackMinX = x + slowTrackInset;
        const float slowResetAreaWidth = 27.0f * controlScale;
        const float slowTrackMaxX = x + width - slowResetAreaWidth;
        const float slowTrackWidth = std::max(slowTrackMaxX - slowTrackMinX, 1.0f);
        
        g_slowTimeSliderMin = ImVec2(x, slowTrackY - 12.0f * controlScale);
        g_slowTimeSliderMax = ImVec2(slowTrackMaxX, slowTrackY + 12.0f * controlScale);
        
        const ImVec2 slowResetCenter(
            x + width - 7.0f * controlScale, slowTrackY);
        
        g_slowTimeResetHitbox.min = ImVec2(
            slowResetCenter.x - 10.0f * controlScale,
            slowResetCenter.y - 10.0f * controlScale);
        g_slowTimeResetHitbox.max = ImVec2(
            slowResetCenter.x + 10.0f * controlScale,
            slowResetCenter.y + 10.0f * controlScale);
    
        const bool slowResetHovered =
            g_settingsMousePos.x >= g_slowTimeResetHitbox.min.x &&
            g_settingsMousePos.x <= g_slowTimeResetHitbox.max.x &&
            g_settingsMousePos.y >= g_slowTimeResetHitbox.min.y &&
            g_settingsMousePos.y <= g_slowTimeResetHitbox.max.y;
    
        if (g_slowTimeSliderDragging)
        {
            const float normalized = std::clamp(
                (g_settingsMousePos.x - slowTrackMinX) / slowTrackWidth, 0.0f, 1.0f);
            Config::g_slowTimeMultiplier = 0.1f + normalized * 0.7f;
        }
    
        Config::g_slowTimeMultiplier = std::clamp(
            Config::g_slowTimeMultiplier, 0.1f, 0.8f);
    
        const float slowControlAlpha = 1.0f;
    
        DrawTextWithShadow(draw, ImVec2(x, slowLabelY),
            FadeColor(IM_COL32(235, 230, 215, 255), alpha * slowControlAlpha),
            Language::Get("slow_time_multiplier").c_str(), alpha * slowControlAlpha);
    
        const std::string slowValue = std::format("{:.2f}x", Config::g_slowTimeMultiplier);
    
        const ImVec2 slowValueSize = ImGui::CalcTextSize(slowValue.c_str());
    
        DrawTextWithShadow(draw, ImVec2(slowTrackMaxX - slowValueSize.x, slowLabelY),
            FadeColor(IM_COL32(180, 165, 130, 230), alpha * slowControlAlpha),
            slowValue.c_str(), alpha * slowControlAlpha);
    
        const float slowNormalized = (Config::g_slowTimeMultiplier - 0.1f) / 0.7f;
    
        draw->AddRectFilled(
            ImVec2(slowTrackMinX, slowTrackY - 6.0f * controlScale),
            ImVec2(slowTrackMaxX, slowTrackY + 6.0f * controlScale),
            FadeColor(IM_COL32(25, 26, 31, 235), alpha * slowControlAlpha),
            4.0f * controlScale);
    
        draw->AddRectFilled(
            ImVec2(slowTrackMinX, slowTrackY - 6.0f * controlScale),
            ImVec2(slowTrackMinX + slowTrackWidth * slowNormalized,
                slowTrackY + 6.0f * controlScale),
            FadeColor(IM_COL32(190, 184, 170, 205), alpha * slowControlAlpha),
            4.0f * controlScale);
    
        const int slowResetShade = slowResetHovered ? 245 : 65;
    
        draw->AddCircleFilled(slowResetCenter, 5.0f * controlScale,
            FadeColor(IM_COL32(slowResetShade, slowResetShade, slowResetShade, 255),
                alpha * slowControlAlpha), 20);
    
        draw->AddCircle(slowResetCenter, 6.5f * controlScale,
            FadeColor(IM_COL32(135, 132, 126, slowResetHovered ? 220 : 120),
                alpha * slowControlAlpha), 20, 1.0f);

        if (slowScopeTooltip)
        {
            const ImVec2 tooltipSize = ImGui::CalcTextSize(slowScopeTooltip);
    
            const ImVec2 tooltipMin(
                g_settingsMousePos.x + 13.0f * controlScale,
                g_settingsMousePos.y + 13.0f * controlScale);
    
            const ImVec2 tooltipMax(
                tooltipMin.x + tooltipSize.x + 16.0f * controlScale,
                tooltipMin.y + tooltipSize.y + 10.0f * controlScale);
    
            draw->AddRectFilled(tooltipMin, tooltipMax,
                FadeColor(IM_COL32(15, 15, 20, 245), alpha),
                4.0f * controlScale);
    
            draw->AddRect(tooltipMin, tooltipMax,
                FadeColor(IM_COL32(190, 185, 170, 150), alpha),
                4.0f * controlScale);
    
            DrawTextWithShadow(draw,
                ImVec2(tooltipMin.x + 8.0f * controlScale,
                    tooltipMin.y + 5.0f * controlScale),
                white, slowScopeTooltip, alpha);
        }
        else if (blurScopeTooltip)
        {
            const ImVec2 tooltipSize = ImGui::CalcTextSize(blurScopeTooltip);
    
            const ImVec2 tooltipMin(
                g_settingsMousePos.x + 13.0f * controlScale,
                g_settingsMousePos.y + 13.0f * controlScale);
    
            const ImVec2 tooltipMax(
                tooltipMin.x + tooltipSize.x + 16.0f * controlScale,
                tooltipMin.y + tooltipSize.y + 10.0f * controlScale);
    
            draw->AddRectFilled(tooltipMin, tooltipMax,
                FadeColor(IM_COL32(15, 15, 20, 245), alpha),
                4.0f * controlScale);
    
            draw->AddRect(tooltipMin, tooltipMax,
                FadeColor(IM_COL32(190, 185, 170, 150), alpha),
                4.0f * controlScale);
    
            DrawTextWithShadow(draw,
                ImVec2(tooltipMin.x + 8.0f * controlScale,
                    tooltipMin.y + 5.0f * controlScale),
                white, blurScopeTooltip, alpha);
        }

        // Mantém as seções seguintes abaixo do slider de slowtime. Antes,
        // ICONS ainda usava uma coordenada antiga e era desenhado por cima
        // do multiplicador depois que o bloco de Blur foi adicionado.
    
        const float iconsTitleY = slowTrackY + 40.0f * controlScale;
    
        const float inventoryTitleY =
            iconsTitleY + (32.0f + 4.0f * 32.0f) * controlScale;
    
        const float fastDragOptionY = inventoryTitleY + 32.0f * controlScale;
    
        DrawTextWithShadow(draw, ImVec2(x, animationTitleY), gold,
            Language::Get("animation").c_str(), alpha);
    
        const ImVec2 animationTitleSize = ImGui::CalcTextSize(Language::Get("animation").c_str());
    
        draw->AddLine(
            ImVec2(x + animationTitleSize.x + 12.0f * controlScale,
                animationTitleY + ImGui::GetFontSize() * 0.5f),
            ImVec2(x + width, animationTitleY + ImGui::GetFontSize() * 0.5f),
            FadeColor(IM_COL32(215, 195, 150, 90), alpha), 1.0f);

        const float selectorLabelGap = 29.0f * controlScale;
    
        float selectorY = animationTitleY + 31.0f * controlScale;

        const auto drawSelector = [&](const char* label, int& selected,
            ImVec2& buttonMin, ImVec2& buttonMax, bool dragging,
            int optionCount, auto optionName) {
    
            DrawTextWithShadow(draw, ImVec2(x, selectorY), white, label, alpha);
    
            const float trackY = selectorY + selectorLabelGap;
    
            const float trackInset = 5.0f * controlScale;
            const float trackMinX = x + trackInset;
            const float trackMaxX = x + width - trackInset;
    
            const float trackWidth = std::max(trackMaxX - trackMinX, 1.0f);
    
            const float trackHalfHeight = 8.0f * controlScale;
    
            buttonMin = ImVec2(x, trackY - 12.0f * controlScale);
            buttonMax = ImVec2(x + width, trackY + 30.0f * controlScale + ImGui::GetFontSize());
    
            if (dragging)
            {
                const float normalized = std::clamp(
                    (g_settingsMousePos.x - trackMinX) / trackWidth, 0.0f, 1.0f);
                selected = std::clamp(static_cast<int>(
                    normalized * static_cast<float>(optionCount)), 0, optionCount - 1);
            }
    
            selected = std::clamp(selected, 0, optionCount - 1);
    
            const bool buttonHovered =
                g_settingsMousePos.x >= buttonMin.x && g_settingsMousePos.x <= buttonMax.x &&
                g_settingsMousePos.y >= buttonMin.y && g_settingsMousePos.y <= buttonMax.y;
    
            draw->AddRectFilled(
                ImVec2(trackMinX, trackY - trackHalfHeight),
                ImVec2(trackMaxX, trackY + trackHalfHeight),
                FadeColor(IM_COL32(25, 26, 31, 235), alpha), 4.0f * controlScale);
    
            draw->AddRect(
                ImVec2(trackMinX, trackY - trackHalfHeight),
                ImVec2(trackMaxX, trackY + trackHalfHeight),
                FadeColor(IM_COL32(145, 145, 150, buttonHovered ? 210 : 130), alpha),
                4.0f * controlScale, 0, 1.2f);
    
            const float cellWidth = trackWidth /
                static_cast<float>(std::max(optionCount, 1));
        
            const float selectedMinX = trackMinX + cellWidth * static_cast<float>(selected);
        
            const float selectedMaxX = selectedMinX + cellWidth;
        
            draw->AddRectFilled(
                ImVec2(selectedMinX + 1.5f * controlScale,
                    trackY - trackHalfHeight + 2.0f * controlScale),
                ImVec2(selectedMaxX - 1.5f * controlScale,
                    trackY + trackHalfHeight - 2.0f * controlScale),
                FadeColor(dragging || buttonHovered ? IM_COL32(245, 240, 220, 245)
                    : IM_COL32(180, 176, 165, 225), alpha), 2.0f * controlScale);
            for (int i = 0; i <= optionCount; ++i)
            {
                const float tickX = trackMinX + cellWidth * static_cast<float>(i);
                draw->AddLine(ImVec2(tickX, trackY - 5.0f * controlScale),
                    ImVec2(tickX, trackY + 5.0f * controlScale),
                    FadeColor(IM_COL32(180, 180, 185, 190), alpha), 1.0f);
            }
            const char* value = optionName(selected);
        
            const ImVec2 valueSize = ImGui::CalcTextSize(value);
        
            DrawTextWithShadow(draw, ImVec2(x + (width - valueSize.x) * 0.5f,
                trackY + 13.0f * controlScale), white, value, alpha);
            selectorY = buttonMax.y + 10.0f * controlScale;
        };

        drawSelector(Language::Get("radial_shape").c_str(), Config::g_radialShape,
            g_radialShapeButtonMin, g_radialShapeButtonMax, g_radialShapeSliderDragging,
            RadialShape::Count(), [](int i) {
                return RadialShape::Name(static_cast<RadialShape::Style>(i));
            });

        DrawTextWithShadow(draw, ImVec2(x, selectorY), white,
            Language::Get("radial_system").c_str(), alpha);
        
        const float mechanismY = selectorY + selectorLabelGap + 16.0f * controlScale;
        
        const float editorGap = 12.0f * controlScale;
        
        const float editorWidth = 104.0f * controlScale;
        
        g_trackModeButtonMin = ImVec2(x, mechanismY - 17.0f * controlScale);
        
        g_trackModeButtonMax = ImVec2(x + width - editorWidth - editorGap,
            mechanismY + 17.0f * controlScale);
        
        g_trackEditorButtonMin = ImVec2(g_trackModeButtonMax.x + editorGap,
            mechanismY - 17.0f * controlScale);
        
        g_trackEditorButtonMax = ImVec2(x + width, mechanismY + 17.0f * controlScale);
        
        const auto drawModeButton = [&](const ImVec2& min, const ImVec2& max,
            const char* text, bool active, bool transparent = false) {
            const bool hovered =
                g_settingsMousePos.x >= min.x && g_settingsMousePos.x <= max.x &&
                g_settingsMousePos.y >= min.y && g_settingsMousePos.y <= max.y;
        
            if (!transparent || hovered)
            {
                const ImU32 background = transparent
                    ? IM_COL32(65, 65, 70, 235)
                    : active
                    ? (hovered ? IM_COL32(225, 213, 178, 250)
                               : IM_COL32(190, 180, 155, 235))
                    : (hovered ? IM_COL32(65, 65, 70, 235) //preto/cinza
                               : IM_COL32(25, 26, 31, 235));
                draw->AddRectFilled(min, max, FadeColor(background, alpha),
                    4.0f * controlScale);
            }
        
            draw->AddRect(min, max,
                FadeColor(hovered && (transparent || !active)
                                  ? IM_COL32(255, 255, 255, 190)
                                  : hovered ? IM_COL32(245, 228, 180, 250)
                                  : IM_COL32(190, 185, 170, 175), alpha),
                4.0f * controlScale, 0, hovered ? 1.8f : 1.2f);
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            
            DrawTextWithShadow(draw, ImVec2((min.x + max.x - textSize.x) * 0.5f,
                (min.y + max.y - textSize.y) * 0.5f),
                active && (!transparent || hovered)
                    ? IM_COL32(215, 195, 150, 255) : white, text, alpha);
        };
        
        const bool customUsable = Config::g_customRadial && Track::HasValidSavedLayout();
        
        drawModeButton(g_trackModeButtonMin, g_trackModeButtonMax,
            Config::g_customRadial ? "CUSTOM" : "LEGACY", customUsable,
            Config::g_customRadial);
        
        drawModeButton(g_trackEditorButtonMin, g_trackEditorButtonMax, "EDITOR", false);
        
        if (Config::g_customRadial && !Track::HasValidSavedLayout())
        {
            const char* fallback = "Invalid custom layout - Legacy fallback";
        
            DrawTextWithShadow(draw, ImVec2(x, g_trackModeButtonMax.y + 4.0f),
                FadeColor(IM_COL32(225, 145, 125, 255), alpha), fallback, alpha);
        }
        
        selectorY = g_trackModeButtonMax.y + 20.0f * controlScale;

        drawSelector(Language::Get("radial_animation").c_str(), Config::g_radialAnimation,
            g_radialAnimationButtonMin, g_radialAnimationButtonMax, g_radialAnimationSliderDragging,
            RadialAnimation::Count(), [](int i) {
                return RadialAnimation::Name(static_cast<RadialAnimation::Style>(i));
            });

        DrawTextWithShadow(
            draw,
            ImVec2(x, iconsTitleY),
            FadeColor(IM_COL32(215, 195, 150, 255), alpha),
            Language::Get("icons").c_str(),
            alpha
        );

        const ImVec2 iconsTitleSize =
            ImGui::CalcTextSize(Language::Get("icons").c_str());

        const float iconsSeparatorY =
            iconsTitleY + ImGui::GetFontSize() * 0.5f;

        draw->AddLine(
            ImVec2(x + iconsTitleSize.x + 12.0f * controlScale, iconsSeparatorY),
            ImVec2(x + width, iconsSeparatorY),
            FadeColor(IM_COL32(215, 195, 150, 90), alpha),
            1.0f
        );

        const std::array<bool*, 4> iconSettings{
            &Config::g_customIcons,
            &Config::g_coloredPotions,
            &Config::g_coloredMagicSchools,
            &Config::g_coloredItemEnchants
        };

        const std::array<const char*, 4> iconLabels{
            "custom_icons",
            "colored_potions",
            "colored_magic_schools",
            "color_item_enchants"
        };
        
        const float iconsOptionStartY = iconsTitleY + 32.0f * controlScale;
        
        const float iconsOptionSpacing = 32.0f * controlScale;
        
        g_gameplayIconButtonRadius = buttonRadius;
        
        const bool customIconsAvailable = IconCustom::HasValidConfiguration();
        
        for (std::size_t i = 0; i < iconSettings.size(); ++i)
        {
            const float optionCenterY = iconsOptionStartY + iconsOptionSpacing * static_cast<float>(i);
            
            g_gameplayIconButtonCenters[i] = ImVec2(x + 13.0f * controlScale, optionCenterY);
            
            const float iconDx = g_settingsMousePos.x - g_gameplayIconButtonCenters[i].x;
            
            const float iconDy = g_settingsMousePos.y - g_gameplayIconButtonCenters[i].y;
            
            const bool optionEnabled = i != 0 || customIconsAvailable;
            
            const bool iconHovered = optionEnabled &&
                iconDx * iconDx + iconDy * iconDy <= buttonRadius * buttonRadius;
            
                g_gameplayIconButtonHoverT[i] = AnimateSettingsValue(
                g_gameplayIconButtonHoverT[i], iconHovered ? 1.0f : 0.0f, 12.0f, ImGui::GetIO().DeltaTime);
            
            const ImU32 iconBackground = !optionEnabled ? IM_COL32(18, 18, 23, 115) : *iconSettings[i]
                ? IM_COL32(235, 235, 235, 140) : IM_COL32(20, 20, 25, 235);
            
            draw->AddCircleFilled(g_gameplayIconButtonCenters[i],
                buttonRadius + g_gameplayIconButtonHoverT[i] * 2.0f,
                FadeColor(iconBackground, alpha), 48);
            
            draw->AddCircle(g_gameplayIconButtonCenters[i],
                buttonRadius + g_gameplayIconButtonHoverT[i] * 2.0f,
                FadeColor(IM_COL32(255, 255, 255,
                    static_cast<int>(90.0f + 140.0f * g_gameplayIconButtonHoverT[i])), alpha), 48, 1.5f);
            
            DrawTextWithShadow(draw,
                ImVec2(g_gameplayIconButtonCenters[i].x + buttonRadius + 12.0f,
                    optionCenterY - ImGui::GetFontSize() * 0.5f),
                FadeColor(optionEnabled ? IM_COL32(235, 230, 215, 255) : IM_COL32(130, 125, 120, 150), alpha),
                Language::Get(iconLabels[i]).c_str(), alpha);

            if (i == 0)
            {
                g_customIconsReloadCenter = ImVec2(x + width - 10.0f * controlScale, optionCenterY);
            
                g_customIconsReloadRadius = 7.0f * controlScale;
            
                const float reloadDx = g_settingsMousePos.x - g_customIconsReloadCenter.x;
            
                const float reloadDy = g_settingsMousePos.y - g_customIconsReloadCenter.y;
            
                const bool reloadHovered = customIconsAvailable &&
                    reloadDx * reloadDx + reloadDy * reloadDy <=
                        g_customIconsReloadRadius * g_customIconsReloadRadius;
                g_customIconsReloadHoverT = AnimateSettingsValue(g_customIconsReloadHoverT,
                    reloadHovered ? 1.0f : 0.0f, 12.0f, ImGui::GetIO().DeltaTime);
            
                const int reloadAlpha = customIconsAvailable
                    ? static_cast<int>(95.0f + 145.0f * g_customIconsReloadHoverT) : 45;
            
                draw->AddCircleFilled(g_customIconsReloadCenter,
                    g_customIconsReloadRadius + g_customIconsReloadHoverT,
                    FadeColor(IM_COL32(245, 245, 245, reloadAlpha), alpha), 24);
            
                draw->AddCircle(g_customIconsReloadCenter,
                    g_customIconsReloadRadius + g_customIconsReloadHoverT,
                    FadeColor(IM_COL32(255, 255, 255, reloadAlpha), alpha), 24, 1.0f);
            
                const ImVec2 arrowA(g_customIconsReloadCenter.x - 2.0f * controlScale,
                    g_customIconsReloadCenter.y - 2.0f * controlScale);
            
                const ImVec2 arrowB(g_customIconsReloadCenter.x + 3.0f * controlScale,
                    g_customIconsReloadCenter.y + 2.0f * controlScale);
                //draw->AddLine(arrowA, arrowB, FadeColor(IM_COL32(30, 30, 34, 235), alpha), 1.2f);
                //draw->AddTriangleFilled(ImVec2(arrowB.x, arrowB.y),
                //    ImVec2(arrowB.x - 3.0f * controlScale, arrowB.y - 1.0f * controlScale),
                //    ImVec2(arrowB.x - 1.0f * controlScale, arrowB.y - 3.0f * controlScale),
                //    FadeColor(IM_COL32(30, 30, 34, 235), alpha));
                customIconsReloadTooltip = reloadHovered;
            }
        }

        DrawTextWithShadow(draw, ImVec2(x, inventoryTitleY), gold,
            Language::Get("inventory_section").c_str(), alpha);
        
        const ImVec2 inventoryTitleSize = ImGui::CalcTextSize(
            Language::Get("inventory_section").c_str());
        
        draw->AddLine(
            ImVec2(x + inventoryTitleSize.x + 12.0f * controlScale,
                inventoryTitleY + ImGui::GetFontSize() * 0.5f),
            ImVec2(x + width, inventoryTitleY + ImGui::GetFontSize() * 0.5f),
            FadeColor(IM_COL32(215, 195, 150, 90), alpha), 1.0f);

        g_fastDragButtonRadius = buttonRadius;
        
        g_fastDragButtonCenter = ImVec2(x + 13.0f * controlScale, fastDragOptionY);
        
        const float fastDragDx = g_settingsMousePos.x - g_fastDragButtonCenter.x;
        
        const float fastDragDy = g_settingsMousePos.y - g_fastDragButtonCenter.y;
        
        const bool fastDragHovered = fastDragDx * fastDragDx + fastDragDy * fastDragDy <=
            buttonRadius * buttonRadius;
        
        g_fastDragButtonHoverT = AnimateSettingsValue(g_fastDragButtonHoverT,
            fastDragHovered ? 1.0f : 0.0f, 12.0f, ImGui::GetIO().DeltaTime);
        
        draw->AddCircleFilled(g_fastDragButtonCenter,
            buttonRadius + g_fastDragButtonHoverT * 2.0f,
            FadeColor(Config::g_fastInventoryDrag
                ? IM_COL32(235, 235, 235, 140)
                : IM_COL32(20, 20, 25, 235), alpha), 48);
        
        draw->AddCircle(g_fastDragButtonCenter,
            buttonRadius + g_fastDragButtonHoverT * 2.0f,
            FadeColor(IM_COL32(255, 255, 255,
                static_cast<int>(90.0f + 140.0f * g_fastDragButtonHoverT)), alpha),
            48, 1.5f);
        
        DrawTextWithShadow(draw,
            ImVec2(g_fastDragButtonCenter.x + buttonRadius + 12.0f,
                fastDragOptionY - ImGui::GetFontSize() * 0.5f),
            white, Language::Get("fast_drag").c_str(), alpha);

        g_languageButtonMin = ImVec2(x, languageY);
        
        g_languageButtonMax = ImVec2(x + width, languageY + languageButtonHeight);
        
        const bool languageHovered =
            g_settingsMousePos.x >= g_languageButtonMin.x &&
            g_settingsMousePos.x <= g_languageButtonMax.x &&
            g_settingsMousePos.y >= g_languageButtonMin.y &&
            g_settingsMousePos.y <= g_languageButtonMax.y;

        draw->AddRectFilled(g_languageButtonMin, g_languageButtonMax,
            FadeColor(languageHovered || g_languageListOpen
                ? IM_COL32(65, 65, 70, 235)
                : IM_COL32(20, 20, 25, 235), alpha), 5.0f * controlScale);
        
        draw->AddRect(g_languageButtonMin, g_languageButtonMax,
            FadeColor(IM_COL32(255, 255, 255,
                languageHovered || g_languageListOpen ? 190 : 85), alpha),
            5.0f * controlScale, 0, 1.5f);

        const std::string languageText = std::format("{}: {}",
            Language::Get("language"), Language::GetCurrentName());
        
        const ImVec2 languageTextSize = ImGui::CalcTextSize(languageText.c_str());
        
        DrawTextWithShadow(draw,
            ImVec2((g_languageButtonMin.x + g_languageButtonMax.x - languageTextSize.x) * 0.5f,
                (g_languageButtonMin.y + g_languageButtonMax.y - languageTextSize.y) * 0.5f),
            FadeColor(IM_COL32(235, 230, 215, 255), alpha),
            languageText.c_str(), alpha);

        g_languageOptionHitboxes.clear();
        
        if (g_languageListOpen)
        {
            const auto& languages = Language::GetAvailableLanguages();
            const float optionHeight = std::max(
                30.0f * controlScale, ImGui::GetFontSize() + 10.0f);
            if (languages.empty())
            {
                DrawTextWithShadow(draw,
                    ImVec2(g_languageButtonMin.x,
                        g_languageButtonMax.y + 8.0f * controlScale),
                    FadeColor(IM_COL32(155, 150, 140, 220), alpha),
                    Language::Get("no_languages").c_str(), alpha);
            }
            else
            {
                for (std::size_t i = 0; i < languages.size(); ++i)
                {
                    LanguageOptionHitbox option;
                    option.min = ImVec2(g_languageButtonMin.x,
                        g_languageButtonMax.y + 4.0f * controlScale +
                            optionHeight * static_cast<float>(i));
        
                    option.max = ImVec2(g_languageButtonMax.x, option.min.y + optionHeight);
        
                    option.language = languages[i];
        
                    const bool optionHovered =
                        g_settingsMousePos.x >= option.min.x && g_settingsMousePos.x <= option.max.x &&
                        g_settingsMousePos.y >= option.min.y && g_settingsMousePos.y <= option.max.y;
        
                    draw->AddRectFilled(option.min, option.max,
                        FadeColor(optionHovered
                            ? IM_COL32(75, 75, 82, 245)
                            : IM_COL32(18, 18, 23, 245), alpha), 3.0f * controlScale);
        
                    const ImVec2 optionTextSize = ImGui::CalcTextSize(option.language.c_str());
        
                    DrawTextWithShadow(draw,
                        ImVec2((option.min.x + option.max.x - optionTextSize.x) * 0.5f,
                            (option.min.y + option.max.y - optionTextSize.y) * 0.5f),
                        FadeColor(IM_COL32(235, 230, 215, 255), alpha),
                        option.language.c_str(), alpha);
                    g_languageOptionHitboxes.push_back(std::move(option));
                }
            }
        }

        draw->PopClipRect();
        
        const float gameplayContentHeight =
            fastDragOptionY + buttonRadius + 20.0f * controlScale +
            g_gameplayPanelScroll - gameplayContentTop;
        
        const float gameplayVisibleHeight = std::max(
            gameplayContentBottom - gameplayContentTop, 1.0f);
        
        g_gameplayPanelMaxScroll = std::max(
            0.0f, gameplayContentHeight - gameplayVisibleHeight + 10.0f * controlScale);
        
        g_gameplayPanelScroll = std::clamp(
            g_gameplayPanelScroll, 0.0f, g_gameplayPanelMaxScroll);

        draw->AddRectFilled(
            ImVec2(gameplayScrollbarX - 2.0f * controlScale, gameplayContentTop),
            ImVec2(gameplayScrollbarX + 2.0f * controlScale, gameplayContentBottom),
            FadeColor(IM_COL32(30, 31, 36, 220), alpha), 3.0f * controlScale);
        
        const float gameplayThumbHeight = std::max(
            22.0f * controlScale,
            gameplayVisibleHeight * std::clamp(
                gameplayVisibleHeight / std::max(gameplayContentHeight, 1.0f), 0.0f, 1.0f));
        
        const float gameplayThumbTravel = std::max(
            0.0f, gameplayVisibleHeight - gameplayThumbHeight);
        
        const float gameplayScrollT = g_gameplayPanelMaxScroll > 0.0f
            ? g_gameplayPanelScroll / g_gameplayPanelMaxScroll : 0.0f;
        
        const float gameplayThumbTop = gameplayContentTop + gameplayThumbTravel * gameplayScrollT;
        
        const bool gameplayScrollbarHovered =
            g_settingsMousePos.x >= g_gameplayScrollbarHitbox.min.x &&
            g_settingsMousePos.x <= g_gameplayScrollbarHitbox.max.x &&
            g_settingsMousePos.y >= g_gameplayScrollbarHitbox.min.y &&
            g_settingsMousePos.y <= g_gameplayScrollbarHitbox.max.y;
        
        draw->AddRectFilled(
            ImVec2(gameplayScrollbarX - 4.0f * controlScale, gameplayThumbTop),
            ImVec2(gameplayScrollbarX + 4.0f * controlScale,
                gameplayThumbTop + gameplayThumbHeight),
            FadeColor(gameplayScrollbarHovered || g_gameplayScrollbarDragging
                ? IM_COL32(235, 230, 215, 230)
                : IM_COL32(95, 94, 98, 220), alpha), 4.0f * controlScale);

        if (customIconsReloadTooltip)
        {
            const auto reloadText = Language::Get("reload");
        
            const ImVec2 textSize = ImGui::CalcTextSize(reloadText.c_str());
        
            const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        
            const float tooltipPadding = 8.0f * controlScale;
        
            ImVec2 tooltipMin(g_settingsMousePos.x + 13.0f * controlScale,
                g_settingsMousePos.y + 13.0f * controlScale);
        
            tooltipMin.x = std::clamp(tooltipMin.x, 4.0f,
                std::max(4.0f, displaySize.x - textSize.x - tooltipPadding * 2.0f - 4.0f));
        
            tooltipMin.y = std::clamp(tooltipMin.y, 4.0f,
                std::max(4.0f, displaySize.y - textSize.y - tooltipPadding * 2.0f - 4.0f));
        
            const ImVec2 tooltipMax(tooltipMin.x + textSize.x + tooltipPadding * 2.0f,
                tooltipMin.y + textSize.y + tooltipPadding * 2.0f);
        
            draw->AddRectFilled(tooltipMin, tooltipMax,
                FadeColor(IM_COL32(13, 14, 18, 248), alpha), 4.0f * controlScale);
        
            draw->AddRect(tooltipMin, tooltipMax,
                FadeColor(IM_COL32(190, 185, 170, 180), alpha), 4.0f * controlScale);
        
            DrawTextWithShadow(draw, ImVec2(tooltipMin.x + tooltipPadding,
                tooltipMin.y + tooltipPadding), white, reloadText.c_str(), alpha);
        }
    }

    static void DrawSettingsLayoutPanel(float alpha)
    {
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        
        const char* previewButtonTooltip = nullptr;
        
        const SettingsInfoPanel panel = GetSettingsInfoPanel(ImGui::GetIO().DisplaySize);
        
        const float controlScale = 1.0f +
            (std::clamp(Config::g_fontSizeScale, 1.0f, 2.5f) - 1.0f) * (2.0f / 3.0f);
        
        const float padding = 10.0f * controlScale;
        
        const float x = panel.min.x + padding;
        
        const float y = panel.min.y + padding;
        
        const float scrollbarWidth = 12.0f * controlScale;
        
        const float width = panel.width - padding * 2.0f;
        
        if (width <= 0.0f)
            return;

        if (g_settingsSectionMorphT < 0.55f)
        {
            DrawTextWithShadow(draw, ImVec2(x, y),
                FadeColor(IM_COL32(215, 195, 150, 255), alpha), Language::Get("layout").c_str(), alpha);
        }
        
        draw->AddLine(ImVec2(x, y + 29.0f * controlScale), ImVec2(x + width, y + 29.0f * controlScale),
            FadeColor(IM_COL32(215, 195, 150, 90), alpha), 1.0f);
        
        struct SliderData
        {
            const char* label;
            float* value;
            float minimum;
            float maximum;
        };
        float radialLineOpacity = Track::RadialLineOpacity();
        float overflowLineOpacity = Track::LineOpacity();
        float radialRotationDegrees = Track::RadialRotation() * 57.2957795f;
        SliderData sliders[] = {
            { Language::Get("font_size").c_str(), &Config::g_fontSizeScale, 1.0f, 2.5f },
            { Language::Get("draw_mark_distance").c_str(), &Config::g_drawMarkDistance, 0.0f, 100.0f },
            { Language::Get("item_opacity").c_str(), &Config::g_itemOpacity, 0.0f, 100.0f },
            { Language::Get("layout_radial_line_opacity").c_str(), &radialLineOpacity, 0.0f, 100.0f },
            { Language::Get("general_item_size").c_str(), &Config::g_generalItemSize, 25.0f, 200.0f },
            { Language::Get("slot_size").c_str(), &Config::g_slotSize, 25.0f, 200.0f },
            { Language::Get("background_opacity").c_str(), &Config::g_itemBackgroundOpacity, 0.0f, 100.0f },
            { Language::Get("border_opacity").c_str(), &Config::g_itemBorderOpacity, 0.0f, 100.0f },
            { Language::Get("icon_size").c_str(), &Config::g_iconSize, 25.0f, 200.0f },
            { Language::Get("base_icon_opacity").c_str(), &Config::g_baseIconOpacity, 0.0f, 100.0f },
            { Language::Get("layout_overflow_line_opacity").c_str(), &overflowLineOpacity, 0.0f, 100.0f },
            { Language::Get("overflow_opacity").c_str(), &Config::g_overflowOpacity, 0.0f, 100.0f },
            { Language::Get("overflow_size").c_str(), &Config::g_overflowSize, 25.0f, 200.0f },
            { Language::Get("overflow_background_opacity").c_str(), &Config::g_overflowBackgroundOpacity, 0.0f, 100.0f },
            { Language::Get("overflow_border_opacity").c_str(), &Config::g_overflowBorderOpacity, 0.0f, 100.0f },
            { Language::Get("item_name_opacity").c_str(), &Config::g_itemNameOpacity, 0.0f, 100.0f },
            { Language::Get("item_name_position_y").c_str(), &Config::g_itemNamePositionY, 0.0f, 100.0f },
            { Language::Get("item_name_position_x").c_str(), &Config::g_itemNamePositionX, 0.0f, 100.0f },
            { Language::Get("center_opacity").c_str(), &Config::g_centerOpacity, 0.0f, 100.0f },
            { Language::Get("radial_quantity").c_str(), &Config::g_radialQuantity, 3.0f, 50.0f },
            { Language::Get("stardust_fade").c_str(), &Config::g_stardustFade, 0.0f, 100.0f },
            { Language::Get("radial_stretch").c_str(), &Config::g_radialStretch, 0.0f, 100.0f },
            { Language::Get("layout_radial_rotation").c_str(), &radialRotationDegrees, -180.0f, 180.0f },
            { Language::Get("radial_position").c_str(), &Config::g_sideRadialPosition, 0.0f, 100.0f },
            { Language::Get("side_opacity").c_str(), &Config::g_sideOpacity, 0.0f, 100.0f },
            { Language::Get("top_position").c_str(), &Config::g_topVerticalPosition, 0.0f, 100.0f },
            { Language::Get("top_stretch").c_str(), &Config::g_topHorizontalStretch, 0.0f, 100.0f },
            { Language::Get("top_opacity").c_str(), &Config::g_topOpacity, 0.0f, 100.0f },
            { Language::Get("bottom_position").c_str(), &Config::g_bottomVerticalPosition, 0.0f, 100.0f },
            { Language::Get("bottom_stretch").c_str(), &Config::g_bottomHorizontalStretch, 0.0f, 100.0f },
            { Language::Get("bottom_opacity").c_str(), &Config::g_bottomOpacity, 0.0f, 100.0f },
            { Language::Get("top_item_quantity").c_str(), &Config::g_topItemStyle.quantity, 3.0f, 50.0f },
            { Language::Get("top_general_item_size").c_str(), &Config::g_topItemStyle.generalSize, 25.0f, 200.0f },
            { Language::Get("top_item_opacity").c_str(), &Config::g_topItemStyle.opacity, 0.0f, 100.0f },
            { Language::Get("top_line_opacity").c_str(), &Config::g_topItemStyle.lineOpacity, 0.0f, 100.0f },
            { Language::Get("top_slot_size").c_str(), &Config::g_topItemStyle.slotSize, 25.0f, 200.0f },
            { Language::Get("top_background_opacity").c_str(), &Config::g_topItemStyle.backgroundOpacity, 0.0f, 100.0f },
            { Language::Get("top_border_opacity").c_str(), &Config::g_topItemStyle.borderOpacity, 0.0f, 100.0f },
            { Language::Get("top_icon_size").c_str(), &Config::g_topItemStyle.iconSize, 25.0f, 200.0f },
            { Language::Get("top_icon_opacity").c_str(), &Config::g_topItemStyle.iconOpacity, 0.0f, 100.0f },
            { Language::Get("bottom_item_quantity").c_str(), &Config::g_bottomItemStyle.quantity, 3.0f, 50.0f },
            { Language::Get("bottom_general_item_size").c_str(), &Config::g_bottomItemStyle.generalSize, 25.0f, 200.0f },
            { Language::Get("bottom_item_opacity").c_str(), &Config::g_bottomItemStyle.opacity, 0.0f, 100.0f },
            { Language::Get("bottom_line_opacity").c_str(), &Config::g_bottomItemStyle.lineOpacity, 0.0f, 100.0f },
            { Language::Get("bottom_slot_size").c_str(), &Config::g_bottomItemStyle.slotSize, 25.0f, 200.0f },
            { Language::Get("bottom_background_opacity").c_str(), &Config::g_bottomItemStyle.backgroundOpacity, 0.0f, 100.0f },
            { Language::Get("bottom_border_opacity").c_str(), &Config::g_bottomItemStyle.borderOpacity, 0.0f, 100.0f },
            { Language::Get("bottom_icon_size").c_str(), &Config::g_bottomItemStyle.iconSize, 25.0f, 200.0f },
            { Language::Get("bottom_icon_opacity").c_str(), &Config::g_bottomItemStyle.iconOpacity, 0.0f, 100.0f },
            { Language::Get("preview_item_size").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Menu).itemSize, 1.0f, 500.0f },
            { Language::Get("preview_item_position_y").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Menu).itemPositionY, 0.0f, 100.0f },
            { Language::Get("preview_item_position_x").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Menu).itemPositionX, 0.0f, 100.0f },
            { Language::Get("item_name_opacity").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Menu).itemNameOpacity, 0.0f, 100.0f },
            { Language::Get("item_name_position_y").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Menu).itemNamePositionY, 0.0f, 100.0f },
            { Language::Get("item_name_position_x").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Menu).itemNamePositionX, 0.0f, 100.0f },
            { Language::Get("preview_item_size").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Top).itemSize, 1.0f, 500.0f },
            { Language::Get("preview_item_position_y").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Top).itemPositionY, 0.0f, 100.0f },
            { Language::Get("preview_item_position_x").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Top).itemPositionX, 0.0f, 100.0f },
            { Language::Get("item_name_opacity").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Top).itemNameOpacity, 0.0f, 100.0f },
            { Language::Get("item_name_position_y").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Top).itemNamePositionY, 0.0f, 100.0f },
            { Language::Get("item_name_position_x").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Top).itemNamePositionX, 0.0f, 100.0f },
            { Language::Get("preview_item_size").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Bottom).itemSize, 1.0f, 500.0f },
            { Language::Get("preview_item_position_y").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Bottom).itemPositionY, 0.0f, 100.0f },
            { Language::Get("preview_item_position_x").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Bottom).itemPositionX, 0.0f, 100.0f },
            { Language::Get("item_name_opacity").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Bottom).itemNameOpacity, 0.0f, 100.0f },
            { Language::Get("item_name_position_y").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Bottom).itemNamePositionY, 0.0f, 100.0f },
            { Language::Get("item_name_position_x").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Bottom).itemNamePositionX, 0.0f, 100.0f },
            { Language::Get("preview_item_size").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Right).itemSize, 1.0f, 500.0f },
            { Language::Get("preview_item_position_y").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Right).itemPositionY, 0.0f, 100.0f },
            { Language::Get("preview_item_position_x").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Right).itemPositionX, 0.0f, 100.0f },
            { Language::Get("item_name_opacity").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Right).itemNameOpacity, 0.0f, 100.0f },
            { Language::Get("item_name_position_y").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Right).itemNamePositionY, 0.0f, 100.0f },
            { Language::Get("item_name_position_x").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Right).itemNamePositionX, 0.0f, 100.0f },
            { Language::Get("preview_item_size").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Left).itemSize, 1.0f, 500.0f },
            { Language::Get("preview_item_position_y").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Left).itemPositionY, 0.0f, 100.0f },
            { Language::Get("preview_item_position_x").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Left).itemPositionX, 0.0f, 100.0f },
            { Language::Get("item_name_opacity").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Left).itemNameOpacity, 0.0f, 100.0f },
            { Language::Get("item_name_position_y").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Left).itemNamePositionY, 0.0f, 100.0f },
            { Language::Get("item_name_position_x").c_str(), &Config::GetItemPreviewLayout(Config::ItemPreviewProfile::Left).itemNamePositionX, 0.0f, 100.0f },
            { Language::Get("preview_spell_size_multiplier").c_str(), &Config::g_itemPreviewCategoryMultipliers[0], 0.2f, 3.0f },
            { Language::Get("preview_weapon_size_multiplier").c_str(), &Config::g_itemPreviewCategoryMultipliers[1], 0.2f, 3.0f },
            { Language::Get("preview_potion_size_multiplier").c_str(), &Config::g_itemPreviewCategoryMultipliers[2], 0.2f, 3.0f },
            { Language::Get("preview_armor_size_multiplier").c_str(), &Config::g_itemPreviewCategoryMultipliers[3], 0.2f, 3.0f },
            { Language::Get("preview_ammo_size_multiplier").c_str(), &Config::g_itemPreviewCategoryMultipliers[4], 0.2f, 3.0f },
            { Language::Get("preview_book_size_multiplier").c_str(), &Config::g_itemPreviewCategoryMultipliers[5], 0.2f, 3.0f },
            { Language::Get("preview_misc_size_multiplier").c_str(), &Config::g_itemPreviewCategoryMultipliers[6], 0.2f, 3.0f },
            { Language::Get("preview_key_size_multiplier").c_str(), &Config::g_itemPreviewCategoryMultipliers[7], 0.2f, 3.0f },
            { Language::Get("preview_soul_gem_size_multiplier").c_str(), &Config::g_itemPreviewCategoryMultipliers[8], 0.2f, 3.0f },
            { Language::Get("preview_ingredient_size_multiplier").c_str(), &Config::g_itemPreviewCategoryMultipliers[9], 0.2f, 3.0f },
            { Language::Get("preview_scroll_size_multiplier").c_str(), &Config::g_itemPreviewCategoryMultipliers[10], 0.2f, 3.0f }
        };
        
        const float rowHeight = std::max(45.0f * controlScale,
            ImGui::GetFontSize() + 25.0f * controlScale);
        
        const float trackHeight = 14.0f * controlScale;
        
        const float resetRadius = 5.0f * controlScale;
        
        const float resetAreaWidth = 24.0f * controlScale;
        
        const float trackMinX = x;
        
        const float trackMaxX = x + width - resetAreaWidth;
        
        const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f);

        g_layoutSliderRows.fill(-1);
        
        g_layoutSliderVisible.fill(false);
        
        int nextRow = 0;
        
        const auto addSliderRow = [&](LayoutSlider slider) {
            const std::size_t index = static_cast<std::size_t>(slider);
            g_layoutSliderRows[index] = nextRow++;
            g_layoutSliderVisible[index] = true;
        };
        
        const auto groupExpanded = [](LayoutGroup group) {
            return g_layoutGroupExpanded[static_cast<std::size_t>(group)];
        };

        const int presetButtonsRow = nextRow++;
        
        const int presetNameRow = g_layoutNameOpen ? nextRow++ : -1;
        
        const int presetNameActionsRow = g_layoutNameOpen ? nextRow++ : -1;
        
        const std::vector<std::string> layoutPresets = g_layoutLoadOpen
            ? Config::LayoutPresets() : std::vector<std::string>{};
        
        const int presetListFirstRow = g_layoutLoadOpen ? nextRow : -1;
        
        if (g_layoutLoadOpen) nextRow += std::max(1, static_cast<int>(layoutPresets.size()));
        
        addSliderRow(LayoutSlider::FontSize);
        
        const int fontButtonRow = nextRow++;
        
        std::array<int, 4> itemToggleRows{ -1, -1, -1, -1 };
        
        for (int& row : itemToggleRows) row = nextRow++;
        int itemPreviewHeaderRow = -1;
        int drawHeaderRow = -1;
        
        int previewMenuHeaderRow = -1;
        int previewTopHeaderRow = -1;
        int previewBottomHeaderRow = -1;
        int previewRightHeaderRow = -1;
        int previewLeftHeaderRow = -1;
        int itemRadialHeaderRow = -1;
        int stardustToggleRow = -1;
        int itemOverflowHeaderRow = -1;
        int itemColorsHeaderRow = -1;
        int itemSlotHeaderRow = -1;
        int itemIconHeaderRow = -1;
        int itemTopHeaderRow = -1;
        int itemTopSlotHeaderRow = -1;
        int itemTopIconHeaderRow = -1;
        int itemBottomHeaderRow = -1;
        int itemBottomSlotHeaderRow = -1;
        int itemBottomIconHeaderRow = -1;
        int backgroundColorRow = -1;
        int borderColorRow = -1;
        int iconColorRow = -1;
        int overflowBackgroundColorRow = -1;
        int overflowBorderColorRow = -1;
        int topBackgroundColorRow = -1;
        int topBorderColorRow = -1;
        int topIconColorRow = -1;
        int bottomBackgroundColorRow = -1;
        int bottomBorderColorRow = -1;
        int bottomIconColorRow = -1;
        int potionsTitleRow = -1;
        int schoolsTitleRow = -1;
        int enchantsTitleRow = -1;
        
        std::array<int, 7> potionColorRows{};
        
        std::array<int, 8> schoolColorRows{};
        
        std::array<int, 5> enchantColorRows{};
        
        
        potionColorRows.fill(-1);
        schoolColorRows.fill(-1);
        enchantColorRows.fill(-1);
        
        int colorPickerRow = -1;
        
        const auto addColorRow = [&](LayoutColorControl control, int& row) {
            row = nextRow++;
            if (g_openLayoutColor == static_cast<int>(control))
            {
                colorPickerRow = nextRow;
                nextRow += 3;
            }
        };
        
        itemRadialHeaderRow = nextRow++;
        if (groupExpanded(LayoutGroup::ItemRadial))
        {
            stardustToggleRow = nextRow++;
            addSliderRow(LayoutSlider::StardustFade);
            addSliderRow(LayoutSlider::RadialStretch);
            addSliderRow(LayoutSlider::RadialRotation);
            addSliderRow(LayoutSlider::SidePosition);
            addSliderRow(LayoutSlider::SideOpacity);
            addSliderRow(LayoutSlider::CenterOpacity);
            addSliderRow(LayoutSlider::RadialQuantity);
            addSliderRow(LayoutSlider::GeneralItemSize);
            addSliderRow(LayoutSlider::ItemOpacity);
            addSliderRow(LayoutSlider::RadialLineOpacity);
            itemSlotHeaderRow = nextRow++;
            if (groupExpanded(LayoutGroup::ItemSlot))
            {
                addSliderRow(LayoutSlider::SlotSize);
                addColorRow(LayoutColorControl::Background, backgroundColorRow);
                addSliderRow(LayoutSlider::ItemBackgroundOpacity);
                addColorRow(LayoutColorControl::Border, borderColorRow);
                addSliderRow(LayoutSlider::ItemBorderOpacity);
            }
            itemIconHeaderRow = nextRow++;
            if (groupExpanded(LayoutGroup::ItemIcon))
            {
                addSliderRow(LayoutSlider::IconSize);
                addColorRow(LayoutColorControl::Icon, iconColorRow);
                addSliderRow(LayoutSlider::BaseIconOpacity);
            }
        }

        itemTopHeaderRow = nextRow++;
        if (groupExpanded(LayoutGroup::ItemTop))
        {
            addSliderRow(LayoutSlider::TopPosition);
            addSliderRow(LayoutSlider::TopStretch);
            addSliderRow(LayoutSlider::TopOpacity);
            addSliderRow(LayoutSlider::TopItemQuantity);
            addSliderRow(LayoutSlider::TopGeneralItemSize);
            addSliderRow(LayoutSlider::TopItemOpacity);
            addSliderRow(LayoutSlider::TopLineOpacity);
            itemTopSlotHeaderRow = nextRow++;
            if (groupExpanded(LayoutGroup::ItemTopSlot))
            {
                addSliderRow(LayoutSlider::TopSlotSize);
                addColorRow(LayoutColorControl::TopBackground, topBackgroundColorRow);
                addSliderRow(LayoutSlider::TopBackgroundOpacity);
                addColorRow(LayoutColorControl::TopBorder, topBorderColorRow);
                addSliderRow(LayoutSlider::TopBorderOpacity);
            }
            itemTopIconHeaderRow = nextRow++;
            if (groupExpanded(LayoutGroup::ItemTopIcon))
            {
                addSliderRow(LayoutSlider::TopIconSize);
                addColorRow(LayoutColorControl::TopIcon, topIconColorRow);
                addSliderRow(LayoutSlider::TopIconOpacity);
            }
        }

        itemBottomHeaderRow = nextRow++;
        if (groupExpanded(LayoutGroup::ItemBottom))
        {
            addSliderRow(LayoutSlider::BottomPosition);
            addSliderRow(LayoutSlider::BottomStretch);
            addSliderRow(LayoutSlider::BottomOpacity);
            addSliderRow(LayoutSlider::BottomItemQuantity);
            addSliderRow(LayoutSlider::BottomGeneralItemSize);
            addSliderRow(LayoutSlider::BottomItemOpacity);
            addSliderRow(LayoutSlider::BottomLineOpacity);
            itemBottomSlotHeaderRow = nextRow++;
            if (groupExpanded(LayoutGroup::ItemBottomSlot))
            {
                addSliderRow(LayoutSlider::BottomSlotSize);
                addColorRow(LayoutColorControl::BottomBackground, bottomBackgroundColorRow);
                addSliderRow(LayoutSlider::BottomBackgroundOpacity);
                addColorRow(LayoutColorControl::BottomBorder, bottomBorderColorRow);
                addSliderRow(LayoutSlider::BottomBorderOpacity);
            }
            itemBottomIconHeaderRow = nextRow++;
            if (groupExpanded(LayoutGroup::ItemBottomIcon))
            {
                addSliderRow(LayoutSlider::BottomIconSize);
                addColorRow(LayoutColorControl::BottomIcon, bottomIconColorRow);
                addSliderRow(LayoutSlider::BottomIconOpacity);
            }
        }

        drawHeaderRow = nextRow++;
        if (groupExpanded(LayoutGroup::Draw))
            addSliderRow(LayoutSlider::DrawMarkDistance);

        itemPreviewHeaderRow = nextRow++;
        if (groupExpanded(LayoutGroup::ItemPreview))
        {
            const auto addPreviewRows = [&](LayoutSlider first) {
                for (std::size_t offset = 0; offset < 6; ++offset)
                    addSliderRow(static_cast<LayoutSlider>(
                        static_cast<std::size_t>(first) + offset));
            };
            previewMenuHeaderRow = nextRow++;
            if (groupExpanded(LayoutGroup::PreviewMenu))
                addPreviewRows(LayoutSlider::PreviewMenuItemSize);
            previewTopHeaderRow = nextRow++;
            if (groupExpanded(LayoutGroup::PreviewTop))
                addPreviewRows(LayoutSlider::PreviewTopItemSize);
            previewBottomHeaderRow = nextRow++;
            if (groupExpanded(LayoutGroup::PreviewBottom))
                addPreviewRows(LayoutSlider::PreviewBottomItemSize);
            previewRightHeaderRow = nextRow++;
            if (groupExpanded(LayoutGroup::PreviewRight))
                addPreviewRows(LayoutSlider::PreviewRightItemSize);
            previewLeftHeaderRow = nextRow++;
            if (groupExpanded(LayoutGroup::PreviewLeft))
                addPreviewRows(LayoutSlider::PreviewLeftItemSize);
            for (std::size_t slider = static_cast<std::size_t>(
                     LayoutSlider::PreviewSpellSizeMultiplier);
                 slider <= static_cast<std::size_t>(
                     LayoutSlider::PreviewScrollSizeMultiplier); ++slider)
                addSliderRow(static_cast<LayoutSlider>(slider));
        }

        itemOverflowHeaderRow = nextRow++;
        if (groupExpanded(LayoutGroup::ItemOverflow))
        {
            addSliderRow(LayoutSlider::OverflowSize);
            addColorRow(LayoutColorControl::OverflowBackground, overflowBackgroundColorRow);
            addSliderRow(LayoutSlider::OverflowBackgroundOpacity);
            addColorRow(LayoutColorControl::OverflowBorder, overflowBorderColorRow);
            addSliderRow(LayoutSlider::OverflowBorderOpacity);
            addSliderRow(LayoutSlider::OverflowLineOpacity);
            addSliderRow(LayoutSlider::OverflowOpacity);
        }
        itemColorsHeaderRow = nextRow++;
        if (groupExpanded(LayoutGroup::ItemColors))
        {
            potionsTitleRow = nextRow++;
            for (std::size_t i = 0; i < potionColorRows.size(); ++i)
                addColorRow(static_cast<LayoutColorControl>(
                    static_cast<std::size_t>(LayoutColorControl::PotionHealth) + i),
                    potionColorRows[i]);
            schoolsTitleRow = nextRow++;
            for (std::size_t i = 0; i < schoolColorRows.size(); ++i)
                addColorRow(static_cast<LayoutColorControl>(
                    static_cast<std::size_t>(LayoutColorControl::SchoolAlteration) + i),
                    schoolColorRows[i]);
            enchantsTitleRow = nextRow++;
            for (std::size_t i = 0; i < enchantColorRows.size(); ++i)
                addColorRow(static_cast<LayoutColorControl>(
                    static_cast<std::size_t>(LayoutColorControl::EnchantFire) + i),
                    enchantColorRows[i]);
        }

        // The first six controls remain visible; sections below are reached
        // by wheel or by the vertical scrollbar on the right.
        
        const float contentTop = y + 40.0f * controlScale;
        
        const float contentBottom =
            GetSettingsPanelContentBottom(panel, padding);
        
        const float visibleContentHeight = std::max(1.0f, contentBottom - contentTop);
        
        const float totalContentHeight = rowHeight * static_cast<float>(nextRow + 1);
        
        g_layoutPanelMaxScroll = std::max(0.0f, totalContentHeight - visibleContentHeight);
        
        g_layoutPanelScroll = std::clamp(g_layoutPanelScroll, 0.0f, g_layoutPanelMaxScroll);

        const float scrollbarX = panel.max.x + 6.0f * controlScale;
        
        const float scrollbarTop = contentTop;
        
        const float scrollbarBottom = contentBottom;
        
        g_layoutScrollbarHitbox.min = ImVec2(scrollbarX - 8.0f * controlScale, scrollbarTop);
        
        g_layoutScrollbarHitbox.max = ImVec2(scrollbarX + 8.0f * controlScale, scrollbarBottom);

        if (g_layoutScrollbarDragging && g_layoutPanelMaxScroll > 0.0f)
        {
            const float normalized = std::clamp(
                (g_settingsMousePos.y - scrollbarTop) /
                    std::max(scrollbarBottom - scrollbarTop, 1.0f),
                0.0f, 1.0f);
            g_layoutPanelScroll = normalized * g_layoutPanelMaxScroll;
        }

        draw->PushClipRect(
            ImVec2(panel.min.x, contentTop),
            ImVec2(panel.max.x, contentBottom),
            true);

        const auto hiddenHitbox = [] {
            return LayoutSliderHitbox{ ImVec2(1.0f, 1.0f), ImVec2(0.0f, 0.0f) };
        };
        
        const auto rowRect = [&](int row) {
            const float rowTop = contentTop + rowHeight * static_cast<float>(row) - g_layoutPanelScroll;
            return LayoutSliderHitbox{ ImVec2(x, rowTop + 5.0f * controlScale),
                ImVec2(x + width, rowTop + rowHeight - 5.0f * controlScale) };
        };
        
        const auto drawLargeButton = [&](LayoutSliderHitbox& hitbox, const LayoutSliderHitbox& bounds,
            const char* label, bool active = false) {
            
            hitbox = bounds;
            
            if (bounds.max.y < contentTop || bounds.min.y > contentBottom)
            {
                hitbox = hiddenHitbox();
                return;
            }
            
            const bool hovered = g_settingsMousePos.x >= bounds.min.x && g_settingsMousePos.x <= bounds.max.x &&
                g_settingsMousePos.y >= bounds.min.y && g_settingsMousePos.y <= bounds.max.y;
            
            
            draw->AddRectFilled(bounds.min, bounds.max,
                FadeColor(active ? IM_COL32(235, 235, 235, 70) :
                    hovered ? IM_COL32(65, 65, 70, 235) : IM_COL32(20, 20, 25, 235), alpha), 5.0f * controlScale);
            
            draw->AddRect(bounds.min, bounds.max,
                FadeColor(active ? IM_COL32(215, 195, 150, 235) :
                    IM_COL32(255, 255, 255, hovered ? 190 : 85), alpha), 5.0f * controlScale);
            const ImVec2 size = ImGui::CalcTextSize(label);
            
            DrawTextWithShadow(draw, ImVec2((bounds.min.x + bounds.max.x - size.x) * 0.5f,
                (bounds.min.y + bounds.max.y - size.y) * 0.5f),
                FadeColor(active ? IM_COL32(215, 195, 150, 255)
                                 : IM_COL32(235, 230, 215, 255), alpha), label, alpha);
        };
        {
            
            const auto row = rowRect(presetButtonsRow);
            const float gap = 8.0f * controlScale;
            const float half = (row.max.x - row.min.x - gap) * 0.5f;
            
            
            drawLargeButton(g_layoutSaveButtonHitbox,
                { row.min, ImVec2(row.min.x + half, row.max.y) }, Language::Get("save_layout").c_str(), g_layoutNameOpen);
            drawLargeButton(g_layoutLoadButtonHitbox,
                { ImVec2(row.min.x + half + gap, row.min.y), row.max }, Language::Get("load_layout").c_str(), g_layoutLoadOpen);
        }
        
        
        g_layoutNameOkHitbox = hiddenHitbox();
        
        g_layoutNameCancelHitbox = hiddenHitbox();
        
        if (presetNameRow >= 0)
        {
            const auto field = rowRect(presetNameRow);
            
            draw->AddRectFilled(field.min, field.max, FadeColor(IM_COL32(18, 19, 23, 235), alpha), 4.0f * controlScale);
            
            draw->AddRect(field.min, field.max, FadeColor(IM_COL32(215, 195, 150, 155), alpha), 4.0f * controlScale);
            
            const std::string visibleName = g_layoutPresetName.empty()
                ? Language::Get("layout_name") : g_layoutPresetName;
            

            DrawTextWithShadow(draw, ImVec2(field.min.x + 10.0f * controlScale,
                (field.min.y + field.max.y - ImGui::GetFontSize()) * 0.5f),
                FadeColor(IM_COL32(235, 230, 215, 255), alpha), visibleName.c_str(), alpha);
            

            const auto actions = rowRect(presetNameActionsRow);
            const float gap = 8.0f * controlScale;
            const float half = (actions.max.x - actions.min.x - gap) * 0.5f;
            

            drawLargeButton(g_layoutNameOkHitbox,
                { actions.min, ImVec2(actions.min.x + half, actions.max.y) }, Language::Get("ok").c_str());
            drawLargeButton(g_layoutNameCancelHitbox,
                { ImVec2(actions.min.x + half + gap, actions.min.y), actions.max }, Language::Get("cancel").c_str());
        }
        
        
        g_layoutPresetHitboxes.clear();
        
        if (g_layoutLoadOpen)
        {
            if (layoutPresets.empty())
            {
                const auto bounds = rowRect(presetListFirstRow);
            
                DrawTextWithShadow(draw, ImVec2(bounds.min.x + 6.0f * controlScale,
                    (bounds.min.y + bounds.max.y - ImGui::GetFontSize()) * 0.5f),
                    FadeColor(IM_COL32(165, 160, 150, 220), alpha), Language::Get("no_saved_layouts").c_str(), alpha);
            }
            else
            {
                for (std::size_t i = 0; i < layoutPresets.size(); ++i)
                {
                    LayoutPresetHitbox entry;
            
                    entry.name = layoutPresets[i];
            
                    const auto bounds = rowRect(presetListFirstRow + static_cast<int>(i));
            
                    const float deleteSpace = 33.0f * controlScale;
                    const float gap = 7.0f * controlScale;
            
                    const ImVec2 buttonMax(bounds.max.x - deleteSpace - gap,
                        bounds.max.y);
            
                    drawLargeButton(entry.hitbox,
                        { bounds.min, buttonMax }, entry.name.c_str());

                    const ImVec2 deleteCenter(
                        bounds.max.x - deleteSpace * 0.5f,
                        (bounds.min.y + bounds.max.y) * 0.5f);
            
                    const float deleteRadius = 9.0f * controlScale;
            
                    entry.deleteHitbox.min = ImVec2(deleteCenter.x - deleteRadius - 5.0f,
                        deleteCenter.y - deleteRadius - 5.0f);
                    entry.deleteHitbox.max = ImVec2(deleteCenter.x + deleteRadius + 5.0f,
                        deleteCenter.y + deleteRadius + 5.0f);
            
                    const bool deleteHovered =
                        g_settingsMousePos.x >= entry.deleteHitbox.min.x &&
                        g_settingsMousePos.x <= entry.deleteHitbox.max.x &&
                        g_settingsMousePos.y >= entry.deleteHitbox.min.y &&
                        g_settingsMousePos.y <= entry.deleteHitbox.max.y;
            
                    const int deleteShade = deleteHovered ? 245 : 115;
            
                    draw->AddCircleFilled(deleteCenter, deleteRadius,
                        FadeColor(IM_COL32(deleteShade, deleteShade, deleteShade, 235), alpha), 20);
                    draw->AddCircle(deleteCenter, deleteRadius,
                        FadeColor(IM_COL32(255, 255, 255, deleteHovered ? 235 : 145), alpha), 20, 1.0f);
            
                    const float cross = 3.2f * controlScale;
            
                    draw->AddLine(ImVec2(deleteCenter.x - cross, deleteCenter.y - cross),
                        ImVec2(deleteCenter.x + cross, deleteCenter.y + cross),
                        FadeColor(IM_COL32(25, 25, 28, 255), alpha), 1.3f);
                    draw->AddLine(ImVec2(deleteCenter.x + cross, deleteCenter.y - cross),
                        ImVec2(deleteCenter.x - cross, deleteCenter.y + cross),
                        FadeColor(IM_COL32(25, 25, 28, 255), alpha), 1.3f);
            
                    g_layoutPresetHitboxes.push_back(std::move(entry));
                }
            }
        }

        for (auto& hitbox : g_layoutGroupHitboxes)
            hitbox = { ImVec2(1.0f, 1.0f), ImVec2(0.0f, 0.0f) };
        
        const auto drawSectionTitle = [&](int row, const char* title,
            LayoutGroup group, float indent = 0.0f) {
            
            if (row < 0) return;
            
            const float titleY = contentTop + rowHeight * static_cast<float>(row) - g_layoutPanelScroll +
                (rowHeight - ImGui::GetFontSize()) * 0.5f;
            
            const float titleX = x + indent;
            
            auto& hitbox = g_layoutGroupHitboxes[static_cast<std::size_t>(group)];
            
            hitbox.min = ImVec2(x, contentTop + rowHeight * static_cast<float>(row) - g_layoutPanelScroll);
            
            hitbox.max = ImVec2(x + width, hitbox.min.y + rowHeight);
            
            if (hitbox.max.y < contentTop || hitbox.min.y > contentBottom)
                hitbox = { ImVec2(1.0f, 1.0f), ImVec2(0.0f, 0.0f) };
            
            const bool hovered = g_settingsMousePos.x >= hitbox.min.x &&
                g_settingsMousePos.x <= hitbox.max.x &&
                g_settingsMousePos.y >= hitbox.min.y &&
                g_settingsMousePos.y <= hitbox.max.y;
            
            const bool expanded = groupExpanded(group);
            const float arrowX = titleX + 5.0f * controlScale;
            const float arrowY = titleY + ImGui::GetFontSize() * 0.52f;
        
            if (expanded)
            {
                draw->AddTriangleFilled(
                    ImVec2(arrowX - 4.0f * controlScale, arrowY - 2.0f * controlScale),
                    ImVec2(arrowX + 4.0f * controlScale, arrowY - 2.0f * controlScale),
                    ImVec2(arrowX, arrowY + 3.0f * controlScale),
                    FadeColor(IM_COL32(215, 195, 150, hovered ? 255 : 190), alpha));
            }
            else
            {
                draw->AddTriangleFilled(
                    ImVec2(arrowX - 2.0f * controlScale, arrowY - 4.0f * controlScale),
                    ImVec2(arrowX - 2.0f * controlScale, arrowY + 4.0f * controlScale),
                    ImVec2(arrowX + 3.0f * controlScale, arrowY),
                    FadeColor(IM_COL32(215, 195, 150, hovered ? 255 : 190), alpha));
            }
        
            const float textX = titleX + 16.0f * controlScale;
        
            DrawTextWithShadow(draw, ImVec2(textX, titleY),
                FadeColor(IM_COL32(215, 195, 150, 235), alpha), title, alpha);
        
            const ImVec2 titleSize = ImGui::CalcTextSize(title);
        
            draw->AddLine(ImVec2(textX + titleSize.x + 10.0f * controlScale, titleY + ImGui::GetFontSize() * 0.55f),
                ImVec2(x + width, titleY + ImGui::GetFontSize() * 0.55f),
                FadeColor(IM_COL32(215, 195, 150, 75), alpha), 1.0f);
        };
        
        drawSectionTitle(itemRadialHeaderRow, Language::Get("layout_central").c_str(),
            LayoutGroup::ItemRadial);
        
        if (groupExpanded(LayoutGroup::ItemRadial))
        {
            drawSectionTitle(itemSlotHeaderRow, Language::Get("slot_section").c_str(),
                LayoutGroup::ItemSlot, 14.0f * controlScale);
            drawSectionTitle(itemIconHeaderRow, Language::Get("icon_section").c_str(),
                LayoutGroup::ItemIcon, 14.0f * controlScale);
        }
        
        drawSectionTitle(itemTopHeaderRow, Language::Get("layout_top").c_str(),
            LayoutGroup::ItemTop);
        
        if (groupExpanded(LayoutGroup::ItemTop))
        {
            drawSectionTitle(itemTopSlotHeaderRow, Language::Get("slot_section").c_str(),
                LayoutGroup::ItemTopSlot, 14.0f * controlScale);
            drawSectionTitle(itemTopIconHeaderRow, Language::Get("icon_section").c_str(),
                LayoutGroup::ItemTopIcon, 14.0f * controlScale);
        }
        
        drawSectionTitle(itemBottomHeaderRow, Language::Get("layout_bottom").c_str(),
            LayoutGroup::ItemBottom);
        
        if (groupExpanded(LayoutGroup::ItemBottom))
        {
            drawSectionTitle(itemBottomSlotHeaderRow, Language::Get("slot_section").c_str(),
                LayoutGroup::ItemBottomSlot, 14.0f * controlScale);
            drawSectionTitle(itemBottomIconHeaderRow, Language::Get("icon_section").c_str(),
                LayoutGroup::ItemBottomIcon, 14.0f * controlScale);
        }

        drawSectionTitle(drawHeaderRow, Language::Get("draw_section").c_str(),
            LayoutGroup::Draw);

        drawSectionTitle(itemPreviewHeaderRow, Language::Get("item_section").c_str(),
            LayoutGroup::ItemPreview);
        if (groupExpanded(LayoutGroup::ItemPreview))
        {
            drawSectionTitle(previewMenuHeaderRow, Language::Get("preview_menu_section").c_str(),
                LayoutGroup::PreviewMenu, 14.0f * controlScale);
            drawSectionTitle(previewTopHeaderRow, Language::Get("top_section").c_str(),
                LayoutGroup::PreviewTop, 14.0f * controlScale);
            drawSectionTitle(previewBottomHeaderRow, Language::Get("bottom_section").c_str(),
                LayoutGroup::PreviewBottom, 14.0f * controlScale);
            drawSectionTitle(previewRightHeaderRow, Language::Get("right_section").c_str(),
                LayoutGroup::PreviewRight, 14.0f * controlScale);
            drawSectionTitle(previewLeftHeaderRow, Language::Get("left_section").c_str(),
                LayoutGroup::PreviewLeft, 14.0f * controlScale);
        }
        
        drawSectionTitle(itemOverflowHeaderRow, Language::Get("overflow_section").c_str(),
            LayoutGroup::ItemOverflow);
        
        drawSectionTitle(itemColorsHeaderRow, Language::Get("colors_section").c_str(),
            LayoutGroup::ItemColors);
        
        const auto drawPlainTitle = [&](int row, const char* title) {
            if (row < 0) return;
            const float titleY = contentTop + rowHeight * static_cast<float>(row) -
                g_layoutPanelScroll + (rowHeight - ImGui::GetFontSize()) * 0.5f;
            DrawTextWithShadow(draw, ImVec2(x + 28.0f * controlScale, titleY),
                FadeColor(IM_COL32(205, 190, 155, 225), alpha), title, alpha);
            const ImVec2 titleSize = ImGui::CalcTextSize(title);
            draw->AddLine(ImVec2(x + 38.0f * controlScale + titleSize.x,
                titleY + ImGui::GetFontSize() * 0.55f),
                ImVec2(x + width, titleY + ImGui::GetFontSize() * 0.55f),
                FadeColor(IM_COL32(215, 195, 150, 60), alpha), 1.0f);
        };
        
        drawPlainTitle(potionsTitleRow, Language::Get("potions_title").c_str());
        
        drawPlainTitle(schoolsTitleRow, Language::Get("schools_title").c_str());
        
        drawPlainTitle(enchantsTitleRow, Language::Get("enchants_title").c_str());

        const float fontButtonY = contentTop + 8.0f * controlScale - g_layoutPanelScroll +
            rowHeight * static_cast<float>(fontButtonRow);
        
        const float fontResetArea = 28.0f * controlScale;
        
        g_fontFamilyButtonHitbox.min = ImVec2(x, fontButtonY);
        
        g_fontFamilyButtonHitbox.max = ImVec2(
            x + width - fontResetArea, fontButtonY + 32.0f * controlScale);
        
        const ImVec2 fontResetCenter(
            x + width - 7.0f * controlScale,
            fontButtonY + 16.0f * controlScale);
        
        g_fontFamilyResetHitbox.min = ImVec2(
            fontResetCenter.x - 10.0f * controlScale,
            fontResetCenter.y - 10.0f * controlScale);
        
        g_fontFamilyResetHitbox.max = ImVec2(
            fontResetCenter.x + 10.0f * controlScale,
            fontResetCenter.y + 10.0f * controlScale);
        
        Config::g_fontFamily = std::clamp(Config::g_fontFamily, 0, Font::Count() - 1);
        
        const bool fontHovered =
            g_settingsMousePos.x >= g_fontFamilyButtonHitbox.min.x &&
            g_settingsMousePos.x <= g_fontFamilyButtonHitbox.max.x &&
            g_settingsMousePos.y >= g_fontFamilyButtonHitbox.min.y &&
            g_settingsMousePos.y <= g_fontFamilyButtonHitbox.max.y;
        
        draw->AddRectFilled(g_fontFamilyButtonHitbox.min, g_fontFamilyButtonHitbox.max,
            FadeColor(fontHovered ? IM_COL32(65, 65, 70, 235) : IM_COL32(20, 20, 25, 235), alpha),
            5.0f * controlScale);
        
        draw->AddRect(g_fontFamilyButtonHitbox.min, g_fontFamilyButtonHitbox.max,
            FadeColor(IM_COL32(255, 255, 255, fontHovered ? 190 : 85), alpha),
            5.0f * controlScale, 0, 1.5f);
        
        const char* fontName = Font::Name(Config::g_fontFamily);
        
        const ImVec2 fontNameSize = ImGui::CalcTextSize(fontName);
        
        DrawTextWithShadow(draw,
            ImVec2((g_fontFamilyButtonHitbox.min.x + g_fontFamilyButtonHitbox.max.x - fontNameSize.x) * 0.5f,
                (g_fontFamilyButtonHitbox.min.y + g_fontFamilyButtonHitbox.max.y - fontNameSize.y) * 0.5f),
            FadeColor(IM_COL32(235, 230, 215, 255), alpha),
            fontName, alpha);
        
        const bool fontResetHovered =
            g_settingsMousePos.x >= g_fontFamilyResetHitbox.min.x &&
            g_settingsMousePos.x <= g_fontFamilyResetHitbox.max.x &&
            g_settingsMousePos.y >= g_fontFamilyResetHitbox.min.y &&
            g_settingsMousePos.y <= g_fontFamilyResetHitbox.max.y;
        
        if (fontResetHovered)
            previewButtonTooltip = Language::Get("reset_value").c_str();
        
        const int fontResetShade = fontResetHovered ? 245 : 65;
        
        draw->AddCircleFilled(fontResetCenter, 5.0f * controlScale,
            FadeColor(IM_COL32(fontResetShade, fontResetShade, fontResetShade, 255), alpha), 20);
        
        draw->AddCircle(fontResetCenter, 6.5f * controlScale,
            FadeColor(IM_COL32(135, 132, 126, fontResetHovered ? 220 : 120), alpha), 20, 1.0f);

        const auto drawLayoutToggle = [&](int row, ImVec2& center, float& radius,
            float& hoverT, bool enabled, const char* label) {
            const float centerY = contentTop + 8.0f * controlScale - g_layoutPanelScroll +
                rowHeight * static_cast<float>(row) + rowHeight * 0.5f;
        
            if (centerY < contentTop || centerY > contentBottom)
            {
                center = ImVec2(-10000.0f, -10000.0f);
                radius = 0.0f;
                return;
            }
        
            radius = 12.0f * controlScale;
        
            center = ImVec2(x + 13.0f * controlScale, centerY);
        
            const float dx = g_settingsMousePos.x - center.x;
        
            const float dy = g_settingsMousePos.y - center.y;
        
            const bool hovered = dx * dx + dy * dy <= radius * radius;
        
            hoverT = AnimateSettingsValue(hoverT, hovered ? 1.0f : 0.0f,
                12.0f, ImGui::GetIO().DeltaTime);
        
            draw->AddCircleFilled(center, radius + hoverT * 2.0f,
                FadeColor(enabled ? IM_COL32(235, 235, 235, 140)
                    : IM_COL32(20, 20, 25, 235), alpha), 48);
        
            draw->AddCircle(center, radius + hoverT * 2.0f,
                FadeColor(IM_COL32(255, 255, 255,
                    static_cast<int>(90.0f + 140.0f * hoverT)), alpha), 48, 1.5f);
        
            DrawTextWithShadow(draw,
                ImVec2(center.x + radius + 12.0f * controlScale,
                    center.y - ImGui::GetFontSize() * 0.5f),
                FadeColor(IM_COL32(235, 230, 215, 255), alpha), label, alpha);
        };
        
        g_gameplayPreviewButtonRadius = 0.0f;
        
        g_gameplayDescriptionButtonRadius = 0.0f;
        
        g_showItemQuantityButtonRadius = 0.0f;
        
        g_showOverflowIconButtonRadius = 0.0f;
        
        g_stardustButtonRadius = 0.0f;
        
        g_gameplayPreviewButtonCenter = ImVec2(-10000.0f, -10000.0f);
        
        g_gameplayDescriptionButtonCenter = ImVec2(-10000.0f, -10000.0f);
        
        g_showItemQuantityButtonCenter = ImVec2(-10000.0f, -10000.0f);
        
        g_showOverflowIconButtonCenter = ImVec2(-10000.0f, -10000.0f);
        
        g_stardustButtonCenter = ImVec2(-10000.0f, -10000.0f);
        
        drawLayoutToggle(itemToggleRows[0], g_gameplayPreviewButtonCenter,
            g_gameplayPreviewButtonRadius, g_gameplayPreviewHoverT,
            Config::g_showItemPreviewGameplay, Language::Get("show_item_preview").c_str());
        
        drawLayoutToggle(itemToggleRows[1], g_gameplayDescriptionButtonCenter,
            g_gameplayDescriptionButtonRadius, g_gameplayDescriptionHoverT,
            Config::g_showGameplayDescription, Language::Get("show_item_information").c_str());
        
        drawLayoutToggle(itemToggleRows[2], g_showItemQuantityButtonCenter,
            g_showItemQuantityButtonRadius, g_showItemQuantityHoverT,
            Config::g_showItemQuantity, Language::Get("show_item_quantity").c_str());
        
        drawLayoutToggle(itemToggleRows[3], g_showOverflowIconButtonCenter,
            g_showOverflowIconButtonRadius, g_showOverflowIconHoverT,
            Config::g_showOverflowIcon, Language::Get("show_overflow_icon").c_str());
        
        if (stardustToggleRow >= 0)
        {
            drawLayoutToggle(stardustToggleRow, g_stardustButtonCenter,
                g_stardustButtonRadius, g_stardustButtonHoverT,
                Config::g_stardustEnabled, Language::Get("stardust").c_str());
        }

        for (auto& hitbox : g_layoutColorHitboxes)
            hitbox = { ImVec2(1.0f, 1.0f), ImVec2(0.0f, 0.0f) };
        
        for (auto& hitbox : g_layoutColorResetHitboxes)
            hitbox = { ImVec2(1.0f, 1.0f), ImVec2(0.0f, 0.0f) };
        
        g_layoutColorPickerHitbox = {
            ImVec2(1.0f, 1.0f), ImVec2(0.0f, 0.0f) };
        
        const auto unpackColor = [](std::uint32_t color) {
            return ImVec4(
                static_cast<float>((color >> 16) & 0xFF) / 255.0f,
                static_cast<float>((color >> 8) & 0xFF) / 255.0f,
                static_cast<float>(color & 0xFF) / 255.0f, 1.0f);
        };
        
        const auto packColor = [](float r, float g, float b) {
            return (static_cast<std::uint32_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f) << 16) |
                (static_cast<std::uint32_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f) << 8) |
                static_cast<std::uint32_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
        };
        
        const auto drawColorControl = [&](int row, LayoutColorControl control,
            const char* label, std::uint32_t color) {
            
            if (row < 0) return;
            
            const float rowY = contentTop + 8.0f * controlScale - g_layoutPanelScroll +
                rowHeight * static_cast<float>(row);
            
            auto& hitbox = g_layoutColorHitboxes[static_cast<std::size_t>(control)];
            
            
            hitbox.min = ImVec2(trackMinX, rowY);
            hitbox.max = ImVec2(trackMaxX, rowY + 30.0f * controlScale);
            
            
            if (hitbox.max.y < contentTop || hitbox.min.y > contentBottom)
            {
                hitbox = { ImVec2(1.0f, 1.0f), ImVec2(0.0f, 0.0f) };
                return;
            }
        
            const bool hovered = g_settingsMousePos.x >= hitbox.min.x &&
                g_settingsMousePos.x <= hitbox.max.x &&
                g_settingsMousePos.y >= hitbox.min.y &&
                g_settingsMousePos.y <= hitbox.max.y;
        
            DrawTextWithShadow(draw, ImVec2(x, rowY),
                FadeColor(IM_COL32(235, 230, 215, 255), alpha), label, alpha);
        
            const float swatchRadius = 11.0f * controlScale;
        
            const ImVec2 swatch(trackMaxX - swatchRadius,
                rowY + ImGui::GetFontSize() * 0.5f);
        
            const ImVec4 rgb = unpackColor(color);
        
            draw->AddCircleFilled(swatch, swatchRadius,
                FadeColor(ImGui::ColorConvertFloat4ToU32(rgb), alpha), 32);
        
            draw->AddCircle(swatch, swatchRadius + 1.5f,
                FadeColor(IM_COL32(255, 255, 255, hovered ? 220 : 110), alpha), 32, 1.5f);
        

            const ImVec2 resetCenter(x + width - 7.0f * controlScale,
                rowY + ImGui::GetFontSize() * 0.5f);
        
            auto& reset = g_layoutColorResetHitboxes[static_cast<std::size_t>(control)];
        

            reset.min = ImVec2(resetCenter.x - 8.0f * controlScale,
                resetCenter.y - 8.0f * controlScale);
        
            reset.max = ImVec2(resetCenter.x + 8.0f * controlScale,
                resetCenter.y + 8.0f * controlScale);
        
            const bool resetHovered = g_settingsMousePos.x >= reset.min.x &&
                g_settingsMousePos.x <= reset.max.x &&
                g_settingsMousePos.y >= reset.min.y &&
                g_settingsMousePos.y <= reset.max.y;
        
            if (resetHovered)
                previewButtonTooltip = Language::Get("reset_value").c_str();
        
        
            draw->AddCircleFilled(resetCenter, 5.0f * controlScale,
                FadeColor(resetHovered ? IM_COL32(250, 250, 250, 255)
                    : IM_COL32(70, 70, 75, 220), alpha), 20);
        
            draw->AddCircle(resetCenter, 6.5f * controlScale,
                FadeColor(IM_COL32(120, 118, 112, 170), alpha), 20, 1.0f);
        };



        drawColorControl(backgroundColorRow, LayoutColorControl::Background,
            Language::Get("background_color").c_str(), Config::g_itemBackgroundColor);
        drawColorControl(borderColorRow, LayoutColorControl::Border,
            Language::Get("border_color").c_str(), Config::g_itemBorderColor);
        drawColorControl(iconColorRow, LayoutColorControl::Icon,
            Language::Get("base_icon_color").c_str(), Config::g_baseIconColor);
        drawColorControl(overflowBackgroundColorRow, LayoutColorControl::OverflowBackground,
            Language::Get("overflow_background_color").c_str(), Config::g_overflowBackgroundColor);
        drawColorControl(overflowBorderColorRow, LayoutColorControl::OverflowBorder,
            Language::Get("overflow_border_color").c_str(), Config::g_overflowBorderColor);
        drawColorControl(topBackgroundColorRow, LayoutColorControl::TopBackground,
            Language::Get("background_color").c_str(), Config::g_topItemStyle.backgroundColor);
        drawColorControl(topBorderColorRow, LayoutColorControl::TopBorder,
            Language::Get("border_color").c_str(), Config::g_topItemStyle.borderColor);
        drawColorControl(topIconColorRow, LayoutColorControl::TopIcon,
            Language::Get("base_icon_color").c_str(), Config::g_topItemStyle.iconColor);
        drawColorControl(bottomBackgroundColorRow, LayoutColorControl::BottomBackground,
            Language::Get("background_color").c_str(), Config::g_bottomItemStyle.backgroundColor);
        drawColorControl(bottomBorderColorRow, LayoutColorControl::BottomBorder,
            Language::Get("border_color").c_str(), Config::g_bottomItemStyle.borderColor);
        drawColorControl(bottomIconColorRow, LayoutColorControl::BottomIcon,
            Language::Get("base_icon_color").c_str(), Config::g_bottomItemStyle.iconColor);



        static constexpr std::array<const char*, 7> potionLabels{
            "potion_health", "potion_stamina", "potion_magicka", "potion_poison",
            "potion_fire", "potion_frost", "potion_shock"
        };


        static constexpr std::array<const char*, 8> schoolLabels{
            "school_alteration", "school_conjuration", "school_destruction",
            "school_illusion", "school_restoration", "magic_fire", "magic_frost", "magic_shock"
        };


        static constexpr std::array<const char*, 5> enchantLabels{
            "enchant_fire", "enchant_frost", "enchant_shock", "enchant_poison",
            "enchant_default"
        };
        
        
        
        for (std::size_t i = 0; i < potionColorRows.size(); ++i)
            drawColorControl(potionColorRows[i], static_cast<LayoutColorControl>(
                static_cast<std::size_t>(LayoutColorControl::PotionHealth) + i),
                Language::Get(potionLabels[i]).c_str(), Config::g_potionColors[i]);
        
        
        for (std::size_t i = 0; i < schoolColorRows.size(); ++i)
            drawColorControl(schoolColorRows[i], static_cast<LayoutColorControl>(
                static_cast<std::size_t>(LayoutColorControl::SchoolAlteration) + i),
                Language::Get(schoolLabels[i]).c_str(), i < Config::g_schoolColors.size()
                    ? Config::g_schoolColors[i]
                    : Config::g_magicElementColors[i - Config::g_schoolColors.size()]);
        
        
        for (std::size_t i = 0; i < enchantColorRows.size(); ++i)
            drawColorControl(enchantColorRows[i], static_cast<LayoutColorControl>(
                static_cast<std::size_t>(LayoutColorControl::EnchantFire) + i),
                Language::Get(enchantLabels[i]).c_str(), Config::g_enchantColors[i]);


        if (colorPickerRow >= 0 && g_openLayoutColor >= 0)
        {
            
            const float pickerTop = contentTop + rowHeight * static_cast<float>(colorPickerRow) -
                g_layoutPanelScroll;
            const float pickerRadius = std::min(width * 0.25f, rowHeight * 1.15f);
            

            const ImVec2 pickerCenter(x + width * 0.5f,
                pickerTop + rowHeight * 1.35f);
            
            g_layoutColorPickerHitbox.min = ImVec2(
                pickerCenter.x - pickerRadius, pickerCenter.y - pickerRadius);
            
            g_layoutColorPickerHitbox.max = ImVec2(
                pickerCenter.x + pickerRadius, pickerCenter.y + pickerRadius);
            
            constexpr int segments = 48;
            
            constexpr int rings = 14;
            
            for (int ring = 0; ring < rings; ++ring)
            {
                const float innerRadius = pickerRadius * static_cast<float>(ring) / rings;
            
                const float outerRadius = pickerRadius * static_cast<float>(ring + 1) / rings;
            
                const float saturationRing = (static_cast<float>(ring) + 0.5f) / rings;
            
            
                for (int segment = 0; segment < segments; ++segment)
                {
            
                    const float a0 = 2.0f * PI * static_cast<float>(segment) / segments;
                    const float a1 = 2.0f * PI * static_cast<float>(segment + 1) / segments;
            
                    float r, g, b;
            
                    ImGui::ColorConvertHSVtoRGB(
                        (static_cast<float>(segment) + 0.5f) / segments,
                        saturationRing, 1.0f, r, g, b);
            
                    draw->AddQuadFilled(
                        ImVec2(pickerCenter.x + std::cos(a0) * innerRadius,
                            pickerCenter.y + std::sin(a0) * innerRadius),
                        ImVec2(pickerCenter.x + std::cos(a0) * outerRadius,
                            pickerCenter.y + std::sin(a0) * outerRadius),
                        ImVec2(pickerCenter.x + std::cos(a1) * outerRadius,
                            pickerCenter.y + std::sin(a1) * outerRadius),
                        ImVec2(pickerCenter.x + std::cos(a1) * innerRadius,
                            pickerCenter.y + std::sin(a1) * innerRadius),
                        FadeColor(IM_COL32(static_cast<int>(r * 255),
                            static_cast<int>(g * 255), static_cast<int>(b * 255), 255), alpha));
                }
            }
            
            const auto openControl = static_cast<LayoutColorControl>(g_openLayoutColor);
            
            std::uint32_t* selectedColor = nullptr;
            
            switch (openControl)
            {
            case LayoutColorControl::Background: selectedColor = &Config::g_itemBackgroundColor; break;
            case LayoutColorControl::Border: selectedColor = &Config::g_itemBorderColor; break;
            case LayoutColorControl::Icon: selectedColor = &Config::g_baseIconColor; break;
            case LayoutColorControl::OverflowBackground: selectedColor = &Config::g_overflowBackgroundColor; break;
            case LayoutColorControl::OverflowBorder: selectedColor = &Config::g_overflowBorderColor; break;
            case LayoutColorControl::TopBackground: selectedColor = &Config::g_topItemStyle.backgroundColor; break;
            case LayoutColorControl::TopBorder: selectedColor = &Config::g_topItemStyle.borderColor; break;
            case LayoutColorControl::TopIcon: selectedColor = &Config::g_topItemStyle.iconColor; break;
            case LayoutColorControl::BottomBackground: selectedColor = &Config::g_bottomItemStyle.backgroundColor; break;
            case LayoutColorControl::BottomBorder: selectedColor = &Config::g_bottomItemStyle.borderColor; break;
            case LayoutColorControl::BottomIcon: selectedColor = &Config::g_bottomItemStyle.iconColor; break;
            
            default:
            
                if (openControl >= LayoutColorControl::PotionHealth && openControl <= LayoutColorControl::PotionShock)
                    selectedColor = &Config::g_potionColors[static_cast<std::size_t>(openControl) -
                        static_cast<std::size_t>(LayoutColorControl::PotionHealth)];
                else if (openControl >= LayoutColorControl::SchoolAlteration && openControl <= LayoutColorControl::SchoolRestoration)
                    selectedColor = &Config::g_schoolColors[static_cast<std::size_t>(openControl) -
                        static_cast<std::size_t>(LayoutColorControl::SchoolAlteration)];
                else if (openControl >= LayoutColorControl::MagicFire && openControl <= LayoutColorControl::MagicShock)
                    selectedColor = &Config::g_magicElementColors[static_cast<std::size_t>(openControl) -
                        static_cast<std::size_t>(LayoutColorControl::MagicFire)];
                else if (openControl >= LayoutColorControl::EnchantFire && openControl <= LayoutColorControl::EnchantDefault)
                    selectedColor = &Config::g_enchantColors[static_cast<std::size_t>(openControl) -
                        static_cast<std::size_t>(LayoutColorControl::EnchantFire)];
                break;
            }
            
            if (!selectedColor) selectedColor = &Config::g_baseIconColor;
            
            float hue = 0.0f, saturation = 0.0f, value = 0.0f;
            
            const ImVec4 current = unpackColor(*selectedColor);
            
            ImGui::ColorConvertRGBtoHSV(current.x, current.y, current.z,
                hue, saturation, value);
            if (g_activeLayoutColor == g_openLayoutColor)
            {
                const float dx = g_settingsMousePos.x - pickerCenter.x;
                const float dy = g_settingsMousePos.y - pickerCenter.y;
                saturation = std::clamp(std::sqrt(dx * dx + dy * dy) /
                    std::max(pickerRadius, 1.0f), 0.0f, 1.0f);
                hue = std::atan2(dy, dx) / (2.0f * PI);
                if (hue < 0.0f) hue += 1.0f;
                float r, g, b;
                ImGui::ColorConvertHSVtoRGB(hue, saturation, 1.0f, r, g, b);
                *selectedColor = packColor(r, g, b);
            }

            const float markerAngle = hue * 2.0f * PI;
            
            const ImVec2 marker(pickerCenter.x + std::cos(markerAngle) * pickerRadius * saturation,
                pickerCenter.y + std::sin(markerAngle) * pickerRadius * saturation);
            
            draw->AddCircle(marker, 5.0f * controlScale,
                FadeColor(IM_COL32(15, 15, 18, 255), alpha), 24, 2.5f);
            draw->AddCircle(marker, 7.0f * controlScale,
                FadeColor(IM_COL32(255, 255, 255, 230), alpha), 24, 1.0f);
        }

        for (std::size_t i = 0; i < std::size(sliders); ++i)
        {
            if (!g_layoutSliderVisible[i])
            {
                g_layoutSliderHitboxes[i] = {
                    ImVec2(1.0f, 1.0f), ImVec2(0.0f, 0.0f) };
                g_layoutResetHitboxes[i] = g_layoutSliderHitboxes[i];
                g_layoutCopyHitboxes[i] = g_layoutSliderHitboxes[i];
                g_layoutMirrorHitboxes[i] = g_layoutSliderHitboxes[i];
                continue;
            }
    
            const LayoutSlider sliderKind = static_cast<LayoutSlider>(i);
    
            Config::ItemPreviewProfile previewProfile{};
            PreviewLayoutField previewField{};
    
            const bool previewControl = GetPreviewSliderInfo(
                sliderKind, previewProfile, previewField);
    
            const bool hasCopy = previewControl &&
                previewProfile != Config::ItemPreviewProfile::Menu;
    
            const bool hasMirror = hasCopy && PreviewFieldSupportsMirror(previewField);
    
            const int auxiliaryButtons = (hasCopy ? 1 : 0) + (hasMirror ? 1 : 0);
    
            const float rowTrackMaxX = trackMaxX -
                static_cast<float>(auxiliaryButtons) * 24.0f * controlScale;
    
            const float rowY = contentTop + 8.0f * controlScale - g_layoutPanelScroll +
                rowHeight * static_cast<float>(g_layoutSliderRows[i]);
    
            const float trackY = rowY + ImGui::GetFontSize() + 10.0f * controlScale;
    
            auto& hitbox = g_layoutSliderHitboxes[i];
    
            hitbox.min = ImVec2(trackMinX, trackY - trackHeight * 0.5f - 3.0f * controlScale);
            hitbox.max = ImVec2(rowTrackMaxX, trackY + trackHeight * 0.5f + 3.0f * controlScale);

            const ImVec2 resetCenter(x + width - resetRadius - 2.0f, trackY);
    
            auto& resetHitbox = g_layoutResetHitboxes[i];
    
            resetHitbox.min = ImVec2(resetCenter.x - 10.0f * controlScale, resetCenter.y - 10.0f * controlScale);
            resetHitbox.max = ImVec2(resetCenter.x + 10.0f * controlScale, resetCenter.y + 10.0f * controlScale);

            auto hideAuxiliary = [&] {
                g_layoutCopyHitboxes[i] = {
                    ImVec2(1.0f, 1.0f), ImVec2(0.0f, 0.0f) };
                g_layoutMirrorHitboxes[i] = g_layoutCopyHitboxes[i];
            };
            hideAuxiliary();
            ImVec2 copyCenter{};
            ImVec2 mirrorCenter{};
            if (hasCopy)
            {
                const int copySlot = hasMirror ? 2 : 1;
                copyCenter = ImVec2(resetCenter.x -
                    24.0f * controlScale * copySlot, trackY);
                auto& copyHitbox = g_layoutCopyHitboxes[i];
                copyHitbox.min = ImVec2(copyCenter.x - 10.0f * controlScale,
                    copyCenter.y - 10.0f * controlScale);
                copyHitbox.max = ImVec2(copyCenter.x + 10.0f * controlScale,
                    copyCenter.y + 10.0f * controlScale);
            }
            if (hasMirror)
            {
                mirrorCenter = ImVec2(resetCenter.x - 24.0f * controlScale, trackY);
                auto& mirrorHitbox = g_layoutMirrorHitboxes[i];
                mirrorHitbox.min = ImVec2(mirrorCenter.x - 10.0f * controlScale,
                    mirrorCenter.y - 10.0f * controlScale);
                mirrorHitbox.max = ImVec2(mirrorCenter.x + 10.0f * controlScale,
                    mirrorCenter.y + 10.0f * controlScale);
            }

            const bool rowVisible =
                hitbox.max.y >= contentTop && hitbox.min.y <= contentBottom;
            if (!rowVisible)
            {
                hitbox = { ImVec2(1.0f, 1.0f), ImVec2(0.0f, 0.0f) };
                resetHitbox = hitbox;
                hideAuxiliary();
            }
            else if (g_gamepadSettingsNeedsCursorSync &&
                static_cast<int>(i) == g_gamepadSettingsNavigationIndex)
            {
                g_settingsMousePos = ImVec2(
                    (hitbox.min.x + hitbox.max.x) * 0.5f,
                    (hitbox.min.y + hitbox.max.y) * 0.5f);
                ImGui::GetIO().MousePos = g_settingsMousePos;
                g_gamepadSettingsNeedsCursorSync = false;
            }

            if (g_activeLayoutSlider == static_cast<LayoutSlider>(i))
            {
    
                const float normalized = std::clamp(
                    (g_settingsMousePos.x - trackMinX) /
                    std::max(rowTrackMaxX - trackMinX, 1.0f),
                    0.0f, 1.0f);
                *sliders[i].value = sliders[i].minimum +
                    (sliders[i].maximum - sliders[i].minimum) * normalized;
    
                std::size_t activePreviewCategoryIndex = 0;
    
                if (GetPreviewCategorySliderInfo(
                        static_cast<LayoutSlider>(i),
                        activePreviewCategoryIndex))
                {
                    // O mesmo modelo permanece carregado durante o drag;
                    // força somente a renormalização para refletir o novo
                    // multiplicador continuamente.
                    ItemPreview::InvalidateSizeScale();
                }
    
                if (static_cast<LayoutSlider>(i) == LayoutSlider::RadialQuantity)
                {
                    *sliders[i].value = std::round(*sliders[i].value);
                    const float requiredStretch = std::max(
                        0.0f, (*sliders[i].value - 25.0f) * (80.0f / 25.0f));
                    Config::g_radialStretch = std::max(Config::g_radialStretch, requiredStretch);
                }
                else if (static_cast<LayoutSlider>(i) == LayoutSlider::RadialStretch)
                {
                    const float maximumQuantity = 25.0f +
                        std::clamp(Config::g_radialStretch, 0.0f, 80.0f) * (25.0f / 80.0f);
                    Config::g_radialQuantity = std::min(
                        Config::g_radialQuantity, std::floor(maximumQuantity));
                }
                else if (static_cast<LayoutSlider>(i) == LayoutSlider::RadialRotation)
                {
                    Track::SetRadialRotation(*sliders[i].value * 0.01745329252f);
                }
                else if (static_cast<LayoutSlider>(i) == LayoutSlider::RadialLineOpacity)
                {
                    Track::SetRadialLineOpacity(*sliders[i].value);
                }
                else if (static_cast<LayoutSlider>(i) == LayoutSlider::OverflowLineOpacity)
                {
                    Track::SetLineOpacity(*sliders[i].value);
                }

                if (previewControl &&
                    (previewField == PreviewLayoutField::ItemPositionX ||
                     previewField == PreviewLayoutField::ItemPositionY))
                {
    
                    auto& preview = Config::GetItemPreviewLayout(previewProfile);
    
                    const WheelLayout wheelLayout = GetWheelLayout();
    
                    const ImVec2 radialCenter = PreviewRadialCenter(
                        previewProfile, wheelLayout);
    
                    const ImVec2 previewPosition(
                        LayoutLerp(wheelLayout.min.x, wheelLayout.max.x,
                            preview.itemPositionX),
                        LayoutLerp(wheelLayout.min.y, wheelLayout.max.y,
                            preview.itemPositionY));
    
                    const float dx = previewPosition.x - radialCenter.x;
    
                    const float dy = previewPosition.y - radialCenter.y;
    
                    const float snapRadius = 44.0f * controlScale;
    
                    if (dx * dx + dy * dy <= snapRadius * snapRadius)
                    {
                        const float layoutWidth = std::max(
                            wheelLayout.max.x - wheelLayout.min.x, 1.0f);
                        const float layoutHeight = std::max(
                            wheelLayout.max.y - wheelLayout.min.y, 1.0f);
                        preview.itemPositionX = std::clamp(
                            (radialCenter.x - wheelLayout.min.x) /
                                layoutWidth * 100.0f,
                            0.0f, 100.0f);
                        preview.itemPositionY = std::clamp(
                            (radialCenter.y - wheelLayout.min.y) /
                                layoutHeight * 100.0f,
                            0.0f, 100.0f);
                    }
                }
            }

            const bool hovered =
                g_settingsMousePos.x >= hitbox.min.x && g_settingsMousePos.x <= hitbox.max.x &&
                g_settingsMousePos.y >= hitbox.min.y && g_settingsMousePos.y <= hitbox.max.y;
    
            const bool resetHovered =
                g_settingsMousePos.x >= resetHitbox.min.x && g_settingsMousePos.x <= resetHitbox.max.x &&
                g_settingsMousePos.y >= resetHitbox.min.y && g_settingsMousePos.y <= resetHitbox.max.y;
    
            if (resetHovered)
                previewButtonTooltip = Language::Get("reset_value").c_str();
    
            const float t = std::clamp(
                (*sliders[i].value - sliders[i].minimum) /
                std::max(sliders[i].maximum - sliders[i].minimum, 0.001f),
                0.0f, 1.0f);
    
            const float fillX = trackMinX + (rowTrackMaxX - trackMinX) * t;

            DrawTextWithShadow(draw, ImVec2(x, rowY),
                FadeColor(IM_COL32(235, 230, 215, 255), alpha), sliders[i].label, alpha);

            std::size_t previewCategoryIndex = 0;
    
            const bool previewCategoryMultiplier =
                GetPreviewCategorySliderInfo(sliderKind, previewCategoryIndex);
    
            const std::string valueText = sliderKind == LayoutSlider::FontSize ||
                previewCategoryMultiplier
                ? std::format("{:.2f}x", *sliders[i].value)
                : sliderKind == LayoutSlider::RadialRotation
                    ? std::format("{:.0f}°", *sliders[i].value)
                : sliderKind == LayoutSlider::RadialQuantity ||
                    sliderKind == LayoutSlider::TopItemQuantity ||
                    sliderKind == LayoutSlider::BottomItemQuantity
                    ? std::format("{:.0f}", *sliders[i].value)
                    : std::format("{:.0f}%", *sliders[i].value);
    
                const ImVec2 valueSize = ImGui::CalcTextSize(valueText.c_str());
    
            DrawTextWithShadow(draw, ImVec2(rowTrackMaxX - valueSize.x, rowY),
                FadeColor(IM_COL32(180, 165, 130, 230), alpha), valueText.c_str(), alpha);

            draw->AddRectFilled(ImVec2(trackMinX, trackY - trackHeight * 0.5f),
                ImVec2(rowTrackMaxX, trackY + trackHeight * 0.5f),
                FadeColor(IM_COL32(28, 29, 34, 235), alpha), 4.0f);
    
            draw->AddRectFilled(ImVec2(trackMinX, trackY - trackHeight * 0.5f),
                ImVec2(fillX, trackY + trackHeight * 0.5f),
                FadeColor(hovered ? IM_COL32(235, 230, 215, 225) : IM_COL32(190, 184, 170, 205), alpha),
                4.0f);
    
            draw->AddRect(ImVec2(trackMinX, trackY - trackHeight * 0.5f),
                ImVec2(rowTrackMaxX, trackY + trackHeight * 0.5f),
                FadeColor(IM_COL32(115, 112, 106, hovered ? 210 : 135), alpha), 4.0f, 0, 1.0f);

            g_layoutResetFlash[i] = std::max(0.0f, g_layoutResetFlash[i] - dt * 3.5f);
    
            const float resetLight = std::max(g_layoutResetFlash[i], resetHovered ? 0.45f : 0.0f);
    
            const int resetShade = static_cast<int>(55.0f + 200.0f * resetLight);
    
            draw->AddCircleFilled(resetCenter, resetRadius,
                FadeColor(IM_COL32(resetShade, resetShade, resetShade, 255), alpha), 20);
    
            draw->AddCircle(resetCenter, resetRadius + 1.5f,
                FadeColor(IM_COL32(125, 122, 116, 150), alpha), 20, 1.0f);

            const auto drawAuxiliaryDot = [&](const ImVec2& center,
                LayoutSliderHitbox& button, float& flash, const char* tooltip) {
    
                if (button.max.x < button.min.x) return;
    
                const bool buttonHovered =
                    g_settingsMousePos.x >= button.min.x && g_settingsMousePos.x <= button.max.x &&
                    g_settingsMousePos.y >= button.min.y && g_settingsMousePos.y <= button.max.y;
    
                flash = std::max(0.0f, flash - dt * 3.5f);
    
                const float light = std::max(flash, buttonHovered ? 0.45f : 0.0f);
    
                const int shade = static_cast<int>(55.0f + 200.0f * light);
    
                draw->AddCircleFilled(center, resetRadius,
                    FadeColor(IM_COL32(shade, shade, shade, 255), alpha), 20);
    
                draw->AddCircle(center, resetRadius + 1.5f,
                    FadeColor(IM_COL32(125, 122, 116, 150), alpha), 20, 1.0f);
    
                if (buttonHovered)
                    previewButtonTooltip = tooltip;
            };
    
            if (hasCopy)
                drawAuxiliaryDot(copyCenter, g_layoutCopyHitboxes[i],
                    g_layoutCopyFlash[i], Language::Get("copy_opposite").c_str());
    
                if (hasMirror)
                drawAuxiliaryDot(mirrorCenter, g_layoutMirrorHitboxes[i],
                    g_layoutMirrorFlash[i], Language::Get("mirror_opposite").c_str());
        }

        draw->PopClipRect();

        draw->AddRectFilled(
            ImVec2(scrollbarX - 2.0f * controlScale, scrollbarTop),
            ImVec2(scrollbarX + 2.0f * controlScale, scrollbarBottom),
            FadeColor(IM_COL32(30, 31, 36, 220), alpha), 3.0f * controlScale);

        const float thumbHeight = std::max(
            22.0f * controlScale,
            (scrollbarBottom - scrollbarTop) *
                std::clamp(visibleContentHeight / std::max(totalContentHeight, 1.0f), 0.0f, 1.0f));
    
            const float thumbTravel = std::max(0.0f,
            (scrollbarBottom - scrollbarTop) - thumbHeight);
    
        const float scrollT = g_layoutPanelMaxScroll > 0.0f
            ? g_layoutPanelScroll / g_layoutPanelMaxScroll
            : 0.0f;
    
        const float thumbTop = scrollbarTop + thumbTravel * scrollT;
    
        const bool scrollbarHovered =
            g_settingsMousePos.x >= g_layoutScrollbarHitbox.min.x &&
            g_settingsMousePos.x <= g_layoutScrollbarHitbox.max.x &&
            g_settingsMousePos.y >= g_layoutScrollbarHitbox.min.y &&
            g_settingsMousePos.y <= g_layoutScrollbarHitbox.max.y;
    
        draw->AddRectFilled(
            ImVec2(scrollbarX - 4.0f * controlScale, thumbTop),
            ImVec2(scrollbarX + 4.0f * controlScale, thumbTop + thumbHeight),
            FadeColor(scrollbarHovered || g_layoutScrollbarDragging
                ? IM_COL32(235, 230, 215, 230)
                : IM_COL32(95, 94, 98, 220), alpha),
            4.0f * controlScale);

        // Tooltip por último para permanecer acima do conteúdo e também da
        // barra de rolagem do painel.
        if (previewButtonTooltip)
        {
    
            const ImVec2 textSize = ImGui::CalcTextSize(previewButtonTooltip);
    
            const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    
            const float tooltipPadding = 7.0f * controlScale;
    
            ImVec2 tooltipMin(
                g_settingsMousePos.x + 14.0f * controlScale,
                g_settingsMousePos.y + 14.0f * controlScale);
    
            tooltipMin.x = std::clamp(tooltipMin.x, 4.0f,
                std::max(4.0f, displaySize.x - textSize.x - tooltipPadding * 2.0f - 4.0f));
    
            tooltipMin.y = std::clamp(tooltipMin.y, 4.0f,
                std::max(4.0f, displaySize.y - textSize.y - tooltipPadding * 2.0f - 4.0f));
    
            const ImVec2 tooltipMax(
                tooltipMin.x + textSize.x + tooltipPadding * 2.0f,
                tooltipMin.y + textSize.y + tooltipPadding * 2.0f);
    
            draw->AddRectFilled(tooltipMin, tooltipMax,
                FadeColor(IM_COL32(13, 14, 18, 245), alpha), 4.0f * controlScale);
    
            draw->AddRect(tooltipMin, tooltipMax,
                FadeColor(IM_COL32(180, 176, 165, 180), alpha), 4.0f * controlScale);
    
            DrawTextWithShadow(draw,
                ImVec2(tooltipMin.x + tooltipPadding,
                    tooltipMin.y + tooltipPadding),
                FadeColor(IM_COL32(240, 237, 226, 255), alpha),
                previewButtonTooltip, alpha);
        }
    }

    struct SettingsBackdropTextureCache
    {
        ID3D11ShaderResourceView* view{};
        ImVec2 minimum{};
        ImVec2 maximum{};
        ImVec2 previewCenter{};
        ImVec2 innerRadius{};
        ImVec2 outerRadius{};
        int width{};
        int height{};

        ~SettingsBackdropTextureCache()
        {
            if (view) view->Release();
        }
    };

    static ID3D11ShaderResourceView* GetSettingsBackdropTexture(
        const ImVec2& minimum, const ImVec2& maximum,
        const ImVec2& previewCenter, const ImVec2& innerRadius,
        const ImVec2& outerRadius, ImVec2& uvMinimum, ImVec2& uvMaximum)
    {
    
        static SettingsBackdropTextureCache cache;
    
        constexpr float cellSize = 8.0f;
    
        const int columns = std::max(1, static_cast<int>(
            std::ceil((maximum.x - minimum.x) / cellSize)));
    
        const int rows = std::max(1, static_cast<int>(
            std::ceil((maximum.y - minimum.y) / cellSize)));
    
        const int width = columns + 1;
    
        const int height = rows + 1;
    
        uvMinimum = ImVec2(0.5f / width, 0.5f / height);
    
        uvMaximum = ImVec2(
            (static_cast<float>(width) - 0.5f) / width,
            (static_cast<float>(height) - 0.5f) / height);
    
        const bool unchanged = cache.view && cache.minimum.x == minimum.x &&
            cache.minimum.y == minimum.y && cache.maximum.x == maximum.x &&
            cache.maximum.y == maximum.y &&
            cache.previewCenter.x == previewCenter.x &&
            cache.previewCenter.y == previewCenter.y &&
            cache.innerRadius.x == innerRadius.x &&
            cache.innerRadius.y == innerRadius.y &&
            cache.outerRadius.x == outerRadius.x &&
            cache.outerRadius.y == outerRadius.y &&
            cache.width == width && cache.height == height;
    
        if (unchanged) return cache.view;

        if (cache.view)
        {
            cache.view->Release();
            cache.view = nullptr;
        }
    
        auto* device = RenderManager::GetDevice();
        if (!device) return nullptr;

        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(width * height * 4));
    
        const float innerRatio = std::min(
            innerRadius.x / outerRadius.x, innerRadius.y / outerRadius.y);
    
        for (int row = 0; row < height; ++row)
        {
            const float y = std::min(minimum.y + row * cellSize, maximum.y);
            for (int column = 0; column < width; ++column)
            {
                const float x = std::min(minimum.x + column * cellSize, maximum.x);
    
                const float dx = std::abs(x - previewCenter.x);
    
                const float dy = std::abs(y - previewCenter.y);
    
                const float distance = std::sqrt(
                    (dx * dx) / (outerRadius.x * outerRadius.x) +
                    (dy * dy) / (outerRadius.y * outerRadius.y));
    
                float t = std::clamp((distance - innerRatio) /
                    (1.0f - innerRatio), 0.0f, 1.0f);
                t = t * t * (3.0f - 2.0f * t);
    
                const std::size_t index = static_cast<std::size_t>(
                    (row * width + column) * 4);
    
                pixels[index] = 5;
                pixels[index + 1] = 6;
                pixels[index + 2] = 9;
                pixels[index + 3] = static_cast<std::uint8_t>(135.0f * t);
            }
        }

        D3D11_TEXTURE2D_DESC description{};
        description.Width = static_cast<UINT>(width);
        description.Height = static_cast<UINT>(height);
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = pixels.data();
        data.SysMemPitch = static_cast<UINT>(width * 4);
        ID3D11Texture2D* texture = nullptr;
    
        if (FAILED(device->CreateTexture2D(&description, &data, &texture)) || !texture)
            return nullptr;
    
        const HRESULT result = device->CreateShaderResourceView(
            texture, nullptr, &cache.view);
    
        texture->Release();
    
        if (FAILED(result))
        {
            cache.view = nullptr;
            return nullptr;
        }
    
        cache.minimum = minimum;
        cache.maximum = maximum;
        cache.previewCenter = previewCenter;
        cache.innerRadius = innerRadius;
        cache.outerRadius = outerRadius;
        cache.width = width;
        cache.height = height;
    
        return cache.view;
    }

    void Menu::DrawSettingsMenu()
    {
        ImGuiIO& io = ImGui::GetIO();

        const ImVec2 screen = io.DisplaySize;

        if (screen.x <= 0.0f || screen.y <= 0.0f)
            return;

        const float dt =
            std::clamp(io.DeltaTime, 0.0f, 0.05f);

        // ============================================================
        // ANIMAÇÃO DE ENTRADA
        // ============================================================

        static float settingsAlpha = 0.0f;
        static float settingsTime = 0.0f;

        settingsAlpha +=
            (1.0f - settingsAlpha) *
            (1.0f - std::exp(-6.0f * dt));

        settingsTime += dt;

        const float alpha =
            std::clamp(settingsAlpha, 0.0f, 0.99f);

        // ============================================================
        // CURSOR DO SKYRIM
        // ============================================================

        if (!g_gamepadSettingsCursorActive)
            g_settingsMousePos = GetSkyrimMousePos();

        // Mantém o ImGui sincronizado com a posição real do cursor.
        // Resolution::BeginFrame has already converted the ImGui sample to
        // p-radials' virtual space. Keep the Scaleform cursor synchronized
        // without queuing a second event for the following frame.
        io.MousePos = g_settingsMousePos;

        // O editor é modal: nenhum radial auxiliar, descriptor, seção ou
        // botão de fechar é desenhado enquanto o circuito está sendo montado.
        if (TrackEditor::IsOpen())
        {
            std::vector<TrackEditor::PreviewIcon> previewIcons;
            previewIcons.reserve(g_sideItems.size());
            for (const auto& item : g_sideItems)
            {
                if (!item.form) continue;
                auto* icon = ItemIcon::Get(item.form);
                previewIcons.push_back({ reinterpret_cast<ImTextureID>(icon),
                    MakeGameplayIconColor(item, 230) });
            }
            TrackEditor::Draw(alpha, g_settingsMousePos, screen, previewIcons,
                std::min(static_cast<int>(previewIcons.size()), GetSideVisibleLimit()));
            return;
        }

        // Quick Draw é um editor modal mais leve que o Editor de trilhos:
        // preserva somente o preview do item, círculo e ações Reset/OK.
        if (g_quickDrawEditorOpen)
        {
            DrawQuickDrawEditor(alpha);
            return;
        }

        // ============================================================
        // HITBOXES
        // ============================================================

        ++g_settingsHitboxGeneration;
        g_settingsItemHitboxes.clear();

        // ============================================================
        // DRAW LISTS
        // ============================================================

        ImDrawList* bg =
            ImGui::GetBackgroundDrawList();

        ImDrawList* fg =
            ImGui::GetForegroundDrawList();

        // Settings usa posições que podem ocupar as áreas adicionais de
        // ultrawide ou de viewports mais altos. Remova o clip raiz 16:9 para
        // todo o painel; os clips internos de cada seção continuam valendo.
        const ImVec2 settingsViewportMin =
            Resolution::ToVirtual(ImVec2(0.0f, 0.0f));
        const ImVec2 settingsViewportMax =
            Resolution::ToVirtual(Resolution::GetRealSize());
        fg->PushClipRect(settingsViewportMin, settingsViewportMax, false);

        const ImU32 gold =
            IM_COL32(215, 195, 150, 255);

        const ImU32 goldSoft =
            IM_COL32(180, 165, 130, 160);

        const ImU32 white =
            IM_COL32(235, 230, 215, 255);

        const ImU32 muted =
            IM_COL32(155, 150, 140, 200);

        // ============================================================
        // FUNDO INTEGRADO AO HUD
        // ============================================================

        
        // ============================================================
        // FUNDO ESCURO COM ABERTURA CENTRAL EM FADE
        // ============================================================

        const ImVec2 previewCenter(
            screen.x * 0.50f,
            screen.y * 0.50f
        );

        // Área central completamente transparente.
        const float innerRadiusX = screen.x * 0.13f;
        const float innerRadiusY = screen.y * 0.27f;

        // Distância até atingir o escurecimento máximo.
        const float outerRadiusX = screen.x * 0.34f;
        const float outerRadiusY = screen.y * 0.68f;

        // ============================================================
        // DESENHA O GRADIENTE
        // ============================================================

        // UI is letterboxed in a virtual 16:9 area, but the Settings backdrop
        // must cover the complete physical viewport (including ultrawide bars).
        const ImVec2 backdropMin = Resolution::ToVirtual(ImVec2(0.0f, 0.0f));
        const ImVec2 backdropMax = Resolution::ToVirtual(Resolution::GetRealSize());

        // The normal background draw-list clip is the virtual 16:9 viewport.
        // The dimmer is intentionally the exception: its clip must include
        // every physical pixel, including letterbox/pillarbox areas.
        bg->PushClipRect(backdropMin, backdropMax, false);

        ImVec2 backdropUvMin{};
        ImVec2 backdropUvMax{};
    
        if (auto* backdrop = GetSettingsBackdropTexture(
            backdropMin, backdropMax, previewCenter,
            ImVec2(innerRadiusX, innerRadiusY),
            ImVec2(outerRadiusX, outerRadiusY),
            backdropUvMin, backdropUvMax))
        {
            bg->AddImage(reinterpret_cast<ImTextureID>(backdrop),
                backdropMin, backdropMax,
                backdropUvMin, backdropUvMax,
                IM_COL32(255, 255, 255,
                    static_cast<int>(255.0f * alpha)));
        }
        else
        {
            constexpr float fallbackCellSize = 8.0f;
    
            const float innerRatio = std::min(
                innerRadiusX / outerRadiusX, innerRadiusY / outerRadiusY);
    
            const auto fallbackColor = [&](float x, float y) {
                const float dx = std::abs(x - previewCenter.x);
                const float dy = std::abs(y - previewCenter.y);
                const float distance = std::sqrt(
                    (dx * dx) / (outerRadiusX * outerRadiusX) +
                    (dy * dy) / (outerRadiusY * outerRadiusY));
                float t = std::clamp((distance - innerRatio) /
                    (1.0f - innerRatio), 0.0f, 1.0f);
                t = t * t * (3.0f - 2.0f * t);
                return IM_COL32(5, 6, 9,
                    static_cast<int>(135.0f * alpha * t));
            };
            for (float y = backdropMin.y; y < backdropMax.y; y += fallbackCellSize)
            {
                for (float x = backdropMin.x; x < backdropMax.x; x += fallbackCellSize)
                {
                    const float x2 = std::min(x + fallbackCellSize, backdropMax.x);
                    const float y2 = std::min(y + fallbackCellSize, backdropMax.y);
                    bg->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x2, y2),
                        fallbackColor(x, y), fallbackColor(x2, y),
                        fallbackColor(x2, y2), fallbackColor(x, y2));
                }
            }
        }

        // Vinheta discreta nas laterais.
        const float sideWidth = (backdropMax.x - backdropMin.x) * 0.25f;

        bg->AddRectFilledMultiColor(
            backdropMin,
            ImVec2(backdropMin.x + sideWidth, backdropMax.y),
            IM_COL32(0, 0, 0, 115),
            IM_COL32(0, 0, 0, 0),
            IM_COL32(0, 0, 0, 0),
            IM_COL32(0, 0, 0, 115)
        );

        bg->AddRectFilledMultiColor(
            ImVec2(backdropMax.x - sideWidth, backdropMin.y),
            backdropMax,
            IM_COL32(0, 0, 0, 0),
            IM_COL32(0, 0, 0, 115),
            IM_COL32(0, 0, 0, 115),
            IM_COL32(0, 0, 0, 0)
        );

        bg->PopClipRect();

        // ============================================================
        // CABEÇALHO
        // ============================================================

        //const char* title = "Three Wheel";

        //const ImVec2 titleSize =
        //    ImGui::CalcTextSize(title);

        //const ImVec2 titlePos(
        //    screen.x * 0.5f - titleSize.x * 0.5f,
        //    screen.y * 0.025f
        //);

        //fg->AddText(
        //    titlePos,
        //    FadeColor(gold, alpha),
        //    title
        //);
        // ============================================================
        // BOTÃO DE FECHAR SETTINGS - HOVER
        // ============================================================

        const WheelLayout wheelLayout = GetWheelLayout();
        const float visibleWidth = wheelLayout.max.x - wheelLayout.min.x;
        const float visibleHeight = wheelLayout.max.y - wheelLayout.min.y;
        const bool layoutSliderDragging =
            g_activeLayoutSlider != LayoutSlider::None;

        // Mantém os controles laterais fora da região central do preview.
        // O alvo fica entre a borda esquerda e a coluna dos locks Top/Bottom.
        constexpr float previewControlClearance = 96.0f;
        const bool sideControlsOverlapPreview =
            std::abs(wheelLayout.leftRadial.x - wheelLayout.center.x) <=
            previewControlClearance;
        const float sideControlsLeftX =
            (wheelLayout.min.x + GetSettingsLockColumnX()) * 0.5f;
        const float sideControlsTargetX = sideControlsOverlapPreview
            ? sideControlsLeftX
            : wheelLayout.leftRadial.x;
        if (g_settingsSideLockAnimatedX <= 0.0f)
            g_settingsSideLockAnimatedX = wheelLayout.leftRadial.x;
        if (!layoutSliderDragging)
        {
            g_settingsSideLockAnimatedX = AnimateSettingsValue(
                g_settingsSideLockAnimatedX,
                sideControlsTargetX,
                8.0f,
                dt);
        }

        // A trava de câmera acompanha o grupo Side. Quando a posição à
        // esquerda ficaria fora do monitor, ela atravessa suavemente para
        // depois do Lock Scroll, mantendo o mesmo espaçamento entre botões.
        constexpr float sideLockRadius = 15.0f;
        const float sideMouseTargetX =
            GetSettingsSideMouseTargetX(sideLockRadius);
        if (g_settingsSideMouseAnimatedX <= 0.0f)
            g_settingsSideMouseAnimatedX = sideMouseTargetX;
        if (!layoutSliderDragging)
        {
            g_settingsSideMouseAnimatedX = AnimateSettingsValue(
                g_settingsSideMouseAnimatedX,
                sideMouseTargetX,
                8.0f,
                dt);
        }

        // Layout fixo: seletor sempre horizontal e painel sempre abaixo.
        GetSettingsInfoPanel(screen);
        g_settingsSectionHorizontalTarget = true;
        g_settingsSectionMorphT = 1.0f;
        g_settingsPanelOffsetY = 0.0f;
        g_settingsSectionOffsetY = 0.0f;

        const ImVec2 closeButtonTarget =
            GetSettingsCloseButtonTarget(wheelLayout);
        if (g_settingsCloseButtonAnimatedY <= 0.0f)
            g_settingsCloseButtonAnimatedY = closeButtonTarget.y;
        g_settingsCloseButtonAnimatedY = AnimateSettingsValue(
            g_settingsCloseButtonAnimatedY,
            closeButtonTarget.y,
            8.0f,
            dt);
        const ImVec2 closeButtonCenter(
            closeButtonTarget.x,
            g_settingsCloseButtonAnimatedY);

        constexpr float closeButtonRadius = 12.0f;

        const float dx =
            g_settingsMousePos.x - closeButtonCenter.x;

        const float dy =
            g_settingsMousePos.y - closeButtonCenter.y;

        const bool closeHovered =
            dx * dx + dy * dy <=
            closeButtonRadius * closeButtonRadius;

        DrawGoldenCircle(
            fg,
            closeButtonCenter,
            closeButtonRadius,
            closeHovered ? 1.0f : 0.5f
        );

        // ============================================================
        // POSIÇÕES DOS RADIAIS
        // ============================================================

        const ImVec2 leftCenter = wheelLayout.leftRadial;
        const ImVec2 topCenter = wheelLayout.topRadial;
        const ImVec2 bottomCenter = wheelLayout.bottomRadial;

        // ============================================================
        // BOTÕES DE TRAVA DOS RADIAIS
        // ============================================================

        const float lockRadius = 15.0f;
        const ImVec2 sideLockCenter = GetSettingsSideLockCenter();

        // ============================================================
        // TOP - LOCK + MOUSE UNLOCK
        // ============================================================

        const ImVec2 topLockCenter(
            wheelLayout.min.x + visibleWidth * 0.11f,
            wheelLayout.min.y + visibleHeight * 0.19f
        );

        // Botão principal de trava
        DrawRadialLockButton(
            topLockCenter,
            lockRadius,
            g_lockTopRadial,
            Language::Get("lock_top_radial").c_str(),
            Language::Get("unlock_top_radial").c_str()
        );

        // Botão secundário de liberação do mouse
        DrawMouseUnlockButton(
            fg,
            topLockCenter,
            lockRadius,
            g_unlockTopMouse,
            alpha,
            true
        );

        // SIDE
        // Lock Cam | Lock Radial | Lock Scroll. A trava de câmera fica à
        // esquerda, na mesma distância que a trava de scroll à direita.
        DrawRadialLockButton(
            GetSettingsSideMouseCenter(lockRadius),
            lockRadius * 0.5f,
            g_lockSideMouse,
            Language::Get("lock_mouse_movement").c_str(),
            Language::Get("unlock_mouse_movement").c_str(),
            true
        );

        DrawRadialLockButton(
            //ImVec2(
            //    screen.x * 0.11f,
            //    screen.y * 0.50f
            //),
            sideLockCenter,
            lockRadius,
            g_lockSideRadial,
            Language::Get("lock_radial").c_str(),
            Language::Get("unlock_radial").c_str()
        );

        const float sideScrollRadius = lockRadius * 0.5f;
        DrawRadialLockButton(
            GetSettingsSideScrollCenter(lockRadius),
            sideScrollRadius,
            Config::g_lockSideScroll,
            Language::Get("lock_scroll").c_str(),
            Language::Get("unlock_scroll").c_str(),
            true
        );

        // BOTTOM

        const ImVec2 bottomLockCenter(
            wheelLayout.min.x + visibleWidth * 0.11f,
            wheelLayout.min.y + visibleHeight * 0.81f
        );

        DrawRadialLockButton(
            bottomLockCenter,
            lockRadius,
            g_lockBottomRadial,
            Language::Get("lock_bottom_radial").c_str(),
            Language::Get("unlock_bottom_radial").c_str()
        );

        DrawMouseUnlockButton(
            fg,
            bottomLockCenter,
            lockRadius,
            g_unlockBottomMouse,
            alpha,
            false
        );

        // ============================================================
        // RADIAIS
        // ============================================================

        ImGui::PushStyleVar(
            ImGuiStyleVar_Alpha,
            alpha
        );

        const float sideRadialPreviewAlpha =
            g_activeLayoutSlider == LayoutSlider::SideOpacity
            ? alpha * std::clamp(Config::g_sideOpacity, 0.0f, 100.0f) * 0.01f
            : alpha;
        const float topRadialPreviewAlpha =
            g_activeLayoutSlider == LayoutSlider::TopOpacity
            ? alpha * std::clamp(Config::g_topOpacity, 0.0f, 100.0f) * 0.01f
            : alpha;
        const float bottomRadialPreviewAlpha =
            g_activeLayoutSlider == LayoutSlider::BottomOpacity
            ? alpha * std::clamp(Config::g_bottomOpacity, 0.0f, 100.0f) * 0.01f
            : alpha;

        // Partículas do WheelSettings pertencem ao fundo dos itens. Desenhar
        // antes do radial impede que o pó cubra ícones e bordas.
        UpdateAndDrawRadialParticles(fg, dt, alpha);

        DrawRadialMenu(
            leftCenter,
            true,
            sideRadialPreviewAlpha
        );

        DrawTopRadialMenu(
            topCenter,
            static_cast<int>(g_topItems.size()),
            topRadialPreviewAlpha
        );

        DrawBottomRadialMenu(
            bottomCenter,
            static_cast<int>(g_bottomItems.size()),
            bottomRadialPreviewAlpha
        );

        if (g_activeLayoutSlider == LayoutSlider::CenterOpacity)
        {
            const float centerPreviewAlpha =
                alpha * std::clamp(Config::g_centerOpacity, 0.0f, 100.0f) * 0.01f;
            fg->AddCircle(wheelLayout.center, RADIAL_DEADZONE,
                FadeColor(IM_COL32(255, 255, 255, 60), centerPreviewAlpha), 64, 1.5f);
            fg->AddCircleFilled(wheelLayout.center, SETTINGS_DOT_MIN_RADIUS,
                FadeColor(IM_COL32(255, 255, 255, 55), centerPreviewAlpha), 32);
        }

        ImGui::PopStyleVar();

        // ============================================================
        // INTERAÇÃO
        // ============================================================

        ProcessSettingsEditor(dt);

        // Atualiza o item selecionado e o timer.
        UpdateSettingsPreviewSelection();

        // Atualiza o modelo 3D utilizando a seleção.
        UpdateSettingsPreview();

        // ============================================================
        // SELETOR DE SEÇÕES
        // ============================================================

        // Fecha o seletor quando o item do preview mudar.
        UpdateSettingsSectionPreviewState();

        // Atualiza hover, abertura e fechamento.
        UpdateSettingsSectionMenu(dt);

        // Desenha o seletor.
        DrawSettingsSectionMenu(alpha);


        // ============================================================
        // SETTINGS - CONTEÚDO DO PAINEL DIREITO
        // ============================================================

        switch (g_settingsSection)
        {
            // ========================================================
            // INFORMAÇÕES DO ITEM
            // ========================================================

            case SettingsSection::ItemInfo:
            {
                RadialItem previewItem{};

                if (GetSettingsPreviewItem(previewItem))
                {
                    UpdateSettingsItemInfoSelection(
                        previewItem
                    );

                    const std::string displayName =
                        GetRadialItemDisplayName(
                            previewItem
                        );

                    const ItemInfo::Data& info =
                        ItemInfo::GetPreviewInfo(
                            previewItem.form,
                            previewItem.uniqueID,
                            previewItem.hasUniqueID,
                            displayName
                        );

                    DrawSettingsItemInfoPanel(
                        info,
                        &previewItem,
                        alpha,
                        GetSettingsInfoPanel(
                            ImGui::GetIO().DisplaySize)
                    );

                    

                }

                break;
            }

            // ========================================================
            // SETTINGS - CONFIGURAÇÕES GERAIS
            // ========================================================

            case SettingsSection::Settings:
            {

                DrawSettingsGeneralPanel(alpha);

                break;
            }

            // ========================================================
            // GAMEPLAY
            // ========================================================

            case SettingsSection::Gameplay:
            {
                DrawSettingsGameplayPanel(alpha);

                break;
            }

            // ========================================================
            // LAYOUT
            // ========================================================

            case SettingsSection::Layout:
            {
                // Futuro painel de configuração do layout.
                DrawSettingsLayoutPanel(alpha);

                break;
            }
        }

        // ============================================================
        // INFORMAÇÕES DO ITEM EM PREVIEW
        // ============================================================
        //old
        RadialItem previewItem{};
    
        Config::ItemPreviewProfile activePreviewProfile =
            Config::ItemPreviewProfile::Menu;
    
        PreviewLayoutField activePreviewField{};
    
        const bool previewLayoutControl =
            g_settingsSection == SettingsSection::Layout &&
            GetActivePreviewControl(activePreviewProfile, activePreviewField);

        Config::ItemPreviewCategory activePreviewCategory{};
    
        const bool previewCategoryControl =
            g_settingsSection == SettingsSection::Layout &&
            GetActivePreviewCategory(activePreviewCategory);

        if (GetLayoutPreviewItem(previewItem,
                previewLayoutControl || previewCategoryControl,
                previewCategoryControl
                    ? std::optional<Config::ItemPreviewCategory>(activePreviewCategory)
                    : std::nullopt))
        {
            // Reinicia o scroll somente quando o item mudar.
        //    UpdateSettingsItemInfoSelection(previewItem);
            
        //    const std::string displayName =
        //        GetRadialItemDisplayName(previewItem);

        //    const ItemInfo::Data& info =
        //        ItemInfo::GetPreviewInfo(
        //            previewItem.form,
        //            previewItem.uniqueID,
        //            previewItem.hasUniqueID,
        //            displayName
        //        );

            // ========================================================
            // POSIÇÃO À DIREITA DO PREVIEW
            // ========================================================

            //const ImVec2 infoPosition(
            //    screen.x * 0.68f,
            //    screen.y * 0.23f
            //);

            //DrawSettingsItemInfo(
            //    info,
            //    infoPosition,
            //    alpha
            //);
            // ========================================================
            // PAINEL À DIREITA DO PREVIEW
            // ========================================================

        //    DrawSettingsItemInfoPanel(
        //        info,
        //        alpha
        //    );

            DrawSelectedItemName(
                previewItem,
                alpha,
                0.67f,
                true,
                true,
                previewLayoutControl ? activePreviewProfile :
                    Config::ItemPreviewProfile::Menu
            );
        }
        else if (previewLayoutControl)
        {
            DrawSelectedItemName(
                previewItem,
                alpha,
                0.67f,
                true,
                true,
                activePreviewProfile
            );
        }

        // ============================================================
        // QUANTIDADE DO ITEM EM PREVIEW
        // ============================================================

        RadialItem quantityPreviewItem{};

        if (GetSettingsPreviewItem(quantityPreviewItem))
        {
            DrawSettingsPreviewQuantity(
                quantityPreviewItem,
                alpha,
                previewLayoutControl ? activePreviewProfile :
                    Config::ItemPreviewProfile::Menu
            );
        }

        // ============================================================
        // PARTÍCULAS DE ITENS DELETADOS
        // ============================================================

        DrawSettingsDeleteParticles(dt);

        // Durante o drag, mostra o nome do item arrastado.
        // Caso contrário, mostra o nome do item em hover.

        const RadialItem* selectedItem = nullptr;

        // ============================================================
        // ITEM SENDO ARRASTADO
        // ============================================================

        if (g_settingsDrag.active)
        {
            selectedItem = &g_settingsDrag.item;
        }
        // ============================================================
        // ITEM SOB O MOUSE
        // ============================================================

        else
        {
            const auto* hitbox =
                GetSettingsHoveredHitbox(g_settingsMousePos);

            if (hitbox)
            {
                auto* items =
                    GetSettingsItemList(hitbox->side);

                if (items)
                {
                    const int index = hitbox->index;

                    if (index >= 0 &&
                        index < static_cast<int>(items->size()))
                    {
                        selectedItem = &(*items)[index];
                    }
                }
            }
        }

        // ============================================================
        // DESENHA O NOME DA INSTÂNCIA
        // ============================================================

        //if (selectedItem)
        //{
        //    DrawSelectedItemName(
        //        *selectedItem,
        //        alpha,
        //        0.67f
        //    );
        //}

        DrawSettingsDraggedItem();
        // O cursor é a camada final do WheelSettings: durante o drag ele
        // permanece visível acima do item carregado.
        DrawSettingsCursor();

        // ============================================================
        // INDICADOR DE EDIÇÃO
        // ============================================================

        const char* editLabel = Language::Get("layout_editor").c_str();

        //fg->AddText(
        //    ImVec2(
        //        screen.x * 0.50f -
        //            ImGui::CalcTextSize(editLabel).x * 0.5f,
        //        screen.y * 0.50f
        //    ),
        //    FadeColor(goldSoft, alpha),
        //    editLabel
        //);

        // ============================================================
        // INSTRUÇÕES INFERIORES
        // ============================================================

        const char* instructions =
            Language::Get("layout_instructions").c_str();

        const ImVec2 instructionSize =
            ImGui::CalcTextSize(instructions);

        //fg->AddText(
        //    ImVec2(
        //        screen.x * 0.5f -
        //            instructionSize.x * 0.5f,
        //        screen.y * 0.965f
        //    ),
        //    FadeColor(muted, alpha),
        //    instructions
        //);
        fg->PopClipRect();
    }

    bool CanDrawWheelMenu()
    {
        auto* ui = RE::UI::GetSingleton();

        if (!ui)
            return false;

        // ============================================================
        // 1. MENUS PERMITIDOS
        // ============================================================

        const bool inventoryOpen =
            ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME);

        const bool magicOpen =
            ui->IsMenuOpen(RE::MagicMenu::MENU_NAME);

        // ============================================================
        // 2. MENUS QUE SEMPRE BLOQUEIAM O WHEEL
        // ============================================================

        static constexpr const char* blockedMenus[] =
        {
            "TweenMenu",
            "Main Menu",
            "Loading Menu",
            "Fader Menu",
            "Console",
            "Journal Menu",
            "MapMenu",
            "StatsMenu",
            "Sleep/Wait Menu",
            "Dialogue Menu",
            "BarterMenu",
            "ContainerMenu",
            "Crafting Menu",
            "Book Menu",
            "Lockpicking Menu",
            "MessageBoxMenu",
            "RaceSex Menu",
            "Training Menu",
            "LevelUp Menu",
            "FavoritesMenu",
            "GiftMenu",
            "Credits Menu"
        };

        // ============================================================
        // 3. INVENTORY E MAGIC MENU
        // ============================================================

        if (inventoryOpen || magicOpen)
            return true;

        for (const char* menuName : blockedMenus)
        {
            if (ui->IsMenuOpen(menuName))
                return false;
        }

        

        // ============================================================
        // 4. QUALQUER OUTRO MENU ABERTO
        // ============================================================

        // Bloqueia outros menus do Skyrim que não estejam
        // explicitamente autorizados.
        //if (ui->IsShowingMenus())
        //    return false;

        // ============================================================
        // 5. GAMEPLAY - PLAYER VÁLIDO
        // ============================================================

        auto* player =
            RE::PlayerCharacter::GetSingleton();

        if (!player)
            return false;

        if (!player->Is3DLoaded())
            return false;

        // Evita desenhar durante morte ou carregamento.
        if (player->IsDead())
            return false;

        return true;
    }

        
    // ============================================================
    // DESCRIPTOR DO ITEM DURANTE GAMEPLAY
    // ============================================================

    static void DrawGameplayItemDescription(float alpha)
    {
        // ========================================================
        // CONFIGURAÇÃO DESATIVADA
        // ========================================================

        if (!Config::g_showGameplayDescription)
            return;

        // ========================================================
        // SOMENTE DURANTE GAMEPLAY
        // ========================================================

        if (!g_showWindow ||
            g_radialMode != RadialMode::Gameplay)
        {
            return;
        }

        // ========================================================
        // NENHUM ITEM SELECIONADO
        // ========================================================

        if (!g_gameplayDescriptionHasItem)
            return;

        const RadialItem& item =
            g_gameplayDescriptionItem;

        if (!item.form)
            return;

        // ========================================================
        // NOME DA INSTÂNCIA
        // ========================================================

        const std::string displayName =
            GetRadialItemDisplayName(item);

        // ========================================================
        // INFORMAÇÕES DO ITEM
        // ========================================================

        const ItemInfo::Data& info =
            ItemInfo::GetPreviewInfo(
                item.form,
                item.uniqueID,
                item.hasUniqueID,
                displayName
            );

        // ========================================================
        // POSIÇÃO AUTOMÁTICA
        // ========================================================

        const SettingsInfoPanel panel =
            GetGameplayInfoPanel();

        // ========================================================
        // DESENHA O DESCRIPTOR
        // ========================================================

        DrawSettingsItemInfoPanel(
            info,
            nullptr,
            alpha,
            panel
        );
    }

    static void DrawGameplayPreviewQuantity(float alpha)
    {
        // ========================================================
        // SOMENTE DURANTE GAMEPLAY
        // ========================================================

        if (!Config::g_showItemPreviewGameplay ||
            !g_showWindow ||
            g_radialMode != RadialMode::Gameplay)
        {
            return;
        }

        // ========================================================
        // ITEM SELECIONADO
        // ========================================================

        if (!g_gameplayDescriptionHasItem)
            return;

        const RadialItem& item =
            g_gameplayDescriptionItem;

        if (!item.form)
            return;

        // ========================================================
        // REUTILIZA A BOLINHA DO SETTINGS
        // ========================================================

        DrawSettingsPreviewQuantity(
            item,
            alpha,
            PreviewProfileForSide(g_radialSide)
        );
    }

    void DrawMenu()
    {
        // PlayerControls pode ser registrado novamente depois de DataLoaded.
        // Enquanto o radial está aberto, mantemos o filtro de mouse em frente
        // dele para que Side sempre bloqueie a câmera e Top/Bottom respeitem
        // suas travas individuais.
        if (g_showWindow && g_radialMode == RadialMode::Gameplay)
            MaintainInputSinkPriority();

        Gamepad::UpdateFeedback();
        // Atualiza a escala para a resolução atual.
        //UIScale::Update();

        float deltaTime = ImGui::GetIO().DeltaTime;
        g_sideScrollStardustEnergy = std::max(
            0.0f, g_sideScrollStardustEnergy -
                std::clamp(deltaTime, 0.0f, 0.05f) * 2.8f);

        // ============================================================
        // RESETA A SELEÇÃO DO DESCRIPTOR NESTE FRAME
        // ============================================================

        g_gameplayDescriptionHasItem = false;
    
        // ============================================================
        // SETTINGS MENU
        // ============================================================

        auto* ui = RE::UI::GetSingleton();

        CheckRadialMenuAutoClose();

        const bool settingsOpen =
            ui &&
            ui->IsMenuOpen("WheelSetting");

        // Escape/fechamentos externos cancelam o editor modal sem salvar o
        // traço parcial, evitando que ele reapareça preso na próxima abertura.
        if (!settingsOpen && g_quickDrawEditorOpen)
        {
            g_quickDrawEditorOpen = false;
            g_quickDrawEditorDrawing = false;
        }

        if (settingsOpen)
        {
            // Mantém o radial normal fechado enquanto
            // o menu de configurações estiver aberto.
            g_showWindow = false;
            SetGameplayBlurApplied(false);

            // O Settings possui seu próprio desenho.
            DrawSettingsMenu();

            return;
        }
        
        constexpr float mainFadeSpeed = 8.0f;
        constexpr float subMenuFadeSpeed = 10.0f;

        //SmoothBlurEffect::GetSingleton().Update(deltaTime);

        RenderInventoryItemOverlay();

        //static bool testStarted = false;
        //auto* player = RE::PlayerCharacter::GetSingleton();

        
        //static bool lastKeyState = false;
        //bool currentKey = (GetAsyncKeyState('T') & 0x8000) != 0; // tecla T so pra teste, por ex.

        //if (currentKey && !lastKeyState)
        //if (!testStarted && player && player->Is3DLoaded())
        //{
        //    if (auto* form = GetRandomInventoryItem())
        //    {
        //        ItemPreview::SetHudPosition(ImVec2(960.0f, 540.0f)); // centro da tela
        //        ItemPreview::Show(form);

        //        Logger::GetSingleton().Print(
        //            "ItemPreview: TESTE com item aleatorio '{}' FormID={:08X}",
        //            form->GetName(), form->GetFormID()
        //        );

                //testStarted = true;
        //    }
        //    lastKeyState = currentKey;
        //}

        // NOVO: remove itens dos radiais assim que a quantidade no
        // inventário chega a zero (vendeu, largou, usou até acabar etc.).
        // Só roda enquanto o menu está de alguma forma visível, para
        // não gastar tempo de CPU o tempo todo em jogo.
        if (g_showWindow || g_globalAlpha > 0.0f)
        {
            PruneMissingRadialItems();
        }

        // 1. ATUALIZAÇÃO DO FADE GLOBAL
        if (g_showWindow)
        {
            g_globalAlpha += deltaTime * mainFadeSpeed;
            if (g_globalAlpha > 1.0f) g_globalAlpha = 1.0f;
        }
        else
        {
            g_globalAlpha -= deltaTime * mainFadeSpeed;
            if (g_globalAlpha < 0.0f) g_globalAlpha = 0.0f;
        }

        // RESET LIMPO: Só limpa o estado quando o menu sumir da tela por completo
        if (g_globalAlpha <= 0.0f)
        {   
            Slowtime::End();
            SetGameplayBlurApplied(false);
            g_radialParticles.clear();
            
            ItemPreview::Hide();
            ItemPreview::SilentPreviewMenu::Close();

            g_menuAlpha = 0.0f;
            g_radialSide = RadialSide::None;
            g_radialLocked = false;
            return;
        }

        // ============================================================
        // INVENTORY MODE
        // ============================================================

        if (g_radialMode == RadialMode::Inventory)
        {
            SetGameplayBlurApplied(false);
            if (g_inventoryTopologySettleRemaining > 0.0f)
            {
                g_inventoryTopologySettleRemaining = std::max(
                    0.0f, g_inventoryTopologySettleRemaining - deltaTime);
            }
            if (g_inventoryTopologySettleRemaining <= 0.0f &&
                g_pendingInventoryScroll != 0)
            {
                const int queuedDirection = g_pendingInventoryScroll < 0 ? -1 : 1;
                g_pendingInventoryScroll -= queuedDirection;
                ScrollSideRadial(queuedDirection);
            }
            UpdateInventoryDrag();
            // No Inventory o rastro fica atrás dos slots/ícones.
            UpdateAndDrawRadialParticles(
                ImGui::GetForegroundDrawList(), deltaTime, g_globalAlpha);
    
            DrawInventoryRadialMenu();
    
            DrawDraggedInventoryItem();
            return;
        }

        // 2. LÓGICA DE MOUSE E SELEÇÃO DE DIREÇÃO
        UpdateRadialMouse();
        //UpdateRadialScroll(); por enquanto chamado no inputsink

        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 screen = io.DisplaySize;
        const ImVec2 screenCenter(screen.x * 0.5f, screen.y * 0.5f);

        ImVec2 mouse = GetRadialMousePosition();

        if (!g_radialLocked && g_showWindow && !g_quickDrawGameplayActive)
        {
            RadialSide direction = GetRadialDirection(mouse, screenCenter);

            if (direction != RadialSide::None &&
                RadialSideHasItems(direction))
            {
                g_radialSide = direction;
                g_radialLocked = true;
                const WheelLayout layout = GetWheelLayout();

                if (direction == RadialSide::Right)
                {
                    g_radialOrigin = layout.rightRadial;
                }
                else if (direction == RadialSide::Left)
                {
                    g_radialOrigin = layout.leftRadial;
                }
                else if (direction == RadialSide::Top)
                {
                    g_radialOrigin = layout.topRadial;
                }
                else if (direction == RadialSide::Bottom)
                {
                    g_radialOrigin = layout.bottomRadial;
                }

                // O vetor antigo não pode continuar sendo usado depois
                // que a origem mudou.
                g_radialVector = ImVec2(0.0f, 0.0f);
                g_radialMouseVelocity = ImVec2(0.0f, 0.0f);

                // Faz o próximo frame começar limpo.
                g_radialRecentered = true;
            }
        }

        const bool slowTimeSideEnabled = g_quickDrawGameplayActive
            ? Config::g_slowTimeDraw
            : (g_radialSide == RadialSide::Top && Config::g_slowTimeTop) ||
              ((g_radialSide == RadialSide::Left ||
                  g_radialSide == RadialSide::Right) && Config::g_slowTimeCentral) ||
              (g_radialSide == RadialSide::Bottom && Config::g_slowTimeBottom);
    
        Slowtime::Update(
            g_showWindow &&
                g_radialMode == RadialMode::Gameplay &&
                slowTimeSideEnabled,
            Config::g_slowTimeMultiplier);

        const bool blurSideEnabled = g_quickDrawGameplayActive
            ? Config::g_blurDraw
            : (g_radialSide == RadialSide::Top && Config::g_blurTop) ||
              ((g_radialSide == RadialSide::Left ||
                  g_radialSide == RadialSide::Right) && Config::g_blurCentral) ||
              (g_radialSide == RadialSide::Bottom && Config::g_blurBottom);
    
        SetGameplayBlurApplied(
            g_showWindow && g_radialMode == RadialMode::Gameplay &&
            blurSideEnabled);

        // 3. FADE DO SUBMENU ATIVO
        if (g_radialSide != RadialSide::None)
        {
            g_menuAlpha += deltaTime * subMenuFadeSpeed;
            if (g_menuAlpha > 1.0f) g_menuAlpha = 1.0f;
        }
        else
        {
            g_menuAlpha = 0.0f;
        }

        const float centerVisualAlpha = g_globalAlpha *
            std::clamp(Config::g_centerOpacity, 0.0f, 100.0f) * 0.01f;
    
        float selectedRadialOpacity = 1.0f;
    
        if (g_radialSide == RadialSide::Left || g_radialSide == RadialSide::Right)
            selectedRadialOpacity = std::clamp(Config::g_sideOpacity, 0.0f, 100.0f) * 0.01f;
        else if (g_radialSide == RadialSide::Top)
            selectedRadialOpacity = std::clamp(Config::g_topOpacity, 0.0f, 100.0f) * 0.01f;
        else if (g_radialSide == RadialSide::Bottom)
            selectedRadialOpacity = std::clamp(Config::g_bottomOpacity, 0.0f, 100.0f) * 0.01f;
    
        const float radialVisualAlpha = g_globalAlpha * selectedRadialOpacity;
    
        const float finalSubMenuAlpha = radialVisualAlpha * g_menuAlpha;

        mouse = GetRadialMousePosition();
    
        ImDrawList* draw = ImGui::GetForegroundDrawList();

        // ------------------------------------------------------------
        // 4. DESENHO DO CENTRO E LINHA GUIA
        // ------------------------------------------------------------

        if (g_radialSide == RadialSide::None)
        {
            // Atualiza a animação normalmente.
            if (g_showWindow)
            {
                UpdateSettingsCharge(deltaTime);
            }

            // ============================================================
            // DESENHA SOMENTE COM O RADIAL NORMAL ABERTO
            // ============================================================

            if (g_showWindow &&
                !SettingsMenu::WheelSettingsMenu::IsOpen())
            {
                const float selectorRadius = g_quickDrawGameplayActive
                    ? [&]()
                    {
                        g_quickDrawGameplayExpandT = std::min(
                            1.0f, g_quickDrawGameplayExpandT + deltaTime * 6.5f);
                        const float t = g_quickDrawGameplayExpandT *
                            g_quickDrawGameplayExpandT *
                            (3.0f - 2.0f * g_quickDrawGameplayExpandT);
                        return RADIAL_DEADZONE +
                            (QUICK_DRAW_EDITOR_RADIUS - RADIAL_DEADZONE) * t;
                    }()
                    : RADIAL_DEADZONE;
                
                // BORDA ORIGINAL
                draw->AddCircle(
                    screenCenter,
                    selectorRadius,
                    FadeColor(
                        IM_COL32(255, 255, 255, 60),
                        centerVisualAlpha
                    ),
                    64,
                    1.5f
                );

                // BOLINHA CENTRAL
                const float dotRadius =
                    SETTINGS_DOT_MIN_RADIUS +
                    (RADIAL_DEADZONE - SETTINGS_DOT_MIN_RADIUS)
                    * g_settingsVisualCharge;

                draw->AddCircleFilled(
                    screenCenter,
                    dotRadius,
                    FadeColor(
                        IM_COL32(255, 255, 255, 55),
                        centerVisualAlpha
                    ),
                    64
                );

                if (g_quickDrawGameplayActive)
                {
                    if (g_quickDrawGameplayDrawing)
                    {
                
                        const ImVec2 drawMouse = GetRadialMousePosition();
                
                        const float dx = drawMouse.x - screenCenter.x;
                
                        const float dy = drawMouse.y - screenCenter.y;
                
                        // A borda do círculo expandido funciona como um gesto
                        // de cancelar: encerra já, sem esperar soltar a tecla
                        // First/Second e sem avaliar uma seleção.
                        const float cancelRadius = std::max(1.0f, selectorRadius - 2.0f);
                
                        if (dx * dx + dy * dy >= cancelRadius * cancelRadius)
                        {
                            CancelQuickDrawGameplay();
                        }
                        else
                        {
                            AddQuickDrawPoint(g_quickDrawGameplayStroke,
                                drawMouse, screenCenter,
                                QUICK_DRAW_EDITOR_RADIUS);
                        }
                    }
                
                    DrawQuickDrawStroke(draw, g_quickDrawGameplayStroke,
                        screenCenter, QUICK_DRAW_EDITOR_RADIUS,
                        FadeColor(IM_COL32(255, 255, 255, 235), centerVisualAlpha),
                        5.0f);
                }
            }
        }
        else
        {
            ResetSettingsCharge();
        }

        if ((g_showWindow && g_radialSide == RadialSide::None) ||
            (g_radialMode == RadialMode::Gameplay &&
            g_radialSide != RadialSide::None &&
            RadialSideHasItems(g_radialSide) &&
            (g_radialSide == RadialSide::Left || g_radialSide == RadialSide::Right)))
        {
    
            const float guideLength = g_quickDrawGameplayActive
                ? QUICK_DRAW_EDITOR_RADIUS
                : GetMenuLineLength();
            ImVec2 lineEnd = ClampPointToDistance(g_radialOrigin, mouse, guideLength);
    
            const float guideAlpha = g_radialSide == RadialSide::None
                ? centerVisualAlpha
                : radialVisualAlpha;

    
            ImVec2 previewCenter{};
    
            float previewMaskRadius = 0.0f;
    
            const bool hasPreviewMask = GetVisibleItemPreviewMask(
                previewCenter, previewMaskRadius);
    
            float lineEndVisibility = 1.0f;

            if (hasPreviewMask)
            {
    
                constexpr int segments = 40;
    
                const float fadeStart = previewMaskRadius * 0.78f;
    
                const float fadeEnd = std::max(
                    previewMaskRadius * 1.16f, fadeStart + 1.0f);
    
                for (int i = 0; i < segments; ++i)
                {
    
                    const float startT = static_cast<float>(i) / segments;
    
                    const float endT = static_cast<float>(i + 1) / segments;
    
                    const float middleT = (startT + endT) * 0.5f;
    
                    const ImVec2 middle(
                        g_radialOrigin.x + (lineEnd.x - g_radialOrigin.x) * middleT,
                        g_radialOrigin.y + (lineEnd.y - g_radialOrigin.y) * middleT);
    
                    const float maskDx = middle.x - previewCenter.x;
    
                    const float maskDy = middle.y - previewCenter.y;
    
                    const float distance = std::sqrt(
                        maskDx * maskDx + maskDy * maskDy);
    
                    float visibility = std::clamp(
                        (distance - fadeStart) / (fadeEnd - fadeStart),
                        0.0f, 1.0f);
    
                    visibility = visibility * visibility * (3.0f - 2.0f * visibility);
    
                    if (visibility <= 0.0f)
                        continue;
    
                    const ImVec2 start(
                        g_radialOrigin.x + (lineEnd.x - g_radialOrigin.x) * startT,
                        g_radialOrigin.y + (lineEnd.y - g_radialOrigin.y) * startT);
    
                    const ImVec2 end(
                        g_radialOrigin.x + (lineEnd.x - g_radialOrigin.x) * endT,
                        g_radialOrigin.y + (lineEnd.y - g_radialOrigin.y) * endT);
    
                    draw->AddLine(start, end,
                        FadeColor(IM_COL32(255, 255, 255, 130), guideAlpha * visibility), 2.0f);
                }

                const float endDx = lineEnd.x - previewCenter.x;
    
                const float endDy = lineEnd.y - previewCenter.y;
    
                const float endDistance = std::sqrt(
                    endDx * endDx + endDy * endDy);
    
                lineEndVisibility = std::clamp(
                    (endDistance - fadeStart) / (fadeEnd - fadeStart),
                    0.0f, 1.0f);
    
                lineEndVisibility = lineEndVisibility * lineEndVisibility *
                    (3.0f - 2.0f * lineEndVisibility);
            }
            else
            {
                draw->AddLine(
                    g_radialOrigin,
                    lineEnd,
                    FadeColor(IM_COL32(255, 255, 255, 130), guideAlpha),
                    2.0f
                );
            }

            draw->AddCircleFilled(
                lineEnd,
                3.0f,
                FadeColor(IM_COL32(255, 255, 255, 255),
                    guideAlpha * lineEndVisibility)
            );
        }

        // ============================================================
        // RESET DE ANIMAÇÃO AO ABRIR/TROCAR SUBMENU
        // ============================================================

        static RadialSide lastAnimatedSide = RadialSide::None;

        if (g_radialSide != lastAnimatedSide)
        {
            if (g_radialSide == RadialSide::Top)
            {
                for (auto& item : g_topItems)
                {
                    if (!item.form)
                        continue;

                    RadialItemAnimation& anim = GetOrCreateAnim(
                        item.form,
                        item.uniqueID,
                        item.hasUniqueID
                    );

                    anim.posInitialized = false;
                    anim.velocity = ImVec2(0.0f, 0.0f);
                }
            }
            else if (g_radialSide == RadialSide::Bottom)
            {
                for (auto& item : g_bottomItems)
                {
                    if (!item.form)
                        continue;

                    RadialItemAnimation& anim = GetOrCreateAnim(
                        item.form,
                        item.uniqueID,
                        item.hasUniqueID
                    );

                    anim.posInitialized = false;
                    anim.velocity = ImVec2(0.0f, 0.0f);
                }
            }

            lastAnimatedSide = g_radialSide;
        }

        // 5. DESENHO DOS SUBMENUS
        if (finalSubMenuAlpha > 0.0f)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, finalSubMenuAlpha);

            if (g_radialSide == RadialSide::Right)
            {
                DrawRadialMenu(g_radialOrigin, false, finalSubMenuAlpha);
            }
            else if (g_radialSide == RadialSide::Left)
            {
                DrawRadialMenu(g_radialOrigin, true, finalSubMenuAlpha);
            }
            else if (g_radialSide == RadialSide::Top)
            {
                DrawTopRadialMenu(
                    g_radialOrigin, static_cast<int>(g_topItems.size()), finalSubMenuAlpha);
            }
            else if (g_radialSide == RadialSide::Bottom)
            {
                DrawBottomRadialMenu(
                    g_radialOrigin, static_cast<int>(g_bottomItems.size()), finalSubMenuAlpha);
            }

            ImGui::PopStyleVar();
        }

        UpdateAndDrawRadialParticles(
            ImGui::GetForegroundDrawList(), deltaTime, finalSubMenuAlpha);

        // ============================================================
        // NOME DO ITEM SELECIONADO
        DrawSelectedRadialInfo();

        // QUANTIDADE DO ITEM SELECIONADO
        DrawGameplayPreviewQuantity(g_globalAlpha);

        // DESCRIPTOR DO ITEM SELECIONADO
        DrawGameplayItemDescription(g_globalAlpha);

        // ============================================================

    }
}

