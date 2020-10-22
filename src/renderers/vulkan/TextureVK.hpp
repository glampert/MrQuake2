//
// TextureVK.hpp
//
#pragma once

#include "UtilsVK.hpp"

namespace MrQ2
{

class DeviceVK;
enum class TextureType : std::uint8_t;

class TextureVK final
{
public:

    TextureVK() = default;
    ~TextureVK() { Shutdown(); }

    // Disallow copy.
    TextureVK(const TextureVK &) = delete;
    TextureVK & operator=(const TextureVK &) = delete;

    void Init(const DeviceVK & device, const TextureType type, const bool is_scrap,
              const ColorRGBA32 * mip_init_data[], const Vec2u16 mip_dimensions[],
              const uint32_t num_mip_levels, const char * const debug_name);

    // Init from existing texture sharing the resource and sampler/SRV (for the scrap texture)
    void Init(const TextureVK & other);

    void Shutdown();

    VkImage     Handle()        const { return m_image_handle; }
    VkImageView ViewHandle()    const { return m_image_view_handle; }
    VkSampler   SamplerHandle() const { return m_sampler_handle; }

private:

    const DeviceVK * m_device_vk{ nullptr };
    VkSampler        m_sampler_handle{ nullptr };
    VkImage          m_image_handle{ nullptr };
    VkImageView      m_image_view_handle{ nullptr };
    VkDeviceMemory   m_image_mem_handle{ nullptr };
    bool             m_owns_resources{ false };
};

} // MrQ2
