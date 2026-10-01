#include "Preview.h"
#include "Item.h"
#include "Logger.h"
#include "Input.h"
#include "Animation/Track/TrackEditor.h"

namespace ItemPreview
{
    namespace
    {
        RE::IMenu* CreateSilentPreviewMenu()
        {
            //Logger::GetSingleton().Print(
            //    "SilentPreviewMenu: factory chamada"
            //);

            return new SilentPreviewMenu();
        }
    }

    void SilentPreviewMenu::Register()
    {
        auto* ui = RE::UI::GetSingleton();

        if (!ui)
        {
            //Logger::GetSingleton().Print(
            //    "SilentPreviewMenu: UI singleton nulo, registro falhou"
            //);

            return;
        }

        ui->Register(
            MENU_NAME,
            CreateSilentPreviewMenu
        );

        Logger::GetSingleton().Print(
            "Preview: registrado como '{}'",
            MENU_NAME
        );
    }

    void SilentPreviewMenu::Open()
    {
        if (IsOpen())
            return;

        auto* queue = RE::UIMessageQueue::GetSingleton();

        if (!queue)
            return;

        queue->AddMessage(
            MENU_NAME,
            RE::UI_MESSAGE_TYPE::kShow,
            nullptr
        );

        //Logger::GetSingleton().Print(
        //    "SilentPreviewMenu: Open() enviado"
        //);
    }

    void SilentPreviewMenu::Close()
    {
        if (!IsOpen())
            return;

        auto* queue = RE::UIMessageQueue::GetSingleton();

        if (!queue)
            return;

        queue->AddMessage(
            MENU_NAME,
            RE::UI_MESSAGE_TYPE::kHide,
            nullptr
        );

        //Logger::GetSingleton().Print(
        //    "SilentPreviewMenu: Close() enviado"
        //);
    }

    bool SilentPreviewMenu::IsOpen()
    {
        auto* ui = RE::UI::GetSingleton();

        if (!ui)
            return false;

        return ui->IsMenuOpen(MENU_NAME);
    }

    void SilentPreviewMenu::Tick()
    {
        // The WheelWheel render pass updates the preview after its ImGui
        // draw data, so the 3D item remains above radial geometry.
    }
}

namespace SettingsMenu
{


    static bool g_blurApplied = false;

    // ============================================================
    // CONSTRUCTOR
    // ============================================================

    WheelSettingsMenu::WheelSettingsMenu()
    {
        menuFlags.set(
            RE::UI_MENU_FLAGS::kPausesGame
            //RE::UI_MENU_FLAGS::kRendersUnderPauseMenu,
            //RE::UI_MENU_FLAGS::kUsesCursor,
            //RE::UI_MENU_FLAGS::kUpdateUsesCursor
        );

        //Logger::GetSingleton().Print(
        //    "Settings: created | flags=0x{:X}",
        //    static_cast<std::uint32_t>(
        //        menuFlags.underlying()
        //    )
        //);
    }

    // ============================================================
    // FACTORY
    // ============================================================

    RE::IMenu* WheelSettingsMenu::Create()
    {
        return new WheelSettingsMenu();
    }

    // ============================================================
    // REGISTER
    // ============================================================

    void WheelSettingsMenu::Register()
    {
        auto* ui = RE::UI::GetSingleton();

        if (!ui)
            return;

        ui->Register(
            MENU_NAME,
            Create
        );

        Logger::GetSingleton().Print(
            "Settings: registered"
        );
    }

    // ============================================================
    // OPEN
    // ============================================================

    void WheelSettingsMenu::Open()
    {
        if (IsOpen())
            return;

        auto* queue =
            RE::UIMessageQueue::GetSingleton();

        if (!queue)
            return;

        // ============================================================
        // BLUR
        // ============================================================

        if (!g_blurApplied)
        {
            auto* blurManager =
                RE::UIBlurManager::GetSingleton();

            if (blurManager)
            {
                blurManager->IncrementBlurCount();
                g_blurApplied = true;
            }
        }

        // ============================================================
        // OPEN SETTINGS
        // ============================================================
        
        queue->AddMessage(
            MENU_NAME,
            RE::UI_MESSAGE_TYPE::kShow,
            nullptr
        );

        //Logger::GetSingleton().Print(
        //    "WheelSettingsMenu: Open() enviado"
        //);
    }

    // ============================================================
    // CLOSE
    // ============================================================

    void WheelSettingsMenu::Close()
    {
        TrackEditor::Cancel();
        ResetWheelInputState();

        auto* queue =
            RE::UIMessageQueue::GetSingleton();

        if (!queue)
            return;

        //reseta posiçao do info item preview no settings
        Menu::ResetSettingsItemInfo();

        // ============================================================
        // REMOVE BLUR
        // ============================================================

        if (g_blurApplied)
        {
            auto* blurManager =
                RE::UIBlurManager::GetSingleton();

            if (blurManager)
            {
                blurManager->DecrementBlurCount();
            }

            g_blurApplied = false;
        }

        // ============================================================
        // CLOSE SETTINGS
        // ============================================================

        queue->AddMessage(
            MENU_NAME,
            RE::UI_MESSAGE_TYPE::kHide,
            nullptr
        );

        //Logger::GetSingleton().Print(
        //    "WheelSettingsMenu: Close() enviado"
        //);
    }

    // ============================================================
    // IS OPEN
    // ============================================================

    bool WheelSettingsMenu::IsOpen()
    {
        auto* ui = RE::UI::GetSingleton();

        return ui &&
            ui->IsMenuOpen(MENU_NAME);
    }
}
