//
// TextureD3D11.cpp
//

#include "../common/TextureStore.hpp"
#include "TextureD3D11.hpp"
#include "DeviceD3D11.hpp"
#include "UploadContextD3D11.hpp"

namespace MrQ2
{

void TextureD3D11::Init(const DeviceD3D11 & device, const TextureType type, const uint32_t width, const uint32_t height,
                        const bool is_scrap, const ColorRGBA32 * mip_init_data[], const Vec2u16 mip_dimensions[], const uint32_t num_mip_levels)
{
    MRQ2_ASSERT((width + height) != 0);
    MRQ2_ASSERT(num_mip_levels >= 1 && num_mip_levels <= TextureImage::kMaxMipLevels);
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first
    (void)is_scrap;

    const uint32_t ms_quality_levels = device.MultisampleQualityLevel(DXGI_FORMAT_R8G8B8A8_UNORM);

    D3D11_TEXTURE2D_DESC tex2d_desc  = {};
    tex2d_desc.Usage                 = D3D11_USAGE_DEFAULT;
    tex2d_desc.BindFlags             = D3D11_BIND_SHADER_RESOURCE;
    tex2d_desc.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex2d_desc.Width                 = width;
    tex2d_desc.Height                = height;
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
    sampler_desc.MaxAnisotropy       = 1;
    sampler_desc.MipLODBias          = 0.0f;
    sampler_desc.MinLOD              = 0.0f;
    sampler_desc.MaxLOD              = D3D11_FLOAT32_MAX;

    D3D11_SUBRESOURCE_DATA res_data[TextureImage::kMaxMipLevels] = {};
    for (uint32_t mip = 0; mip < num_mip_levels; ++mip)
    {
        res_data[mip].pSysMem = mip_init_data[mip];
        res_data[mip].SysMemPitch = mip_dimensions[mip].x * 4; // RGBA-8888
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

D3D11_FILTER TextureD3D11::FilterForTextureType(const TextureType type)
{
    static const CvarWrapper r_tex_filtering = GameInterface::Cvar::Get("r_tex_filtering", "0", CvarWrapper::kFlagArchive);

    static const D3D11_FILTER s_filtering_options[] = {
        D3D11_FILTER_MIN_MAG_MIP_POINT,               // 0
        D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR,        // 1
        D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,  // 2
        D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR,        // 3
        D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT,        // 4
        D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR, // 5
        D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,        // 6
        D3D11_FILTER_MIN_MAG_MIP_LINEAR,              // 7
        D3D11_FILTER_ANISOTROPIC,                     // 8
    };

    if (type < TextureType::kPic) // with mipmaps
    {
        size_t opt = r_tex_filtering.AsInt();

        if (opt >= ArrayLength(s_filtering_options))
        {
            opt = ArrayLength(s_filtering_options) - 1;
        }

        return s_filtering_options[opt];
    }
    else // no mipmaps (UI/Cinematic frames)
    {
        return D3D11_FILTER_MIN_MAG_MIP_POINT;
    }

        /*
    switch (type)
    {
    // TODO: Maybe have a Cvar to select between different filter modes?
    // Should also generate mipmaps for the non-UI textures!!!
    // Bi/Tri-linear filtering for cinematics? In that case, need a new type for them...
    case TextureType::kSkin   : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureType::kSprite : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureType::kWall   : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureType::kSky    : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureType::kPic    : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    default : GameInterface::Errorf("Invalid TextureType enum!");
    } // switch
	*/
}

} // namespace MrQ2
