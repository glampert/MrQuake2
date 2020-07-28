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

    void Init(const DeviceD3D11 & device, const TextureType type, const bool is_scrap,
              const ColorRGBA32 * mip_init_data[], const Vec2u16 mip_dimensions[],
              const uint32_t num_mip_levels, const char * const debug_name);

    // Init from existing texture sharing the resource and sampler/SRV (for the scrap texture)
    void Init(const TextureD3D11 & other);

    void Shutdown();

private:

    const DeviceD3D11 *                 m_device{ nullptr };
    D11ComPtr<ID3D11Texture2D>          m_resource;
    D11ComPtr<ID3D11SamplerState>       m_sampler;
    D11ComPtr<ID3D11ShaderResourceView> m_srv;

    static D3D11_FILTER FilterForTextureType(const TextureType type);
};

} // MrQ2
