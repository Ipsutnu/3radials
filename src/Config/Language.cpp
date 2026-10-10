#include "Language.h"

#include "Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>

namespace Language
{
    namespace
    {
        using Dictionary = std::unordered_map<std::string, std::string>;

        const Dictionary kEnglish{
            { "settings", "SETTINGS" }, { "gameplay", "GAMEPLAY" }, { "layout", "LAYOUT" },
            { "information", "INFORMATION" }, { "quantity", "Quantity" },
            { "attributes", "ATTRIBUTES" }, { "base_damage", "Base Damage" },
            { "attack_speed", "Attack Speed" }, { "reach", "Reach" },
            { "stagger", "Stagger" }, { "critical_damage", "Critical Damage" },
            { "base_armor", "Base Armor" }, { "weight", "Weight" }, { "value", "Value" },
            { "item_health_multiplier", "Item Health Multiplier" }, { "magic", "MAGIC" },
            { "school", "School" }, { "magicka_cost", "Magicka Cost" },
            { "charge_time", "Charge Time" }, { "range", "Range" },
            { "enchantment", "ENCHANTMENT" }, { "effects", "EFFECTS" },
            { "unknown_effect", "Unknown Effect" }, { "magnitude", "Magnitude" },
            { "duration", "Duration" }, { "duration_seconds", "{} s" }, { "none", "None" },
            { "wheel_key", "Wheel Key" }, { "first_key", "First Key" },
            { "second_key", "Second Key" },
            { "automatic_arrow_menus", "Open Menu with arrows" },
            { "radial_animation", "Radial Animation" },
            { "radial_shape", "Radial Shape" }, { "animation", "ANIMATION" },
            { "overflow_mechanism", "Overflow Mechanism" },
            { "radial_system", "Radial System" },
            { "waiting_key", "Waiting key..." },
            { "show_item_information", "Show item information during Gameplay" },
            { "menu_section", "MENU" },
            { "slow_time_during_radial_selection", "Slowtime during radial selection" },
            { "slow_time_multiplier", "Stop Multiplier" },
            { "slow_time_top", "Top" }, { "slow_time_left_right", "Left/Right" },
            { "slow_time_bottom", "Bottom" },
            { "slowtime", "Slowtime" },
            { "draw", "Draw" },
            { "blur", "Blur" },
            { "icons", "ICONS" }, { "custom_icons", "Custom Icons" }, { "reload", "Reload" },
            { "colored_potions", "Colored Potions" },
            { "colored_magic_schools", "Colored Magic Schools" },
            { "color_item_enchants", "Color Item Enchants" },
            { "inventory_section", "INVENTORY" }, { "fast_drag", "Fast Drag" },
            { "language", "Language" }, { "no_languages", "No valid language files" },
            { "radial_quantity", "Radial Quantity" }, { "center_opacity", "Center Opacity" },
            { "radial_stretch", "Radial Stretch" }, { "stardust", "Stardust" },
            { "stardust_fade", "Stardust Fade" }, { "side_opacity", "Side Opacity" },
            { "mouse_sensitivity", "Mouse Sensitivity" }, { "mouse_smooth", "Mouse Smooth" },
            { "analog_sensitivity", "Analog Sensitivity" }, { "analog_smooth", "Analog Smooth" },
            { "top_opacity", "Top Opacity" }, { "bottom_opacity", "Bottom Opacity" },
            { "font", "FONT" }, { "font_size", "Font Size" }, { "radial_position", "Radial Position" },
            { "show_item_preview", "Show Item Preview during Gameplay" },
            { "show_item_quantity", "Show Item Quantity" },
            { "show_overflow_icon", "Show Icon inside Overflow" },
            { "draw_mark_distance", "Draw Mark Distance" },
            { "item_opacity", "General Item Opacity" },
            { "general_item_size", "General Item Size" },
            { "slot_section", "SLOT" }, { "icon_section", "ICON" },
            { "slot_size", "Slot Size" }, { "background_color", "Background Color" },
            { "background_opacity", "Background Opacity" },
            { "border_color", "Border Color" }, { "border_opacity", "Border Opacity" },
            { "icon_size", "Icon Size" }, { "base_icon_color", "Base Icon Color" },
            { "base_icon_opacity", "Base Icon Opacity" },
            { "overflow_size", "Overflow Size" },
            { "overflow_background_color", "Overflow Background" },
            { "overflow_background_opacity", "Overflow Background Opacity" },
            { "overflow_border_color", "Overflow Border Color" },
            { "overflow_border_opacity", "Overflow Border Opacity" },
            { "potions_title", "POTIONS" }, { "schools_title", "SCHOOLS" },
            { "enchants_title", "ENCHANTS" },
            { "potion_health", "Health" }, { "potion_stamina", "Stamina" },
            { "potion_magicka", "Magicka" }, { "potion_poison", "Poison" },
            { "potion_fire", "Fire" }, { "potion_frost", "Frost" },
            { "potion_shock", "Shock" },
            { "school_alteration", "Alteration" }, { "school_conjuration", "Conjuration" },
            { "school_destruction", "Destruction" }, { "school_illusion", "Illusion" },
            { "school_restoration", "Restoration" },
            { "magic_fire", "Fire Magic" }, { "magic_frost", "Frost Magic" },
            { "magic_shock", "Shock Magic" },
            { "enchant_fire", "Fire" }, { "enchant_frost", "Frost" },
            { "enchant_shock", "Shock" }, { "enchant_poison", "Poison" },
            { "enchant_default", "Other Enchantments" },
            { "overflow_opacity", "Overflow General Opacity" },
            { "layout_radial_line_opacity", "Radial Line Opacity" },
            { "layout_overflow_line_opacity", "Overflow Line Opacity" },
            { "save_layout", "SAVE" },
            { "load_layout", "LOAD" },
            { "layout_name", "Layout name" },
            { "default_layout_name", "My Layout" },
            { "no_saved_layouts", "No saved layouts" },
            { "top_item_quantity", "Top Item Quantity" },
            { "top_general_item_size", "Top General Item Size" },
            { "top_item_opacity", "Top Item Opacity" },
            { "top_line_opacity", "Top Radial Line Opacity" },
            { "top_slot_size", "Top Slot Size" },
            { "top_background_opacity", "Top Background Opacity" },
            { "top_border_opacity", "Top Border Opacity" },
            { "top_icon_size", "Top Icon Size" },
            { "top_icon_opacity", "Top Icon Opacity" },
            { "bottom_item_quantity", "Bottom Item Quantity" },
            { "bottom_general_item_size", "Bottom General Item Size" },
            { "bottom_item_opacity", "Bottom Item Opacity" },
            { "bottom_line_opacity", "Bottom Radial Line Opacity" },
            { "bottom_slot_size", "Bottom Slot Size" },
            { "bottom_background_opacity", "Bottom Background Opacity" },
            { "bottom_border_opacity", "Bottom Border Opacity" },
            { "bottom_icon_size", "Bottom Icon Size" },
            { "bottom_icon_opacity", "Bottom Icon Opacity" },
            { "item_preview_section", "ITEM PREVIEW" },
            { "preview_menu_section", "MENU" },
            { "right_section", "RIGHT" }, { "left_section", "LEFT" },
            { "preview_item_size", "Item Size" },
            { "preview_spell_size_multiplier", "Spell Size Multiplier" },
            { "preview_weapon_size_multiplier", "Weapon Size Multiplier" },
            { "preview_potion_size_multiplier", "Potion Size Multiplier" },
            { "preview_armor_size_multiplier", "Armor Size Multiplier" },
            { "preview_ammo_size_multiplier", "Ammo Size Multiplier" },
            { "preview_book_size_multiplier", "Book Size Multiplier" },
            { "preview_misc_size_multiplier", "Misc Size Multiplier" },
            { "preview_key_size_multiplier", "Key Size Multiplier" },
            { "preview_soul_gem_size_multiplier", "Soul Gem Size Multiplier" },
            { "preview_ingredient_size_multiplier", "Ingredient Size Multiplier" },
            { "preview_scroll_size_multiplier", "Scroll Size Multiplier" },
            { "preview_item_position_x", "Item Position X" },
            { "preview_item_position_y", "Item Position Y" },
            { "copy_opposite", "Copy opposite" },
            { "mirror_opposite", "Mirror opposite" },
            { "reset_value", "Reset" },
            { "alt_config_key", "Alt Config Key" },
            { "item_name", "Item Name" }, { "item_name_opacity", "Show Item Name Opacity" },
            { "item_name_position_x", "Show Item Name Position X" },
            { "item_name_position_y", "Show Item Name Position Y" },
            { "radial_section", "RADIAL" }, { "overflow_section", "OVERFLOW" },
            { "colors_section", "COLORS" }, { "top_section", "TOP" },
            { "central_section", "CENTRAL" }, { "bottom_section", "BOTTOM" },
            { "top_position", "Top Position" }, { "bottom_position", "Bottom Position" },
            { "top_stretch", "Top Stretch" }, { "bottom_stretch", "Bottom Stretch" },
            { "lock_top_radial", "Lock Top Radial" }, { "unlock_top_radial", "Unlock Top Radial" },
            { "lock_radial", "Lock Radial" }, { "unlock_radial", "Unlock Radial" },
            { "lock_bottom_radial", "Lock Bottom Radial" }, { "unlock_bottom_radial", "Unlock Bottom Radial" },
            { "lock_scroll", "Lock Scroll" }, { "unlock_scroll", "Unlock Scroll" },
            { "lock_mouse_movement", "Lock Mouse Movement" }, { "unlock_mouse_movement", "Unlock Mouse Movement" },
            { "reset_all_configuration", "Reset all configuration to Default" },
            { "item", "Item" }, { "item_section", "ITEM" },
            { "layout_editor", "LAYOUT EDITOR" },
            { "layout_instructions", "LEFT CLICK  -  MOVE ITEM       RIGHT CLICK  -  REMOVE ITEM" },
            { "custom_radial_editor", "CUSTOM RADIAL EDITOR" },
            { "zoom_out", "Zoom -" }, { "zoom_in", "Zoom +" },
            { "reset_view", "Reset view" }, { "clear_terminals", "Clear terminals" },
            { "save", "SAVE" }, { "load", "LOAD" }, { "reset", "RESET" },
            { "cancel", "CANCEL" }, { "invalid", "INVALID" },
            { "pieces", "PIECES" }, { "draw_path", "DRAW PATH" },
            { "smooth_line", "SMOOTH LINE" },
            { "smoothing_amount", "SMOOTHING" },
            { "curve_circle_tool", "CURVE / CIRCLE" },
            { "duplicate_selected", "DUPLICATE SELECTED" },
            { "erase_radial", "ERASE RADIAL" },
            { "editor_eraser", "ERASER" },
            { "eraser_radial_mode", "Radial" },
            { "eraser_overflow_mode", "Overflow" },
            { "radial_editor_section", "RADIAL" },
            { "eraser_size", "ERASER SIZE" },
            { "eraser_hardness", "ERASER HARDNESS" },
            { "radial_rotation", "RADIAL ROTATION" },
            { "layout_radial_rotation", "Radial Rotation" },
            { "radial_line_opacity", "RADIAL LINE OPACITY" },
            { "overflow_line_opacity", "OVERFLOW LINE OPACITY" },
            { "main_radial_items", "MAIN RADIAL ITEMS" },
            { "main_radial_stretch", "MAIN RADIAL STRETCH" },
            { "editor_ghost_items", "EDITOR GHOST ITEMS" },
            { "circuit_ready", "Circuit ready" },
            { "load_preset", "LOAD PRESET" },
            { "no_saved_presets", "No saved presets" },
            { "preset_name", "PRESET NAME" },
            { "ok", "OK" }, { "rename", "Rename" },
            { "duplicate", "Duplicate" }, { "delete", "Delete" },
            { "default_preset_name", "My Radial" },
            { "shape_classic_orbit", "Classic Orbit" },
            { "shape_harmonic_flower", "Harmonic Flower" },
            { "shape_dual_orbit", "Dual Orbit" },
            { "shape_turbine", "Turbine" },
            { "shape_spiral_galaxy", "Spiral Galaxy" },
            { "shape_pulsar_crown", "Pulsar Crown" },
            { "shape_liquid_diamond", "Liquid Diamond" },
            { "shape_comet_tail", "Comet Tail" },
            { "shape_rose_engine", "Rose Engine" },
            { "shape_quantum_ripple", "Quantum Ripple" },
            { "shape_star", "Star" },
            { "editor_instructions", "Hover a piece end: RED = exit, GREEN = entry\nScroll the canvas to preview both rotation directions" },
            { "track_need_piece", "Add at least one piece" },
            { "track_terminal_detached", "A radial terminal is not attached to a piece end" },
            { "track_duplicate_terminal", "A piece cannot have two equal terminals" },
            { "track_free_end", "Every free piece end needs a radial terminal" },
            { "track_need_entry_exit", "Add at least one entry and one exit" },
            { "track_disconnected_exit", "Each exit must reach an entry through one continuous path" },
            { "track_unused_piece", "Every piece must belong to a radial entry/exit path" },
            { "key_insert", "INSERT" }, { "key_delete", "DELETE" }, { "key_home", "HOME" },
            { "key_end", "END" }, { "key_page_up", "PAGEUP" }, { "key_page_down", "PAGEDOWN" },
            { "key_up", "UP" }, { "key_down", "DOWN" }, { "key_left", "LEFT" },
            { "key_right", "RIGHT" }, { "key_space", "SPACE" }, { "key_tab", "TAB" },
            { "key_enter", "ENTER" }, { "key_escape", "ESC" },
            { "key_backspace", "BACKSPACE" }, { "key_caps_lock", "CAPSLOCK" }
        };

        Dictionary g_current = kEnglish;
        std::string g_currentName = "EN";
        std::vector<std::string> g_available;

        std::string Trim(std::string a_value)
        {
            const auto first = a_value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                return {};
            const auto last = a_value.find_last_not_of(" \t\r\n");
            return a_value.substr(first, last - first + 1);
        }

        std::string DecodeEscapes(std::string_view a_value)
        {
            std::string result;
            result.reserve(a_value.size());
            for (std::size_t i = 0; i < a_value.size(); ++i)
            {
                if (a_value[i] == '\\' && i + 1 < a_value.size())
                {
                    const char next = a_value[++i];
                    if (next == 'n') result.push_back('\n');
                    else if (next == 't') result.push_back('\t');
                    else if (next == '\\') result.push_back('\\');
                    else { result.push_back('\\'); result.push_back(next); }
                }
                else
                {
                    result.push_back(a_value[i]);
                }
            }
            return result;
        }

        bool LoadFile(const std::filesystem::path& a_path, Dictionary& a_result)
        {
            std::ifstream file(a_path, std::ios::binary);
            if (!file)
                return false;

            Dictionary parsed;
            std::string line;
            bool firstLine = true;
            while (std::getline(file, line))
            {
                if (firstLine && line.size() >= 3 &&
                    static_cast<unsigned char>(line[0]) == 0xEF &&
                    static_cast<unsigned char>(line[1]) == 0xBB &&
                    static_cast<unsigned char>(line[2]) == 0xBF)
                {
                    line.erase(0, 3);
                }
                firstLine = false;
                const std::string trimmed = Trim(line);
                if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';')
                    continue;
                const auto separator = trimmed.find('=');
                if (separator == std::string::npos)
                    return false;
                const std::string key = Trim(trimmed.substr(0, separator));
                const std::string value = DecodeEscapes(Trim(trimmed.substr(separator + 1)));
                if (key.empty() || value.empty())
                    return false;
                // Traduções instaladas podem conservar chaves de versões
                // anteriores (por exemplo, nomes de presets removidos). Elas
                // não invalidam o arquivo inteiro; apenas deixam de ser usadas.
                if (!kEnglish.contains(key))
                    continue;
                if (parsed.contains(key))
                    return false;
                parsed.emplace(key, value);
            }

            // Keep existing translation files valid when new interface text is
            // introduced. Missing entries use the English source text until the
            // language file is updated, while malformed or unknown entries are
            // still rejected above.
            if (parsed.empty())
                return false;

            Dictionary merged = kEnglish;
            for (auto& [key, value] : parsed)
                merged[key] = std::move(value);

            a_result = std::move(merged);
            return true;
        }

        std::string NormalizeName(std::string a_name)
        {
            std::transform(a_name.begin(), a_name.end(), a_name.begin(),
                [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return a_name;
        }
    }

    std::filesystem::path GetLanguagesDirectory()
    {
        return Config::GetConfigPath().parent_path() / "languages";
    }

    void RefreshAvailableLanguages()
    {
        g_available.clear();
        std::error_code ec;
        const auto directory = GetLanguagesDirectory();
        std::filesystem::create_directories(directory, ec);
        for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
        {
            if (ec || !entry.is_regular_file() || entry.path().extension() != ".txt")
                continue;
            Dictionary candidate;
            if (LoadFile(entry.path(), candidate))
                g_available.push_back(entry.path().stem().string());
        }
        std::sort(g_available.begin(), g_available.end(),
            [](const std::string& a, const std::string& b) {
                return NormalizeName(a) < NormalizeName(b);
            });
    }

    bool SetCurrent(std::string_view a_languageName)
    {
        RefreshAvailableLanguages();
        const std::string wanted = NormalizeName(std::string(a_languageName));
        for (const std::string& available : g_available)
        {
            if (NormalizeName(available) != wanted)
                continue;
            Dictionary loaded;
            if (!LoadFile(GetLanguagesDirectory() / (available + ".txt"), loaded))
                return false;
            g_current = std::move(loaded);
            g_currentName = available;
            return true;
        }
        if (wanted == "EN" && std::find_if(g_available.begin(), g_available.end(),
            [](const std::string& name) { return NormalizeName(name) == "EN"; }) == g_available.end())
        {
            g_current = kEnglish;
            g_currentName = "EN";
            return true;
        }
        return false;
    }

    void Initialize(std::string_view a_selectedLanguage)
    {
        g_current = kEnglish;
        g_currentName = "EN";
        RefreshAvailableLanguages();
        SetCurrent(a_selectedLanguage.empty() ? "EN" : a_selectedLanguage);
    }

    const std::string& Get(std::string_view a_key)
    {
        const auto current = g_current.find(std::string(a_key));
        if (current != g_current.end())
            return current->second;
        const auto fallback = kEnglish.find(std::string(a_key));
        if (fallback != kEnglish.end())
            return fallback->second;
        static const std::string empty;
        return empty;
    }

    const std::string& GetCurrentName() { return g_currentName; }
    const std::vector<std::string>& GetAvailableLanguages() { return g_available; }
}
