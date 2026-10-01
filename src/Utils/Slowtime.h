#pragma once

namespace Slowtime
{
    bool Begin(float multiplier);
    void End();
    void Update(bool shouldBeActive, float multiplier);
    [[nodiscard]] bool IsActive();
}
