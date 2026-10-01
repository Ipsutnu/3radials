#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Language
{
    // Loads the selected UTF-8 translation. English remains the built-in
    // fallback for every missing/unavailable external resource.
    void Initialize(std::string_view a_selectedLanguage);
    void RefreshAvailableLanguages();
    bool SetCurrent(std::string_view a_languageName);

    [[nodiscard]] const std::string& Get(std::string_view a_key);
    [[nodiscard]] const std::string& GetCurrentName();
    [[nodiscard]] const std::vector<std::string>& GetAvailableLanguages();
    [[nodiscard]] std::filesystem::path GetLanguagesDirectory();
}
