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
    MRQ2_ASSERT(m_device_vk == nullptr);

    m_upload_cmd_buffer.Init(device);
    m_device_vk = &device;
}

void UploadContextVK::Shutdown()
{
    m_upload_cmd_buffer.Shutdown();
    m_device_vk = nullptr;
}

void UploadContextVK::UploadTexture(const TextureUploadVK & upload_info)
{
    MRQ2_ASSERT(m_device_vk != nullptr);
    MRQ2_ASSERT(upload_info.mipmaps.mip_dimensions[0].x != 0);
    MRQ2_ASSERT(upload_info.mipmaps.mip_init_data[0] != nullptr);
    MRQ2_ASSERT(upload_info.mipmaps.num_mip_levels >= 1 && upload_info.mipmaps.num_mip_levels <= TextureImage::kMaxMipLevels);

    // TODO
}

} // MrQ2
