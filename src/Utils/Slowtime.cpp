#include "PCH.h"
#include "Slowtime.h"

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
            return false;

        const float requested = std::clamp(multiplier, 0.1f, 0.8f);
        if (g_active)
        {
            if (std::abs(*value - g_ownedMultiplier) <= kTolerance)
            {
                *value = requested;
                g_ownedMultiplier = requested;
            }
            return true;
        }

        g_previousMultiplier = *value;
        if (std::abs(g_previousMultiplier - kNormalTime) > kTolerance)
        {
            g_previousMultiplier = kNormalTime;
            return false;
        }

        *value = requested;
        g_ownedMultiplier = requested;
        g_active = true;
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
        }

        g_active = false;
        g_previousMultiplier = kNormalTime;
        g_ownedMultiplier = kNormalTime;
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
