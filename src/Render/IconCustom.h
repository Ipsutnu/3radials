#pragma once

#include <RE/Skyrim.h>
#include <d3d11.h>

// Resolve ícones externos por keyword: KWD_<EditorID>.svg ou .png em
// Data/SKSE/Plugins/p-radials/Icons. O resolvedor PNG legado continua em
// Icon.cpp e entra automaticamente como fallback quando não houver match.
namespace IconCustom
{
    bool Initialize(ID3D11Device* a_device);
    void Shutdown();

    // Reindexa os arquivos KWD_ e descarta o cache de texturas. Pode ser
    // chamado pelo botão Reload em tempo real.
    bool Reload();

    [[nodiscard]] bool HasValidConfiguration();

    // Retorna nullptr quando nenhum keyword externo se aplica ou quando a
    // fonte não pôde ser convertida. Icon.cpp então usa o PNG interno.
    ID3D11ShaderResourceView* Get(RE::TESForm* a_form);
}
