#include "IconCustom.h"

#include "Config.h"
#include "Logger.h"

#include <Windows.h>
// O SDK marca as interfaces SVG modernas como API de app. Skyrim continua
// sendo um processo desktop normal; liberamos só as declarações deste header.
#pragma push_macro("WINAPI_FAMILY")
#pragma push_macro("WINAPI_PARTITION_APP")
#pragma push_macro("NTDDI_VERSION")
#undef WINAPI_FAMILY
#undef WINAPI_PARTITION_APP
#undef NTDDI_VERSION
#define WINAPI_FAMILY WINAPI_FAMILY_APP
#define WINAPI_PARTITION_APP 1
#define NTDDI_VERSION NTDDI_WIN10_RS2
#include <d2d1_3.h>
#include <d2d1helper.h>
#pragma pop_macro("NTDDI_VERSION")
#pragma pop_macro("WINAPI_PARTITION_APP")
#pragma pop_macro("WINAPI_FAMILY")
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace IconCustom
{
    namespace
    {
        // O pack legado do 3radials usa arte quadrada para os seus ícones.
        // Normalizar os custom assets neste canvas impede que 512/1024 px
        // alterem o tamanho visual relativo a esse fallback.
        constexpr UINT kIconTextureSize = 256;
        constexpr std::wstring_view kKeywordPrefix = L"KWD_";

        struct Source
        {
            std::filesystem::path path;
            bool svg = false;
        };

        ID3D11Device* g_device = nullptr;
        IWICImagingFactory* g_wicFactory = nullptr;
        Microsoft::WRL::ComPtr<ID2D1Factory1> g_d2dFactory;
        Microsoft::WRL::ComPtr<ID2D1Device> g_d2dDevice;
        Microsoft::WRL::ComPtr<ID2D1DeviceContext5> g_d2dContext;
        bool g_initialized = false;

        // Keyword em minúsculas -> arquivo KWD_<keyword>.svg/png.
        // Quando os dois existem, SVG sempre tem prioridade.
        std::unordered_map<std::string, Source> g_sources;
        std::unordered_map<std::string, ID3D11ShaderResourceView*> g_textures;
        std::unordered_map<RE::FormID, ID3D11ShaderResourceView*> g_formCache;
        std::unordered_set<RE::FormID> g_unresolvedForms;

        [[nodiscard]] std::string ToLower(std::string a_value)
        {
            std::ranges::transform(a_value, a_value.begin(), [](unsigned char a_character) {
                return static_cast<char>(std::tolower(a_character));
            });
            return a_value;
        }

        [[nodiscard]] std::string ToUtf8(const std::wstring& a_value)
        {
            if (a_value.empty())
                return {};
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, a_value.data(),
                static_cast<int>(a_value.size()), nullptr, 0, nullptr, nullptr);
            if (bytes <= 0)
                return {};
            std::string result(static_cast<std::size_t>(bytes), '\0');
            WideCharToMultiByte(CP_UTF8, 0, a_value.data(), static_cast<int>(a_value.size()),
                result.data(), bytes, nullptr, nullptr);
            return result;
        }

        void ReleaseTextures()
        {
            for (auto& [_, texture] : g_textures)
                if (texture)
                    texture->Release();
            g_textures.clear();
            g_formCache.clear();
            g_unresolvedForms.clear();
        }

        [[nodiscard]] bool InitializeD2D()
        {
            if (!g_device)
                return false;
            if (g_d2dContext)
                return true;

            Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
            if (FAILED(g_device->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()))))
                return false;

            D2D1_FACTORY_OPTIONS options{};
            Microsoft::WRL::ComPtr<ID2D1DeviceContext> baseContext;
            if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                    __uuidof(ID2D1Factory1), &options,
                    reinterpret_cast<void**>(g_d2dFactory.GetAddressOf()))) ||
                FAILED(g_d2dFactory->CreateDevice(dxgiDevice.Get(), g_d2dDevice.GetAddressOf())) ||
                FAILED(g_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                    baseContext.GetAddressOf())) ||
                FAILED(baseContext.As(&g_d2dContext)))
            {
                g_d2dContext.Reset();
                g_d2dDevice.Reset();
                g_d2dFactory.Reset();
                return false;
            }
            return true;
        }

        ID3D11ShaderResourceView* CreateTexture(const void* a_pixels, UINT a_pitch)
        {
            if (!g_device || !a_pixels || !a_pitch)
                return nullptr;

            D3D11_TEXTURE2D_DESC description{};
            description.Width = kIconTextureSize;
            description.Height = kIconTextureSize;
            description.MipLevels = 1;
            description.ArraySize = 1;
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.Usage = D3D11_USAGE_DEFAULT;
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA data{};
            data.pSysMem = a_pixels;
            data.SysMemPitch = a_pitch;

            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
            if (FAILED(g_device->CreateTexture2D(&description, &data, texture.GetAddressOf())) ||
                FAILED(g_device->CreateShaderResourceView(texture.Get(), nullptr, view.GetAddressOf())))
                return nullptr;
            return view.Detach();
        }

        ID3D11ShaderResourceView* LoadPng(const std::filesystem::path& a_path)
        {
            if (!g_wicFactory || !std::filesystem::is_regular_file(a_path))
                return nullptr;

            Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
            if (FAILED(g_wicFactory->CreateDecoderFromFilename(a_path.c_str(), nullptr,
                    GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf())))
                return nullptr;

            Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
            if (FAILED(decoder->GetFrame(0, frame.GetAddressOf())))
                return nullptr;

            Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
            if (FAILED(g_wicFactory->CreateBitmapScaler(scaler.GetAddressOf())) ||
                FAILED(scaler->Initialize(frame.Get(), kIconTextureSize, kIconTextureSize,
                    WICBitmapInterpolationModeFant)))
                return nullptr;

            Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
            if (FAILED(g_wicFactory->CreateFormatConverter(converter.GetAddressOf())) ||
                FAILED(converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppRGBA,
                    WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
                return nullptr;

            std::vector<std::uint8_t> pixels(kIconTextureSize * kIconTextureSize * 4);
            if (FAILED(converter->CopyPixels(nullptr, kIconTextureSize * 4,
                    static_cast<UINT>(pixels.size()), pixels.data())))
                return nullptr;
            return CreateTexture(pixels.data(), kIconTextureSize * 4);
        }

        ID3D11ShaderResourceView* LoadSvg(const std::filesystem::path& a_path)
        {
            if (!InitializeD2D() || !std::filesystem::is_regular_file(a_path))
                return nullptr;

            std::ifstream file(a_path, std::ios::binary);
            const std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());
            if (!file || bytes.empty())
                return nullptr;

            HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
            if (!memory)
                return nullptr;
            void* destination = GlobalLock(memory);
            if (!destination)
            {
                GlobalFree(memory);
                return nullptr;
            }
            std::memcpy(destination, bytes.data(), bytes.size());
            GlobalUnlock(memory);

            Microsoft::WRL::ComPtr<IStream> stream;
            if (FAILED(CreateStreamOnHGlobal(memory, TRUE, stream.GetAddressOf())))
            {
                GlobalFree(memory);
                return nullptr;
            }

            Microsoft::WRL::ComPtr<ID2D1SvgDocument> document;
            if (FAILED(g_d2dContext->CreateSvgDocument(stream.Get(),
                    D2D1::SizeF(static_cast<float>(kIconTextureSize),
                        static_cast<float>(kIconTextureSize)), document.GetAddressOf())))
                return nullptr;

            D3D11_TEXTURE2D_DESC description{};
            description.Width = kIconTextureSize;
            description.Height = kIconTextureSize;
            description.MipLevels = 1;
            description.ArraySize = 1;
            description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.Usage = D3D11_USAGE_DEFAULT;
            description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            Microsoft::WRL::ComPtr<IDXGISurface> surface;
            Microsoft::WRL::ComPtr<ID2D1Bitmap1> target;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
            if (FAILED(g_device->CreateTexture2D(&description, nullptr, texture.GetAddressOf())) ||
                FAILED(texture.As(&surface)))
                return nullptr;

            const auto properties = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                    D2D1_ALPHA_MODE_PREMULTIPLIED));
            if (FAILED(g_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(),
                    &properties, target.GetAddressOf())))
                return nullptr;

            // SVG é rasterizado apenas em memória e diretamente na GPU.
            // Não há arquivo PNG intermediário nem readback para CPU.
            g_d2dContext->SetTarget(target.Get());
            g_d2dContext->BeginDraw();
            g_d2dContext->Clear(D2D1::ColorF(0, 0.0f));
            g_d2dContext->DrawSvgDocument(document.Get());
            const HRESULT result = g_d2dContext->EndDraw();
            g_d2dContext->SetTarget(nullptr);
            if (FAILED(result) || FAILED(g_device->CreateShaderResourceView(texture.Get(),
                    nullptr, view.GetAddressOf())))
                return nullptr;
            return view.Detach();
        }

        void IndexSources()
        {
            g_sources.clear();
            const auto root = std::filesystem::current_path() / "Data" / "SKSE" /
                "Plugins" / "3radials" / "Icons";
            std::error_code error;
            if (!std::filesystem::is_directory(root, error))
                return;

            for (const auto& entry : std::filesystem::directory_iterator(root, error))
            {
                if (error || !entry.is_regular_file())
                    continue;
                const auto extension = ToLower(entry.path().extension().string());
                if (extension != ".png" && extension != ".svg")
                    continue;

                const std::wstring stem = entry.path().stem().wstring();
                if (stem.size() <= kKeywordPrefix.size() ||
                    _wcsnicmp(stem.c_str(), kKeywordPrefix.data(), kKeywordPrefix.size()) != 0)
                    continue;

                const std::string keyword = ToLower(ToUtf8(stem.substr(kKeywordPrefix.size())));
                if (keyword.empty())
                    continue;
                const bool svg = extension == ".svg";
                const auto existing = g_sources.find(keyword);
                if (existing == g_sources.end() || (svg && !existing->second.svg))
                    g_sources.insert_or_assign(keyword, Source{ entry.path(), svg });
            }
            Logger::GetSingleton().Print("Custom Icons: indexed {} KWD SVG/PNG icons.",
                g_sources.size());
        }

        ID3D11ShaderResourceView* LoadSource(const Source& a_source)
        {
            const std::string key = a_source.path.lexically_normal().string();
            if (const auto found = g_textures.find(key); found != g_textures.end())
                return found->second;
            auto* texture = a_source.svg ? LoadSvg(a_source.path) : LoadPng(a_source.path);
            if (texture)
                g_textures.emplace(key, texture);
            return texture;
        }
    }

    bool Initialize(ID3D11Device* a_device)
    {
        if (!a_device)
            return false;
        g_device = a_device;
        if (!g_wicFactory && FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_wicFactory))))
            return false;
        g_initialized = true;
        Reload();
        return true;
    }

    void Shutdown()
    {
        ReleaseTextures();
        g_sources.clear();
        g_d2dContext.Reset();
        g_d2dDevice.Reset();
        g_d2dFactory.Reset();
        if (g_wicFactory) { g_wicFactory->Release(); g_wicFactory = nullptr; }
        g_device = nullptr;
        g_initialized = false;
    }

    bool Reload()
    {
        if (!g_initialized)
            return false;
        ReleaseTextures();
        IndexSources();
        if (g_sources.empty())
        {
            Config::g_customIcons = false;
            return false;
        }
        if (!Config::HasCustomIconsPreference())
        {
            Config::SetCustomIconsPreference(true);
            Config::SaveConfig();
        }
        return true;
    }

    bool HasValidConfiguration()
    {
        return !g_sources.empty();
    }

    ID3D11ShaderResourceView* Get(RE::TESForm* a_form)
    {
        if (!g_initialized || !a_form || g_sources.empty())
            return nullptr;
        const RE::FormID formID = a_form->GetFormID();
        if (const auto cached = g_formCache.find(formID); cached != g_formCache.end())
            return cached->second;
        if (g_unresolvedForms.contains(formID))
            return nullptr;

        // O índice ordenado torna a prioridade estável quando um item possui
        // mais de um keyword com arquivo KWD_.
        std::vector<std::string> keywords;
        keywords.reserve(g_sources.size());
        for (const auto& [keyword, _] : g_sources)
            keywords.push_back(keyword);
        std::ranges::sort(keywords);
        for (const auto& keyword : keywords)
        {
            if (!a_form->HasKeywordByEditorID(keyword))
                continue;
            if (auto* texture = LoadSource(g_sources.at(keyword)))
            {
                g_formCache.emplace(formID, texture);
                return texture;
            }
            break;
        }
        g_unresolvedForms.emplace(formID);
        return nullptr;
    }
}
