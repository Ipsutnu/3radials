#include "PCH.h"
#include "Logger.h"
#include "Config.h"
#include "Input.h"
#include "Preview.h"
#include "RenderHooks.h"
#include "Language.h"
#include "Serialization.h"
#include "Menu/Menu.h"
#include "Animation/Track/TrackLayout.h"

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    Serialization::Register();

    auto* messaging = SKSE::GetMessagingInterface();

    if (!messaging)
    {
        return false;
    }

    messaging->RegisterListener(
        [](SKSE::MessagingInterface::Message* message)
        {
            if (!message)
            {
                return;
            }

            switch (message->type)
            {
            case SKSE::MessagingInterface::kDataLoaded:
            {
                Logger::GetSingleton().Print("SKSEPluginLoad called :)");

                Config::MigrateLegacyStorage();
                Track::Load();
                Config::LoadConfig();
                Language::Initialize(Config::g_language);
                Config::g_language = Language::GetCurrentName();

                ItemPreview::SilentPreviewMenu::Register();
                SettingsMenu::WheelSettingsMenu::Register();

                Menu::RegisterRadialUniqueIDListener();
                //BlurController::Init();

                RegisterInputSink();

                break;
            }

            case SKSE::MessagingInterface::kPreLoadGame:
            {
                /*
                 * Se futuramente precisarmos resetar estado
                 * runtime antes de carregar save.
                 */

                break;
            }

            case SKSE::MessagingInterface::kPostLoadGame:
            {
                Logger::GetSingleton().Print("State: PostLoadGame");

                break;
            }
            }
        }
    );

    //============================================================
    // INICIA LOGGER
    // ===========================================================

    Logger::Initialize();

    /*
     * ============================================================
     * INSTALA O HOOK DO RENDERER
     * ============================================================
     *
     * NÃO espera PostLoadGame.
     */

    if (!RenderHooks::Install())
    {
        Logger::GetSingleton().Print("RenderHooks::Install FAILED.");

        return false;
    }

    Logger::GetSingleton().Print("RENDER HOOK READY.");

    return true;
}
