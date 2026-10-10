#include <fstream>
#include <filesystem>
#include <string>
#include <cctype>
#include <windows.h>
#include "Config.h"
#include "Language.h"
#include "Gamepad.h"
#include "Animation/Animation.h"
#include "Animation/RadialShape.h"
#include "Animation/OverflowMechanism.h"
#include "Animation/Track/TrackLayout.h"

#include <vector>
#include <system_error>
#include <algorithm>


namespace Config
{
        
    // Variável global (ou membro de classe) para a tecla

    int g_toggleKey = 'X';
    int g_secondaryKey = 0;
    int g_altConfigKey = 0;
    bool g_automaticArrowMenus = false;
    int g_radialAnimation = static_cast<int>(RadialAnimation::Style::SimpleRadial);
    int g_radialShape = static_cast<int>(RadialShape::Style::ClassicOrbit);
    bool g_customRadial = false;
    float g_slowTimeMultiplier = 0.15f;
    bool g_slowTimeTop = true;
    bool g_slowTimeCentral = true;
    bool g_slowTimeBottom = true;
    bool g_slowTimeDraw = false;
    bool g_blurTop = false;
    bool g_blurCentral = false;
    bool g_blurBottom = false;
    bool g_blurDraw = false;

    bool g_showGameplayDescription = false;
    bool g_coloredPotions = true;
    bool g_coloredMagicSchools = false;
    bool g_coloredItemEnchants = false;
    bool g_fastInventoryDrag = false;
    bool g_customIcons = false;
    bool g_customIconsHasPreference = false;
    std::string g_language = "EN";

    float g_centerOpacity = kDefaultCenterOpacity;
    float g_radialQuantity = kDefaultRadialQuantity;
    float g_radialStretch = kDefaultRadialStretch;
    float g_sideMouseSensitivity = kDefaultSideMouseSensitivity;
    float g_sideMouseSmooth = kDefaultSideMouseSmooth;
    float g_gamepadAnalogSensitivity = kDefaultGamepadAnalogSensitivity;
    float g_gamepadAnalogSmooth = kDefaultGamepadAnalogSmooth;
    bool g_lockSideScroll = true;
    bool g_showItemPreviewGameplay = true;
    bool g_showItemQuantity = true;
    bool g_showOverflowIcon = true;
    bool g_stardustEnabled = kDefaultStardustEnabled;
    float g_stardustFade = kDefaultStardustFade;
    float g_itemOpacity = kDefaultItemOpacity;
    float g_generalItemSize = kDefaultGeneralItemSize;
    float g_slotSize = kDefaultSlotSize;
    std::uint32_t g_itemBackgroundColor = kDefaultItemBackgroundColor;
    float g_itemBackgroundOpacity = kDefaultItemBackgroundOpacity;
    std::uint32_t g_itemBorderColor = kDefaultItemBorderColor;
    float g_itemBorderOpacity = kDefaultItemBorderOpacity;
    float g_iconSize = kDefaultIconSize;
    std::uint32_t g_baseIconColor = kDefaultBaseIconColor;
    float g_baseIconOpacity = kDefaultBaseIconOpacity;
    ItemStyleConfig g_topItemStyle{};
    ItemStyleConfig g_bottomItemStyle{};
    float g_overflowSize = kDefaultOverflowSize;
    std::uint32_t g_overflowBackgroundColor = kDefaultOverflowBackgroundColor;
    float g_overflowBackgroundOpacity = kDefaultOverflowBackgroundOpacity;
    std::uint32_t g_overflowBorderColor = kDefaultOverflowBorderColor;
    float g_overflowBorderOpacity = kDefaultOverflowBorderOpacity;
    std::array<std::uint32_t, 7> g_potionColors = kDefaultPotionColors;
    std::array<std::uint32_t, 5> g_schoolColors = kDefaultSchoolColors;
    std::array<std::uint32_t, 3> g_magicElementColors = kDefaultMagicElementColors;
    std::array<std::uint32_t, 5> g_enchantColors = kDefaultEnchantColors;
    float g_overflowOpacity = kDefaultOverflowOpacity;
    float g_itemNameOpacity = kDefaultItemNameOpacity;
    float g_itemNamePositionX = kDefaultItemNamePositionX;
    float g_itemNamePositionY = kDefaultItemNamePositionY;
    std::array<ItemPreviewLayoutConfig,
        static_cast<std::size_t>(ItemPreviewProfile::Count)> g_itemPreviewLayouts{};
    std::array<float,
        static_cast<std::size_t>(ItemPreviewCategory::Count)> g_itemPreviewCategoryMultipliers{
            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    float g_sideOpacity = kDefaultSideOpacity;
    float g_topOpacity = kDefaultTopOpacity;
    float g_bottomOpacity = kDefaultBottomOpacity;
    float g_fontSizeScale = kDefaultFontSizeScale;
    int g_fontFamily = 0;
    float g_sideRadialPosition = kDefaultSideRadialPosition;
    float g_topVerticalPosition = kDefaultTopVerticalPosition;
    float g_bottomVerticalPosition = kDefaultBottomVerticalPosition;
    float g_topHorizontalStretch = kDefaultTopHorizontalStretch;
    float g_bottomHorizontalStretch = kDefaultBottomHorizontalStretch;

    ItemPreviewLayoutConfig& GetItemPreviewLayout(ItemPreviewProfile profile)
    {
        return g_itemPreviewLayouts[std::min(static_cast<std::size_t>(profile),
            g_itemPreviewLayouts.size() - 1)];
    }

    bool HasCustomIconsPreference()
    {
        return g_customIconsHasPreference;
    }

    void SetCustomIconsPreference(bool a_enabled)
    {
        g_customIcons = a_enabled;
        g_customIconsHasPreference = true;
    }

    const ItemPreviewLayoutConfig& GetItemPreviewLayoutConst(ItemPreviewProfile profile)
    {
        return g_itemPreviewLayouts[std::min(static_cast<std::size_t>(profile),
            g_itemPreviewLayouts.size() - 1)];
    }

    float GetItemPreviewCategoryMultiplier(ItemPreviewCategory category)
    {
        return g_itemPreviewCategoryMultipliers[std::min(
            static_cast<std::size_t>(category),
            g_itemPreviewCategoryMultipliers.size() - 1)];
    }

    std::filesystem::path GetConfigPath()
    {
        return std::filesystem::current_path() / "Data" / "SKSE" /
            "Plugins" / "p-radials" / "p-radials.ini";
    }

    void MigrateLegacyStorage()
    {
        const auto newConfig = GetConfigPath();
        const auto newRoot = newConfig.parent_path();
        const auto plugins = newRoot.parent_path();
        const auto oldConfig = plugins / "Wheel.ini";
        // Compatibilidade de atualização: os nomes antigos só existem aqui
        // para importar uma única vez os dados para p-radials.
        const auto previousConfig = plugins / "3radials.ini";
        const auto previousRoot = plugins / "3radials";
        const auto interimConfig = plugins / "pradials.ini";
        const auto interimRoot = plugins / "pradials";
        const auto oldRoot = plugins / "Wheel";
        std::error_code ec;

        if (!std::filesystem::exists(newConfig, ec) &&
            std::filesystem::exists(interimRoot / "pradials.ini", ec))
        {
            std::filesystem::create_directories(newRoot, ec);
            ec.clear();
            std::filesystem::copy_file(interimRoot / "pradials.ini", newConfig,
                std::filesystem::copy_options::skip_existing, ec);
            ec.clear();
        }
        else if (!std::filesystem::exists(newConfig, ec) &&
            std::filesystem::exists(previousRoot / "3radials.ini", ec))
        {
            std::filesystem::create_directories(newRoot, ec);
            ec.clear();
            std::filesystem::copy_file(previousRoot / "3radials.ini", newConfig,
                std::filesystem::copy_options::skip_existing, ec);
            ec.clear();
        }
        else if (!std::filesystem::exists(newConfig, ec) &&
            std::filesystem::exists(interimConfig, ec))
        {
            std::filesystem::create_directories(newRoot, ec);
            ec.clear();
            std::filesystem::copy_file(interimConfig, newConfig,
                std::filesystem::copy_options::skip_existing, ec);
            ec.clear();
        }
        else if (!std::filesystem::exists(newConfig, ec) &&
            std::filesystem::exists(previousConfig, ec))
        {
            std::filesystem::create_directories(newRoot, ec);
            ec.clear();
            std::filesystem::copy_file(previousConfig, newConfig,
                std::filesystem::copy_options::skip_existing, ec);
            ec.clear();
        }
        else if (!std::filesystem::exists(newConfig, ec) &&
            std::filesystem::exists(oldConfig, ec))
        {
            std::filesystem::create_directories(newRoot, ec);
            ec.clear();
            std::filesystem::copy_file(oldConfig, newConfig,
                std::filesystem::copy_options::skip_existing, ec);
            ec.clear();
        }

        const auto migrateRoot = [&](const std::filesystem::path& sourceRoot,
            const std::filesystem::path& legacyConfigName) {
            ec.clear();
            if (!std::filesystem::exists(sourceRoot, ec))
            {
                ec.clear();
                return;
            }

            std::filesystem::create_directories(newRoot, ec);
            ec.clear();
            for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceRoot, ec))
            {
                if (ec) break;
                const auto relative = std::filesystem::relative(entry.path(), sourceRoot, ec);
                if (ec) { ec.clear(); continue; }
                // The configuration was copied above under its new name.
                if (entry.is_regular_file() && relative == legacyConfigName)
                    continue;

                const auto destination = newRoot / relative;
                if (entry.is_directory())
                    std::filesystem::create_directories(destination, ec);
                else if (entry.is_regular_file() && !std::filesystem::exists(destination, ec))
                {
                    std::filesystem::create_directories(destination.parent_path(), ec);
                    ec.clear();
                    std::filesystem::copy_file(entry.path(), destination,
                        std::filesystem::copy_options::skip_existing, ec);
                }
                ec.clear();
            }
        };

        migrateRoot(interimRoot, "pradials.ini");
        migrateRoot(previousRoot, "3radials.ini");
        migrateRoot(oldRoot, "Wheel.ini");

        // A pasta de layouts também mudou de singular para plural. Preserve
        // current.ini e presets existentes sem apagar a pasta antiga.
        const auto singularLayouts = newRoot / "layout";
        const auto pluralLayouts = newRoot / "layouts";
        if (std::filesystem::exists(singularLayouts, ec))
        {
            std::filesystem::create_directories(pluralLayouts, ec);
            ec.clear();
            for (const auto& entry : std::filesystem::directory_iterator(
                singularLayouts, ec))
            {
                if (!entry.is_regular_file() || entry.path().extension() != ".ini")
                    continue;
                std::filesystem::copy_file(entry.path(),
                    pluralLayouts / entry.path().filename(),
                    std::filesystem::copy_options::skip_existing, ec);
                ec.clear();
            }
        }
    }

        
    int ParseKey(const std::string& key)
    {
        std::string k = key;

        for (char& c : k)
        {
            c = static_cast<char>(
                std::toupper(
                    static_cast<unsigned char>(c)
                )
            );
        }

        // ========================================================
        // TECLA PADRÃO
        // ========================================================

        if (k.empty())
            return 'G';

        // Letras.
        if (k.size() == 1 &&
            k[0] >= 'A' &&
            k[0] <= 'Z')
        {
            return k[0];
        }

        // Números.
        if (k.size() == 1 &&
            k[0] >= '0' &&
            k[0] <= '9')
        {
            return k[0];
        }

        // ========================================================
        // F1 - F24
        // ========================================================

        if (k[0] == 'F' && k.size() <= 3)
        {
            try
            {
                const int num =
                    std::stoi(k.substr(1));

                if (num >= 1 && num <= 24)
                    return VK_F1 + (num - 1);
            }
            catch (...)
            {
            }
        }

        // ========================================================
        // TECLAS ESPECIAIS
        // ========================================================

        if (k == "INSERT")   return VK_INSERT;
        if (k == "DELETE" || k == "DEL") return VK_DELETE;
        if (k == "HOME")     return VK_HOME;
        if (k == "END")      return VK_END;

        if (k == "PAGEUP")   return VK_PRIOR;
        if (k == "PAGEDOWN") return VK_NEXT;

        if (k == "UP")       return VK_UP;
        if (k == "DOWN")     return VK_DOWN;
        if (k == "LEFT")     return VK_LEFT;
        if (k == "RIGHT")    return VK_RIGHT;

        if (k == "SPACE")    return VK_SPACE;
        if (k == "TAB")      return VK_TAB;
        if (k == "ENTER")    return VK_RETURN;
        if (k == "ESC" || k == "ESCAPE") return VK_ESCAPE;

        if (k == "BACKSPACE") return VK_BACK;
        if (k == "CAPSLOCK")  return VK_CAPITAL;

        // ========================================================
        // TECLAS NUMÉRICAS DO NUMPAD
        // ========================================================

        if (k.size() == 7 &&
            k.substr(0, 6) == "NUMPAD" &&
            k[6] >= '0' &&
            k[6] <= '9')
        {
            return VK_NUMPAD0 + (k[6] - '0');
        }

        // Padrão.
        return 'X';
    }

        
    // ============================================================
    // REMOVE ESPAÇOS NO INÍCIO E NO FIM
    // ============================================================

    static std::string Trim(const std::string& text)
    {
        const auto first =
            text.find_first_not_of(" \t\r\n");

        if (first == std::string::npos)
            return {};

        const auto last =
            text.find_last_not_of(" \t\r\n");

        return text.substr(
            first,
            last - first + 1
        );
    }

    // ============================================================
    // CONVERTE TEXTO PARA MAIÚSCULAS
    // ============================================================

    static std::string ToUpper(std::string text)
    {
        std::transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(
                    std::toupper(c)
                );
            }
        );

        return text;
    }

    // ============================================================
    // ATUALIZA OU CRIA UMA CONFIGURAÇÃO NO INI
    //
    // Mantém outras seções e configurações existentes.
    // ============================================================

    static void SetIniValue(
        std::vector<std::string>& lines,
        const std::string& section,
        const std::string& key,
        const std::string& value)
    {
        bool insideSection = false;
        bool sectionFound = false;

        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const std::string line =
                Trim(lines[i]);

            // ====================================================
            // IDENTIFICA SEÇÕES
            // ====================================================

            if (line.size() >= 2 &&
                line.front() == '[' &&
                line.back() == ']')
            {
                // Estávamos na seção desejada e chegamos
                // à próxima seção sem encontrar a chave.
                if (insideSection)
                {
                    lines.insert(
                        lines.begin() + i,
                        key + "=" + value
                    );

                    return;
                }

                const std::string currentSection =
                    Trim(
                        line.substr(
                            1,
                            line.size() - 2
                        )
                    );

                insideSection =
                    currentSection == section;

                if (insideSection)
                    sectionFound = true;

                continue;
            }

            if (!insideSection)
                continue;

            // Ignora comentários.
            if (line.empty() ||
                line[0] == ';' ||
                line[0] == '#')
            {
                continue;
            }

            const auto equal =
                line.find('=');

            if (equal == std::string::npos)
                continue;

            const std::string currentKey =
                Trim(line.substr(0, equal));

            if (currentKey != key)
                continue;

            // ====================================================
            // ATUALIZA O VALOR EXISTENTE
            // ====================================================

            lines[i] =
                key + "=" + value;

            return;
        }

        // ========================================================
        // SEÇÃO NÃO EXISTE
        // ========================================================

        if (!sectionFound)
        {
            if (!lines.empty() &&
                !lines.back().empty())
            {
                lines.emplace_back("");
            }

            lines.emplace_back(
                "[" + section + "]"
            );
        }

        // ========================================================
        // ADICIONA A CONFIGURAÇÃO
        // ========================================================

        lines.emplace_back(
            key + "=" + value
        );
    }

    static bool ParseEnabled(const std::string& value)
    {
        const std::string normalized = ToUpper(value);
        return normalized == "1" || normalized == "TRUE" || normalized == "ON";
    }

    static bool ApplyLayoutValue(const std::string& key, const std::string& value)
    {
        if (key == "ShowItemPreviewGameplay") { g_showItemPreviewGameplay = ParseEnabled(value); return true; }
        if (key == "ShowGameplayDescription") { g_showGameplayDescription = ParseEnabled(value); return true; }
        if (key == "ShowItemQuantity") { g_showItemQuantity = ParseEnabled(value); return true; }
        if (key == "ShowOverflowIcon") { g_showOverflowIcon = ParseEnabled(value); return true; }
        if (key == "StardustEnabled") { g_stardustEnabled = ParseEnabled(value); return true; }

        try
        {
            const auto applyItemStyle = [&](std::string_view prefix, ItemStyleConfig& style) {
                const std::string suffix = key.starts_with(prefix) ? key.substr(prefix.size()) : std::string{};
                if (suffix.empty()) return false;
                if (suffix == "Quantity") style.quantity = std::clamp(std::stof(value), 3.0f, 50.0f);
                else if (suffix == "GeneralItemSize") style.generalSize = std::clamp(std::stof(value), 25.0f, 200.0f);
                else if (suffix == "ItemOpacity") style.opacity = std::clamp(std::stof(value), 0.0f, 100.0f);
                else if (suffix == "LineOpacity") style.lineOpacity = std::clamp(std::stof(value), 0.0f, 100.0f);
                else if (suffix == "SlotSize") style.slotSize = std::clamp(std::stof(value), 25.0f, 200.0f);
                else if (suffix == "BackgroundColor") style.backgroundColor = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)) & 0xFFFFFFu;
                else if (suffix == "BackgroundOpacity") style.backgroundOpacity = std::clamp(std::stof(value), 0.0f, 100.0f);
                else if (suffix == "BorderColor") style.borderColor = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)) & 0xFFFFFFu;
                else if (suffix == "BorderOpacity") style.borderOpacity = std::clamp(std::stof(value), 0.0f, 100.0f);
                else if (suffix == "IconSize") style.iconSize = std::clamp(std::stof(value), 25.0f, 200.0f);
                else if (suffix == "IconColor") style.iconColor = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)) & 0xFFFFFFu;
                else if (suffix == "IconOpacity") style.iconOpacity = std::clamp(std::stof(value), 0.0f, 100.0f);
                else return false;
                return true;
            };
            if (applyItemStyle("Top", g_topItemStyle) ||
                applyItemStyle("Bottom", g_bottomItemStyle)) return true;

            static constexpr std::array<std::pair<std::string_view,
                ItemPreviewProfile>, 5> previewPrefixes{{
                { "PreviewMenu", ItemPreviewProfile::Menu },
                { "PreviewTop", ItemPreviewProfile::Top },
                { "PreviewBottom", ItemPreviewProfile::Bottom },
                { "PreviewRight", ItemPreviewProfile::Right },
                { "PreviewLeft", ItemPreviewProfile::Left }
            }};
            for (const auto& [prefix, profile] : previewPrefixes)
            {
                if (!key.starts_with(prefix)) continue;
                auto& preview = GetItemPreviewLayout(profile);
                const std::string suffix = key.substr(prefix.size());
                float* previewValue = nullptr;
                float minimum = 0.0f;
                float maximum = 100.0f;
                if (suffix == "ItemSize") { previewValue = &preview.itemSize; minimum = 1.0f; maximum = 500.0f; }
                else if (suffix == "ItemPositionY") previewValue = &preview.itemPositionY;
                else if (suffix == "ItemPositionX") previewValue = &preview.itemPositionX;
                else if (suffix == "ItemNameOpacity") previewValue = &preview.itemNameOpacity;
                else if (suffix == "ItemNamePositionY") previewValue = &preview.itemNamePositionY;
                else if (suffix == "ItemNamePositionX") previewValue = &preview.itemNamePositionX;
                if (!previewValue) break;
                *previewValue = std::clamp(std::stof(value), minimum, maximum);
                return true;
            }

            static constexpr std::array<const char*,
                static_cast<std::size_t>(ItemPreviewCategory::Count)> previewCategoryKeys{
                "PreviewSpellSizeMultiplier", "PreviewWeaponSizeMultiplier",
                "PreviewPotionSizeMultiplier", "PreviewArmorSizeMultiplier",
                "PreviewAmmoSizeMultiplier", "PreviewBookSizeMultiplier",
                "PreviewMiscSizeMultiplier", "PreviewKeySizeMultiplier",
                "PreviewSoulGemSizeMultiplier", "PreviewIngredientSizeMultiplier",
                "PreviewScrollSizeMultiplier" };
            for (std::size_t i = 0; i < previewCategoryKeys.size(); ++i)
            {
                if (key != previewCategoryKeys[i]) continue;
                g_itemPreviewCategoryMultipliers[i] =
                    std::clamp(std::stof(value), 0.2f, 3.0f);
                return true;
            }

            if (key == "FontFamily") { g_fontFamily = std::clamp(std::stoi(value), 0, 8); return true; }
            if (key == "RadialLineOpacity") { Track::SetRadialLineOpacity(std::stof(value)); return true; }
            if (key == "OverflowLineOpacity") { Track::SetLineOpacity(std::stof(value)); return true; }
            if (key == "RadialRotation") { Track::SetRadialRotation(std::stof(value) * 0.01745329252f); return true; }

            std::uint32_t* color = nullptr;
            if (key == "ItemBackgroundColor") color = &g_itemBackgroundColor;
            else if (key == "ItemBorderColor") color = &g_itemBorderColor;
            else if (key == "BaseIconColor") color = &g_baseIconColor;
            else if (key == "OverflowBackgroundColor") color = &g_overflowBackgroundColor;
            else if (key == "OverflowBorderColor") color = &g_overflowBorderColor;
            if (color)
            {
                *color = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)) & 0xFFFFFFu;
                return true;
            }

            static constexpr std::array<const char*, 7> potionKeys{
                "PotionHealthColor", "PotionStaminaColor", "PotionMagickaColor",
                "PotionPoisonColor", "PotionFireColor", "PotionFrostColor", "PotionShockColor"
            };
            static constexpr std::array<const char*, 5> schoolKeys{
                "SchoolAlterationColor", "SchoolConjurationColor", "SchoolDestructionColor",
                "SchoolIllusionColor", "SchoolRestorationColor"
            };
            static constexpr std::array<const char*, 3> magicKeys{
                "MagicFireColor", "MagicFrostColor", "MagicShockColor"
            };
            static constexpr std::array<const char*, 5> enchantKeys{
                "EnchantFireColor", "EnchantFrostColor", "EnchantShockColor",
                "EnchantPoisonColor", "EnchantDefaultColor"
            };
            const auto applyColorArray = [&](const auto& keys, auto& colors) {
                for (std::size_t i = 0; i < keys.size(); ++i)
                {
                    if (key != keys[i]) continue;
                    colors[i] = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)) & 0xFFFFFFu;
                    return true;
                }
                return false;
            };
            if (applyColorArray(potionKeys, g_potionColors) ||
                applyColorArray(schoolKeys, g_schoolColors) ||
                applyColorArray(magicKeys, g_magicElementColors) ||
                applyColorArray(enchantKeys, g_enchantColors)) return true;

            float* target = nullptr;
            float minimum = 0.0f;
            float maximum = 100.0f;
            if (key == "CenterOpacity") target = &g_centerOpacity;
            else if (key == "StardustFade") target = &g_stardustFade;
            else if (key == "RadialQuantity") { target = &g_radialQuantity; minimum = 3.0f; maximum = 50.0f; }
            else if (key == "RadialStretch") target = &g_radialStretch;
            else if (key == "ItemOpacity") target = &g_itemOpacity;
            else if (key == "GeneralItemSize") { target = &g_generalItemSize; minimum = 25.0f; maximum = 200.0f; }
            else if (key == "SlotSize") { target = &g_slotSize; minimum = 25.0f; maximum = 200.0f; }
            else if (key == "ItemBackgroundOpacity") target = &g_itemBackgroundOpacity;
            else if (key == "ItemBorderOpacity") target = &g_itemBorderOpacity;
            else if (key == "IconSize") { target = &g_iconSize; minimum = 25.0f; maximum = 200.0f; }
            else if (key == "BaseIconOpacity") target = &g_baseIconOpacity;
            else if (key == "OverflowSize") { target = &g_overflowSize; minimum = 25.0f; maximum = 200.0f; }
            else if (key == "OverflowBackgroundOpacity") target = &g_overflowBackgroundOpacity;
            else if (key == "OverflowBorderOpacity") target = &g_overflowBorderOpacity;
            else if (key == "OverflowOpacity") target = &g_overflowOpacity;
            else if (key == "ItemNameOpacity") target = &g_itemNameOpacity;
            else if (key == "ItemNamePositionX") target = &g_itemNamePositionX;
            else if (key == "ItemNamePositionY") target = &g_itemNamePositionY;
            else if (key == "SideOpacity") target = &g_sideOpacity;
            else if (key == "TopOpacity") target = &g_topOpacity;
            else if (key == "BottomOpacity") target = &g_bottomOpacity;
            else if (key == "FontSizeScale") { target = &g_fontSizeScale; minimum = 1.0f; maximum = 2.5f; }
            else if (key == "SideRadialPosition") target = &g_sideRadialPosition;
            else if (key == "TopVerticalPosition") target = &g_topVerticalPosition;
            else if (key == "BottomVerticalPosition") target = &g_bottomVerticalPosition;
            else if (key == "TopHorizontalStretch") target = &g_topHorizontalStretch;
            else if (key == "BottomHorizontalStretch") target = &g_bottomHorizontalStretch;
            if (target)
            {
                *target = std::clamp(std::stof(value), minimum, maximum);
                return true;
            }
        }
        catch (...) {}
        return false;
    }

    static ItemStyleConfig MainItemStyle()
    {
        ItemStyleConfig style;
        style.quantity = g_radialQuantity;
        style.generalSize = g_generalItemSize;
        style.opacity = g_itemOpacity;
        style.lineOpacity = Track::RadialLineOpacity();
        style.slotSize = g_slotSize;
        style.backgroundColor = g_itemBackgroundColor;
        style.backgroundOpacity = g_itemBackgroundOpacity;
        style.borderColor = g_itemBorderColor;
        style.borderOpacity = g_itemBorderOpacity;
        style.iconSize = g_iconSize;
        style.iconColor = g_baseIconColor;
        style.iconOpacity = g_baseIconOpacity;
        return style;
    }

    static bool LoadLayoutFile(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        // Layouts antigos não possuem os multiplicadores por categoria.
        // Começar em 1x preserva exatamente os offsets visuais históricos.
        g_itemPreviewCategoryMultipliers.fill(1.0f);
        std::string section;
        std::string line;
        bool topStyleFound = false;
        bool bottomStyleFound = false;
        bool previewProfilesFound = false;
        while (std::getline(file, line))
        {
            line = Trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            if (line.front() == '[' && line.back() == ']')
            {
                section = Trim(line.substr(1, line.size() - 2));
                continue;
            }
            if (section != "Layout") continue;
            const auto equal = line.find('=');
            if (equal == std::string::npos) continue;
            const std::string key = Trim(line.substr(0, equal));
            topStyleFound = topStyleFound || key == "TopQuantity";
            bottomStyleFound = bottomStyleFound || key == "BottomQuantity";
            previewProfilesFound = previewProfilesFound || key == "PreviewMenuItemSize";
            ApplyLayoutValue(key, Trim(line.substr(equal + 1)));
        }
        if (!topStyleFound) g_topItemStyle = MainItemStyle();
        if (!bottomStyleFound) g_bottomItemStyle = MainItemStyle();
        if (!previewProfilesFound)
        {
            ItemPreviewLayoutConfig migrated;
            migrated.itemNameOpacity = g_itemNameOpacity;
            migrated.itemNamePositionX = g_itemNamePositionX;
            migrated.itemNamePositionY = g_itemNamePositionY;
            g_itemPreviewLayouts.fill(migrated);
        }
        return true;
    }

    static bool WriteLayoutFile(const std::filesystem::path& path)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) return false;
        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open()) return false;
        file << "[Layout]\n";
        const auto write = [&](std::string_view key, const auto& value) { file << key << '=' << value << '\n'; };
        write("ShowItemPreviewGameplay", g_showItemPreviewGameplay ? 1 : 0);
        write("ShowGameplayDescription", g_showGameplayDescription ? 1 : 0);
        write("ShowItemQuantity", g_showItemQuantity ? 1 : 0);
        write("ShowOverflowIcon", g_showOverflowIcon ? 1 : 0);
        write("StardustEnabled", g_stardustEnabled ? 1 : 0);
        write("StardustFade", g_stardustFade);
        write("FontSizeScale", g_fontSizeScale); write("FontFamily", g_fontFamily);
        write("CenterOpacity", g_centerOpacity); write("RadialQuantity", g_radialQuantity);
        write("RadialStretch", g_radialStretch);
        write("ItemOpacity", g_itemOpacity); write("GeneralItemSize", g_generalItemSize);
        write("SlotSize", g_slotSize); write("ItemBackgroundColor", g_itemBackgroundColor);
        write("ItemBackgroundOpacity", g_itemBackgroundOpacity); write("ItemBorderColor", g_itemBorderColor);
        write("ItemBorderOpacity", g_itemBorderOpacity); write("IconSize", g_iconSize);
        write("BaseIconColor", g_baseIconColor); write("BaseIconOpacity", g_baseIconOpacity);
        const auto writeItemStyle = [&](std::string_view prefix, const ItemStyleConfig& style) {
            write(std::string(prefix) + "Quantity", style.quantity);
            write(std::string(prefix) + "GeneralItemSize", style.generalSize);
            write(std::string(prefix) + "ItemOpacity", style.opacity);
            write(std::string(prefix) + "LineOpacity", style.lineOpacity);
            write(std::string(prefix) + "SlotSize", style.slotSize);
            write(std::string(prefix) + "BackgroundColor", style.backgroundColor);
            write(std::string(prefix) + "BackgroundOpacity", style.backgroundOpacity);
            write(std::string(prefix) + "BorderColor", style.borderColor);
            write(std::string(prefix) + "BorderOpacity", style.borderOpacity);
            write(std::string(prefix) + "IconSize", style.iconSize);
            write(std::string(prefix) + "IconColor", style.iconColor);
            write(std::string(prefix) + "IconOpacity", style.iconOpacity);
        };
        writeItemStyle("Top", g_topItemStyle);
        writeItemStyle("Bottom", g_bottomItemStyle);
        const auto writePreview = [&](std::string_view prefix,
            const ItemPreviewLayoutConfig& preview) {
            write(std::string(prefix) + "ItemSize", preview.itemSize);
            write(std::string(prefix) + "ItemPositionY", preview.itemPositionY);
            write(std::string(prefix) + "ItemPositionX", preview.itemPositionX);
            write(std::string(prefix) + "ItemNameOpacity", preview.itemNameOpacity);
            write(std::string(prefix) + "ItemNamePositionY", preview.itemNamePositionY);
            write(std::string(prefix) + "ItemNamePositionX", preview.itemNamePositionX);
        };
        writePreview("PreviewMenu", GetItemPreviewLayoutConst(ItemPreviewProfile::Menu));
        writePreview("PreviewTop", GetItemPreviewLayoutConst(ItemPreviewProfile::Top));
        writePreview("PreviewBottom", GetItemPreviewLayoutConst(ItemPreviewProfile::Bottom));
        writePreview("PreviewRight", GetItemPreviewLayoutConst(ItemPreviewProfile::Right));
        writePreview("PreviewLeft", GetItemPreviewLayoutConst(ItemPreviewProfile::Left));
        static constexpr std::array<const char*,
            static_cast<std::size_t>(ItemPreviewCategory::Count)> previewCategoryKeys{
            "PreviewSpellSizeMultiplier", "PreviewWeaponSizeMultiplier",
            "PreviewPotionSizeMultiplier", "PreviewArmorSizeMultiplier",
            "PreviewAmmoSizeMultiplier", "PreviewBookSizeMultiplier",
            "PreviewMiscSizeMultiplier", "PreviewKeySizeMultiplier",
            "PreviewSoulGemSizeMultiplier", "PreviewIngredientSizeMultiplier",
            "PreviewScrollSizeMultiplier" };
        for (std::size_t i = 0; i < previewCategoryKeys.size(); ++i)
            write(previewCategoryKeys[i], g_itemPreviewCategoryMultipliers[i]);
        write("OverflowSize", g_overflowSize); write("OverflowBackgroundColor", g_overflowBackgroundColor);
        write("OverflowBackgroundOpacity", g_overflowBackgroundOpacity);
        write("OverflowBorderColor", g_overflowBorderColor); write("OverflowBorderOpacity", g_overflowBorderOpacity);
        write("OverflowOpacity", g_overflowOpacity); write("RadialLineOpacity", Track::RadialLineOpacity());
        write("OverflowLineOpacity", Track::LineOpacity());
        write("RadialRotation", Track::RadialRotation() * 57.2957795f);
        write("ItemNameOpacity", g_itemNameOpacity); write("ItemNamePositionX", g_itemNamePositionX);
        write("ItemNamePositionY", g_itemNamePositionY); write("SideOpacity", g_sideOpacity);
        write("TopOpacity", g_topOpacity); write("BottomOpacity", g_bottomOpacity);
        write("SideRadialPosition", g_sideRadialPosition); write("TopVerticalPosition", g_topVerticalPosition);
        write("BottomVerticalPosition", g_bottomVerticalPosition); write("TopHorizontalStretch", g_topHorizontalStretch);
        write("BottomHorizontalStretch", g_bottomHorizontalStretch);
        static constexpr std::array<const char*, 7> potionKeys{ "PotionHealthColor", "PotionStaminaColor", "PotionMagickaColor", "PotionPoisonColor", "PotionFireColor", "PotionFrostColor", "PotionShockColor" };
        static constexpr std::array<const char*, 5> schoolKeys{ "SchoolAlterationColor", "SchoolConjurationColor", "SchoolDestructionColor", "SchoolIllusionColor", "SchoolRestorationColor" };
        static constexpr std::array<const char*, 3> magicKeys{ "MagicFireColor", "MagicFrostColor", "MagicShockColor" };
        static constexpr std::array<const char*, 5> enchantKeys{ "EnchantFireColor", "EnchantFrostColor", "EnchantShockColor", "EnchantPoisonColor", "EnchantDefaultColor" };
        for (std::size_t i = 0; i < potionKeys.size(); ++i) write(potionKeys[i], g_potionColors[i]);
        for (std::size_t i = 0; i < schoolKeys.size(); ++i) write(schoolKeys[i], g_schoolColors[i]);
        for (std::size_t i = 0; i < magicKeys.size(); ++i) write(magicKeys[i], g_magicElementColors[i]);
        for (std::size_t i = 0; i < enchantKeys.size(); ++i) write(enchantKeys[i], g_enchantColors[i]);
        return file.good();
    }

    static std::string SafeLayoutName(std::string_view name)
    {
        std::string safe;
        for (char value : name)
            if (std::isalnum(static_cast<unsigned char>(value)) || value == ' ' || value == '-' || value == '_')
                safe.push_back(value);
        if (safe.empty()) safe = "Layout";
        if (ToUpper(safe) == "CURRENT") safe += " Layout";
        return safe;
    }

    static void RemoveIniSection(std::vector<std::string>& lines, std::string_view sectionName)
    {
        bool removing = false;
        lines.erase(std::remove_if(lines.begin(), lines.end(), [&](const std::string& raw) {
            const std::string line = Trim(raw);
            if (line.size() >= 2 && line.front() == '[' && line.back() == ']')
            {
                removing = Trim(line.substr(1, line.size() - 2)) == sectionName;
                return removing;
            }
            return removing;
        }), lines.end());
    }

    static void RemoveIniKey(std::vector<std::string>& lines,
        std::string_view sectionName, std::string_view keyName)
    {
        std::string section;
        lines.erase(std::remove_if(lines.begin(), lines.end(), [&](const std::string& raw) {
            const std::string line = Trim(raw);
            if (line.size() >= 2 && line.front() == '[' && line.back() == ']')
            {
                section = Trim(line.substr(1, line.size() - 2));
                return false;
            }
            if (section != sectionName) return false;
            const auto equal = line.find('=');
            return equal != std::string::npos && Trim(line.substr(0, equal)) == keyName;
        }), lines.end());
    }

        
    void LoadConfig()
    {
        const auto path =
            GetConfigPath();

        std::ifstream file(path);

        if (!file.is_open())
        {
            if (!LoadLayoutFile(GetCurrentLayoutPath())) SaveConfig();
            return;
        }

        std::string section;
        std::string line;

        while (std::getline(file, line))
        {
            line = Trim(line);

            if (line.empty())
                continue;

            // ========================================================
            // IGNORA COMENTÁRIOS
            // ========================================================

            if (line[0] == ';' ||
                line[0] == '#')
            {
                continue;
            }

            // ========================================================
            // IDENTIFICA A SEÇÃO
            // ========================================================

            if (line.front() == '[' &&
                line.back() == ']')
            {
                section = Trim(
                    line.substr(
                        1,
                        line.size() - 2
                    )
                );

                continue;
            }

            // ========================================================
            // IDENTIFICA KEY=VALUE
            // ========================================================

            const auto equal =
                line.find('=');

            if (equal == std::string::npos)
                continue;

            const std::string key =
                Trim(line.substr(0, equal));

            const std::string value =
                Trim(line.substr(equal + 1));

            // ========================================================
            // SETTINGS
            // ========================================================

            if (section == "Settings")
            {
                if (key == "ToggleKey")
                {
                    if (!value.empty())
                    {
                        int gamepadBinding = 0;
                        g_toggleKey = Gamepad::ParseBinding(value, gamepadBinding)
                            ? gamepadBinding
                            : ParseKey(value);
                    }
                }
                else if (key == "SecondaryKey")
                {
                    if (!value.empty() && ToUpper(value) != "NONE")
                    {
                        int gamepadBinding = 0;
                        g_secondaryKey = Gamepad::ParseBinding(value, gamepadBinding)
                            ? gamepadBinding
                            : ParseKey(value);
                    }
                    else
                    {
                        g_secondaryKey = 0;
                    }
                }
                else if (key == "AltConfigKey")
                {
                    if (!value.empty() && ToUpper(value) != "NONE")
                    {
                        int gamepadBinding = 0;
                        g_altConfigKey = Gamepad::ParseBinding(value, gamepadBinding)
                            ? gamepadBinding
                            : ParseKey(value);
                    }
                    else
                    {
                        g_altConfigKey = 0;
                    }
                }
                else if (key == "AutomaticArrowMenus")
                {
                    const std::string normalized = ToUpper(value);
                    g_automaticArrowMenus = normalized == "1" ||
                        normalized == "TRUE" || normalized == "ON";
                }
                else if (key == "RadialAnimation")
                {
                    g_radialAnimation = static_cast<int>(
                        RadialAnimation::FromName(value));
                }
                else if (key == "RadialShape")
                {
                    g_radialShape = static_cast<int>(
                        RadialShape::FromName(value));
                }
                else if (key == "CustomRadial")
                {
                    const std::string normalized = ToUpper(value);
                    g_customRadial = normalized == "1" || normalized == "TRUE" || normalized == "ON";
                }
                else if (key == "SlowTimeMultiplier")
                {
                    try { g_slowTimeMultiplier = std::clamp(std::stof(value), 0.1f, 0.8f); }
                    catch (...) {}
                }
                else if (key == "SlowTimeTop") g_slowTimeTop = ParseEnabled(value);
                else if (key == "SlowTimeCentral") g_slowTimeCentral = ParseEnabled(value);
                else if (key == "SlowTimeBottom") g_slowTimeBottom = ParseEnabled(value);
                else if (key == "SlowTimeDraw") g_slowTimeDraw = ParseEnabled(value);
                else if (key == "BlurTop") g_blurTop = ParseEnabled(value);
                else if (key == "BlurCentral") g_blurCentral = ParseEnabled(value);
                else if (key == "BlurBottom") g_blurBottom = ParseEnabled(value);
                else if (key == "BlurDraw") g_blurDraw = ParseEnabled(value);
                else if (key == "SideMouseSensitivity")
                {
                    try { g_sideMouseSensitivity = std::clamp(std::stof(value), 0.25f, 3.0f); }
                    catch (...) {}
                }
                else if (key == "SideMouseSmooth")
                {
                    try { g_sideMouseSmooth = std::clamp(std::stof(value), 0.0f, 100.0f); }
                    catch (...) {}
                }
                else if (key == "GamepadAnalogSensitivity")
                {
                    try { g_gamepadAnalogSensitivity = std::clamp(std::stof(value), 0.25f, 3.0f); }
                    catch (...) {}
                }
                else if (key == "GamepadAnalogSmooth")
                {
                    try { g_gamepadAnalogSmooth = std::clamp(std::stof(value), 0.0f, 100.0f); }
                    catch (...) {}
                }
                else if (key == "LockSideScroll")
                {
                    g_lockSideScroll = ParseEnabled(value);
                }
                else if (key == "ShowItemPreviewGameplay")
                {
                    // Migração de versões antigas; o próximo salvamento move
                    // esta opção para Wheel/layout/current.ini.
                    g_showItemPreviewGameplay = ParseEnabled(value);
                }
            }

            // ========================================================
            // GAMEPLAY
            // ========================================================

            else if (section == "Gameplay")
            {
                if (key == "ShowGameplayDescription")
                {
                    const std::string normalized =
                        ToUpper(value);

                    g_showGameplayDescription =
                        normalized == "1" ||
                        normalized == "TRUE" ||
                        normalized == "ON";
                }
                else if (key == "RadialAnimation")
                {
                    g_radialAnimation = static_cast<int>(
                        RadialAnimation::FromName(value));
                }
                else if (key == "RadialShape")
                {
                    g_radialShape = static_cast<int>(
                        RadialShape::FromName(value));
                }
                else if (key == "CustomRadial")
                {
                    const std::string normalized = ToUpper(value);
                    g_customRadial = normalized == "1" || normalized == "TRUE" || normalized == "ON";
                }
                else if (key == "SlowTimeMultiplier")
                {
                    try { g_slowTimeMultiplier = std::clamp(std::stof(value), 0.1f, 0.8f); }
                    catch (...) {}
                }
                else if (key == "SlowTimeTop") g_slowTimeTop = ParseEnabled(value);
                else if (key == "SlowTimeCentral") g_slowTimeCentral = ParseEnabled(value);
                else if (key == "SlowTimeBottom") g_slowTimeBottom = ParseEnabled(value);
                else if (key == "SlowTimeDraw") g_slowTimeDraw = ParseEnabled(value);
                else if (key == "BlurTop") g_blurTop = ParseEnabled(value);
                else if (key == "BlurCentral") g_blurCentral = ParseEnabled(value);
                else if (key == "BlurBottom") g_blurBottom = ParseEnabled(value);
                else if (key == "BlurDraw") g_blurDraw = ParseEnabled(value);
                else if (key == "ColoredPotions" || key == "ColoredMagicSchools" || key == "ColoredItemEnchants")
                {
                    const std::string normalized = ToUpper(value);
                    const bool enabled = normalized == "1" || normalized == "TRUE" || normalized == "ON";
                    if (key == "ColoredPotions")
                        g_coloredPotions = enabled;
                    else if (key == "ColoredMagicSchools")
                        g_coloredMagicSchools = enabled;
                    else
                        g_coloredItemEnchants = enabled;
                }
                else if (key == "FastInventoryDrag")
                {
                    g_fastInventoryDrag = ParseEnabled(value);
                }
                else if (key == "CustomIcons")
                {
                    g_customIcons = ParseEnabled(value);
                    g_customIconsHasPreference = true;
                }
            }

            // ========================================================
            // LAYOUT
            // ========================================================

            else if (section == "Layout")
            {
                if (key == "MenuOpacity")
                {
                    try
                    {
                        const float legacyOpacity = std::clamp(std::stof(value), 0.0f, 100.0f);
                        g_centerOpacity = legacyOpacity;
                        g_sideOpacity = legacyOpacity;
                        g_topOpacity = legacyOpacity;
                        g_bottomOpacity = legacyOpacity;
                    }
                    catch (...)
                    {
                        // Mantém o valor padrão.
                    }
                }
                else if (key == "LockSideScroll")
                {
                    const std::string normalized = ToUpper(value);
                    g_lockSideScroll = normalized == "1" || normalized == "TRUE" || normalized == "ON";
                }
                else if (key == "ShowItemPreviewGameplay")
                {
                    const std::string normalized = ToUpper(value);
                    g_showItemPreviewGameplay = normalized == "1" || normalized == "TRUE" || normalized == "ON";
                }
                else if (key == "ShowItemQuantity" || key == "ShowOverflowIcon")
                {
                    const std::string normalized = ToUpper(value);
                    const bool enabled = normalized == "1" || normalized == "TRUE" || normalized == "ON";
                    if (key == "ShowItemQuantity") g_showItemQuantity = enabled;
                    else g_showOverflowIcon = enabled;
                }
                else if (key == "CenterOpacity" || key == "SideOpacity" ||
                    key == "TopOpacity" || key == "BottomOpacity")
                {
                    float* target = key == "CenterOpacity" ? &g_centerOpacity :
                        key == "SideOpacity" ? &g_sideOpacity :
                        key == "TopOpacity" ? &g_topOpacity : &g_bottomOpacity;
                    try { *target = std::clamp(std::stof(value), 0.0f, 100.0f); }
                    catch (...) {}
                }
                else if (key == "FontSizeScale")
                {
                    try
                    {
                        g_fontSizeScale = std::clamp(std::stof(value), 1.0f, 2.5f);
                    }
                    catch (...)
                    {
                        // Mantem o valor padrao.
                    }
                }
                else if (key == "FontFamily")
                {
                    try { g_fontFamily = std::clamp(std::stoi(value), 0, 8); }
                    catch (...) {}
                }
                else if (key == "RadialQuantity")
                {
                    try { g_radialQuantity = std::clamp(std::stof(value), 3.0f, 50.0f); }
                    catch (...) {}
                }
                else if (key == "SideMouseSensitivity")
                {
                    try { g_sideMouseSensitivity = std::clamp(std::stof(value), 0.25f, 3.0f); }
                    catch (...) {}
                }
                else if (key == "GamepadAnalogSensitivity")
                {
                    try { g_gamepadAnalogSensitivity = std::clamp(std::stof(value), 0.25f, 3.0f); }
                    catch (...) {}
                }
                else
                {
                    float* target = nullptr;
                    if (key == "SideRadialPosition") target = &g_sideRadialPosition;
                    else if (key == "TopVerticalPosition") target = &g_topVerticalPosition;
                    else if (key == "BottomVerticalPosition") target = &g_bottomVerticalPosition;
                    else if (key == "TopHorizontalStretch") target = &g_topHorizontalStretch;
                    else if (key == "BottomHorizontalStretch") target = &g_bottomHorizontalStretch;
                    else if (key == "RadialStretch") target = &g_radialStretch;
                    else if (key == "SideMouseSmooth") target = &g_sideMouseSmooth;
                    else if (key == "GamepadAnalogSmooth") target = &g_gamepadAnalogSmooth;
                    else if (key == "ItemNameOpacity") target = &g_itemNameOpacity;
                    else if (key == "ItemOpacity") target = &g_itemOpacity;
                    else if (key == "GeneralItemSize") target = &g_generalItemSize;
                    else if (key == "SlotSize") target = &g_slotSize;
                    else if (key == "ItemBackgroundOpacity") target = &g_itemBackgroundOpacity;
                    else if (key == "ItemBorderOpacity") target = &g_itemBorderOpacity;
                    else if (key == "IconSize") target = &g_iconSize;
                    else if (key == "BaseIconOpacity") target = &g_baseIconOpacity;
                    else if (key == "OverflowSize") target = &g_overflowSize;
                    else if (key == "OverflowBackgroundOpacity") target = &g_overflowBackgroundOpacity;
                    else if (key == "OverflowBorderOpacity") target = &g_overflowBorderOpacity;
                    else if (key == "OverflowOpacity") target = &g_overflowOpacity;
                    else if (key == "ItemNamePositionX") target = &g_itemNamePositionX;
                    else if (key == "ItemNamePositionY") target = &g_itemNamePositionY;
                    if (target)
                    {
                        try
                        {
                            const bool sizeValue = key == "GeneralItemSize" ||
                                key == "SlotSize" || key == "IconSize" ||
                                key == "OverflowSize";
                            *target = std::clamp(std::stof(value),
                                sizeValue ? 25.0f : 0.0f,
                                sizeValue ? 200.0f : 100.0f);
                        }
                        catch (...) {}
                    }
                }
                if (key == "ItemBackgroundColor" || key == "ItemBorderColor" ||
                    key == "BaseIconColor" || key == "OverflowBackgroundColor" ||
                    key == "OverflowBorderColor")
                {
                    try
                    {
                        const auto parsed = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)) & 0xFFFFFFu;
                        if (key == "ItemBackgroundColor") g_itemBackgroundColor = parsed;
                        else if (key == "ItemBorderColor") g_itemBorderColor = parsed;
                        else if (key == "BaseIconColor") g_baseIconColor = parsed;
                        else if (key == "OverflowBackgroundColor") g_overflowBackgroundColor = parsed;
                        else g_overflowBorderColor = parsed;
                    }
                    catch (...) {}
                }
                static constexpr std::array<const char*, 7> potionKeys{
                    "PotionHealthColor", "PotionStaminaColor", "PotionMagickaColor",
                    "PotionPoisonColor", "PotionFireColor", "PotionFrostColor", "PotionShockColor"
                };
                static constexpr std::array<const char*, 5> schoolKeys{
                    "SchoolAlterationColor", "SchoolConjurationColor", "SchoolDestructionColor",
                    "SchoolIllusionColor", "SchoolRestorationColor"
                };
                static constexpr std::array<const char*, 5> enchantKeys{
                    "EnchantFireColor", "EnchantFrostColor", "EnchantShockColor",
                    "EnchantPoisonColor", "EnchantDefaultColor"
                };
                static constexpr std::array<const char*, 3> magicElementKeys{
                    "MagicFireColor", "MagicFrostColor", "MagicShockColor"
                };
                const auto loadColorArray = [&](const auto& keys, auto& colors) {
                    for (std::size_t i = 0; i < keys.size(); ++i)
                    {
                        if (key != keys[i]) continue;
                        try { colors[i] = static_cast<std::uint32_t>(
                            std::stoul(value, nullptr, 0)) & 0xFFFFFFu; }
                        catch (...) {}
                        return true;
                    }
                    return false;
                };
                loadColorArray(potionKeys, g_potionColors) ||
                    loadColorArray(schoolKeys, g_schoolColors) ||
                    loadColorArray(magicElementKeys, g_magicElementColors) ||
                    loadColorArray(enchantKeys, g_enchantColors);
            }
            else if (section == "Language" && key == "Selected")
            {
                g_language = Trim(value);
            }
        }
        if (!LoadLayoutFile(GetCurrentLayoutPath())) SaveConfig();
    }

        
    std::string KeyToString(int key)
    {
        if (Gamepad::IsBinding(key))
            return Gamepad::ButtonName(Gamepad::Decode(key));

        // ========================================================
        // LETRAS E NÚMEROS
        // ========================================================

        if ((key >= 'A' && key <= 'Z') ||
            (key >= '0' && key <= '9'))
        {
            return std::string(
                1,
                static_cast<char>(key)
            );
        }

        // ========================================================
        // TECLAS F1 - F24
        // ========================================================

        if (key >= VK_F1 && key <= VK_F24)
        {
            return "F" +
                std::to_string(
                    key - VK_F1 + 1
                );
        }

        // ========================================================
        // NUMPAD
        // ========================================================

        if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9)
        {
            return "NUMPAD" +
                std::to_string(
                    key - VK_NUMPAD0
                );
        }

        // ========================================================
        // TECLAS ESPECIAIS
        // ========================================================

        switch (key)
        {
        case VK_INSERT:  return Language::Get("key_insert");
        case VK_DELETE:  return Language::Get("key_delete");

        case VK_HOME:    return Language::Get("key_home");
        case VK_END:     return Language::Get("key_end");

        case VK_PRIOR:   return Language::Get("key_page_up");
        case VK_NEXT:    return Language::Get("key_page_down");

        case VK_UP:      return Language::Get("key_up");
        case VK_DOWN:    return Language::Get("key_down");
        case VK_LEFT:    return Language::Get("key_left");
        case VK_RIGHT:   return Language::Get("key_right");

        case VK_SPACE:   return Language::Get("key_space");
        case VK_TAB:     return Language::Get("key_tab");
        case VK_RETURN:  return Language::Get("key_enter");
        case VK_ESCAPE:  return Language::Get("key_escape");

        case VK_BACK:    return Language::Get("key_backspace");
        case VK_CAPITAL: return Language::Get("key_caps_lock");

        default:
            return "";
        }
    }

            
        
    void ResetToDefaults()
    {
        g_toggleKey = 'X';
        g_secondaryKey = 0;
        g_altConfigKey = 0;
        g_automaticArrowMenus = false;
        g_radialAnimation = static_cast<int>(RadialAnimation::Style::SimpleRadial);
        g_radialShape = static_cast<int>(RadialShape::Style::ClassicOrbit);
        g_customRadial = false;
        g_slowTimeMultiplier = 0.15f;
        g_slowTimeTop = true;
        g_slowTimeCentral = true;
        g_slowTimeBottom = true;
        g_slowTimeDraw = false;
        g_blurTop = false;
        g_blurCentral = false;
        g_blurBottom = false;
        g_blurDraw = false;
        g_showGameplayDescription = false;
        g_coloredPotions = true;
        g_coloredMagicSchools = false;
        g_coloredItemEnchants = false;
        g_fastInventoryDrag = false;
        g_customIcons = false;
        g_customIconsHasPreference = false;
        g_language = "EN";
        g_centerOpacity = kDefaultCenterOpacity;
        g_radialQuantity = kDefaultRadialQuantity;
        g_radialStretch = kDefaultRadialStretch;
        g_sideMouseSensitivity = kDefaultSideMouseSensitivity;
        g_sideMouseSmooth = kDefaultSideMouseSmooth;
        g_gamepadAnalogSensitivity = kDefaultGamepadAnalogSensitivity;
        g_gamepadAnalogSmooth = kDefaultGamepadAnalogSmooth;
        g_lockSideScroll = true;
        g_showItemPreviewGameplay = true;
        g_showItemQuantity = true;
        g_showOverflowIcon = true;
        g_stardustEnabled = kDefaultStardustEnabled;
        g_stardustFade = kDefaultStardustFade;
        g_itemOpacity = kDefaultItemOpacity;
        g_generalItemSize = kDefaultGeneralItemSize;
        g_slotSize = kDefaultSlotSize;
        g_itemBackgroundColor = kDefaultItemBackgroundColor;
        g_itemBackgroundOpacity = kDefaultItemBackgroundOpacity;
        g_itemBorderColor = kDefaultItemBorderColor;
        g_itemBorderOpacity = kDefaultItemBorderOpacity;
        g_iconSize = kDefaultIconSize;
        g_baseIconColor = kDefaultBaseIconColor;
        g_baseIconOpacity = kDefaultBaseIconOpacity;
        g_topItemStyle = ItemStyleConfig{};
        g_bottomItemStyle = ItemStyleConfig{};
        g_overflowSize = kDefaultOverflowSize;
        g_overflowBackgroundColor = kDefaultOverflowBackgroundColor;
        g_overflowBackgroundOpacity = kDefaultOverflowBackgroundOpacity;
        g_overflowBorderColor = kDefaultOverflowBorderColor;
        g_overflowBorderOpacity = kDefaultOverflowBorderOpacity;
        g_potionColors = kDefaultPotionColors;
        g_schoolColors = kDefaultSchoolColors;
        g_magicElementColors = kDefaultMagicElementColors;
        g_enchantColors = kDefaultEnchantColors;
        g_overflowOpacity = kDefaultOverflowOpacity;
        g_itemNameOpacity = kDefaultItemNameOpacity;
        g_itemNamePositionX = kDefaultItemNamePositionX;
        g_itemNamePositionY = kDefaultItemNamePositionY;
        g_itemPreviewLayouts.fill(ItemPreviewLayoutConfig{});
        g_itemPreviewCategoryMultipliers.fill(1.0f);
        g_sideOpacity = kDefaultSideOpacity;
        g_topOpacity = kDefaultTopOpacity;
        g_bottomOpacity = kDefaultBottomOpacity;
        g_fontSizeScale = kDefaultFontSizeScale;
        g_fontFamily = 0;
        g_sideRadialPosition = kDefaultSideRadialPosition;
        g_topVerticalPosition = kDefaultTopVerticalPosition;
        g_bottomVerticalPosition = kDefaultBottomVerticalPosition;
        g_topHorizontalStretch = kDefaultTopHorizontalStretch;
        g_bottomHorizontalStretch = kDefaultBottomHorizontalStretch;
        Language::SetCurrent(g_language);
        SaveConfig();
    }

    std::filesystem::path GetLayoutDirectory()
    {
        return GetConfigPath().parent_path() / "layouts";
    }

    std::filesystem::path GetCurrentLayoutPath()
    {
        return GetLayoutDirectory() / "current.ini";
    }

    void SaveConfig()
    {
        const auto path =
            GetConfigPath();

        // ========================================================
        // LÊ O ARQUIVO EXISTENTE
        //
        // Preserva outras configurações e seções.
        // ========================================================

        std::vector<std::string> lines;

        {
            std::ifstream file(path);

            std::string line;

            while (std::getline(file, line))
            {
                lines.push_back(line);
            }
        }

        // ========================================================
        // SETTINGS
        // ========================================================

        SetIniValue(
            lines,
            "Settings",
            "ToggleKey",
            KeyToString(g_toggleKey)
        );
        SetIniValue(lines, "Settings", "SecondaryKey",
            g_secondaryKey != 0 ? KeyToString(g_secondaryKey) : "NONE");
        SetIniValue(lines, "Settings", "AltConfigKey",
            g_altConfigKey != 0 ? KeyToString(g_altConfigKey) : "NONE");
        SetIniValue(lines, "Settings", "AutomaticArrowMenus",
            g_automaticArrowMenus ? "1" : "0");
        SetIniValue(lines, "Settings", "SideMouseSensitivity", std::to_string(g_sideMouseSensitivity));
        SetIniValue(lines, "Settings", "SideMouseSmooth", std::to_string(g_sideMouseSmooth));
        SetIniValue(lines, "Settings", "GamepadAnalogSensitivity", std::to_string(g_gamepadAnalogSensitivity));
        SetIniValue(lines, "Settings", "GamepadAnalogSmooth", std::to_string(g_gamepadAnalogSmooth));
        SetIniValue(lines, "Gameplay", "RadialAnimation",
            RadialAnimation::Name(static_cast<RadialAnimation::Style>(
                std::clamp(g_radialAnimation, 0, RadialAnimation::Count() - 1))));
        SetIniValue(lines, "Gameplay", "RadialShape",
            RadialShape::Name(static_cast<RadialShape::Style>(
                std::clamp(g_radialShape, 0, RadialShape::Count() - 1))));
        SetIniValue(lines, "Gameplay", "CustomRadial", g_customRadial ? "1" : "0");
        SetIniValue(lines, "Gameplay", "SlowTimeMultiplier",
            std::to_string(std::clamp(g_slowTimeMultiplier, 0.1f, 0.8f)));
        SetIniValue(lines, "Gameplay", "SlowTimeTop", g_slowTimeTop ? "1" : "0");
        SetIniValue(lines, "Gameplay", "SlowTimeCentral", g_slowTimeCentral ? "1" : "0");
        SetIniValue(lines, "Gameplay", "SlowTimeBottom", g_slowTimeBottom ? "1" : "0");
        SetIniValue(lines, "Gameplay", "SlowTimeDraw", g_slowTimeDraw ? "1" : "0");
        SetIniValue(lines, "Gameplay", "BlurTop", g_blurTop ? "1" : "0");
        SetIniValue(lines, "Gameplay", "BlurCentral", g_blurCentral ? "1" : "0");
        SetIniValue(lines, "Gameplay", "BlurBottom", g_blurBottom ? "1" : "0");
        SetIniValue(lines, "Gameplay", "BlurDraw", g_blurDraw ? "1" : "0");

        // ========================================================
        // GAMEPLAY
        // ========================================================

        SetIniValue(
            lines,
            "Gameplay",
            "ShowGameplayDescription",
            g_showGameplayDescription ? "1" : "0"
        );
        SetIniValue(lines, "Gameplay", "ColoredPotions", g_coloredPotions ? "1" : "0");
        SetIniValue(lines, "Gameplay", "ColoredMagicSchools", g_coloredMagicSchools ? "1" : "0");
        SetIniValue(lines, "Gameplay", "ColoredItemEnchants", g_coloredItemEnchants ? "1" : "0");
        SetIniValue(lines, "Gameplay", "FastInventoryDrag", g_fastInventoryDrag ? "1" : "0");
        SetIniValue(lines, "Gameplay", "CustomIcons", g_customIcons ? "1" : "0");

        // ========================================================
        // LAYOUT
        // ========================================================

        SetIniValue(lines, "Layout", "CenterOpacity", std::to_string(g_centerOpacity));
        SetIniValue(lines, "Layout", "RadialQuantity", std::to_string(g_radialQuantity));
        SetIniValue(lines, "Layout", "RadialStretch", std::to_string(g_radialStretch));
        SetIniValue(lines, "Layout", "SideMouseSensitivity", std::to_string(g_sideMouseSensitivity));
        SetIniValue(lines, "Layout", "SideMouseSmooth", std::to_string(g_sideMouseSmooth));
        SetIniValue(lines, "Layout", "GamepadAnalogSensitivity", std::to_string(g_gamepadAnalogSensitivity));
        SetIniValue(lines, "Layout", "GamepadAnalogSmooth", std::to_string(g_gamepadAnalogSmooth));
        SetIniValue(lines, "Settings", "LockSideScroll", g_lockSideScroll ? "1" : "0");
        SetIniValue(lines, "Settings", "ShowItemPreviewGameplay", g_showItemPreviewGameplay ? "1" : "0");
        SetIniValue(lines, "Layout", "ShowItemQuantity", g_showItemQuantity ? "1" : "0");
        SetIniValue(lines, "Layout", "ShowOverflowIcon", g_showOverflowIcon ? "1" : "0");
        SetIniValue(lines, "Layout", "ItemOpacity", std::to_string(g_itemOpacity));
        SetIniValue(lines, "Layout", "GeneralItemSize", std::to_string(g_generalItemSize));
        SetIniValue(lines, "Layout", "SlotSize", std::to_string(g_slotSize));
        SetIniValue(lines, "Layout", "ItemBackgroundColor", std::to_string(g_itemBackgroundColor));
        SetIniValue(lines, "Layout", "ItemBackgroundOpacity", std::to_string(g_itemBackgroundOpacity));
        SetIniValue(lines, "Layout", "ItemBorderColor", std::to_string(g_itemBorderColor));
        SetIniValue(lines, "Layout", "ItemBorderOpacity", std::to_string(g_itemBorderOpacity));
        SetIniValue(lines, "Layout", "IconSize", std::to_string(g_iconSize));
        SetIniValue(lines, "Layout", "BaseIconColor", std::to_string(g_baseIconColor));
        SetIniValue(lines, "Layout", "BaseIconOpacity", std::to_string(g_baseIconOpacity));
        SetIniValue(lines, "Layout", "OverflowSize", std::to_string(g_overflowSize));
        SetIniValue(lines, "Layout", "OverflowBackgroundColor", std::to_string(g_overflowBackgroundColor));
        SetIniValue(lines, "Layout", "OverflowBackgroundOpacity", std::to_string(g_overflowBackgroundOpacity));
        SetIniValue(lines, "Layout", "OverflowBorderColor", std::to_string(g_overflowBorderColor));
        SetIniValue(lines, "Layout", "OverflowBorderOpacity", std::to_string(g_overflowBorderOpacity));
        static constexpr std::array<const char*, 7> potionKeys{
            "PotionHealthColor", "PotionStaminaColor", "PotionMagickaColor",
            "PotionPoisonColor", "PotionFireColor", "PotionFrostColor", "PotionShockColor"
        };
        static constexpr std::array<const char*, 5> schoolKeys{
            "SchoolAlterationColor", "SchoolConjurationColor", "SchoolDestructionColor",
            "SchoolIllusionColor", "SchoolRestorationColor"
        };
        static constexpr std::array<const char*, 5> enchantKeys{
            "EnchantFireColor", "EnchantFrostColor", "EnchantShockColor",
            "EnchantPoisonColor", "EnchantDefaultColor"
        };
        static constexpr std::array<const char*, 3> magicElementKeys{
            "MagicFireColor", "MagicFrostColor", "MagicShockColor"
        };
        for (std::size_t i = 0; i < potionKeys.size(); ++i)
            SetIniValue(lines, "Layout", potionKeys[i], std::to_string(g_potionColors[i]));
        for (std::size_t i = 0; i < schoolKeys.size(); ++i)
            SetIniValue(lines, "Layout", schoolKeys[i], std::to_string(g_schoolColors[i]));
        for (std::size_t i = 0; i < magicElementKeys.size(); ++i)
            SetIniValue(lines, "Layout", magicElementKeys[i], std::to_string(g_magicElementColors[i]));
        for (std::size_t i = 0; i < enchantKeys.size(); ++i)
            SetIniValue(lines, "Layout", enchantKeys[i], std::to_string(g_enchantColors[i]));
        SetIniValue(lines, "Layout", "OverflowOpacity", std::to_string(g_overflowOpacity));
        SetIniValue(lines, "Layout", "ItemNameOpacity", std::to_string(g_itemNameOpacity));
        SetIniValue(lines, "Layout", "ItemNamePositionX", std::to_string(g_itemNamePositionX));
        SetIniValue(lines, "Layout", "ItemNamePositionY", std::to_string(g_itemNamePositionY));
        SetIniValue(lines, "Layout", "SideOpacity", std::to_string(g_sideOpacity));
        SetIniValue(lines, "Layout", "TopOpacity", std::to_string(g_topOpacity));
        SetIniValue(lines, "Layout", "BottomOpacity", std::to_string(g_bottomOpacity));
        SetIniValue(lines, "Layout", "FontSizeScale", std::to_string(g_fontSizeScale));
        SetIniValue(lines, "Layout", "FontFamily", std::to_string(g_fontFamily));
        SetIniValue(lines, "Layout", "SideRadialPosition", std::to_string(g_sideRadialPosition));
        SetIniValue(lines, "Layout", "TopVerticalPosition", std::to_string(g_topVerticalPosition));
        SetIniValue(lines, "Layout", "BottomVerticalPosition", std::to_string(g_bottomVerticalPosition));
        SetIniValue(lines, "Layout", "TopHorizontalStretch", std::to_string(g_topHorizontalStretch));
        SetIniValue(lines, "Layout", "BottomHorizontalStretch", std::to_string(g_bottomHorizontalStretch));
        SetIniValue(lines, "Language", "Selected", g_language);
        RemoveIniSection(lines, "Layout");
        RemoveIniKey(lines, "Settings", "ShowItemPreviewGameplay");
        RemoveIniKey(lines, "Gameplay", "ShowGameplayDescription");

        // ========================================================
        // CRIA O DIRETÓRIO, CASO NECESSÁRIO
        // ========================================================

        std::error_code ec;

        std::filesystem::create_directories(
            path.parent_path(),
            ec
        );

        if (ec)
            return;

        // ========================================================
        // SALVA O ARQUIVO
        // ========================================================

        std::ofstream file(
            path,
            std::ios::trunc
        );

        if (!file.is_open())
            return;

        for (const auto& line : lines)
        {
            file << line << '\n';
        }

        file.flush();
        SaveLayoutConfig();
    }

    bool SaveLayoutConfig()
    {
        return WriteLayoutFile(GetCurrentLayoutPath());
    }

    bool LoadLayoutConfig()
    {
        return LoadLayoutFile(GetCurrentLayoutPath());
    }

    bool SaveLayoutPreset(std::string_view name)
    {
        return WriteLayoutFile(GetLayoutDirectory() / (SafeLayoutName(name) + ".ini"));
    }

    bool LoadLayoutPreset(std::string_view name)
    {
        if (!LoadLayoutFile(GetLayoutDirectory() / (SafeLayoutName(name) + ".ini"))) return false;
        return SaveLayoutConfig();
    }

    bool DeleteLayoutPreset(std::string_view name)
    {
        const std::string safeName = SafeLayoutName(name);
        if (ToUpper(safeName) == "CURRENT")
            return false;

        const auto path = GetLayoutDirectory() / (safeName + ".ini");
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec) || ec)
            return false;
        return std::filesystem::remove(path, ec) && !ec;
    }

    std::vector<std::string> LayoutPresets()
    {
        std::vector<std::string> result;
        std::error_code ec;
        std::filesystem::create_directories(GetLayoutDirectory(), ec);
        for (const auto& item : std::filesystem::directory_iterator(GetLayoutDirectory(), ec))
        {
            if (!item.is_regular_file() || item.path().extension() != ".ini" ||
                ToUpper(item.path().filename().string()) == "CURRENT.INI") continue;
            result.push_back(item.path().stem().string());
        }
        std::ranges::sort(result);
        return result;
    }
}
