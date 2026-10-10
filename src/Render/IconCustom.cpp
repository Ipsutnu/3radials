#include "IconCustom.h"

#include "Config.h"
#include "Logger.h"
#include "SvgRasterizer.h"

#include <Windows.h>
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
        // O pack legado do p-radials usa arte quadrada para os seus ícones.
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

        void IndexSources()
        {
            g_sources.clear();
            const auto root = std::filesystem::current_path() / "Data" / "SKSE" /
                "Plugins" / "p-radials" / "Icons";
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
            auto* texture = a_source.svg
                ? SvgRasterizer::Load(g_device, a_source.path, kIconTextureSize)
                : LoadPng(a_source.path);
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
