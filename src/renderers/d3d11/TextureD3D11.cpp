//
// TextureD3D11.cpp
//

#include "../common/TextureStore.hpp"
#include "TextureD3D11.hpp"
#include "DeviceD3D11.hpp"

namespace MrQ2
{

void TextureD3D11::Init(const DeviceD3D11 & device, const TextureType type, const bool is_scrap,
                        const ColorRGBA32 * mip_init_data[], const Vec2u16 mip_dimensions[],
                        const uint32_t num_mip_levels, const char * const debug_name)
{
    MRQ2_ASSERT(num_mip_levels >= 1 && num_mip_levels <= TextureImage::kMaxMipLevels);
    MRQ2_ASSERT((mip_dimensions[0].x + mip_dimensions[0].y) != 0);
    MRQ2_ASSERT(mip_init_data[0] != nullptr);
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first
    (void)debug_name; // unused

    static auto r_max_anisotropy = GameInterface::Cvar::Get("r_max_anisotropy", "1", CvarWrapper::kFlagArchive);
    int max_anisotropy = r_max_anisotropy.AsInt();

    if (max_anisotropy > 16)
        max_anisotropy = 16;

    if (max_anisotropy < 1)
        max_anisotropy = 1;

    const uint32_t ms_quality_levels = device.MultisampleQualityLevel(DXGI_FORMAT_R8G8B8A8_UNORM);

    D3D11_TEXTURE2D_DESC tex2d_desc  = {};
    tex2d_desc.Usage                 = (is_scrap ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE);
    tex2d_desc.BindFlags             = D3D11_BIND_SHADER_RESOURCE;
    tex2d_desc.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex2d_desc.Width                 = mip_dimensions[0].x;
    tex2d_desc.Height                = mip_dimensions[0].y;
    tex2d_desc.MipLevels             = num_mip_levels;
    tex2d_desc.ArraySize             = 1;
    tex2d_desc.SampleDesc.Count      = 1;
    tex2d_desc.SampleDesc.Quality    = ms_quality_levels - 1;

    D3D11_SAMPLER_DESC sampler_desc  = {};
    sampler_desc.Filter              = FilterForTextureType(type);
    sampler_desc.AddressU            = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.AddressV            = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.AddressW            = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.ComparisonFunc      = D3D11_COMPARISON_ALWAYS;
    sampler_desc.MaxAnisotropy       = max_anisotropy;
    sampler_desc.MipLODBias          = 0.0f;
    sampler_desc.MinLOD              = 0.0f;
    sampler_desc.MaxLOD              = D3D11_FLOAT32_MAX;

    D3D11_SUBRESOURCE_DATA res_data[TextureImage::kMaxMipLevels] = {};
    for (uint32_t mip = 0; mip < num_mip_levels; ++mip)
    {
        res_data[mip].pSysMem = mip_init_data[mip];
        res_data[mip].SysMemPitch = mip_dimensions[mip].x * TextureImage::kBytesPerPixel;
    }

    auto * device11 = device.device;
    D11CHECK(device11->CreateTexture2D(&tex2d_desc, res_data, m_resource.GetAddressOf()));
    D11CHECK(device11->CreateShaderResourceView(m_resource.Get(), nullptr, m_srv.GetAddressOf()));
    D11CHECK(device11->CreateSamplerState(&sampler_desc, m_sampler.GetAddressOf()));

    m_device = &device;
}

void TextureD3D11::Init(const TextureD3D11 & other)
{
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first
    MRQ2_ASSERT(other.m_resource != nullptr);

    // Share the scrap texture resource(s)
    m_device   = other.m_device;
    m_resource = other.m_resource;
    m_sampler  = other.m_sampler;
    m_srv      = other.m_srv;
}

void TextureD3D11::Shutdown()
{
    m_device   = nullptr;
    m_resource = nullptr;
    m_sampler  = nullptr;
    m_srv      = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// Texture filtering selection:
///////////////////////////////////////////////////////////////////////////////

D3D11_FILTER TextureD3D11::FilterForTextureType(const TextureType type)
{
    static const D3D11_FILTER d3d_tex_filtering_options[] = {
        D3D11_FILTER_MIN_MAG_MIP_POINT,        // 0 nearest
        D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, // 1 bilinear
        D3D11_FILTER_MIN_MAG_MIP_LINEAR,       // 2 trilinear
        D3D11_FILTER_ANISOTROPIC,              // 3 anisotropic
    };

    if (type < TextureType::kPic) // with mipmaps
    {
        static auto r_tex_filtering = GameInterface::Cvar::Get("r_tex_filtering", "0", CvarWrapper::kFlagArchive);

        uint32_t opt = r_tex_filtering.AsInt();
        if (opt >= ArrayLength(d3d_tex_filtering_options))
        {
            opt = ArrayLength(d3d_tex_filtering_options) - 1;
        }

        return d3d_tex_filtering_options[opt];
    }
    else if (type == TextureType::kLightmap)
    {
        return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    }
    else // no mipmaps (UI/Cinematic frames), use point/nearest sampling
    {
        return D3D11_FILTER_MIN_MAG_MIP_POINT;
    }
}

} // namespace MrQ2
