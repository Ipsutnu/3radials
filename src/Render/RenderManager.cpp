#include "RenderManager.h"
#include "PCH.h"
#include "Item.h"
#include "Icon.h"
#include "Preview.h"
#include "Blur.h"
#include "Present.h"
#include "Resolution.h"
#include "Config.h"
#include "Font.h"

#include "Logger.h"
//#include "Blur.h"

#include <RE/Skyrim.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <vector>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <cwctype>

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <wrl/client.h>



namespace RenderManager
{
    namespace
    {
        ID3D11Device* g_device = nullptr;

        ID3D11DeviceContext* g_context = nullptr;

        IDXGISwapChain* g_swapChain = nullptr;

        HWND g_gameWindow = nullptr;

        bool g_initialized = false;
        bool g_drawDataReady = false;
        float g_presentDeltaTime = 1.0f / 60.0f;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> g_presentBackBuffer;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> g_presentTarget;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> g_cleanPresentBuffer;
        Microsoft::WRL::ComPtr<IDXGISwapChain3> g_indexedSwapChain;
        bool g_backBufferModeDetected = false;
        bool g_restoreCleanBufferAfterPresent = false;
        bool g_loggedCommunityShadersUITarget = false;
        bool g_loggedSeparateUITarget = false;

        bool IsCommunityShadersSwapChainProxy()
        {
            if (!g_swapChain)
                return false;

            auto** vtable = *reinterpret_cast<void***>(g_swapChain);
            if (!vtable || !vtable[9])  // IDXGISwapChain::GetBuffer
                return false;

            HMODULE owner = nullptr;
            const auto address = reinterpret_cast<LPCWSTR>(vtable[9]);
            if (!GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    address, &owner) || !owner)
                return false;

            wchar_t modulePath[MAX_PATH]{};
            if (!GetModuleFileNameW(owner, modulePath, MAX_PATH))
                return false;

            std::wstring normalized(modulePath);
            std::transform(normalized.begin(), normalized.end(),
                normalized.begin(), [](wchar_t value) {
                    return static_cast<wchar_t>(std::towlower(value));
                });
            return normalized.find(L"communityshaders.dll") !=
                std::wstring::npos;
        }

        bool AcquireCurrentBackBuffer(
            Microsoft::WRL::ComPtr<ID3D11Texture2D>& result)
        {
            result.Reset();
            if (!g_swapChain)
                return false;

            if (!g_backBufferModeDetected)
            {
                g_backBufferModeDetected = true;

                DXGI_SWAP_CHAIN_DESC desc{};
                const bool communityShadersProxy =
                    IsCommunityShadersSwapChainProxy();

                if (!communityShadersProxy &&
                    SUCCEEDED(g_swapChain->GetDesc(&desc)) &&
                    desc.BufferCount > 1)
                {
                    // Só consulte IDXGISwapChain3 em objetos DXGI reais. O
                    // proxy do CS retorna sucesso para essa interface, mas sua
                    // vtable implementa apenas IDXGISwapChain; chamar métodos
                    // de SwapChain3 nele causa acesso inválido.
                    if (SUCCEEDED(g_swapChain->QueryInterface(IID_PPV_ARGS(
                            g_indexedSwapChain.GetAddressOf()))))
                    {
                        Logger::GetSingleton().Print(
                            "Present: flip-model swap chain detected; using the current back-buffer index.");
                    }
                }

                if (!g_indexedSwapChain)
                {
                    if (communityShadersProxy)
                    {
                        Logger::GetSingleton().Print(
                            "Present: Community Shaders swap-chain proxy detected; safely using buffer 0.");
                    }
                    else
                    {
                        Logger::GetSingleton().Print(
                            "Present: single-buffer interface detected; using buffer 0.");
                    }
                }
            }

            const UINT bufferIndex = g_indexedSwapChain ?
                g_indexedSwapChain->GetCurrentBackBufferIndex() : 0;
            return SUCCEEDED(g_swapChain->GetBuffer(bufferIndex,
                       IID_PPV_ARGS(result.GetAddressOf()))) && result;
        }

        bool GetRenderTargetSize(ID3D11RenderTargetView* a_target,
            ImVec2& a_size)
        {
            a_size = {};
            if (!a_target)
                return false;

            Microsoft::WRL::ComPtr<ID3D11Resource> resource;
            a_target->GetResource(resource.GetAddressOf());
            if (!resource)
                return false;

            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            if (FAILED(resource.As(&texture)) || !texture)
                return false;

            D3D11_TEXTURE2D_DESC desc{};
            texture->GetDesc(&desc);
            if (desc.Width == 0 || desc.Height == 0)
                return false;

            a_size = ImVec2(static_cast<float>(desc.Width),
                static_cast<float>(desc.Height));
            return true;
        }

        // Frame-generation/upscaler proxies may bind a dedicated UI target
        // before forwarding Present. Compare the underlying resource instead
        // of relying on a proxy DLL name.
        bool IsSwapChainBackBuffer(ID3D11RenderTargetView* a_target,
            ID3D11Texture2D* a_backBuffer)
        {
            if (!a_target || !a_backBuffer)
                return false;

            Microsoft::WRL::ComPtr<ID3D11Resource> targetResource;
            a_target->GetResource(targetResource.GetAddressOf());
            if (!targetResource)
                return false;

            Microsoft::WRL::ComPtr<ID3D11Texture2D> targetTexture;
            return SUCCEEDED(targetResource.As(&targetTexture)) &&
                targetTexture.Get() == a_backBuffer;
        }

        //float g_globalAlpha = 0.0f;

        constexpr float g_fadeSpeed = 5.0f;

        //newrender
        std::vector<std::uint8_t> g_uiPixels;
        UINT g_uiWidth = 0;
        UINT g_uiHeight = 0;
        bool g_uiCaptured = false;
    }



    bool Init()
    {
        if (g_initialized)
        {
            return true;
        }


        /*
         * ============================================================
         * SKYRIM RENDERER
         * ============================================================
         */

        auto* renderer =
            RE::BSGraphics::Renderer::GetSingleton();

        if (!renderer)
        {
            Logger::GetSingleton().Print(
                "RenderManager: Renderer == nullptr."
            );

            return false;
        }


        auto& runtimeData =
            renderer->GetRuntimeData();


        /*
         * ============================================================
         * DEVICE
         * ============================================================
         */

        g_device =
            reinterpret_cast<ID3D11Device*>(
                runtimeData.forwarder
            );


        /*
         * ============================================================
         * CONTEXT
         * ============================================================
         */

        g_context =
            reinterpret_cast<ID3D11DeviceContext*>(
                runtimeData.context
            );


        /*
         * ============================================================
         * SWAPCHAIN
         * ============================================================
         */

        g_swapChain =
            reinterpret_cast<IDXGISwapChain*>(
                runtimeData.renderWindows[0].swapChain
            );


        if (!g_device)
        {
            Logger::GetSingleton().Print(
                "RenderManager: Device == nullptr."
            );

            return false;
        }

        if (!g_context)
        {
            Logger::GetSingleton().Print(
                "RenderManager: Context == nullptr."
            );

            return false;
        }

        if (!g_swapChain)
        {
            Logger::GetSingleton().Print(
                "RenderManager: SwapChain == nullptr."
            );

            return false;
        }


        /*
         * ============================================================
         * WINDOW
         * ============================================================
         */

        DXGI_SWAP_CHAIN_DESC desc{};

        if (FAILED(
            g_swapChain->GetDesc(&desc)))
        {
            Logger::GetSingleton().Print(
                "RenderManager: SwapChain::GetDesc() FAILED."
            );

            return false;
        }

        g_gameWindow =
            desc.OutputWindow;


        if (!g_gameWindow)
        {
            Logger::GetSingleton().Print(
                "RenderManager: Game HWND == nullptr."
            );

            return false;
        }

        
        /*
         * ============================================================
         * BACKBUFFER
         * ============================================================
         */

        //ID3D11Texture2D* backBuffer = nullptr;

        //const HRESULT bufferResult =
        //    g_swapChain->GetBuffer(
        //        0,
        //        IID_PPV_ARGS(&backBuffer)
        //    );


        //if (FAILED(bufferResult) ||
        //    !backBuffer)
        //{
        //    Logger::GetSingleton().Print(
        //        "RenderManager: GetBuffer() FAILED."
        //    );

        //    return false;
        //}


        /*
         * ============================================================
         * RENDER TARGET
         * ============================================================
         */

        //const HRESULT targetResult =
        //    g_device->CreateRenderTargetView(
        //        backBuffer,
        //        nullptr,
        //        &g_renderTarget
        //    );


        //backBuffer->Release();


        //if (FAILED(targetResult) ||
        //    !g_renderTarget)
        //{
        //    Logger::GetSingleton().Print(
        //        "RenderManager: CreateRenderTargetView() FAILED."
        //    );

        //    return false;
        //}


        /*
         * ============================================================
         * IMGUI
         * ============================================================
         */

        IMGUI_CHECKVERSION();

        ImGui::CreateContext();

        ImGuiIO& io =
            ImGui::GetIO();

        Font::Initialize(io);


        //io.ConfigFlags |=
        //    ImGuiConfigFlags_NavEnableKeyboard;

        //io.ConfigFlags |=
        //    ImGuiConfigFlags_NavEnableGamepad;

        //io.ConfigFlags |=
        //    ImGuiConfigFlags_NoMouseCursorChange;

        //io.KeyRepeatDelay = 0.30f;

        //io.KeyRepeatRate = 0.06f;


        /*
         * ============================================================
         * WIN32 BACKEND
         * ============================================================
         *
         * NÃO instalamos WndProc hook aqui.
         *
         * OAR também não usa ImGui_ImplWin32_WndProcHandler
         * no WndProc para alimentar a interface.
         */

        if (!ImGui_ImplWin32_Init(g_gameWindow))
        {
            Logger::GetSingleton().Print(
                "RenderManager: ImGui_ImplWin32_Init() FAILED."
            );



            ImGui::DestroyContext();

            return false;
        }


        /*
         * ============================================================
         * DX11 BACKEND
         * ============================================================
         */

        if (!ImGui_ImplDX11_Init(
            g_device,
            g_context))
        {
            Logger::GetSingleton().Print(
                "RenderManager: ImGui_ImplDX11_Init() FAILED."
            );

            ImGui_ImplWin32_Shutdown();
      
            ImGui::DestroyContext();

            return false;
        }

        // Experimental e isolado: uma falha no blur não impede a interface
        // principal de inicializar.
        if (!Blur::Initialize(g_device, g_context, g_swapChain))
        {
            Logger::GetSingleton().Print(
                "RenderManager: blur is unavailable; UI will continue without it.");
        }


        g_initialized = true;


        Logger::GetSingleton().Print(
            "RenderManager: INITIALIZED."
        );

        Logger::GetSingleton().Print(
            "RenderManager: HWND = {:X}",
            reinterpret_cast<std::uintptr_t>(
                g_gameWindow
            )
        );

        Logger::GetSingleton().Print(
            "RenderManager: Device = {:X}",
            reinterpret_cast<std::uintptr_t>(
                g_device
            )
        );

        Logger::GetSingleton().Print(
            "RenderManager: Context = {:X}",
            reinterpret_cast<std::uintptr_t>(
                g_context
            )
        );

        Logger::GetSingleton().Print(
            "RenderManager: SwapChain = {:X}",
            reinterpret_cast<std::uintptr_t>(
                g_swapChain
            )
        );


        return true;
    }


        void Render()
        {

            //Menu::UpdatePendingWeaponSwitch();
            //Menu::UpdatePendingWeaponAction();
            Menu::UpdatePendingNormalWeaponEquip();
            Menu::UpdatePendingTwoHandedWeaponEquip();
            Menu::UpdatePendingSpellHandRefresh();

            if (!g_initialized)
            {
                return;
            }

            if (!g_device || !g_context || !g_swapChain) {
                Logger::GetSingleton().Print("Render Error: D3D11 resources null! device: {}, context: {}, swapChain: {}",
                    (void*)g_device, (void*)g_context, (void*)g_swapChain);
                return;
            }

            if (!ImGui::GetCurrentContext())
            {
                static bool logged = false;
                if (!logged)
                {
                    Logger::GetSingleton().Print("RenderManager: ERROR - ImGui context is NULL!");
                    logged = true;
                }
                return;
            }

            // SSE Display Tweaks may render to a smaller surface than the
            // physical window. Capture the current UI target before placing
            // the 3D preview, then use the same space for that projection.
            Microsoft::WRL::ComPtr<ID3D11RenderTargetView> frameTarget;
            g_context->OMGetRenderTargets(1, frameTarget.GetAddressOf(), nullptr);
            ImVec2 frameTargetSize{};
            if (GetRenderTargetSize(frameTarget.Get(), frameTargetSize))
                Resolution::SetRenderTargetSize(frameTargetSize);

            static bool lastShowState = false;
            if (lastShowState != g_showWindow)
            {
                //Logger::GetSingleton().Print("RenderManager: g_showWindow = {}", g_showWindow ? "TRUE" : "FALSE");
                lastShowState = g_showWindow;
            }

            //// ============================================================
            //// SALVA ESTADO ATUAL (pra restaurar depois do preview 3D)
            //// ============================================================

            //ID3D11RenderTargetView* savedRTV = nullptr;
            //ID3D11DepthStencilView* savedDSV = nullptr;
            //g_context->OMGetRenderTargets(1, &savedRTV, &savedDSV);

            //D3D11_VIEWPORT savedViewport{};
            //UINT numViewports = 1;
            //g_context->RSGetViewports(&numViewports, &savedViewport);

            // ============================================================
            // ITEM PREVIEW 3D
            // ============================================================

            //if (ItemPreview::IsVisible())
            //{
            //    DXGI_SWAP_CHAIN_DESC scDesc{};
            //    g_swapChain->GetDesc(&scDesc);

            //    D3D11_VIEWPORT fullVp{};
            //    fullVp.TopLeftX = 0.0f;
            //    fullVp.TopLeftY = 0.0f;
            //    fullVp.Width    = static_cast<float>(scDesc.BufferDesc.Width);
            //    fullVp.Height   = static_cast<float>(scDesc.BufferDesc.Height);
            //    fullVp.MinDepth = 0.0f;
            //    fullVp.MaxDepth = 1.0f;

            //    g_context->RSSetViewports(1, &fullVp);

            //    D3D11_RECT fullScissor{};
            //    fullScissor.left   = 0;
            //    fullScissor.top    = 0;
            //    fullScissor.right  = static_cast<LONG>(scDesc.BufferDesc.Width);
            //    fullScissor.bottom = static_cast<LONG>(scDesc.BufferDesc.Height);
            //    g_context->RSSetScissorRects(1, &fullScissor);

                // ---- Rasterizer sem cull (evita descartar a malha por winding) ----
            //    static ID3D11RasterizerState* s_noCullState = nullptr;
            //    if (!s_noCullState)
            //    {
            //        D3D11_RASTERIZER_DESC rastDesc{};
            //        rastDesc.FillMode = D3D11_FILL_SOLID;
            //        rastDesc.CullMode = D3D11_CULL_NONE;
            //        rastDesc.DepthClipEnable = true;
            //        g_device->CreateRasterizerState(&rastDesc, &s_noCullState);
            //    }
            //    g_context->RSSetState(s_noCullState);

            //    static ID3D11DepthStencilState* s_noDepthState = nullptr;
            //    if (!s_noDepthState)
            //    {
            //        D3D11_DEPTH_STENCIL_DESC dsDesc{};
            //        dsDesc.DepthEnable = FALSE;
            //        dsDesc.StencilEnable = FALSE;
            //        g_device->CreateDepthStencilState(&dsDesc, &s_noDepthState);
            //    }
            //    g_context->OMSetDepthStencilState(s_noDepthState, 0);

                // Blend/depth: NAO mexemos, deixamos o que o Render() do manager configura.

                // ============================================================
                // TESTE TEMPORARIO: retangulo vermelho solido
                // Se isso aparecer na tela, o pipeline esta correto
                // ============================================================

                //D3D11_RECT testRect{ 100, 100, 300, 300 };
                //g_context->RSSetScissorRects(1, &testRect);

                //float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
                //g_context->ClearRenderTargetView(savedRTV, red);

                //g_context->RSSetScissorRects(1, &fullScissor); // volta ao scissor cheio

                // ============================================================
                // FIM DO TESTE
                // ============================================================

                //InventoryHack::OpenHidden();
                //InventoryHack::HideScaleformIfOpen();

                //ItemPreview::Update(); // chama manager->Render() la dentro
            //}
            //else
            //{
                //InventoryHack::CloseHidden();
            //}

            // ============================================================
            // RESTAURA ESTADO PRA NAO AFETAR O IMGUI
            // ============================================================

            //g_context->OMSetRenderTargets(1, &savedRTV, savedDSV);
            //g_context->RSSetViewports(numViewports, &savedViewport);

            //if (savedRTV) savedRTV->Release();
            //if (savedDSV) savedDSV->Release();

            /*
            * ============================================================
            * NEW FRAME
            * ============================================================
            */

            //Menu::ProcessSettingsOpenRequest();

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            Resolution::PrepareFrame();
            Font::Apply();
            ImGui::GetStyle().FontScaleMain =
                std::clamp(Config::g_fontSizeScale, 1.0f, 2.5f);
            ImGui::NewFrame();
            g_drawDataReady = false;
            g_presentDeltaTime = ImGui::GetIO().DeltaTime;
            Resolution::BeginFrame();

            static bool lastShowState2 = false;
            if (lastShowState2 != g_showWindow)
            {
                //Logger::GetSingleton().Print("RenderManager: NewFrame OK, showWindow={}", g_showWindow ? "TRUE" : "FALSE");
                lastShowState2 = g_showWindow;
            }

            //RADIAL

            Menu::UpdateSideRadialScroll();

            //ItemIcon::Update();

            Menu::DrawMenu();

            // O Inventory3DManager é atualizado pelo PostDisplay do menu
            // silencioso Preview. Isso mantém o desenho no ciclo nativo de
            // UI/3D do Skyrim e evita uma composição duplicada com CS.

            /*
            * ============================================================
            * RENDER
            * ============================================================
            */

            ImGui::Render();

            g_drawDataReady = true;
            Resolution::EndFrame();
        }

    void Present()
    {
        if (!g_initialized || !g_drawDataReady || !g_device || !g_context ||
            !g_swapChain || !ImGui::GetCurrentContext())
            return;

        // O Community Shaders prepara e vincula seu buffer de UI dentro de
        // HandleSwapChainPresent, desenha os overlays próprios e só então
        // chama a cadeia de Present onde este hook está instalado. No proxy,
        // esse alvo já é a superfície correta para HUD/Frame Generation.
        if (IsCommunityShadersSwapChainProxy())
        {
            Microsoft::WRL::ComPtr<ID3D11RenderTargetView> uiTarget;
            Microsoft::WRL::ComPtr<ID3D11DepthStencilView> uiDepth;
            Present::MarkWatchdogPhase("CS: OMGetRenderTargets");
            g_context->OMGetRenderTargets(1, uiTarget.GetAddressOf(),
                uiDepth.GetAddressOf());

            if (uiTarget)
            {
                if (!g_loggedCommunityShadersUITarget)
                {
                    Logger::GetSingleton().Print(
                        "Present: composing Wheel into the Community Shaders UI target.");
                    g_loggedCommunityShadersUITarget = true;
                }

                // O blur atua na cena final do proxy. Depois dele, restaure o
                // alvo de UI que o CS deixou vinculado para manter o Wheel
                // fora da textura temporal/interpolada da cena.
                Present::MarkWatchdogPhase("CS: Blur::Render");
                Blur::Render(g_presentDeltaTime);

                auto* target = uiTarget.Get();
                g_context->OMSetRenderTargets(1, &target, uiDepth.Get());
                ImVec2 targetSize = Resolution::GetRealSize();
                GetRenderTargetSize(target, targetSize);
                Resolution::TransformDrawData(ImGui::GetDrawData(), targetSize);
                Present::MarkWatchdogPhase("CS: ImGui render");
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

                // O CS continua a cadeia esperando seu alvo de UI ativo.
                g_context->OMSetRenderTargets(1, &target, uiDepth.Get());
                g_restoreCleanBufferAfterPresent = false;
                return;
            }
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        Present::MarkWatchdogPhase("fallback: acquire back buffer");
        if (!AcquireCurrentBackBuffer(backBuffer))
        {
            return;
        }

        // Do not require Community Shaders for protected UI composition. A
        // proxy such as NVIDIA/Streamline Frame Generation can expose its own
        // active target; draw there whenever it differs from the current
        // swap-chain buffer.
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> proxyUiTarget;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> proxyUiDepth;
        Present::MarkWatchdogPhase("fallback: inspect active render target");
        g_context->OMGetRenderTargets(1, proxyUiTarget.GetAddressOf(),
            proxyUiDepth.GetAddressOf());

        if (proxyUiTarget && !IsSwapChainBackBuffer(proxyUiTarget.Get(),
                backBuffer.Get()))
        {
            if (!g_loggedSeparateUITarget)
            {
                Logger::GetSingleton().Print(
                    "Present: composing Wheel into active off-swap-chain UI target.");
                g_loggedSeparateUITarget = true;
            }

            Present::MarkWatchdogPhase("proxy UI target: Blur::Render");
            Blur::Render(backBuffer.Get(), g_presentDeltaTime);

            auto* target = proxyUiTarget.Get();
            g_context->OMSetRenderTargets(1, &target, proxyUiDepth.Get());
            ImVec2 targetSize = Resolution::GetRealSize();
            GetRenderTargetSize(target, targetSize);
            Resolution::TransformDrawData(ImGui::GetDrawData(), targetSize);
            Present::MarkWatchdogPhase("proxy UI target: ImGui render");
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            g_context->OMSetRenderTargets(1, &target, proxyUiDepth.Get());
            g_restoreCleanBufferAfterPresent = false;
            return;
        }

        D3D11_TEXTURE2D_DESC backDesc{};
        backBuffer->GetDesc(&backDesc);

        bool recreateCleanBuffer = !g_cleanPresentBuffer;
        if (g_cleanPresentBuffer)
        {
            D3D11_TEXTURE2D_DESC cleanDesc{};
            g_cleanPresentBuffer->GetDesc(&cleanDesc);
            recreateCleanBuffer = cleanDesc.Width != backDesc.Width ||
                cleanDesc.Height != backDesc.Height ||
                cleanDesc.Format != backDesc.Format ||
                cleanDesc.SampleDesc.Count != backDesc.SampleDesc.Count ||
                cleanDesc.SampleDesc.Quality != backDesc.SampleDesc.Quality;
        }

        if (recreateCleanBuffer)
        {
            g_cleanPresentBuffer.Reset();
            D3D11_TEXTURE2D_DESC cleanDesc = backDesc;
            cleanDesc.Usage = D3D11_USAGE_DEFAULT;
            cleanDesc.BindFlags = 0;
            cleanDesc.CPUAccessFlags = 0;
            cleanDesc.MiscFlags = 0;
            if (FAILED(g_device->CreateTexture2D(&cleanDesc, nullptr,
                    g_cleanPresentBuffer.GetAddressOf())))
                g_cleanPresentBuffer.Reset();
        }

        // Preserve a imagem final sem o Wheel. O proxy do Community Shaders
        // mantém um recurso D3D11 compartilhado entre apresentações; sem esta
        // cópia, camadas translúcidas do HUD acumulam no frame seguinte.
        if (g_cleanPresentBuffer)
            g_context->CopyResource(g_cleanPresentBuffer.Get(), backBuffer.Get());

        g_restoreCleanBufferAfterPresent =
            g_cleanPresentBuffer &&
            (backDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) != 0;

        // Neste ponto o jogo e o upscaler já terminaram a imagem. O blur e
        // o ImGui são compostos no buffer que será apresentado.
        Present::MarkWatchdogPhase("fallback: Blur::Render");
        Blur::Render(backBuffer.Get(), g_presentDeltaTime);

        if (g_presentBackBuffer.Get() != backBuffer.Get())
        {
            g_presentTarget.Reset();
            g_presentBackBuffer = backBuffer;
            if (FAILED(g_device->CreateRenderTargetView(
                    g_presentBackBuffer.Get(), nullptr,
                    g_presentTarget.GetAddressOf())))
            {
                g_presentBackBuffer.Reset();
                return;
            }
        }

        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> previousTarget;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> previousDepth;
        g_context->OMGetRenderTargets(1, previousTarget.GetAddressOf(),
            previousDepth.GetAddressOf());

        auto* target = g_presentTarget.Get();
        g_context->OMSetRenderTargets(1, &target, nullptr);

        Resolution::TransformDrawData(ImGui::GetDrawData(),
            ImVec2(static_cast<float>(backDesc.Width),
                static_cast<float>(backDesc.Height)));

        Present::MarkWatchdogPhase("fallback: ImGui render");
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        auto* oldTarget = previousTarget.Get();
        g_context->OMSetRenderTargets(1, &oldTarget, previousDepth.Get());

        // Não consuma o ImDrawData aqui. Upscalers e Frame Generation podem
        // apresentar mais de uma vez entre duas atualizações da UI. O mesmo
        // HUD precisa estar disponível para todas essas apresentações; ele só
        // será invalidado quando o próximo ImGui::NewFrame começar.
    }

    void AfterPresent()
    {
        if (!g_initialized || !g_restoreCleanBufferAfterPresent ||
            !g_context || !g_presentBackBuffer || !g_cleanPresentBuffer)
        {
            g_restoreCleanBufferAfterPresent = false;
            return;
        }

        // O Present do proxy já copiou o recurso compartilhado para o buffer
        // real D3D12. Agora podemos retirar nossa camada do recurso reutilizado
        // sem alterar a imagem que acabou de ser apresentada.
        Present::MarkWatchdogPhase("fallback: restoring clean buffer");
        g_context->CopyResource(
            g_presentBackBuffer.Get(), g_cleanPresentBuffer.Get());
        g_restoreCleanBufferAfterPresent = false;
    }

    
    void Shutdown()
    {
        if (!g_initialized)
        {
            return;
        }


        if (ImGui::GetCurrentContext())
        {
            Present::Uninstall();
            Blur::Shutdown();
            ImGui_ImplDX11_Shutdown();

            ImGui_ImplWin32_Shutdown();

            ImGui::DestroyContext();
        }


        g_device = nullptr;

        g_context = nullptr;

        g_swapChain = nullptr;
        g_presentTarget.Reset();
        g_presentBackBuffer.Reset();
        g_cleanPresentBuffer.Reset();
        g_indexedSwapChain.Reset();
        g_backBufferModeDetected = false;
        g_restoreCleanBufferAfterPresent = false;
        g_loggedCommunityShadersUITarget = false;
        g_loggedSeparateUITarget = false;
        g_drawDataReady = false;

        g_gameWindow = nullptr;

        g_initialized = false;

        g_globalAlpha = 0.0f;


        Logger::GetSingleton().Print(
            "RenderManager: shutdown."
        );
    }


    bool IsInitialized()
    {
        return g_initialized;
    }


    ID3D11Device* GetDevice()
    {
        return g_device;
    }


    ID3D11DeviceContext* GetContext()
    {
        return g_context;
    }


    IDXGISwapChain* GetSwapChain()
    {
        return g_swapChain;
    }


    HWND GetWindow()
    {
        return g_gameWindow;
    }


}
