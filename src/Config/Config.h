#pragma once

#include <string>
#include <filesystem>
#include <cstdint>
#include <array>
#include <vector>
#include <string_view>

namespace Config
{
    struct ItemStyleConfig
    {
        float quantity = 13.0f;
        float generalSize = 100.0f;
        float opacity = 100.0f;
        float lineOpacity = 100.0f;
        float slotSize = 100.0f;
        std::uint32_t backgroundColor = 0x141419;
        float backgroundOpacity = 90.0f;
        std::uint32_t borderColor = 0xFFFFFF;
        float borderOpacity = 35.0f;
        float iconSize = 100.0f;
        std::uint32_t iconColor = 0xFFFFFF;
        float iconOpacity = 50.0f;
    };

    enum class ItemPreviewProfile : std::size_t
    {
        Menu,
        Top,
        Bottom,
        Right,
        Left,
        Count
    };

    struct ItemPreviewLayoutConfig
    {
        float itemSize = 100.0f;
        float itemPositionY = 50.0f;
        float itemPositionX = 50.0f;
        float itemNameOpacity = 100.0f;
        float itemNamePositionY = 72.0f;
        float itemNamePositionX = 50.0f;
    };

    enum class ItemPreviewCategory : std::size_t
    {
        Spell,
        Weapon,
        Potion,
        Armor,
        Ammo,
        Book,
        Misc,
        Key,
        SoulGem,
        Ingredient,
        Scroll,
        Count
    };
    // Armazena o código de tecla (DirectInput/SKSE ScanCode)
    // Valor padrão: 0x22 (Tecla 'G')
    //inline uint32_t g_toggleKey = 0x22;
    
    extern int g_toggleKey;
    extern int g_secondaryKey;
    extern int g_altConfigKey;
    extern bool g_automaticArrowMenus;
    extern int g_radialAnimation;
    extern int g_radialShape;
    extern bool g_customRadial;
    extern float g_slowTimeMultiplier;
    extern bool g_slowTimeTop;
    extern bool g_slowTimeCentral;
    extern bool g_slowTimeBottom;
    extern bool g_slowTimeDraw;
    extern bool g_blurTop;
    extern bool g_blurCentral;
    extern bool g_blurBottom;
    extern bool g_blurDraw;
    
    extern bool g_showGameplayDescription;
    extern bool g_coloredPotions;
    extern bool g_coloredMagicSchools;
    extern bool g_coloredItemEnchants;
    extern bool g_fastInventoryDrag;
    // Ativado apenas quando o leitor encontra ao menos uma regra I4 válida.
    extern bool g_customIcons;
    bool HasCustomIconsPreference();
    void SetCustomIconsPreference(bool a_enabled);
    extern std::string g_language;

    // Exemplo para configurações futuras de Layout.
    // Normalized percentages. Defaults preserve the original 1920x1080 layout.
    extern float g_centerOpacity;
    extern float g_radialQuantity;
    extern float g_radialStretch;
    extern float g_sideMouseSensitivity;
    extern float g_sideMouseSmooth;
    extern float g_gamepadAnalogSensitivity;
    extern float g_gamepadAnalogSmooth;
    extern bool g_lockSideScroll;
    extern bool g_showItemPreviewGameplay;
    extern bool g_showItemQuantity;
    extern bool g_showOverflowIcon;
    extern bool g_stardustEnabled;
    extern float g_stardustFade;
    extern float g_itemOpacity;
    extern float g_generalItemSize;
    extern float g_slotSize;
    extern std::uint32_t g_itemBackgroundColor;
    extern float g_itemBackgroundOpacity;
    extern std::uint32_t g_itemBorderColor;
    extern float g_itemBorderOpacity;
    extern float g_iconSize;
    extern std::uint32_t g_baseIconColor;
    extern float g_baseIconOpacity;
    extern ItemStyleConfig g_topItemStyle;
    extern ItemStyleConfig g_bottomItemStyle;
    extern float g_overflowSize;
    extern std::uint32_t g_overflowBackgroundColor;
    extern float g_overflowBackgroundOpacity;
    extern std::uint32_t g_overflowBorderColor;
    extern float g_overflowBorderOpacity;
    extern std::array<std::uint32_t, 7> g_potionColors;
    extern std::array<std::uint32_t, 5> g_schoolColors;
    extern std::array<std::uint32_t, 3> g_magicElementColors;
    extern std::array<std::uint32_t, 5> g_enchantColors;
    extern float g_overflowOpacity;
    extern float g_itemNameOpacity;
    extern float g_itemNamePositionX;
    extern float g_itemNamePositionY;
    extern std::array<ItemPreviewLayoutConfig,
        static_cast<std::size_t>(ItemPreviewProfile::Count)> g_itemPreviewLayouts;
    ItemPreviewLayoutConfig& GetItemPreviewLayout(ItemPreviewProfile profile);
    const ItemPreviewLayoutConfig& GetItemPreviewLayoutConst(ItemPreviewProfile profile);
    extern std::array<float,
        static_cast<std::size_t>(ItemPreviewCategory::Count)> g_itemPreviewCategoryMultipliers;
    float GetItemPreviewCategoryMultiplier(ItemPreviewCategory category);
    extern float g_sideOpacity;
    extern float g_topOpacity;
    extern float g_bottomOpacity;
    extern float g_fontSizeScale;
    extern int g_fontFamily;
    extern float g_sideRadialPosition;
    extern float g_topVerticalPosition;
    extern float g_bottomVerticalPosition;
    extern float g_topHorizontalStretch;
    extern float g_bottomHorizontalStretch;

    inline constexpr float kDefaultCenterOpacity = 100.0f;
    inline constexpr float kDefaultRadialQuantity = 13.0f;
    inline constexpr float kDefaultRadialStretch = 0.0f;
    inline constexpr bool kDefaultStardustEnabled = true;
    inline constexpr float kDefaultStardustFade = 35.0f;
    inline constexpr float kDefaultSideMouseSensitivity = 1.0f;
    inline constexpr float kDefaultSideMouseSmooth = 0.0f;
    inline constexpr float kDefaultGamepadAnalogSensitivity = 1.0f;
    inline constexpr float kDefaultGamepadAnalogSmooth = 0.0f;
    inline constexpr float kDefaultItemNameOpacity = 100.0f;
    inline constexpr float kDefaultItemOpacity = 100.0f;
    inline constexpr float kDefaultGeneralItemSize = 100.0f;
    inline constexpr float kDefaultSlotSize = 100.0f;
    inline constexpr std::uint32_t kDefaultItemBackgroundColor = 0x141419;
    inline constexpr float kDefaultItemBackgroundOpacity = 90.0f;
    inline constexpr std::uint32_t kDefaultItemBorderColor = 0xFFFFFF;
    inline constexpr float kDefaultItemBorderOpacity = 35.0f;
    inline constexpr float kDefaultIconSize = 100.0f;
    inline constexpr std::uint32_t kDefaultBaseIconColor = 0xFFFFFF;
    inline constexpr float kDefaultBaseIconOpacity = 50.0f;
    inline constexpr float kDefaultOverflowSize = 100.0f;
    inline constexpr std::uint32_t kDefaultOverflowBackgroundColor = 0xFFFFFF;
    inline constexpr float kDefaultOverflowBackgroundOpacity = 14.0f;
    inline constexpr std::uint32_t kDefaultOverflowBorderColor = 0xFFFFFF;
    inline constexpr float kDefaultOverflowBorderOpacity = 27.0f;
    inline constexpr std::array<std::uint32_t, 7> kDefaultPotionColors{
        0xD7463C, 0x4BBE5A, 0x467DE1, 0x824BB4, 0xE6552D, 0x55BEEB, 0xAF5FE6
    };
    inline constexpr std::array<std::uint32_t, 5> kDefaultSchoolColors{
        0x46B9D2, 0xA055DC, 0xE65F37, 0xDC50B4, 0xEBC850
    };
    inline constexpr std::array<std::uint32_t, 3> kDefaultMagicElementColors{
        0xE6552D, 0x55BEEB, 0xAF5FE6
    };
    inline constexpr std::array<std::uint32_t, 5> kDefaultEnchantColors{
        0xE6552D, 0x55BEEB, 0xAF5FE6, 0x50B95A, 0xE1B455
    };
    inline constexpr float kDefaultOverflowOpacity = 100.0f;
    inline constexpr float kDefaultItemNamePositionX = 50.0f;
    inline constexpr float kDefaultItemNamePositionY = 72.0f;
    inline constexpr float kDefaultItemPreviewSize = 100.0f;
    inline constexpr float kDefaultItemPreviewPositionX = 50.0f;
    inline constexpr float kDefaultItemPreviewPositionY = 50.0f;
    inline constexpr float kDefaultSideOpacity = 100.0f;
    inline constexpr float kDefaultTopOpacity = 100.0f;
    inline constexpr float kDefaultBottomOpacity = 100.0f;
    inline constexpr float kDefaultFontSizeScale = 1.3f;
    inline constexpr float kDefaultSideRadialPosition = 56.0f;
    inline constexpr float kDefaultTopVerticalPosition = 13.05f;
    inline constexpr float kDefaultBottomVerticalPosition = 45.76f;
    inline constexpr float kDefaultTopHorizontalStretch = 42.86f;
    inline constexpr float kDefaultBottomHorizontalStretch = 42.86f;

    // Obtém o caminho do arquivo: "Data/SKSE/Plugins/3radials/3radials.ini"
    std::filesystem::path GetConfigPath();
    std::filesystem::path GetLayoutDirectory();
    std::filesystem::path GetCurrentLayoutPath();
    void MigrateLegacyStorage();

    // Carrega e processa a ToggleKey do arquivo 3radials.ini
    void LoadConfig();

    std::string KeyToString(int key);

    // Salva a ToggleKey atual no arquivo 3radials.ini
    void SaveConfig();
    bool SaveLayoutConfig();
    bool LoadLayoutConfig();
    bool SaveLayoutPreset(std::string_view name);
    bool LoadLayoutPreset(std::string_view name);
    bool DeleteLayoutPreset(std::string_view name);
    std::vector<std::string> LayoutPresets();
    void ResetToDefaults();

    // Converte a string de texto do INI (ex: "G", "F3", "SPACE") no ScanCode DirectInput correto
}
