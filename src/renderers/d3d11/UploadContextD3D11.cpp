//
// UploadContextD3D11.cpp
//

#include "../common/TextureStore.hpp"
#include "UploadContextD3D11.hpp"
#include "TextureD3D11.hpp"
#include "DeviceD3D11.hpp"

namespace MrQ2
{

void UploadContextD3D11::Init(const DeviceD3D11 & device)
{
    MRQ2_ASSERT(m_device == nullptr);
    m_device = &device;
}

void UploadContextD3D11::Shutdown()
{
    m_device = nullptr;
}

void UploadContextD3D11::UploadTextureImmediate(const TextureUploadD3D11 & upload_info)
{
    MRQ2_ASSERT(m_device != nullptr);
    MRQ2_ASSERT(upload_info.texture->m_resource != nullptr);
    MRQ2_ASSERT(upload_info.mipmaps.mip_dimensions[0].x != 0);
    MRQ2_ASSERT(upload_info.mipmaps.mip_init_data[0] != nullptr);
    MRQ2_ASSERT(upload_info.mipmaps.num_mip_levels >= 1 && upload_info.mipmaps.num_mip_levels <= TextureImage::kMaxMipLevels);

    auto * texture_resource = upload_info.texture->m_resource.Get();

    for (uint32_t mip = 0; mip < upload_info.mipmaps.num_mip_levels; ++mip)
    {
        const uint32_t row_pitch = upload_info.mipmaps.mip_dimensions[mip].x * TextureImage::kBytesPerPixel;
        const void * const data  = upload_info.mipmaps.mip_init_data[mip];

        m_device->context->UpdateSubresource(texture_resource, mip, nullptr, data, row_pitch, 0);
    }
}

} // MrQ2
