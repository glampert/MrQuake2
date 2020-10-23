//
// UploadContextVK.cpp
//

#include "../common/TextureStore.hpp"
#include "../common/OptickProfiler.hpp"
#include "UploadContextVK.hpp"
#include "DeviceVK.hpp"
#include "SwapChainVK.hpp"
#include "TextureVK.hpp"

namespace MrQ2
{

void UploadContextVK::Init(const DeviceVK & device, SwapChainVK & swap_chain)
{
    MRQ2_ASSERT(m_device_vk == nullptr);

    m_upload_cmd_buffer.Init(device);
    m_upload_cmd_buffer.BeginRecording();

    m_device_vk  = &device;
    m_swap_chain = &swap_chain;
}

void UploadContextVK::Shutdown()
{
    for (uint32_t e = 0; e < m_num_pending_texture_creates; ++e)
    {
        m_pending_texture_creates[e].Shutdown();
    }
    m_num_pending_texture_creates = 0;

    for (UploadEntry & entry : m_pending_texture_uploads)
    {
        entry.Reset();
    }
    m_num_pending_texture_uploads = 0;

    m_upload_cmd_buffer.Shutdown();
    m_device_vk  = nullptr;
    m_swap_chain = nullptr;
}

void UploadContextVK::UploadTexture(const TextureUploadVK & upload_info)
{
    OPTICK_EVENT();

    MRQ2_ASSERT(m_device_vk != nullptr);
    MRQ2_ASSERT(RenderInterfaceVK::IsFrameStarted()); // Must happen between a Begin/EndFrame

    if (m_num_pending_texture_uploads == ArrayLength(m_pending_texture_uploads))
    {
        GameInterface::Errorf("Max number of pending Vulkan texture uploads reached!");
    }

    // Find a free entry
    UploadEntry * free_entry = nullptr;
    for (UploadEntry & entry : m_pending_texture_uploads)
    {
        if (entry.texture_handle == nullptr)
        {
            free_entry = &entry;
            break;
        }
    }

    MRQ2_ASSERT(free_entry != nullptr);
    MRQ2_ASSERT(free_entry->cmd_buffer == nullptr); // not kicked

    ++m_num_pending_texture_uploads;
    CreateUploadBuffer(upload_info, nullptr, free_entry, &free_entry->upload_buffer);
}

void UploadContextVK::CreateTexture(const TextureUploadVK & upload_info)
{
    OPTICK_EVENT();

    MRQ2_ASSERT(m_device_vk != nullptr);
    // NOTE: Not required to happen between Begin/EndFrame

    if (m_num_pending_texture_creates == ArrayLength(m_pending_texture_creates))
    {
        // Flush any queued texture creates to make room
        FlushTextureCreates();
    }

    MRQ2_ASSERT(m_num_pending_texture_creates < ArrayLength(m_pending_texture_creates));
    StagingBuffer & upload_buffer = m_pending_texture_creates[m_num_pending_texture_creates++];
    CreateUploadBuffer(upload_info, &m_upload_cmd_buffer, nullptr, &upload_buffer);
}

void UploadContextVK::FlushTextureCreates()
{
    if (m_num_pending_texture_creates == 0)
    {
        return;
    }

    OPTICK_EVENT();

    m_upload_cmd_buffer.EndRecording();
    m_upload_cmd_buffer.Submit();
    m_upload_cmd_buffer.WaitComplete();
    m_upload_cmd_buffer.Reset();
    m_upload_cmd_buffer.BeginRecording();

    // We have synced the command buffer, all pending upload buffers can now be freed.
    for (uint32_t e = 0; e < m_num_pending_texture_creates; ++e)
    {
        m_pending_texture_creates[e].Shutdown();
    }
    m_num_pending_texture_creates = 0;
}

void UploadContextVK::UpdateCompletedUploads()
{
    if (m_num_pending_texture_uploads == 0)
    {
        return;
    }

    OPTICK_EVENT();

    // Kick this frame's deferred uploads
    for (UploadEntry & entry : m_pending_texture_uploads)
    {
        if ((entry.texture_handle != nullptr) && (entry.cmd_buffer == nullptr))
        {
            KickTextureUpload(&entry);
        }
    }

    // Garbage collect upload_buffers from completed uploads of previous frames
    for (UploadEntry & entry : m_pending_texture_uploads)
    {
        if (entry.cmd_buffer != nullptr)
        {
            if (entry.cmd_buffer->IsFinishedExecuting())
            {
                entry.Reset();

                --m_num_pending_texture_uploads;
                MRQ2_ASSERT(m_num_pending_texture_uploads >= 0);
                if (m_num_pending_texture_uploads == 0)
                {
                    break; // Freed all.
                }
            }
        }
    }
}

void UploadContextVK::KickTextureUpload(UploadEntry * entry)
{
    CommandBufferVK & current_cmd_buffer = m_swap_chain->CurrentCmdBuffer();
    entry->cmd_buffer = &current_cmd_buffer;

    PushTextureCopyCommands(&current_cmd_buffer, &entry->upload_buffer, entry->texture_handle,
                            entry->old_image_layout, entry->new_image_layout, entry->num_mips, entry->copy_regions);
}

void UploadContextVK::CreateUploadBuffer(const TextureUploadVK & upload_info, CommandBufferVK * upload_cmd_buffer,
                                         UploadEntry * deferred_upload_entry, StagingBuffer * out_upload_buff)
{
    MRQ2_ASSERT(upload_info.texture != nullptr);
    MRQ2_ASSERT(upload_info.texture->Handle() != nullptr);

    const auto &   mipmaps  = upload_info.mipmaps;
    const uint32_t num_mips = mipmaps.num_mip_levels;

    // At least 1 mipmap level.
    MRQ2_ASSERT(mipmaps.mip_init_data[0] != nullptr);
    MRQ2_ASSERT(mipmaps.mip_dimensions[0].x != 0 && mipmaps.mip_dimensions[0].y != 0);
    MRQ2_ASSERT(num_mips >= 1 && num_mips <= TextureImage::kMaxMipLevels);

    uint32_t buffer_size_in_bytes = 0;
    for (uint32_t mip = 0; mip < num_mips; ++mip)
    {
        buffer_size_in_bytes += mipmaps.mip_dimensions[mip].x * mipmaps.mip_dimensions[mip].y * TextureImage::kBytesPerPixel;
    }

    // Create a host-visible staging buffer that will contain the raw image data:
    out_upload_buff->Init(*m_device_vk, buffer_size_in_bytes);

    // Copy texture data into the staging buffer:
    auto * dest_pixels = static_cast<std::uint8_t *>(out_upload_buff->Map());
    for (uint32_t mip = 0; mip < num_mips; ++mip)
    {
        const auto * mip_pixels = mipmaps.mip_init_data[mip];
        const size_t mip_size   = mipmaps.mip_dimensions[mip].x * mipmaps.mip_dimensions[mip].y * TextureImage::kBytesPerPixel;

        std::memcpy(dest_pixels, mip_pixels, mip_size);
        dest_pixels += mip_size;
    }
    out_upload_buff->Unmap();

    // Setup buffer copy regions for each mip level:
    VkDeviceSize buffer_offset = 0;
    TextureCopyRegions texture_copy_regions;

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
        texture_copy_regions.push_back(copy_region);

        buffer_offset += mipmaps.mip_dimensions[mip].x * mipmaps.mip_dimensions[mip].y * TextureImage::kBytesPerPixel;
    }

    VkImageLayout old_image_layout, new_image_layout;

    // If the texture is a scrap (dynamic upload) it will be already in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL state.
    if (upload_info.is_scrap)
    {
        old_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        new_image_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }
    else // If we're creating a new texture then we're in VK_IMAGE_LAYOUT_UNDEFINED state.
    {
        old_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        new_image_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    }

    if (deferred_upload_entry != nullptr)
    {
        MRQ2_ASSERT(upload_cmd_buffer == nullptr);
        MRQ2_ASSERT(out_upload_buff   == &deferred_upload_entry->upload_buffer);

        // Defer the upload to the end of the frame after we have completed the main render pass
        // since Vulkan disallows texture updates while inside a render pass.
        // Note that this will introduce a frame of delay to the update of cinematic textures
        // and the scrap atlas. For lightmaps we already have a frame of delay in the update since
        // they are only submitted at the end of the frame after draw commands have been pushed.
        deferred_upload_entry->texture_handle   = upload_info.texture->Handle();
        deferred_upload_entry->num_mips         = num_mips;
        deferred_upload_entry->old_image_layout = old_image_layout;
        deferred_upload_entry->new_image_layout = new_image_layout;
        deferred_upload_entry->copy_regions     = texture_copy_regions;
    }
    else
    {
        PushTextureCopyCommands(upload_cmd_buffer, out_upload_buff, upload_info.texture->Handle(),
                                old_image_layout, new_image_layout, num_mips, texture_copy_regions);
    }
}

void UploadContextVK::PushTextureCopyCommands(CommandBufferVK * upload_cmd_buffer, const StagingBuffer * upload_buffer,
                                              VkImage texture_handle, const VkImageLayout old_image_layout, const VkImageLayout new_image_layout,
                                              const uint32_t num_mips, const TextureCopyRegions & copy_regions) const
{
    MRQ2_ASSERT(upload_cmd_buffer != nullptr);
    MRQ2_ASSERT(upload_buffer     != nullptr);
    MRQ2_ASSERT(texture_handle    != nullptr);

    // Image barrier for optimal image (target)
    // Optimal image will be used as destination for the copy.
    VulkanChangeImageLayout(*upload_cmd_buffer,
                            texture_handle,
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            old_image_layout,
                            new_image_layout,
                            0, num_mips);

    // Copy mip levels from staging buffer:
    vkCmdCopyBufferToImage(upload_cmd_buffer->Handle(),
                           upload_buffer->Handle(),
                           texture_handle,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           copy_regions.size(),
                           copy_regions.data());

    // Change texture image layout to shader read after all mip levels have been copied:
    VulkanChangeImageLayout(*upload_cmd_buffer,
                            texture_handle,
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            0, num_mips);
}

} // MrQ2
