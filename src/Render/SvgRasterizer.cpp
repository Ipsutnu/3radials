#include "SvgRasterizer.h"

#include "Logger.h"

#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace SvgRasterizer
{
    namespace
    {
        void LogFailure(const std::filesystem::path& a_path, std::string_view a_reason)
        {
            Logger::GetSingleton().Print(
                "SVG: could not rasterize '{}' ({})", a_path.string(), a_reason);
        }

        [[nodiscard]] ID3D11ShaderResourceView* CreateTexture(
            ID3D11Device* a_device,
            const std::vector<std::uint8_t>& a_pixels,
            UINT a_size)
        {
            D3D11_TEXTURE2D_DESC description{};
            description.Width = a_size;
            description.Height = a_size;
            description.MipLevels = 1;
            description.ArraySize = 1;
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.Usage = D3D11_USAGE_DEFAULT;
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA data{};
            data.pSysMem = a_pixels.data();
            data.SysMemPitch = a_size * 4;

            ID3D11Texture2D* texture = nullptr;
            if (FAILED(a_device->CreateTexture2D(&description, &data, &texture)) || !texture)
                return nullptr;

            ID3D11ShaderResourceView* view = nullptr;
            const HRESULT result = a_device->CreateShaderResourceView(texture, nullptr, &view);
            texture->Release();
            return SUCCEEDED(result) ? view : nullptr;
        }
    }

    ID3D11ShaderResourceView* Load(
        ID3D11Device* a_device,
        const std::filesystem::path& a_path,
        UINT a_size)
    {
        if (!a_device || !a_size || !std::filesystem::is_regular_file(a_path))
            return nullptr;

        std::ifstream file(a_path, std::ios::binary);
        std::vector<char> source((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        if (!file || source.empty())
        {
            LogFailure(a_path, "file is empty or unreadable");
            return nullptr;
        }

        // nsvgParse manipula o buffer de entrada durante o parsing.
        source.push_back('\0');
        NSVGimage* image = nsvgParse(source.data(), "px", 96.0f);
        if (!image || image->width <= 0.0f || image->height <= 0.0f)
        {
            if (image)
                nsvgDelete(image);
            LogFailure(a_path, "NanoSVG parser rejected the document");
            return nullptr;
        }

        NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
        if (!rasterizer)
        {
            nsvgDelete(image);
            LogFailure(a_path, "could not create NanoSVG rasterizer");
            return nullptr;
        }

        const float scale = std::min(
            static_cast<float>(a_size) / image->width,
            static_cast<float>(a_size) / image->height);
        const float drawWidth = image->width * scale;
        const float drawHeight = image->height * scale;
        const float offsetX = (static_cast<float>(a_size) - drawWidth) * 0.5f;
        const float offsetY = (static_cast<float>(a_size) - drawHeight) * 0.5f;

        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(a_size) * a_size * 4, 0);
        nsvgRasterize(rasterizer, image, offsetX, offsetY, scale,
            pixels.data(), static_cast<int>(a_size), static_cast<int>(a_size),
            static_cast<int>(a_size * 4));

        nsvgDeleteRasterizer(rasterizer);
        nsvgDelete(image);
        return CreateTexture(a_device, pixels, a_size);
    }
}
