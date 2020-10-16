//
// UploadContextVK.cpp
//

#include "../common/TextureStore.hpp"
#include "UploadContextVK.hpp"
#include "TextureVK.hpp"
#include "DeviceVK.hpp"
#include "BufferVK.hpp"

namespace MrQ2
{

void UploadContextVK::Init(const DeviceVK & device)
{
    MRQ2_ASSERT(m_device_vk == nullptr);

    m_upload_buffer.Init(device);
    m_device_vk = &device;
}

void UploadContextVK::Shutdown()
{
    m_upload_buffer.Shutdown();
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

void UploadContextVK::UploadStagingBuffer(const BufferVK & buffer)
{
    MRQ2_ASSERT(m_device_vk != nullptr);

    m_upload_buffer.BeginRecording();

    VulkanCopyBuffer(m_upload_buffer, buffer.m_staging_buffer_handle, buffer.m_buffer_handle, buffer.m_size_in_bytes);

    // TODO: Need to profile this and see if it is a bottleneck.
    m_upload_buffer.EndRecording();
    m_upload_buffer.Submit();
    m_upload_buffer.WaitComplete();
    m_upload_buffer.Reset();
}

} // MrQ2
