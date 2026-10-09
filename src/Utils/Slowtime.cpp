#include "PCH.h"
#include "Slowtime.h"
#include "Logger.h"

#include <cmath>

namespace Slowtime
{
    namespace
    {
        constexpr float kNormalTime = 1.0f;
        constexpr float kTolerance = 0.0025f;
        bool g_active = false;
        float g_previousMultiplier = kNormalTime;
        float g_ownedMultiplier = kNormalTime;
        bool g_loggedExternalOverride = false;
        bool g_loggedUnavailable = false;
        bool g_loggedBlockedMultiplier = false;
        float g_lastBlockedMultiplier = kNormalTime;

        float* GetGlobalTimeMultiplier()
        {
            static REL::Relocation<float*> value{
                REL::RelocationID(511883, 388443)
            };
            return value.get();
        }
    }

    bool Begin(float multiplier)
    {
        float* value = GetGlobalTimeMultiplier();
        if (!value)
        {
            if (!g_loggedUnavailable)
            {
                Logger::GetSingleton().Print(
                    "Slowtime: global time multiplier relocation is unavailable; activation skipped.");
                g_loggedUnavailable = true;
            }
            return false;
        }
        g_loggedUnavailable = false;

        const float requested = std::clamp(multiplier, 0.1f, 0.8f);
        if (g_active)
        {
            if (std::abs(*value - g_ownedMultiplier) <= kTolerance)
            {
                *value = requested;
                g_ownedMultiplier = requested;
                g_loggedExternalOverride = false;
            }
            else if (!g_loggedExternalOverride)
            {
                Logger::GetSingleton().Print(
                    "Slowtime: an external system changed the multiplier from our {:.3f} to {:.3f}; preserving the external value.",
                    g_ownedMultiplier, *value);
                g_loggedExternalOverride = true;
            }
            return true;
        }

        g_previousMultiplier = *value;
        if (std::abs(g_previousMultiplier - kNormalTime) > kTolerance)
        {
            if (!g_loggedBlockedMultiplier || std::abs(g_lastBlockedMultiplier -
                    g_previousMultiplier) > kTolerance)
            {
                Logger::GetSingleton().Print(
                    "Slowtime: activation skipped; current multiplier {:.3f} is not the base value {:.3f}.",
                    g_previousMultiplier, kNormalTime);
                g_lastBlockedMultiplier = g_previousMultiplier;
                g_loggedBlockedMultiplier = true;
            }
            g_previousMultiplier = kNormalTime;
            return false;
        }

        *value = requested;
        g_ownedMultiplier = requested;
        g_active = true;
        g_loggedExternalOverride = false;
        g_loggedBlockedMultiplier = false;
        Logger::GetSingleton().Print(
            "Slowtime: activated {:.3f} -> {:.3f}.",
            g_previousMultiplier, g_ownedMultiplier);
        return true;
    }

    void End()
    {
        if (!g_active)
            return;

        if (float* value = GetGlobalTimeMultiplier(); value &&
            std::abs(*value - g_ownedMultiplier) <= kTolerance)
        {
            *value = g_previousMultiplier;
            Logger::GetSingleton().Print(
                "Slowtime: restored multiplier to {:.3f}.", g_previousMultiplier);
        }
        else if (value)
        {
            Logger::GetSingleton().Print(
                "Slowtime: restore skipped; external multiplier {:.3f} replaced our {:.3f}.",
                *value, g_ownedMultiplier);
        }
        else
        {
            Logger::GetSingleton().Print(
                "Slowtime: restore skipped; global time multiplier relocation is unavailable.");
        }

        g_active = false;
        g_previousMultiplier = kNormalTime;
        g_ownedMultiplier = kNormalTime;
        g_loggedExternalOverride = false;
        g_loggedBlockedMultiplier = false;
    }

    void Update(bool shouldBeActive, float multiplier)
    {
        if (shouldBeActive)
            Begin(multiplier);
        else
            End();
    }

    bool IsActive()
    {
        return g_active;
    }
}
