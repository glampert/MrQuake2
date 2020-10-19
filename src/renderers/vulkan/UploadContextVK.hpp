//
// UploadContextVK.hpp
//
#pragma once

#include "UtilsVK.hpp"
#include "BufferVK.hpp"

namespace MrQ2
{

class DeviceVK;
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

    // Disallow copy.
    UploadContextVK(const UploadContextVK &) = delete;
    UploadContextVK & operator=(const UploadContextVK &) = delete;

    void Init(const DeviceVK & device);
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

    void CreateUploadBuffer(const TextureUploadVK & upload_info, StagingBuffer * out_upload_buff);

    const DeviceVK * m_device_vk{ nullptr };
    CommandBufferVK  m_upload_cmd_buffer;
    uint32_t         m_num_pending_texture_creates{ 0 };
    StagingBuffer    m_pending_texture_creates[512];
};

} // MrQ2
