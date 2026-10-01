#pragma once

#include "Logger.h"
#include <RE/Skyrim.h>

namespace ItemPreview
{
    class SilentPreviewMenu : public RE::IMenu
    {
    public:
        static constexpr const char* MENU_NAME = "Preview";

        SilentPreviewMenu()
        {
            // ============================================================
            // MENU FAKE PARA PREVIEW 3D
            // ============================================================
            // criamos um IMenu normal e damos a ele somente
            // os flags relevantes para o pipeline de item 3D.
            // ============================================================

            menuFlags.set(
                //RE::UI_MENU_FLAGS::kPausesGame,
                RE::UI_MENU_FLAGS::kDisablePauseMenu
                //RE::UI_MENU_FLAGS::kUpdateUsesCursor,
                //RE::UI_MENU_FLAGS::kInventoryItemMenu
                //RE::UI_MENU_FLAGS::kCustomRendering
            );

            //Logger::GetSingleton().Print(
            //    "SilentPreviewMenu: flags=0x{:X}",
            //    static_cast<std::uint32_t>(menuFlags.underlying())
            //);
        }

        ~SilentPreviewMenu() override = default;

        // ============================================================
        // PROCESS MESSAGE
        // ============================================================

        RE::UI_MESSAGE_RESULTS ProcessMessage(
            RE::UIMessage& a_message) override
        {
            return RE::UI_MESSAGE_RESULTS::kPassOn;
        }

        // ============================================================
        // SEM SCALEFORM
        // ============================================================

        void AdvanceMovie(
            float a_interval,
            std::uint32_t a_currentTime) override
        {
            // Sem SWF.
        }

        void PreDisplay() override
        {
            // Sem Scaleform.
        }

        void PostDisplay() override
        {
            Tick();

            static std::uint32_t tickCount = 0;

            if (++tickCount % 60 == 0)
            {
                //Logger::GetSingleton().Print(
                //    "SilentPreviewMenu: PostDisplay tick={}",
                //    tickCount
                //);
            }
        }

        void RefreshPlatform() override
        {
            // Sem Scaleform.
        }

        // ============================================================
        // API
        // ============================================================

        static void Register();
        static void Open();
        static void Close();
        static bool IsOpen();

    private:
        void Tick();
    };
}

namespace SettingsMenu
{
    class WheelSettingsMenu final : public RE::IMenu
    {
    public:
        inline static constexpr const char* MENU_NAME =
            "WheelSetting";

        WheelSettingsMenu();

        static RE::IMenu* Create();

        static void Register();

        static void Open();

        static void Close();

        static bool IsOpen();
    };
}