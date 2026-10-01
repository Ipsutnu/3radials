#pragma once

struct ImGuiIO;

namespace Font
{
    void Initialize(ImGuiIO& io);
    void Apply();
    int Count();
    const char* Name(int index);
}
