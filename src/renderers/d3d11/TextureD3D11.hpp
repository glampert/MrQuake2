//
// TextureD3D11.hpp
//
#pragma once

#include "UtilsD3D11.hpp"

namespace MrQ2
{

class DeviceD3D11;
enum class TextureType : std::uint8_t;

class TextureD3D11 final
{
    friend class UploadContextD3D11;
    friend class GraphicsContextD3D11;

public:

    void Init(const DeviceD3D11 & device, const TextureType type, const uint32_t width, const uint32_t height, const bool is_scrap, const ColorRGBA32 * const init_data);
    void Init(const TextureD3D11 & other); // Init from existing texture sharing the resource and sampler/SRV (for the scrap texture)
    void Shutdown();

private:

    static D3D11_FILTER FilterForTextureType(const TextureType type);

    const DeviceD3D11 *                 m_device{ nullptr };
    D11ComPtr<ID3D11Texture2D>          m_resource;
    D11ComPtr<ID3D11SamplerState>       m_sampler;
    D11ComPtr<ID3D11ShaderResourceView> m_srv;
};

} // MrQ2
