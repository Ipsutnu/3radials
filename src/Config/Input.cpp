#include "PCH.h"

#include "Input.h"
#include "ItemInfo.h"
#include "Config.h"
#include "Resolution.h"
#include "Gamepad.h"
#include "Animation/Track/TrackEditor.h"

namespace
{
    bool g_settingsOwnsLeftClick = false;
    bool g_settingsOwnsRightClick = false;
    bool g_primaryActivationHeld = false;
    bool g_secondaryActivationHeld = false;
    bool g_automaticArrowHeld = false;
    float g_settingsLeftTriggerNextRepeat = 0.35f;
    float g_settingsRightTriggerNextRepeat = 0.35f;
    float g_gamepadDPadUpNextRepeat = 0.32f;
    float g_gamepadDPadDownNextRepeat = 0.32f;
    float g_gamepadDPadLeftNextRepeat = 0.32f;
    float g_gamepadDPadRightNextRepeat = 0.32f;

    bool ShouldRepeatSettingsTrigger(RE::ButtonEvent* button, float& nextRepeat)
    {
        if (!button->IsPressed())
        {
            nextRepeat = 0.35f;
            return false;
        }
        const float held = button->GetRuntimeData().heldDownSecs;
        if (button->IsDown() && held <= 0.0f)
        {
            nextRepeat = 0.35f;
            return true;
        }
        if (held < nextRepeat)
            return false;
        do
        {
            nextRepeat += 0.11f;
        } while (nextRepeat <= held);
        return true;
    }

    bool ShouldRepeatGameplayDPad(RE::ButtonEvent* button, float& nextRepeat)
    {
        if (!button->IsPressed())
        {
            nextRepeat = 0.32f;
            return false;
        }

        const float held = button->GetRuntimeData().heldDownSecs;
        if (button->IsDown() && held <= 0.0f)
        {
            nextRepeat = 0.32f;
            return true;
        }
        if (held < nextRepeat)
            return false;

        do
        {
            nextRepeat += 0.12f;
        } while (nextRepeat <= held);
        return true;
    }

    std::uint32_t VirtualKeyToSkyrimScan(int virtualKey)
    {
        const UINT scan = MapVirtualKeyW(
            static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC_EX);
        const UINT prefix = scan & 0xFF00u;
        const UINT code = scan & 0xFFu;

        // Skyrim/DirectInput representa as teclas estendidas E0 usando o bit
        // alto do scan code (por exemplo, Up = 0xC8 em vez de 0x48).
        return prefix == 0xE000u ? code | 0x80u : code;
    }

    bool MatchesKeyboardVirtualKey(std::uint32_t eventScan, int virtualKey)
    {
        const UINT mapped = MapVirtualKeyW(
            static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC_EX);
        const std::uint32_t rawScan = mapped & 0xFFu;
        const std::uint32_t skyrimScan = VirtualKeyToSkyrimScan(virtualKey);
        if (eventScan == rawScan || eventScan == skyrimScan)
            return true;

        // Alguns runtimes entregam as teclas E0 já no formato DirectInput
        // (bit 7 ligado), enquanto outros mantêm o scan code base. Converte
        // ambos de volta para VK para a comparação não depender do backend.
        UINT scanForWindows = eventScan;
        if ((eventScan & 0x80u) != 0)
            scanForWindows = 0xE000u | (eventScan & 0x7Fu);
        const UINT eventVirtualKey = MapVirtualKeyW(
            scanForWindows, MAPVK_VSC_TO_VK_EX);
        return eventVirtualKey == static_cast<UINT>(virtualKey);
    }

    bool AnyActivationHeld()
    {
        return g_primaryActivationHeld || g_secondaryActivationHeld || g_automaticArrowHeld;
    }

    void UpdateActivationState(bool previous)
    {
        const bool current = AnyActivationHeld();
        Gamepad::SetActivationHeld(current);
        if (!previous && current)
        {
            if (Menu::IsRadialLockedOpen() || Menu::CanDrawWheelMenu())
                Menu::HandleGPressed();
        }
        else if (previous && !current)
        {
            Menu::HandleGReleased();
        }
    }
}

InputHandler* InputHandler::GetSingleton()
{
    static InputHandler instance;
    return &instance;
}

void ResetWheelInputState()
{
    g_primaryActivationHeld = false;
    g_secondaryActivationHeld = false;
    g_automaticArrowHeld = false;
    g_settingsOwnsLeftClick = false;
    g_settingsOwnsRightClick = false;
    g_settingsLeftTriggerNextRepeat = 0.35f;
    g_settingsRightTriggerNextRepeat = 0.35f;
    g_gamepadDPadUpNextRepeat = 0.32f;
    g_gamepadDPadDownNextRepeat = 0.32f;
    g_gamepadDPadLeftNextRepeat = 0.32f;
    g_gamepadDPadRightNextRepeat = 0.32f;
    Gamepad::ResetRuntimeState();
}

void RegisterInputSink()
{
    auto* deviceManager = RE::BSInputDeviceManager::GetSingleton();
    if (!deviceManager)
        return;

    // Registra o nosso sink
    deviceManager->AddEventSink(InputHandler::GetSingleton());

    // Reordena os sinks para colocar o NOSSO no topo (índice 0)
    // Isso garante que tratamos a tecla 'E' ANTES do PlayerControls do Skyrim
    auto& sinks = deviceManager->sinks;

    for (RE::BSTArray<RE::BSTEventSink<RE::InputEvent*>*>::size_type i = 0;
        i < sinks.size();
        ++i)
    {
        if (sinks[i] == InputHandler::GetSingleton())
        {
            std::swap(sinks[i], sinks[0]);
            break;
        }
    }
}

void InputHandler::Register()
{
    auto* inputManager = RE::BSInputDeviceManager::GetSingleton();
    if (inputManager)
    {
        inputManager->AddEventSink(GetSingleton());
    }
}

RE::BSEventNotifyControl InputHandler::ProcessEvent(
    RE::InputEvent* const* a_event,
    RE::BSTEventSource<RE::InputEvent*>*)
{
    if (!a_event || !*a_event)
        return RE::BSEventNotifyControl::kContinue;

    // Iteração pelos eventos de entrada
    for (auto* event = *a_event; event; event = event->next)
    {
        if (!event)
            continue;

        if (event->eventType == RE::INPUT_EVENT_TYPE::kButton)
        {
            auto* button = event->AsButtonEvent();

            if (button)
            {

                // ============================================================
                // CAPTURA DA TECLA NO SETTINGS
                // ============================================================

                if (button &&
                    Menu::CaptureWheelKey(button))
                {
                    // Consome a tecla capturada para impedir que
                    // o Skyrim ou o WheelWheel a processe normalmente.

                    button->GetRuntimeData().value = 0.0f;

                    button->GetRuntimeData().heldDownSecs = 0.0f;

                    button->SetUserEvent("");

                    button->SetIDCode(0xFF);

                    continue;
                }   

                // ============================================================
                // ATUALIZA SCAN CODE DA TECLA CONFIGURADA IMEDIATAMENTE
                // ============================================================

                if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard)
                {
                    const bool previousDeviceState = AnyActivationHeld();
                    if (Gamepad::IsBinding(Config::g_toggleKey))
                        g_primaryActivationHeld = false;
                    if (Gamepad::IsBinding(Config::g_secondaryKey))
                        g_secondaryActivationHeld = false;
                    if (previousDeviceState != AnyActivationHeld())
                        UpdateActivationState(previousDeviceState);

                    const auto matchesKeyboardBinding = [&](int binding) {
                        if (binding == 0 || Gamepad::IsBinding(binding))
                            return false;
                        return MatchesKeyboardVirtualKey(
                            button->GetIDCode(), binding);
                    };
                    const bool primary = matchesKeyboardBinding(Config::g_toggleKey);
                    const bool secondary = matchesKeyboardBinding(Config::g_secondaryKey);
                    const bool altConfig = matchesKeyboardBinding(Config::g_altConfigKey);
                    // A tecla direta não toma prioridade quando foi configurada
                    // também como uma das teclas modificadoras do radial.
                    if (altConfig && !primary && !secondary)
                    {
                        if (button->IsDown() && button->GetRuntimeData().heldDownSecs <= 0.0f)
                            Menu::ToggleSettingsMenuFromAltKey();
                        button->GetRuntimeData().value = 0.0f;
                        button->GetRuntimeData().heldDownSecs = 0.0f;
                        button->SetUserEvent("");
                        button->SetIDCode(0xFF);
                        continue;
                    }
                    if (primary || secondary)
                    {
                        const bool previous = AnyActivationHeld();
                        if (button->IsDown() && button->GetRuntimeData().heldDownSecs <= 0.0f)
                        {
                            if (primary) g_primaryActivationHeld = true;
                            if (secondary) g_secondaryActivationHeld = true;
                        }
                        else if (!button->IsPressed())
                        {
                            if (primary) g_primaryActivationHeld = false;
                            if (secondary) g_secondaryActivationHeld = false;
                        }
                        UpdateActivationState(previous);
                        button->GetRuntimeData().value = 0.0f;
                        button->GetRuntimeData().heldDownSecs = 0.0f;
                        button->SetUserEvent("");
                        button->SetIDCode(0xFF);
                    }
                }
            }
        }
    }

    auto consumeButton = [](RE::ButtonEvent* button) {
        button->GetRuntimeData().value = 0.0f;
        button->GetRuntimeData().heldDownSecs = 0.0f;
        button->SetUserEvent("");
        button->SetIDCode(0xFF);
    };

    auto* gamepadUI = RE::UI::GetSingleton();
    const bool gamepadSettingsOpen =
        gamepadUI && gamepadUI->IsMenuOpen("WheelSetting");

    for (auto* event = *a_event; event; event = event->next)
    {
        if (!event)
            continue;

        if (auto* stick = event->AsThumbstickEvent())
        {
            const bool wheelOwnsStick = gamepadSettingsOpen ||
                Gamepad::IsActivationHeld() || g_showWindow;
            if (!wheelOwnsStick)
                continue;

            const bool rightStick =
                stick->GetIDCode() == RE::ThumbstickEvent::InputType::kRightThumbstick;
            const float lengthSq = stick->xValue * stick->xValue + stick->yValue * stick->yValue;

            if (!gamepadSettingsOpen && Gamepad::IsActivationHeld() &&
                g_radialMode == RadialMode::Gameplay && g_radialSide == RadialSide::None &&
                lengthSq > 0.36f)
            {
                Menu::GamepadOpenRadial(rightStick ? RadialSide::Right : RadialSide::Left);
            }
            else
            {
                Menu::GamepadMoveCursor(stick->xValue, stick->yValue, rightStick);
            }

            stick->xValue = 0.0f;
            stick->yValue = 0.0f;
            continue;
        }

        auto* button = event->AsButtonEvent();
        if (!button || button->GetDevice() != RE::INPUT_DEVICE::kGamepad)
            continue;

        const std::uint32_t id = button->GetIDCode();
        const bool previousDeviceState = AnyActivationHeld();
        if (!Gamepad::IsBinding(Config::g_toggleKey))
            g_primaryActivationHeld = false;
        if (!Gamepad::IsBinding(Config::g_secondaryKey))
            g_secondaryActivationHeld = false;
        if (previousDeviceState != AnyActivationHeld())
            UpdateActivationState(previousDeviceState);
        const bool primaryBinding = Gamepad::Matches(Config::g_toggleKey, id);
        const bool secondaryBinding = Gamepad::Matches(Config::g_secondaryKey, id);
        const bool altConfigBinding = Gamepad::Matches(Config::g_altConfigKey, id);
        // A tecla configurada sempre tem prioridade sobre qualquer comando
        // nativo do botão (incluindo L3/R3) e é consumida como modificadora.
        if (primaryBinding || secondaryBinding)
        {
            const bool previous = AnyActivationHeld();
            if (button->IsDown() && button->GetRuntimeData().heldDownSecs <= 0.0f)
            {
                if (primaryBinding) g_primaryActivationHeld = true;
                if (secondaryBinding) g_secondaryActivationHeld = true;
            }
            else if (!button->IsPressed())
            {
                if (primaryBinding) g_primaryActivationHeld = false;
                if (secondaryBinding) g_secondaryActivationHeld = false;
            }
            UpdateActivationState(previous);
            consumeButton(button);
            continue;
        }

        const bool firstPress = button->IsDown() &&
            button->GetRuntimeData().heldDownSecs <= 0.0f;
        if (altConfigBinding && !primaryBinding && !secondaryBinding)
        {
            if (firstPress)
                Menu::ToggleSettingsMenuFromAltKey();
            consumeButton(button);
            continue;
        }
        const bool wheelOwnsButton = gamepadSettingsOpen ||
            Gamepad::IsActivationHeld() || g_showWindow;
        if (!wheelOwnsButton)
            continue;

        using Key = RE::BSWin32GamepadDevice::Key;
        bool handled = false;

        if (gamepadSettingsOpen)
        {
            if (id == Key::kA || id == Key::kX)
            {
                if (firstPress)
                {
                    Menu::GamepadSettingsPrimaryDown();
                    handled = true;
                }
                else if (!button->IsPressed())
                {
                    Menu::GamepadSettingsPrimaryUp();
                    handled = true;
                }
            }
            else if (firstPress && id == Key::kB)
            {
                Menu::GamepadSettingsDelete();
                handled = true;
            }
            else if (firstPress && id == Key::kY)
            {
                Menu::GamepadSettingsClose(); handled = true;
            }
            else if (firstPress && id == Key::kUp)
            {
                Menu::GamepadSettingsDirectional(0, -1); handled = true;
            }
            else if (firstPress && id == Key::kDown)
            {
                Menu::GamepadSettingsDirectional(0, 1); handled = true;
            }
            else if (firstPress && id == Key::kLeft)
            {
                Menu::GamepadSettingsDirectional(-1, 0); handled = true;
            }
            else if (firstPress && id == Key::kRight)
            {
                Menu::GamepadSettingsDirectional(1, 0); handled = true;
            }
            else if (firstPress && id == Key::kLeftShoulder)
            {
                Menu::GamepadSettingsAdjustSlider(-1); handled = true;
            }
            else if (firstPress && id == Key::kRightShoulder)
            {
                Menu::GamepadSettingsAdjustSlider(1); handled = true;
            }
            else if (id == Key::kLeftTrigger)
            {
                if (ShouldRepeatSettingsTrigger(button, g_settingsLeftTriggerNextRepeat))
                    Menu::GamepadSettingsCycleRadial(-1);
                handled = true;
            }
            else if (id == Key::kRightTrigger)
            {
                if (ShouldRepeatSettingsTrigger(button, g_settingsRightTriggerNextRepeat))
                    Menu::GamepadSettingsCycleRadial(1);
                handled = true;
            }
        }
        else if (Gamepad::IsActivationHeld() ||
            (g_showWindow && g_radialSide != RadialSide::None))
        {
            float* dpadRepeat = nullptr;
            if (id == Key::kUp) dpadRepeat = &g_gamepadDPadUpNextRepeat;
            else if (id == Key::kDown) dpadRepeat = &g_gamepadDPadDownNextRepeat;
            else if (id == Key::kLeft) dpadRepeat = &g_gamepadDPadLeftNextRepeat;
            else if (id == Key::kRight) dpadRepeat = &g_gamepadDPadRightNextRepeat;
            const bool dpadStep = dpadRepeat &&
                ShouldRepeatGameplayDPad(button, *dpadRepeat);
            const bool dpadPressed = dpadRepeat && button->IsDown();

            if (Gamepad::IsActivationHeld() && firstPress &&
                (id == Key::kLeftThumb || id == Key::kRightThumb))
            {
                Menu::OpenSettingsMenu(); handled = true;
            }
            else if (g_radialMode == RadialMode::Inventory &&
                (id == Key::kUp || id == Key::kDown || id == Key::kLeft || id == Key::kRight))
            {
                const float dx = id == Key::kLeft ? -1.0f : id == Key::kRight ? 1.0f : 0.0f;
                const float dy = id == Key::kUp ? 1.0f : id == Key::kDown ? -1.0f : 0.0f;
                Menu::GamepadMoveCursor(dx, dy, false); handled = true;
            }
            else if (Gamepad::IsActivationHeld() && dpadPressed &&
                g_radialSide == RadialSide::None &&
                (id == Key::kLeft || id == Key::kRight))
            {
                handled = Menu::GamepadOpenRadial(
                    id == Key::kLeft ? RadialSide::Left : RadialSide::Right);
            }
            else if (Gamepad::IsActivationHeld() && dpadPressed &&
                g_radialSide == RadialSide::None &&
                (id == Key::kUp || id == Key::kDown))
            {
                const bool hasTop = !Menu::g_topItems.empty();
                const bool hasBottom = !Menu::g_bottomItems.empty();
                RadialSide target = RadialSide::None;
                if (hasTop && hasBottom)
                    target = id == Key::kUp ? RadialSide::Top : RadialSide::Bottom;
                else if (hasTop)
                    target = RadialSide::Top;
                else if (hasBottom)
                    target = RadialSide::Bottom;
                handled = target != RadialSide::None &&
                    Menu::GamepadOpenRadial(target);
            }
            else if (dpadStep &&
                (g_radialSide == RadialSide::Left ||
                 g_radialSide == RadialSide::Right) &&
                (id == Key::kLeft || id == Key::kRight))
            {
                Menu::GamepadStepSideSelection(id == Key::kLeft ? -1 : 1);
                handled = true;
            }
            else if (dpadStep &&
                (g_radialSide == RadialSide::Top ||
                 g_radialSide == RadialSide::Bottom) &&
                (id == Key::kUp || id == Key::kDown))
            {
                // Em ambos os radiais verticais, Up avança e Down volta.
                Menu::GamepadShoulder(id == Key::kUp ? 1 : -1);
                handled = true;
            }
            else if (dpadPressed && (id == Key::kUp || id == Key::kDown))
            {
                const bool hasTop = !Menu::g_topItems.empty();
                const bool hasBottom = !Menu::g_bottomItems.empty();
                RadialSide target = RadialSide::None;
                if (hasTop && hasBottom)
                    target = id == Key::kUp ? RadialSide::Top : RadialSide::Bottom;
                else if (hasTop)
                    target = RadialSide::Top;
                else if (hasBottom)
                    target = RadialSide::Bottom;
                handled = target != RadialSide::None &&
                    Menu::GamepadOpenRadial(target);
            }
            else if (firstPress && id == Key::kLeft &&
                g_radialSide != RadialSide::None)
            {
                Menu::GamepadShoulder(-1); handled = true;
            }
            else if (firstPress && id == Key::kRight &&
                g_radialSide != RadialSide::None)
            {
                Menu::GamepadShoulder(1); handled = true;
            }
            else if (firstPress && id == Key::kLeftShoulder)
            {
                Menu::GamepadShoulder(1); handled = true;
            }
            else if (firstPress && id == Key::kRightShoulder)
            {
                Menu::GamepadShoulder(-1); handled = true;
            }
        }

        if (handled)
            consumeButton(button);
    }

    if (!gamepadSettingsOpen)
    {
        for (auto* event = *a_event; event; event = event->next)
        {
            auto* button = event ? event->AsButtonEvent() : nullptr;
            if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard)
                continue;

            RadialSide side = RadialSide::None;
            if (MatchesKeyboardVirtualKey(button->GetIDCode(), VK_UP)) side = RadialSide::Top;
            else if (MatchesKeyboardVirtualKey(button->GetIDCode(), VK_DOWN)) side = RadialSide::Bottom;
            else if (MatchesKeyboardVirtualKey(button->GetIDCode(), VK_LEFT)) side = RadialSide::Left;
            else if (MatchesKeyboardVirtualKey(button->GetIDCode(), VK_RIGHT)) side = RadialSide::Right;
            else if (MatchesKeyboardVirtualKey(button->GetIDCode(), VK_NUMPAD8))
                side = RadialSide::Top;
            else if (MatchesKeyboardVirtualKey(button->GetIDCode(), VK_NUMPAD2))
                side = RadialSide::Bottom;
            else if (MatchesKeyboardVirtualKey(button->GetIDCode(), VK_NUMPAD4))
                side = RadialSide::Left;
            else if (MatchesKeyboardVirtualKey(button->GetIDCode(), VK_NUMPAD6))
                side = RadialSide::Right;
            if (side == RadialSide::None)
                continue;

            const bool physicalActivationHeld =
                g_primaryActivationHeld || g_secondaryActivationHeld;
            const bool firstPress = button->IsDown() &&
                button->GetRuntimeData().heldDownSecs <= 0.0f;
            bool handled = false;

            // Uma única opção controla tanto as setas dedicadas quanto
            // 8/2/4/6 do Numpad.
            const bool automaticOpen = Config::g_automaticArrowMenus;
            if (firstPress && (physicalActivationHeld || automaticOpen))
            {
                if (!physicalActivationHeld && automaticOpen)
                {
                    const bool previous = AnyActivationHeld();
                    g_automaticArrowHeld = true;
                    UpdateActivationState(previous);
                }
                handled = Menu::GamepadOpenRadial(side) || g_showWindow;
            }
            else if (!button->IsPressed() && g_automaticArrowHeld && automaticOpen)
            {
                const bool previous = AnyActivationHeld();
                g_automaticArrowHeld = false;
                UpdateActivationState(previous);
                handled = true;
            }
            else if (physicalActivationHeld)
            {
                handled = true;
            }

            if (handled)
                consumeButton(button);
        }
    }


    // Se o menu do Skyrim estiver aberto (inventário, pausa, etc.), não bloqueia nada
    //auto* ui = RE::UI::GetSingleton();
    //if (ui && ui->IsItemMenuOpen())
    //{
    //    return RE::BSEventNotifyControl::kContinue;
    //}

    auto* ui = RE::UI::GetSingleton();

    const bool settingsOpen =
        ui && ui->IsMenuOpen("WheelSetting");

    if (!settingsOpen)
    {
        g_settingsOwnsLeftClick = false;
        g_settingsOwnsRightClick = false;
    }

    if (!g_showWindow && !settingsOpen)
    {
        return RE::BSEventNotifyControl::kContinue;
    }

    // Processamento dos Eventos de Input
    for (auto* event = *a_event; event; event = event->next)
    {
        if (!event)
            continue;

        if (settingsOpen)
        {
            auto* layoutKey = event->AsButtonEvent();
            if (layoutKey && layoutKey->GetDevice() == RE::INPUT_DEVICE::kKeyboard)
            {
                const bool firstPress = layoutKey->IsDown() &&
                    layoutKey->GetRuntimeData().heldDownSecs <= 0.0f;
                if (Menu::SettingsLayoutKeyboardInput(layoutKey->GetIDCode(), firstPress))
                {
                    consumeButton(layoutKey);
                    continue;
                }
            }
        }

        if (settingsOpen && TrackEditor::IsOpen())
        {
            auto* editorKey = event->AsButtonEvent();
            if (editorKey && editorKey->GetDevice() == RE::INPUT_DEVICE::kKeyboard)
            {
                const bool editorFirstPress = editorKey->IsDown() &&
                    editorKey->GetRuntimeData().heldDownSecs <= 0.0f;
                if (TrackEditor::KeyboardInput(editorKey->GetIDCode(), editorFirstPress))
                {
                    consumeButton(editorKey);
                    continue;
                }
                const bool space = MatchesKeyboardVirtualKey(
                    editorKey->GetIDCode(), VK_SPACE);
                const bool control = MatchesKeyboardVirtualKey(
                    editorKey->GetIDCode(), VK_CONTROL) ||
                    MatchesKeyboardVirtualKey(editorKey->GetIDCode(), VK_LCONTROL) ||
                    MatchesKeyboardVirtualKey(editorKey->GetIDCode(), VK_RCONTROL);
                if (space || control)
                {
                    if (space) TrackEditor::SetPanModifier(editorKey->IsPressed());
                    if (control) TrackEditor::SetZoomModifier(editorKey->IsPressed());
                    consumeButton(editorKey);
                    continue;
                }
            }
        }

        // ============================================================
        // A. MOVIMENTO DO MOUSE
        // ============================================================

        if (event->GetEventType() ==
            RE::INPUT_EVENT_TYPE::kMouseMove)
        {
            auto* mouseMove =
                static_cast<RE::MouseMoveEvent*>(event);

            auto* ui = RE::UI::GetSingleton();

            const bool settingsOpen =
                ui && ui->IsMenuOpen("WheelSetting");

            // ========================================================
            // SETTINGS
            // ========================================================

            if (settingsOpen)
            {
                Menu::UsePhysicalSettingsCursor();
                // Deixa o Skyrim atualizar o cursor do menu.
                // Não bloqueia o movimento aqui.
                continue;
            }

            // Mouse events arrive in real pixels; all WheelWheel interaction
            // state is kept in the virtual 1920x1080 coordinate space.
            const ImVec2 virtualDelta = Resolution::ToVirtualDelta(ImVec2(
                static_cast<float>(mouseMove->mouseInputX),
                static_cast<float>(mouseMove->mouseInputY)));
            const float deltaX = virtualDelta.x;
            const float deltaY = virtualDelta.y;

            // ========================================================
            // INVENTÁRIO
            // ========================================================

            if (g_radialMode == RadialMode::Inventory &&
                g_inventoryDragMode == InventoryDragMode::KeyDrag &&
                !g_inventoryItemJustGrabbed)
            {
                g_inventoryDraggedPosition.x += deltaX;
                g_inventoryDraggedPosition.y += deltaY;
            }

            // ========================================================
            // GAMEPLAY - LIBERA MOVIMENTO DO MOUSE
            // ========================================================

            if (Menu::IsCurrentRadialMouseUnlocked())
            {
                // Não zera os deltas.
                // Permite que o Skyrim receba o movimento original.
                continue;
            }

            // ========================================================
            // GAMEPLAY - BLOQUEIA MOVIMENTO DA CÂMERA
            // ========================================================

            mouseMove->mouseInputX = 0;
            mouseMove->mouseInputY = 0;

            continue;
        }

        // ------------------------------------------------------------
        // B. BLOQUEIO E LEITURA DE SCROLL DO MOUSE (Button 8 e 9)
        // ------------------------------------------------------------

        auto* buttonEvent = event->AsButtonEvent();

        if (!buttonEvent)
            continue;

        if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kMouse)
        {
            const uint32_t mouseButton =
                buttonEvent->GetIDCode();
            
            // ========================================================
            // SETTINGS - CLIQUES DO MOUSE
            // ========================================================

            if (settingsOpen)
            {

                // ====================================================
                // BOTÃO ESQUERDO - LOCK / DRAG
                // ====================================================

                if (mouseButton == 0)
                {
                    if (TrackEditor::IsOpen())
                    {
                        if (buttonEvent->IsDown() &&
                            buttonEvent->GetRuntimeData().heldDownSecs <= 0.0f)
                            TrackEditor::MouseDown(
                                Resolution::ToVirtual(ImGui::GetIO().MousePos), 0);
                        else if (!buttonEvent->IsPressed())
                            TrackEditor::MouseUp(
                                Resolution::ToVirtual(ImGui::GetIO().MousePos), 0);
                        consumeButton(buttonEvent);
                        continue;
                    }
                    bool consumeClick = g_settingsOwnsLeftClick;

                    if (buttonEvent->IsDown())
                    {
                        // Executa somente no primeiro pressionamento.
                        if (buttonEvent->GetRuntimeData().heldDownSecs <= 0.0f)
                        {
                            // ====================================================
                            // 1: BOTÃO DE FECHAR SETTINGS
                            // ====================================================

                            if (Menu::SettingsCloseButtonClick())
                            {
                                // Settings fechado.
                                // Não inicia drag nem aciona outros botões.
                                g_settingsOwnsLeftClick = true;
                            }
                            // ====================================================
                            // 2. BOTÕES DETECT WHEEL TOGGLE KEY
                            // ====================================================

                            else if (Menu::SettingsWheelKeyClick())
                            {
                                // Começou a capturar a nova tecla.
                                g_settingsOwnsLeftClick = true;
                            }

                            // ============================================================
                            // 3. Click do description gameplay
                            // ============================================================

                            else if (Menu::SettingsGameplayDescriptionClick())
                            {
                                // Alterna a descrição durante o gameplay.
                                g_settingsOwnsLeftClick = true;
                            }
                            // ====================================================
                            // 4. BOTÕES DE TRAVA DOS RADIAIS
                            // ====================================================

                            else if (Menu::SettingsLockClick())
                            {
                                // Clique processado.
                                g_settingsOwnsLeftClick = true;
                            }

                            // ====================================================
                            // 5. BOTÕES DE LIBERAÇÃO DO MOUSE
                            // ====================================================

                            else if (Menu::HandleMouseUnlockClick())
                            {
                                // Clique processado.
                                g_settingsOwnsLeftClick = true;
                            }

                            // ====================================================
                            // 6. DRAG NORMAL DOS ITENS
                            // ====================================================

                            else
                            {
                                g_settingsOwnsLeftClick = Menu::SettingsMouseDown();
                            }

                            consumeClick = g_settingsOwnsLeftClick;
                        }
                    }
                    // IsUp() requires a positive held duration in CommonLib.
                    // A very quick click can report value 0 with duration 0,
                    // so use the actual pressed state to always release a
                    // captured settings slider.
                    else if (!buttonEvent->IsPressed())
                    {
                        if (g_settingsOwnsLeftClick)
                        {
                            Menu::SettingsMouseUp();
                            g_settingsOwnsLeftClick = false;
                            consumeClick = true;
                        }
                    }

                    if (consumeClick)
                    {
                        // Consome somente cliques pertencentes ao Wheel.
                        buttonEvent->GetRuntimeData().value = 0.0f;
                        buttonEvent->GetRuntimeData().heldDownSecs = 0.0f;
                        buttonEvent->SetUserEvent("");
                        buttonEvent->SetIDCode(0xFF);
                    }

                    continue;
                }

                // ====================================================
                // BOTÃO DIREITO - DELETAR ITEM
                // ====================================================

                if (mouseButton == 1)
                {
                    if (TrackEditor::IsOpen())
                    {
                        if (buttonEvent->IsDown() &&
                            buttonEvent->GetRuntimeData().heldDownSecs <= 0.0f)
                            TrackEditor::MouseDown(
                                Resolution::ToVirtual(ImGui::GetIO().MousePos), 1);
                        else if (!buttonEvent->IsPressed())
                            TrackEditor::MouseUp(
                                Resolution::ToVirtual(ImGui::GetIO().MousePos), 1);
                        consumeButton(buttonEvent);
                        continue;
                    }
                    bool consumeClick = g_settingsOwnsRightClick;

                    if (buttonEvent->IsDown() &&
                        buttonEvent->GetRuntimeData().heldDownSecs <= 0.0f)
                    {
                        g_settingsOwnsRightClick = Menu::SettingsRightClick();
                        consumeClick = g_settingsOwnsRightClick;
                    }

                    if (!buttonEvent->IsPressed())
                    {
                        consumeClick = g_settingsOwnsRightClick;
                        g_settingsOwnsRightClick = false;
                    }

                    if (consumeClick)
                    {
                        // Consome somente o clique de remoção do Wheel.
                        buttonEvent->GetRuntimeData().value = 0.0f;
                        buttonEvent->GetRuntimeData().heldDownSecs = 0.0f;
                        buttonEvent->SetUserEvent("");
                        buttonEvent->SetIDCode(0xFF);
                    }

                    continue;
                }
            }
            
            // ========================================================
            // SETTINGS - SCROLL INDEPENDENTE
            // ========================================================

            if (settingsOpen &&
                (mouseButton == 8 || mouseButton == 9))
            {
                if (buttonEvent->GetRuntimeData().value != 0.0f)
                {
                    const int direction =
                        (mouseButton == 8) ? 1 : -1;

                    if (TrackEditor::IsOpen())
                    {
                        TrackEditor::Scroll(direction);
                        consumeButton(buttonEvent);
                        continue;
                    }

                    if (!Menu::ScrollSettingsItemInfo(direction))
                    {
                        // Mouse fora do painel:
                        // continua rolando os radiais normalmente.

                        Menu::ScrollSettingsRadial(direction);
                    }
                }

                // Impede o Skyrim de processar o scroll.
                buttonEvent->GetRuntimeData().value = 0.0f;
                buttonEvent->GetRuntimeData().heldDownSecs = 0.0f;
                buttonEvent->SetUserEvent("");
                buttonEvent->SetIDCode(0xFF);

                continue;
            }

            // ========================================================
            // INPUT ORIGINAL
            // ========================================================

            if (mouseButton == 8 || mouseButton == 9)
            {
                if (g_showWindow)
                {
                    if (buttonEvent->GetRuntimeData().value != 0.0f)
                    {
                        // ============================================================
                        // SETTINGS - SCROLL NO CENTRO DO RADIAL
                        // ============================================================

                        if (g_radialMode == RadialMode::Gameplay &&
                            g_radialSide == RadialSide::None)
                        {
                            Menu::HandleSettingsScroll();
                        }
                        else
                        {
                            // Aqui permanece sua lógica original de scroll
                            // dos radiais Top, Bottom, Left e Right.
                        }
                        const float scrollDelta =
                            (mouseButton == 8) ? -1.0f : 1.0f;

                        // ====================================================
                        // TOP / BOTTOM
                        // ====================================================

                        if (g_radialSide == RadialSide::Top ||
                            g_radialSide == RadialSide::Bottom)
                        {
                            const int direction =
                                (mouseButton == 8) ? 1 : -1;

                            Menu::ScrollTopBottomRadial(direction);
                        }
                        else if (
                            g_radialSide == RadialSide::Left ||
                            g_radialSide == RadialSide::Right)
                        {
                            Menu::ScrollSideRadial(
                                scrollDelta > 0.0f ? -1 : 1
                            );
                        }

                        // ====================================================
                        // LEFT / RIGHT
                        // ====================================================

                        else if (
                            g_radialSide == RadialSide::Left ||
                            g_radialSide == RadialSide::Right)
                        {
                            Menu::ScrollSideRadial(
                                scrollDelta > 0.0f ? -1 : 1
                            );
                        }
                    }

                    // ========================================================
                    // CONSOME O WHEEL
                    // ========================================================

                    buttonEvent->GetRuntimeData().value = 0.0f;
                    buttonEvent->GetRuntimeData().heldDownSecs = 0.0f;
                    buttonEvent->SetUserEvent("");
                    buttonEvent->SetIDCode(0xFF);
                }

                continue;
            }
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}
