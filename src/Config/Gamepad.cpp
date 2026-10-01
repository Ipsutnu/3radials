#include "PCH.h"
#include "Gamepad.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <unordered_map>

namespace
{
    bool g_activationHeld = false;
    bool g_feedbackActive = false;
    std::chrono::steady_clock::time_point g_feedbackEnd{};

    void SetFeedback(float largeMotor, float smallMotor)
    {
        auto* manager = RE::BSInputDeviceManager::GetSingleton();
        auto* gamepad = manager ? manager->GetGamepad() : nullptr;
        if (gamepad)
            gamepad->SetVibration(largeMotor, smallMotor);

        // O delegate do Skyrim não encaminha vibração em todas as combinações
        // de runtime/backend. Envia também diretamente ao XInput, mantendo o
        // caminho acima para dispositivos tratados pelo próprio jogo.
        using XInputSetState_t = DWORD(WINAPI*)(DWORD, REX::W32::XINPUT_VIBRATION*);
        static XInputSetState_t xinputSetState = []() -> XInputSetState_t {
            constexpr const wchar_t* libraries[]{
                L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"
            };
            for (const auto* library : libraries)
            {
                if (const HMODULE module = GetModuleHandleW(library) ?
                    GetModuleHandleW(library) : LoadLibraryW(library))
                {
                    if (const auto proc = GetProcAddress(module, "XInputSetState"))
                        return reinterpret_cast<XInputSetState_t>(proc);
                }
            }
            return nullptr;
        }();
        if (xinputSetState)
        {
            REX::W32::XINPUT_VIBRATION vibration{};
            vibration.leftMotorSpeed = static_cast<std::uint16_t>(
                std::clamp(largeMotor, 0.0f, 1.0f) * 65535.0f);
            vibration.rightMotorSpeed = static_cast<std::uint16_t>(
                std::clamp(smallMotor, 0.0f, 1.0f) * 65535.0f);
            for (DWORD index = 0; index < 4; ++index)
                xinputSetState(index, &vibration);
        }
    }

    std::string Normalize(std::string value)
    {
        value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
            return std::isspace(c) || c == '_' || c == '-';
        }), value.end());
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return value;
    }
}

namespace Gamepad
{
    bool IsBinding(int value) { return (value & kBindingFlag) != 0; }
    int Encode(std::uint32_t button) { return kBindingFlag | static_cast<int>(button); }
    std::uint32_t Decode(int value) { return static_cast<std::uint32_t>(value & 0xFFFF); }
    bool Matches(int binding, std::uint32_t button)
    {
        return IsBinding(binding) && Decode(binding) == button;
    }

    std::string ButtonName(std::uint32_t button)
    {
        using Key = RE::BSWin32GamepadDevice::Key;
        switch (button)
        {
        case Key::kUp: return "GAMEPAD_DPAD_UP";
        case Key::kDown: return "GAMEPAD_DPAD_DOWN";
        case Key::kLeft: return "GAMEPAD_DPAD_LEFT";
        case Key::kRight: return "GAMEPAD_DPAD_RIGHT";
        case Key::kStart: return "GAMEPAD_START";
        case Key::kBack: return "GAMEPAD_BACK";
        case Key::kLeftThumb: return "GAMEPAD_L3";
        case Key::kRightThumb: return "GAMEPAD_R3";
        case Key::kLeftShoulder: return "GAMEPAD_L1";
        case Key::kRightShoulder: return "GAMEPAD_R1";
        case Key::kA: return "GAMEPAD_A";
        case Key::kB: return "GAMEPAD_B";
        case Key::kX: return "GAMEPAD_X";
        case Key::kY: return "GAMEPAD_Y";
        case Key::kLeftTrigger: return "GAMEPAD_L2";
        case Key::kRightTrigger: return "GAMEPAD_R2";
        default: return "GAMEPAD_" + std::to_string(button);
        }
    }

    bool ParseBinding(const std::string& text, int& binding)
    {
        const std::string key = Normalize(text);
        using Key = RE::BSWin32GamepadDevice::Key;
        static const std::unordered_map<std::string, std::uint32_t> names{
            { "GAMEPADDPADUP", Key::kUp },
            { "GAMEPADDPADDOWN", Key::kDown },
            { "GAMEPADDPADLEFT", Key::kLeft }, { "GAMEPADDPADRIGHT", Key::kRight },
            { "GAMEPADSTART", Key::kStart }, { "GAMEPADBACK", Key::kBack },
            { "GAMEPADL3", Key::kLeftThumb }, { "GAMEPADR3", Key::kRightThumb },
            { "GAMEPADL1", Key::kLeftShoulder }, { "GAMEPADR1", Key::kRightShoulder },
            { "GAMEPADA", Key::kA }, { "GAMEPADB", Key::kB },
            { "GAMEPADX", Key::kX }, { "GAMEPADY", Key::kY },
            { "GAMEPADL2", Key::kLeftTrigger }, { "GAMEPADR2", Key::kRightTrigger }
        };
        if (const auto it = names.find(key); it != names.end())
        {
            binding = Encode(it->second);
            return true;
        }
        return false;
    }

    void SetActivationHeld(bool held) { g_activationHeld = held; }
    bool IsActivationHeld() { return g_activationHeld; }

    void PulseSelection()
    {
        // Pulso intencionalmente curto e leve: apenas confirma que um novo
        // alvo foi adquirido sem competir com a vibração normal do jogo.
        SetFeedback(0.56f, 0.96f);
        g_feedbackActive = true;
        g_feedbackEnd = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(95);
    }

    void PulseItemSelection()
    {
        SetFeedback(1.0f, 1.0f);
        g_feedbackActive = true;
        g_feedbackEnd = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(250);
    }

    void UpdateFeedback()
    {
        if (g_feedbackActive && std::chrono::steady_clock::now() >= g_feedbackEnd)
        {
            SetFeedback(0.0f, 0.0f);
            g_feedbackActive = false;
        }
    }

    void ResetRuntimeState()
    {
        g_activationHeld = false;
        if (g_feedbackActive)
            SetFeedback(0.0f, 0.0f);
        g_feedbackActive = false;
    }
}
