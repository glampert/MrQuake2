//
// UploadContextVK.cpp
//

#include "../common/TextureStore.hpp"
#include "../common/OptickProfiler.hpp"
#include "UploadContextVK.hpp"
#include "TextureVK.hpp"
#include "DeviceVK.hpp"

namespace MrQ2
{

void UploadContextVK::Init(const DeviceVK & device)
{
    MRQ2_ASSERT(m_device_vk == nullptr);

    m_upload_cmd_buffer.Init(device);
    m_upload_cmd_buffer.BeginRecording();

    m_device_vk = &device;
}

void UploadContextVK::Shutdown()
{
    for (uint32_t e = 0; e < m_num_creates; ++e)
    {
        m_creates[e].upload_buffer.Shutdown();
    }
    m_num_creates = 0;

    m_upload_cmd_buffer.Shutdown();
    m_device_vk = nullptr;
}

void UploadContextVK::UploadTexture(const TextureUploadVK & upload_info)
{
    OPTICK_EVENT();

    MRQ2_ASSERT(m_device_vk != nullptr);
    MRQ2_ASSERT(upload_info.mipmaps.mip_dimensions[0].x != 0);
    MRQ2_ASSERT(upload_info.mipmaps.mip_init_data[0] != nullptr);
    MRQ2_ASSERT(upload_info.mipmaps.num_mip_levels >= 1 && upload_info.mipmaps.num_mip_levels <= TextureImage::kMaxMipLevels);

    // TODO
}

void UploadContextVK::CreateTexture(const TextureUploadVK & upload_info)
{
    OPTICK_EVENT();

    MRQ2_ASSERT(m_device_vk != nullptr);
    MRQ2_ASSERT(upload_info.texture != nullptr);
    MRQ2_ASSERT(upload_info.texture->Handle() != nullptr);
    // NOTE: Not required to happen between Begin/EndFrame

    if (m_num_creates == ArrayLength(m_creates))
    {
        // Flush any queued texture creates to make room
        FlushTextureCreates();
    }

    MRQ2_ASSERT(m_num_creates < ArrayLength(m_creates));
    CreateEntry & entry = m_creates[m_num_creates++];
    CreateUploadBuffer(upload_info, &entry.upload_buffer);
}

void UploadContextVK::FlushTextureCreates()
{
    if (m_num_creates == 0)
    {
        return;
    }

    OPTICK_EVENT();

    m_upload_cmd_buffer.EndRecording();
    m_upload_cmd_buffer.Submit();
    m_upload_cmd_buffer.WaitComplete();
    m_upload_cmd_buffer.Reset();
    m_upload_cmd_buffer.BeginRecording();

    // We have synced the queue, all pending upload_buffers can now be freed
    for (uint32_t e = 0; e < m_num_creates; ++e)
    {
        m_creates[e].upload_buffer.Shutdown();
    }
    m_num_creates = 0;
}

void UploadContextVK::UpdateCompletedUploads()
{
    OPTICK_EVENT();

    // TODO
}

void UploadContextVK::CreateUploadBuffer(const TextureUploadVK & upload_info, StagingBuffer * out_upload_buff)
{
    const auto &   mipmaps  = upload_info.mipmaps;
    const uint32_t num_mips = mipmaps.num_mip_levels;

    uint32_t buffer_size_in_bytes = 0;
    for (uint32_t mip = 0; mip < num_mips; ++mip)
    {
        buffer_size_in_bytes += mipmaps.mip_dimensions[mip].x * mipmaps.mip_dimensions[mip].y * TextureImage::kBytesPerPixel;
    }

    // Create a host-visible staging buffer that will contain the raw image data:
    out_upload_buff->Init(*m_device_vk, buffer_size_in_bytes);

    // Copy texture data into the staging buffer:
    void * memory = out_upload_buff->Map();
    {
        auto * dest_pixels = static_cast<std::uint8_t *>(memory);
        for (uint32_t mip = 0; mip < num_mips; ++mip)
        {
            const auto * mip_pixels = mipmaps.mip_init_data[mip];
            const size_t mip_size   = mipmaps.mip_dimensions[mip].x * mipmaps.mip_dimensions[mip].y * TextureImage::kBytesPerPixel;

            std::memcpy(dest_pixels, mip_pixels, mip_size);
            dest_pixels += mip_size;
        }
    }
    out_upload_buff->Unmap();

    // Setup buffer copy regions for each mip level:
    uint32_t buffer_offset = 0;
    FixedSizeArray<VkBufferImageCopy, TextureImage::kMaxMipLevels> buffer_copy_regions;

    for (uint32_t mip = 0; mip < num_mips; ++mip)
    {
        VkBufferImageCopy copy_region{};
        copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_region.imageSubresource.mipLevel       = mip;
        copy_region.imageSubresource.baseArrayLayer = 0;
        copy_region.imageSubresource.layerCount     = 1;
        copy_region.imageExtent.width               = mipmaps.mip_dimensions[mip].x;
        copy_region.imageExtent.height              = mipmaps.mip_dimensions[mip].y;
        copy_region.imageExtent.depth               = 1;
        copy_region.bufferOffset                    = buffer_offset;
        buffer_copy_regions.push_back(copy_region);

        buffer_offset += mipmaps.mip_dimensions[mip].x * mipmaps.mip_dimensions[mip].y * TextureImage::kBytesPerPixel;
    }

    MRQ2_ASSERT(!upload_info.is_scrap);

    // Image barrier for optimal image (target)
    // Optimal image will be used as destination for the copy.
    VulkanChangeImageLayout(m_upload_cmd_buffer,
                            upload_info.texture->Handle(),
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            0, num_mips);

    // Copy mip levels from staging buffer:
    vkCmdCopyBufferToImage(m_upload_cmd_buffer.Handle(),
                           out_upload_buff->Handle(),
                           upload_info.texture->Handle(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           buffer_copy_regions.size(),
                           buffer_copy_regions.data());

    // Change texture image layout to shader read after all mip levels have been copied:
    VulkanChangeImageLayout(m_upload_cmd_buffer,
                            upload_info.texture->Handle(),
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            0, num_mips);
}

} // MrQ2
