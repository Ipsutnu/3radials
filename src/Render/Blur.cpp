#include "PCH.h"
#include "Blur.h"
#include "Logger.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

namespace Blur
{
    namespace
    {
        using Microsoft::WRL::ComPtr;

        struct BlurConstants
        {
            float texelX = 0.0f;
            float texelY = 0.0f;
            float directionX = 0.0f;
            float directionY = 0.0f;
            float strength = 0.0f;
            float padding[3]{};
        };

        ID3D11Device* g_device = nullptr;
        ID3D11DeviceContext* g_context = nullptr;
        IDXGISwapChain* g_swapChain = nullptr;

        bool g_initialized = false;
        bool g_targetActive = false;
        float g_strength = 0.0f;

        UINT g_width = 0;
        UINT g_height = 0;
        DXGI_FORMAT g_format = DXGI_FORMAT_UNKNOWN;
        UINT g_sampleCount = 1;
        UINT g_sampleQuality = 0;
        bool g_loggedNoTarget = false;
        bool g_loggedTargetFailure = false;
        bool g_loggedCallerTarget = false;

        ComPtr<ID3D11Texture2D> g_sceneTexture;
        ComPtr<ID3D11ShaderResourceView> g_sceneView;
        ComPtr<ID3D11Texture2D> g_blurTextureA;
        ComPtr<ID3D11ShaderResourceView> g_blurViewA;
        ComPtr<ID3D11RenderTargetView> g_blurTargetA;
        ComPtr<ID3D11Texture2D> g_blurTextureB;
        ComPtr<ID3D11ShaderResourceView> g_blurViewB;
        ComPtr<ID3D11RenderTargetView> g_blurTargetB;

        ComPtr<ID3D11VertexShader> g_vertexShader;
        ComPtr<ID3D11PixelShader> g_blurShader;
        ComPtr<ID3D11PixelShader> g_compositeShader;
        ComPtr<ID3D11SamplerState> g_sampler;
        ComPtr<ID3D11Buffer> g_constants;
        ComPtr<ID3D11BlendState> g_alphaBlend;
        ComPtr<ID3D11BlendState> g_opaqueBlend;
        ComPtr<ID3D11DepthStencilState> g_depthDisabled;
        ComPtr<ID3D11RasterizerState> g_rasterizer;

        constexpr float kFadeInSpeed = 4.5f;
        constexpr float kFadeOutSpeed = 3.5f;
        constexpr UINT kDownsample = 4;

        void ReleaseSizedResources()
        {
            g_sceneView.Reset();
            g_sceneTexture.Reset();
            g_blurTargetA.Reset();
            g_blurViewA.Reset();
            g_blurTextureA.Reset();
            g_blurTargetB.Reset();
            g_blurViewB.Reset();
            g_blurTextureB.Reset();
            g_width = 0;
            g_height = 0;
            g_format = DXGI_FORMAT_UNKNOWN;
            g_sampleCount = 1;
            g_sampleQuality = 0;
        }

        bool CompileShader(const char* source, const char* entry,
            const char* target, ID3DBlob** bytecode)
        {
            ComPtr<ID3DBlob> errors;
            const HRESULT result = D3DCompile(source, std::strlen(source),
                "p_radials_blur", nullptr, nullptr, entry, target,
                D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
                0, bytecode, errors.GetAddressOf());
            if (FAILED(result))
            {
                const char* message = errors
                    ? static_cast<const char*>(errors->GetBufferPointer())
                    : "unknown shader compilation error";
                Logger::GetSingleton().Print("Blur: shader error: {}", message);
                return false;
            }
            return true;
        }

        bool CreatePipeline()
        {
            static constexpr const char* vertexSource = R"(
                struct VSOut { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
                VSOut main(uint id : SV_VertexID)
                {
                    VSOut output;
                    float2 uv = float2((id << 1) & 2, id & 2);
                    output.position = float4(uv.x * 2.0 - 1.0,
                                             1.0 - uv.y * 2.0, 0.0, 1.0);
                    output.uv = uv;
                    return output;
                })";
            static constexpr const char* blurSource = R"(
                Texture2D sceneTexture : register(t0);
                SamplerState sceneSampler : register(s0);
                cbuffer BlurData : register(b0)
                {
                    float2 texelSize;
                    float2 direction;
                    float strength;
                    float3 padding;
                };
                float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
                {
                    float2 stepUV = texelSize * direction;
                    float4 color = sceneTexture.Sample(sceneSampler, uv) * 0.227027;
                    color += sceneTexture.Sample(sceneSampler, uv + stepUV * 1.384615) * 0.316216;
                    color += sceneTexture.Sample(sceneSampler, uv - stepUV * 1.384615) * 0.316216;
                    color += sceneTexture.Sample(sceneSampler, uv + stepUV * 3.230769) * 0.070270;
                    color += sceneTexture.Sample(sceneSampler, uv - stepUV * 3.230769) * 0.070270;
                    return float4(color.rgb, 1.0);
                })";
            static constexpr const char* compositeSource = R"(
                Texture2D blurTexture : register(t0);
                Texture2D sharpTexture : register(t1);
                SamplerState sceneSampler : register(s0);
                cbuffer BlurData : register(b0)
                {
                    float2 texelSize;
                    float2 direction;
                    float strength;
                    float3 padding;
                };
                float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
                {
                    float3 sharp = sharpTexture.Sample(sceneSampler, uv).rgb;
                    float3 blurred = blurTexture.Sample(sceneSampler, uv).rgb;

                    // Distância calculada em pixels: a máscara permanece um
                    // círculo real em ultrawide e em telas mais quadradas.
                    float2 pixelDelta = (uv - float2(0.5, 0.5)) / texelSize;
                    float shortestSide = min(1.0 / texelSize.x, 1.0 / texelSize.y);
                    float clearRadius = shortestSide * 0.145;
                    float softEdge = shortestSide * 0.225;
                    float outsideCenter = smoothstep(clearRadius,
                                                     clearRadius + softEdge,
                                                     length(pixelDelta));
                    float blurAmount = saturate(strength) * outsideCenter;
                    return float4(lerp(sharp, blurred, blurAmount), 1.0);
                })";

            ComPtr<ID3DBlob> vertexCode;
            ComPtr<ID3DBlob> blurCode;
            ComPtr<ID3DBlob> compositeCode;
            if (!CompileShader(vertexSource, "main", "vs_5_0", vertexCode.GetAddressOf()) ||
                !CompileShader(blurSource, "main", "ps_5_0", blurCode.GetAddressOf()) ||
                !CompileShader(compositeSource, "main", "ps_5_0", compositeCode.GetAddressOf()))
                return false;

            if (FAILED(g_device->CreateVertexShader(vertexCode->GetBufferPointer(),
                    vertexCode->GetBufferSize(), nullptr, g_vertexShader.GetAddressOf())) ||
                FAILED(g_device->CreatePixelShader(blurCode->GetBufferPointer(),
                    blurCode->GetBufferSize(), nullptr, g_blurShader.GetAddressOf())) ||
                FAILED(g_device->CreatePixelShader(compositeCode->GetBufferPointer(),
                    compositeCode->GetBufferSize(), nullptr, g_compositeShader.GetAddressOf())))
                return false;

            D3D11_SAMPLER_DESC sampler{};
            sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.MaxLOD = D3D11_FLOAT32_MAX;
            if (FAILED(g_device->CreateSamplerState(&sampler, g_sampler.GetAddressOf())))
                return false;

            D3D11_BUFFER_DESC buffer{};
            buffer.ByteWidth = sizeof(BlurConstants);
            buffer.Usage = D3D11_USAGE_DYNAMIC;
            buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            buffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if (FAILED(g_device->CreateBuffer(&buffer, nullptr, g_constants.GetAddressOf())))
                return false;

            D3D11_BLEND_DESC opaque{};
            opaque.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            if (FAILED(g_device->CreateBlendState(&opaque, g_opaqueBlend.GetAddressOf())))
                return false;

            D3D11_BLEND_DESC alpha = opaque;
            alpha.RenderTarget[0].BlendEnable = TRUE;
            alpha.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
            alpha.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            alpha.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            alpha.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            alpha.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            alpha.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            if (FAILED(g_device->CreateBlendState(&alpha, g_alphaBlend.GetAddressOf())))
                return false;

            D3D11_DEPTH_STENCIL_DESC depth{};
            depth.DepthEnable = FALSE;
            depth.StencilEnable = FALSE;
            if (FAILED(g_device->CreateDepthStencilState(&depth,
                    g_depthDisabled.GetAddressOf())))
                return false;

            D3D11_RASTERIZER_DESC raster{};
            raster.FillMode = D3D11_FILL_SOLID;
            raster.CullMode = D3D11_CULL_NONE;
            raster.DepthClipEnable = TRUE;
            return SUCCEEDED(g_device->CreateRasterizerState(&raster,
                g_rasterizer.GetAddressOf()));
        }

        bool CreateSizedResources(ID3D11Texture2D* backBuffer)
        {
            D3D11_TEXTURE2D_DESC backDesc{};
            backBuffer->GetDesc(&backDesc);
            if (g_sceneTexture && g_width == backDesc.Width &&
                g_height == backDesc.Height && g_format == backDesc.Format &&
                g_sampleCount == backDesc.SampleDesc.Count &&
                g_sampleQuality == backDesc.SampleDesc.Quality)
                return true;

            ReleaseSizedResources();
            g_width = backDesc.Width;
            g_height = backDesc.Height;
            g_format = backDesc.Format;
            g_sampleCount = backDesc.SampleDesc.Count;
            g_sampleQuality = backDesc.SampleDesc.Quality;

            Logger::GetSingleton().Print(
                "Blur: allocating resources {}x{}, format={}, samples={}:{}.",
                g_width, g_height, static_cast<unsigned int>(g_format),
                g_sampleCount, g_sampleQuality);

            D3D11_TEXTURE2D_DESC scene = backDesc;
            scene.ArraySize = 1;
            scene.MipLevels = 1;
            scene.SampleDesc = { 1, 0 };
            scene.Usage = D3D11_USAGE_DEFAULT;
            scene.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            scene.CPUAccessFlags = 0;
            scene.MiscFlags = 0;
            const HRESULT sceneTextureResult = g_device->CreateTexture2D(&scene,
                nullptr, g_sceneTexture.GetAddressOf());
            if (FAILED(sceneTextureResult))
            {
                Logger::GetSingleton().Print(
                    "Blur: failed to create scene texture (HRESULT=0x{:08X}).",
                    static_cast<unsigned long>(sceneTextureResult));
                return false;
            }
            const HRESULT sceneViewResult = g_device->CreateShaderResourceView(
                g_sceneTexture.Get(), nullptr, g_sceneView.GetAddressOf());
            if (FAILED(sceneViewResult))
            {
                Logger::GetSingleton().Print(
                    "Blur: failed to create scene SRV (HRESULT=0x{:08X}).",
                    static_cast<unsigned long>(sceneViewResult));
                return false;
            }

            D3D11_TEXTURE2D_DESC blur = scene;
            blur.Width = std::max<UINT>(1, backDesc.Width / kDownsample);
            blur.Height = std::max<UINT>(1, backDesc.Height / kDownsample);
            blur.BindFlags = D3D11_BIND_SHADER_RESOURCE |
                D3D11_BIND_RENDER_TARGET;
            const auto createBlurTexture = [&](ComPtr<ID3D11Texture2D>& texture,
                                               ComPtr<ID3D11ShaderResourceView>& view,
                                               ComPtr<ID3D11RenderTargetView>& target) {
                const HRESULT textureResult = g_device->CreateTexture2D(&blur,
                    nullptr, texture.GetAddressOf());
                const HRESULT viewResult = SUCCEEDED(textureResult)
                    ? g_device->CreateShaderResourceView(texture.Get(), nullptr,
                        view.GetAddressOf())
                    : textureResult;
                const HRESULT targetResult = SUCCEEDED(viewResult)
                    ? g_device->CreateRenderTargetView(texture.Get(), nullptr,
                        target.GetAddressOf())
                    : viewResult;
                if (FAILED(targetResult))
                {
                    Logger::GetSingleton().Print(
                        "Blur: failed to create intermediate texture resources (HRESULT=0x{:08X}).",
                        static_cast<unsigned long>(targetResult));
                    return false;
                }
                return true;
            };
            return createBlurTexture(g_blurTextureA, g_blurViewA, g_blurTargetA) &&
                createBlurTexture(g_blurTextureB, g_blurViewB, g_blurTargetB);
        }

        void UpdateConstants(float texelX, float texelY,
            float directionX, float directionY, float strength)
        {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(g_context->Map(g_constants.Get(), 0,
                    D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return;
            auto* data = static_cast<BlurConstants*>(mapped.pData);
            *data = { texelX, texelY, directionX, directionY, strength, {} };
            g_context->Unmap(g_constants.Get(), 0);
        }

        void DrawPass(ID3D11RenderTargetView* target,
            ID3D11ShaderResourceView* source, ID3D11PixelShader* shader,
            UINT width, UINT height, bool alphaBlend,
            float texelX, float texelY, float directionX, float directionY,
            ID3D11ShaderResourceView* secondarySource = nullptr)
        {
            ID3D11ShaderResourceView* nullViews[2]{ nullptr, nullptr };
            g_context->PSSetShaderResources(0, 2, nullViews);
            g_context->OMSetRenderTargets(1, &target, nullptr);
            D3D11_VIEWPORT viewport{};
            viewport.Width = static_cast<float>(width);
            viewport.Height = static_cast<float>(height);
            viewport.MaxDepth = 1.0f;
            g_context->RSSetViewports(1, &viewport);
            g_context->IASetInputLayout(nullptr);
            g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_context->VSSetShader(g_vertexShader.Get(), nullptr, 0);
            g_context->PSSetShader(shader, nullptr, 0);
            g_context->PSSetSamplers(0, 1, g_sampler.GetAddressOf());
            UpdateConstants(texelX, texelY, directionX, directionY, g_strength);
            g_context->PSSetConstantBuffers(0, 1, g_constants.GetAddressOf());
            g_context->OMSetBlendState(alphaBlend ? g_alphaBlend.Get() :
                g_opaqueBlend.Get(), nullptr, 0xFFFFFFFF);
            g_context->OMSetDepthStencilState(g_depthDisabled.Get(), 0);
            g_context->RSSetState(g_rasterizer.Get());
            g_context->PSSetShaderResources(0, 1, &source);
            if (secondarySource)
                g_context->PSSetShaderResources(1, 1, &secondarySource);
            g_context->Draw(3, 0);
            g_context->PSSetShaderResources(0, 2, nullViews);
        }
    }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
        IDXGISwapChain* swapChain)
    {
        if (g_initialized)
            return true;
        g_device = device;
        g_context = context;
        g_swapChain = swapChain;
        if (!g_device || !g_context || !g_swapChain || !CreatePipeline())
        {
            Shutdown();
            Logger::GetSingleton().Print("Blur: initialization failed.");
            return false;
        }
        g_initialized = true;
        Logger::GetSingleton().Print("Blur: renderer initialized.");
        return true;
    }

    void SetTarget(bool active)
    {
        if (g_targetActive == active)
            return;

        g_targetActive = active;
        Logger::GetSingleton().Print("Blur: target {}.",
            active ? "enabled" : "disabled");
    }

    void Reset()
    {
        g_targetActive = false;
        g_strength = 0.0f;
        Logger::GetSingleton().Print("Blur: reset.");
    }

    void Render(ID3D11Texture2D* targetTexture, float deltaTime)
    {
        if (!g_initialized)
            return;

        const float target = g_targetActive ? 1.0f : 0.0f;
        const float speed = g_targetActive ? kFadeInSpeed : kFadeOutSpeed;
        const float dt = std::clamp(deltaTime, 0.0f, 0.05f);
        const float factor = 1.0f - std::exp(-speed * dt);
        g_strength += (target - g_strength) * factor;
        if (!g_targetActive && g_strength < 0.002f)
        {
            g_strength = 0.0f;
            return;
        }

        ComPtr<ID3D11Texture2D> ownedBackBuffer;
        ID3D11Texture2D* backBuffer = targetTexture;
        if (!backBuffer)
        {
            const HRESULT getBufferResult = g_swapChain->GetBuffer(0,
                IID_PPV_ARGS(ownedBackBuffer.GetAddressOf()));
            if (FAILED(getBufferResult) || !ownedBackBuffer)
            {
                if (!g_loggedNoTarget)
                {
                    Logger::GetSingleton().Print(
                        "Blur: cannot acquire fallback swap-chain buffer 0 (HRESULT=0x{:08X}).",
                        static_cast<unsigned long>(getBufferResult));
                    g_loggedNoTarget = true;
                }
                return;
            }
            backBuffer = ownedBackBuffer.Get();
        }
        else if (!g_loggedCallerTarget)
        {
            Logger::GetSingleton().Print(
                "Blur: rendering into the caller-selected present buffer.");
            g_loggedCallerTarget = true;
        }

        if (!CreateSizedResources(backBuffer))
            return;

        D3D11_TEXTURE2D_DESC backDesc{};
        backBuffer->GetDesc(&backDesc);
        if (backDesc.SampleDesc.Count > 1)
            g_context->ResolveSubresource(g_sceneTexture.Get(), 0,
                backBuffer, 0, backDesc.Format);
        else
            g_context->CopyResource(g_sceneTexture.Get(), backBuffer);

        ComPtr<ID3D11RenderTargetView> backTarget;
        const HRESULT targetResult = g_device->CreateRenderTargetView(backBuffer,
            nullptr, backTarget.GetAddressOf());
        if (FAILED(targetResult))
        {
            if (!g_loggedTargetFailure)
            {
                Logger::GetSingleton().Print(
                    "Blur: cannot create render target for active buffer (HRESULT=0x{:08X}).",
                    static_cast<unsigned long>(targetResult));
                g_loggedTargetFailure = true;
            }
            return;
        }

        const UINT blurWidth = std::max<UINT>(1, g_width / kDownsample);
        const UINT blurHeight = std::max<UINT>(1, g_height / kDownsample);
        DrawPass(g_blurTargetA.Get(), g_sceneView.Get(), g_blurShader.Get(),
            blurWidth, blurHeight, false,
            1.0f / static_cast<float>(g_width),
            1.0f / static_cast<float>(g_height), 1.0f, 0.0f);
        DrawPass(g_blurTargetB.Get(), g_blurViewA.Get(), g_blurShader.Get(),
            blurWidth, blurHeight, false,
            1.0f / static_cast<float>(blurWidth),
            1.0f / static_cast<float>(blurHeight), 0.0f, 1.0f);
        DrawPass(backTarget.Get(), g_blurViewB.Get(), g_compositeShader.Get(),
            g_width, g_height, false,
            1.0f / static_cast<float>(g_width),
            1.0f / static_cast<float>(g_height), 0.0f, 0.0f,
            g_sceneView.Get());
    }

    void Render(float deltaTime)
    {
        Render(nullptr, deltaTime);
    }

    void Shutdown()
    {
        ReleaseSizedResources();
        g_vertexShader.Reset();
        g_blurShader.Reset();
        g_compositeShader.Reset();
        g_sampler.Reset();
        g_constants.Reset();
        g_alphaBlend.Reset();
        g_opaqueBlend.Reset();
        g_depthDisabled.Reset();
        g_rasterizer.Reset();
        g_device = nullptr;
        g_context = nullptr;
        g_swapChain = nullptr;
        g_initialized = false;
        g_targetActive = false;
        g_strength = 0.0f;
        g_loggedNoTarget = false;
        g_loggedTargetFailure = false;
        g_loggedCallerTarget = false;
    }

    float Strength()
    {
        return g_strength;
    }
}
