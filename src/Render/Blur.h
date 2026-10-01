#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;

namespace Blur
{
    // Blur experimental isolado. Remover estas quatro chamadas e os dois
    // arquivos restaura o comportamento anterior sem afetar o menu.
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
        IDXGISwapChain* swapChain);
    void SetTarget(bool active);
    void Render(float deltaTime);
    void Shutdown();

    float Strength();
}
