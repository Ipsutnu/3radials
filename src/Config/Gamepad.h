#pragma once

#include <cstdint>
#include <string>

namespace Gamepad
{
    inline constexpr int kBindingFlag = 0x10000;

    bool IsBinding(int value);
    int Encode(std::uint32_t button);
    std::uint32_t Decode(int value);
    bool Matches(int binding, std::uint32_t button);
    std::string ButtonName(std::uint32_t button);
    bool ParseBinding(const std::string& text, int& binding);

    void SetActivationHeld(bool held);
    bool IsActivationHeld();
    void PulseSelection();
    void PulseItemSelection();
    void UpdateFeedback();
    void ResetRuntimeState();
}
