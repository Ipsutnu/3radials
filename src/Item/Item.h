// ItemPreview.h
#pragma once

#include <RE/Skyrim.h>
#include <imgui.h>

namespace ItemPreview
{
    bool Show(
        RE::TESForm* form,
        std::uint16_t uniqueID = 0,
        bool hasUniqueID = false
    );
    void Update();
    void Hide();
    bool IsVisible();

    // Posição no HUD (em pixels de tela) onde o item 3D deve projetar.
    // Chame ANTES de Update() no mesmo frame, ou ele mantém a última
    // posição definida.


    void SetHudPosition(ImVec2 a_screenPos);
    void SetSizeScale(float a_scale);
    void InvalidateSizeScale();

    void SetRotationDirection(float a_direction);

    // True quando o modelo já tem geometria carregada (radius > 0) e já
    // recebeu ao menos uma atualização de posição/rotação nesse estado.
    bool IsReady();

    //var para calculo de boundbox do item
    static bool g_scaleNormalized = false;

    // Tamanho de referência que queremos para todos os modelos.
    // Depois você ajusta este número visualmente.
    static constexpr float kTargetBoundRadius = 50.0f;

    RE::TESForm* GetCurrentForm();
}
