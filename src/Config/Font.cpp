#include "Font.h"

#include "Config.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace Font
{
    namespace
    {
        struct Definition
        {
            const char* name;
            const char* file;
        };

        constexpr std::array<Definition, 9> kDefinitions{
            Definition{ "Segoe UI", "segoeui.ttf" },
            Definition{ "Arial", "arial.ttf" },
            Definition{ "Tahoma", "tahoma.ttf" },
            Definition{ "Verdana", "verdana.ttf" },
            Definition{ "Segoe Script", "segoesc.ttf" },
            Definition{ "Comic Sans MS", "comic.ttf" },
            Definition{ "Papyrus", "papyrus.ttf" },
            Definition{ "Lucida Handwriting", "lhandw.ttf" },
            Definition{ "Monotype Corsiva", "mtcorsva.ttf" }
        };

        std::vector<ImFont*> g_fonts;
    }

    int Count()
    {
        return static_cast<int>(kDefinitions.size());
    }

    const char* Name(int index)
    {
        return kDefinitions[std::clamp(index, 0, Count() - 1)].name;
    }

    void Initialize(ImGuiIO& io)
    {
        g_fonts.clear();
        const std::filesystem::path fontsDirectory =
            std::filesystem::path(std::getenv("WINDIR") ? std::getenv("WINDIR") : "C:\\Windows") /
            "Fonts";

        const auto addBaseFont = [&](const char* name) -> ImFont* {
            const auto path = fontsDirectory / name;
            return std::filesystem::exists(path)
                ? io.Fonts->AddFontFromFileTTF(path.string().c_str(), 13.0f)
                : nullptr;
        };
        const auto mergeFont = [&](ImFont* baseFont, const char* name) {
            const auto path = fontsDirectory / name;
            if (!baseFont || !std::filesystem::exists(path))
                return;
            ImFontConfig config;
            config.MergeMode = true;
            config.DstFont = baseFont;
            config.PixelSnapH = false;
            io.Fonts->AddFontFromFileTTF(path.string().c_str(), 13.0f, &config);
        };

        for (const auto& definition : kDefinitions)
        {
            ImFont* font = addBaseFont(definition.file);
            if (!font)
                font = g_fonts.empty() ? io.Fonts->AddFontDefaultVector() : g_fonts.front();
            g_fonts.push_back(font);
            mergeFont(font, "msyh.ttc");
            mergeFont(font, "msjh.ttc");
            mergeFont(font, "meiryo.ttc");
            mergeFont(font, "malgun.ttf");
        }
        io.FontDefault = g_fonts.front();
    }

    void Apply()
    {
        if (g_fonts.empty())
            return;
        Config::g_fontFamily = std::clamp(
            Config::g_fontFamily, 0, static_cast<int>(g_fonts.size()) - 1);
        ImGui::GetIO().FontDefault = g_fonts[Config::g_fontFamily];
    }
}
