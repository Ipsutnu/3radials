#pragma once

#include <d3d11.h>

#include <filesystem>

// Rasterizador SVG independente da pilha Direct2D do jogo. NanoSVG aceita
// os SVGs exportados pelo Inkscape usados pelos packs de ícones e produz uma
// textura RGBA normal para o mesmo caminho D3D11 já usado pelos PNGs.
namespace SvgRasterizer
{
    inline constexpr UINT kDefaultIconSize = 256;

    [[nodiscard]] ID3D11ShaderResourceView* Load(
        ID3D11Device* a_device,
        const std::filesystem::path& a_path,
        UINT a_size = kDefaultIconSize);
}
