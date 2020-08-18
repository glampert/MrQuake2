//
// TextureD3D12.cpp
//

#include "../common/TextureStore.hpp"
#include "TextureD3D12.hpp"
#include "DeviceD3D12.hpp"
#include "UploadContextD3D12.hpp"

namespace MrQ2
{

void TextureD3D12::Init(const DeviceD3D12 & device, const TextureType type, const bool is_scrap,
                        const ColorRGBA32 * mip_init_data[], const Vec2u16 mip_dimensions[],
                        const uint32_t num_mip_levels, const char * const debug_name)
{
    MRQ2_ASSERT(num_mip_levels >= 1 && num_mip_levels <= TextureImage::kMaxMipLevels);
    MRQ2_ASSERT((mip_dimensions[0].x + mip_dimensions[0].y) != 0);
    MRQ2_ASSERT(mip_init_data[0] != nullptr);
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first
    (void)is_scrap; // unused

    m_srv_descriptor     = device.descriptor_heap->AllocateDescriptor(DescriptorD3D12::kSRV);
    m_sampler_descriptor = device.descriptor_heap->AllocateDescriptor(DescriptorD3D12::kSampler);
    m_shared_descriptors = false;

    // Texture resource:
    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type                  = D3D12_HEAP_TYPE_DEFAULT;
    heap_props.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC res_desc     = {};
    res_desc.Dimension               = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    res_desc.Alignment               = 0;
    res_desc.Width                   = mip_dimensions[0].x;
    res_desc.Height                  = mip_dimensions[0].y;
    res_desc.DepthOrArraySize        = 1;
    res_desc.MipLevels               = static_cast<uint16_t>(num_mip_levels);
    res_desc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    res_desc.SampleDesc.Count        = 1;
    res_desc.SampleDesc.Quality      = 0;
    res_desc.Layout                  = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    res_desc.Flags                   = D3D12_RESOURCE_FLAG_NONE;

    D12CHECK(device.device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_resource)));

    size_t num_converted = 0;
    mbstowcs_s(&num_converted, m_debug_name, debug_name, ArrayLength(m_debug_name));
    MRQ2_ASSERT(num_converted != 0);
    D12SetDebugName(m_resource, m_debug_name);

    // Upload texture pixels:
    TextureUploadD3D12 upload_info{};

    upload_info.texture  = this;
    upload_info.is_scrap = false; // Transition to PIXEL_SHADER_RESOURCE
    upload_info.mipmaps.num_mip_levels = num_mip_levels;
    upload_info.mipmaps.mip_init_data  = mip_init_data;
    upload_info.mipmaps.mip_dimensions = mip_dimensions;
    device.UploadContext().UploadTextureImmediate(upload_info);

    // Create texture view:
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format                          = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels             = num_mip_levels;
    srv_desc.Texture2D.MostDetailedMip       = 0;
    srv_desc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    device.device->CreateShaderResourceView(m_resource.Get(), &srv_desc, m_srv_descriptor.cpu_handle);

    static auto r_max_anisotropy = GameInterface::Cvar::Get("r_max_anisotropy", "1", CvarWrapper::kFlagArchive);
    int max_anisotropy = r_max_anisotropy.AsInt();

    if (max_anisotropy > 16)
        max_anisotropy = 16;

    if (max_anisotropy < 1)
        max_anisotropy = 1;

    // Create a sampler:
    D3D12_SAMPLER_DESC sampler_desc  = {};
    sampler_desc.Filter              = FilterForTextureType(type);
    sampler_desc.AddressU            = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler_desc.AddressV            = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler_desc.AddressW            = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler_desc.ComparisonFunc      = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler_desc.MaxAnisotropy       = max_anisotropy;
    sampler_desc.MipLODBias          = 0.0f;
    sampler_desc.MinLOD              = 0.0f;
    sampler_desc.MaxLOD              = D3D12_FLOAT32_MAX;
    device.device->CreateSampler(&sampler_desc, m_sampler_descriptor.cpu_handle);

    m_device = &device;
}

void TextureD3D12::Init(const TextureD3D12 & other)
{
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first
    MRQ2_ASSERT(other.m_resource != nullptr);

    // Share the other texture resource(s)
    m_resource           = other.m_resource;
    m_srv_descriptor     = other.m_srv_descriptor;
    m_sampler_descriptor = other.m_sampler_descriptor;
    m_device             = other.m_device;
    m_shared_descriptors = true;

    wcscpy_s(m_debug_name, other.m_debug_name);
}

void TextureD3D12::Shutdown()
{
    if (m_device != nullptr)
    {
        if (!m_shared_descriptors)
        {
            m_device->descriptor_heap->FreeDescriptor(m_srv_descriptor);
            m_device->descriptor_heap->FreeDescriptor(m_sampler_descriptor);
        }

        m_resource           = nullptr;
        m_srv_descriptor     = {};
        m_sampler_descriptor = {};
        m_device             = nullptr;
        m_shared_descriptors = false;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Texture filtering selection:
///////////////////////////////////////////////////////////////////////////////

D3D12_FILTER TextureD3D12::FilterForTextureType(const TextureType type)
{
    static const D3D12_FILTER d3d_tex_filtering_options[] = {
        D3D12_FILTER_MIN_MAG_MIP_POINT,        // 0 nearest
        D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, // 1 bilinear
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,       // 2 trilinear
        D3D12_FILTER_ANISOTROPIC,              // 3 anisotropic
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
        return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    }
    else // no mipmaps (UI/Cinematic frames), use point/nearest sampling
    {
        return D3D12_FILTER_MIN_MAG_MIP_POINT;
    }
}

} // namespace MrQ2
