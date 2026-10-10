#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11Texture2D;

namespace Blur
{
    // Blur experimental isolado. Remover estas quatro chamadas e os dois
    // arquivos restaura o comportamento anterior sem afetar o menu.
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
        IDXGISwapChain* swapChain);
    void SetTarget(bool active);
    // Desativa o efeito imediatamente, sem aguardar o fade-out.
    void Reset();
    // When the caller already knows which swap-chain image is being
    // presented, use it instead of assuming buffer zero.
    void Render(ID3D11Texture2D* target, float deltaTime);
    void Render(float deltaTime);
    void Shutdown();

    float Strength();
}
