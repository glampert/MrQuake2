//
// TextureD3D12.cpp
//

#include "TextureD3D12.hpp"
#include "DeviceD3D12.hpp"
#include "UploadContextD3D12.hpp"

namespace MrQ2
{

void TextureD3D12::Init(const DeviceD3D12 & device, const uint32_t width, const uint32_t height, const bool is_scrap, const ColorRGBA32* const init_data)
{
    MRQ2_ASSERT((width + height) != 0);
    MRQ2_ASSERT(m_resource == nullptr); // Shutdown first

    m_srv_descriptor = device.descriptor_heap->AllocateDescriptor(DescriptorD3D12::kSRV);
    m_is_descriptor_shared = false;

    // Texture resource:
    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type                  = D3D12_HEAP_TYPE_DEFAULT;
    heap_props.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC res_desc     = {};
    res_desc.Dimension               = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    res_desc.Alignment               = 0;
    res_desc.Width                   = width;
    res_desc.Height                  = height;
    res_desc.DepthOrArraySize        = 1;
    res_desc.MipLevels               = 1;
    res_desc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    res_desc.SampleDesc.Count        = 1;
    res_desc.SampleDesc.Quality      = 0;
    res_desc.Layout                  = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    res_desc.Flags                   = D3D12_RESOURCE_FLAG_NONE;

    D12CHECK(device.device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_resource)));
    D12SetDebugName(m_resource, L"Texture2D");

    // Upload texture pixels:
    if (init_data != nullptr)
    {
        TextureUploadD3D12 upload_info{};
        upload_info.texture  = this;
        upload_info.pixels   = init_data;
        upload_info.width    = width;
        upload_info.height   = height;
        upload_info.is_scrap = is_scrap;
        device.UploadContext().UploadTextureImmediate(upload_info);
    }

    // Create texture view:
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format                          = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels             = res_desc.MipLevels;
    srv_desc.Texture2D.MostDetailedMip       = 0;
    srv_desc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    device.device->CreateShaderResourceView(m_resource.Get(), &srv_desc, m_srv_descriptor.cpu_handle);

    m_device = &device;
}

void TextureD3D12::Init(const TextureD3D12 & other)
{
    MRQ2_ASSERT(m_resource == nullptr); // Shutdown first
    MRQ2_ASSERT(other.m_resource != nullptr);

    // Share the other texture resource(s)
    m_resource             = other.m_resource;
    m_srv_descriptor       = other.m_srv_descriptor;
    m_is_descriptor_shared = true;
}

void TextureD3D12::Shutdown()
{
    if (m_device != nullptr)
    {
        if (!m_is_descriptor_shared)
        {
            m_device->descriptor_heap->FreeDescriptor(m_srv_descriptor);
        }

        m_srv_descriptor       = {};
        m_resource             = nullptr;
        m_device               = nullptr;
        m_is_descriptor_shared = false;
    }
}

} // namespace MrQ2
