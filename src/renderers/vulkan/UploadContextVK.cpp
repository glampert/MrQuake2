//
// UploadContextVK.cpp
//

#include "../common/TextureStore.hpp"
#include "UploadContextVK.hpp"
#include "TextureVK.hpp"
#include "DeviceVK.hpp"

namespace MrQ2
{

void UploadContextVK::Init(const DeviceVK & device)
{
    MRQ2_ASSERT(m_device == nullptr);
    m_device = &device;
}

void UploadContextVK::Shutdown()
{
    m_device = nullptr;
}

void UploadContextVK::UploadTexture(const TextureUploadVK & upload_info)
{
    MRQ2_ASSERT(m_device != nullptr);
    MRQ2_ASSERT(upload_info.mipmaps.mip_dimensions[0].x != 0);
    MRQ2_ASSERT(upload_info.mipmaps.mip_init_data[0] != nullptr);
    MRQ2_ASSERT(upload_info.mipmaps.num_mip_levels >= 1 && upload_info.mipmaps.num_mip_levels <= TextureImage::kMaxMipLevels);

    // TODO
}

} // MrQ2
