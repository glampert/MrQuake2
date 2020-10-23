//
// UploadContextVK.hpp
//
#pragma once

#include "UtilsVK.hpp"
#include "BufferVK.hpp"

namespace MrQ2
{

class DeviceVK;
class SwapChainVK;
class TextureVK;

struct TextureUploadVK final
{
    const TextureVK * texture;
    bool is_scrap;

    struct {
        uint32_t             num_mip_levels;
        const ColorRGBA32 ** mip_init_data;
        const Vec2u16 *      mip_dimensions;
    } mipmaps;
};

class UploadContextVK final
{
public:

    UploadContextVK() = default;
    ~UploadContextVK() { Shutdown(); }

    // Disallow copy.
    UploadContextVK(const UploadContextVK &) = delete;
    UploadContextVK & operator=(const UploadContextVK &) = delete;

    void Init(const DeviceVK & device, SwapChainVK & swap_chain);
    void Shutdown();

    void UploadTexture(const TextureUploadVK & upload_info);

    // VK internal
    void CreateTexture(const TextureUploadVK & upload_info);
    void FlushTextureCreates();
    void UpdateCompletedUploads();

private:

    class StagingBuffer final : public BufferVK
    {
    public:
        void Init(const DeviceVK & device, const uint32_t buffer_size_in_bytes)
        {
            VkMemoryRequirements mem_requirements{};
            InitBufferInternal(device, buffer_size_in_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &mem_requirements);

            if (mem_requirements.size < buffer_size_in_bytes)
            {
                GameInterface::Errorf("VkMemoryRequirements::size (%zd) < Staging buffer size (%u)!", mem_requirements.size, buffer_size_in_bytes);
            }
        }
    };

    using TextureCopyRegions = FixedSizeArray<VkBufferImageCopy, 8>; // TextureImage::kMaxMipLevels

    // We need to defer submitting the commands to the upload cmd buffer to after the main render pass has been completed.
    struct UploadEntry
    {
        VkImage            texture_handle{ nullptr };
        CommandBufferVK *  cmd_buffer{ nullptr };
        uint32_t           num_mips{ 0 };
        VkImageLayout      old_image_layout{};
        VkImageLayout      new_image_layout{};
        StagingBuffer      upload_buffer;
        TextureCopyRegions copy_regions;

        void Reset()
        {
            texture_handle   = nullptr;
            cmd_buffer       = nullptr;
            num_mips         = 0;
            old_image_layout = {};
            new_image_layout = {};

            upload_buffer.Shutdown();
            copy_regions.clear();
        }
    };

    void KickTextureUpload(UploadEntry * entry);

    void CreateUploadBuffer(const TextureUploadVK & upload_info, CommandBufferVK * upload_cmd_buffer,
                            UploadEntry * deferred_upload_entry, StagingBuffer * out_upload_buff);

    void PushTextureCopyCommands(CommandBufferVK * upload_cmd_buffer, const StagingBuffer * upload_buffer,
                                 VkImage texture_handle, const VkImageLayout old_image_layout, const VkImageLayout new_image_layout,
                                 const uint32_t num_mips, const TextureCopyRegions & copy_regions) const;

    const DeviceVK * m_device_vk{ nullptr };
    SwapChainVK *    m_swap_chain{ nullptr };
    CommandBufferVK  m_upload_cmd_buffer; // Used for texture creates only

    uint32_t         m_num_pending_texture_creates{ 0 };
    StagingBuffer    m_pending_texture_creates[512];

    uint32_t         m_num_pending_texture_uploads{ 0 };
    UploadEntry      m_pending_texture_uploads[8];
};

} // MrQ2
