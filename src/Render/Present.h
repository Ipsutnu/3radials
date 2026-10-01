#pragma once

struct IDXGISwapChain;

namespace Present
{
    // Instala o hook diretamente no swapchain que o Skyrim recebeu. Quando o
    // Community Shaders usa Frame Generation, este objeto já é o proxy dele.
    bool Install(IDXGISwapChain* swapChain);
    void Uninstall();
    bool IsInstalled();

    // Diagnóstico de travamento: registra a última etapa alcançada pela
    // cadeia de apresentação. As fases são literais estáticos.
    void MarkWatchdogPhase(const char* phase) noexcept;
}
