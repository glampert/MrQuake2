//
// TextureVK.cpp
//

#include "../common/TextureStore.hpp"
#include "TextureVK.hpp"
#include "DeviceVK.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// Texture filtering selection:
///////////////////////////////////////////////////////////////////////////////

static void FilterForTextureType(const TextureType type, VkSamplerCreateInfo * sampler_info)
{
    static const struct {
        VkFilter            min_filter;
        VkFilter            mag_filter;
        VkSamplerMipmapMode mipmap_mode;
        bool                anisotropic;
    } s_vk_tex_filter_options[] = {
        { VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, false }, // 0 nearest
        { VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_SAMPLER_MIPMAP_MODE_NEAREST, false }, // 1 bilinear
        { VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_SAMPLER_MIPMAP_MODE_LINEAR,  false }, // 2 trilinear
        { VK_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_SAMPLER_MIPMAP_MODE_LINEAR,  true  }, // 3 anisotropic
    };

    if (type < TextureType::kPic) // with mipmaps
    {
        uint32_t opt = Config::r_tex_filtering.AsInt();
        if (opt >= ArrayLength(s_vk_tex_filter_options))
        {
            opt = ArrayLength(s_vk_tex_filter_options) - 1;
        }

        sampler_info->minFilter        = s_vk_tex_filter_options[opt].min_filter;
        sampler_info->magFilter        = s_vk_tex_filter_options[opt].mag_filter;
        sampler_info->mipmapMode       = s_vk_tex_filter_options[opt].mipmap_mode;
        sampler_info->anisotropyEnable = s_vk_tex_filter_options[opt].anisotropic;
    }
    else if (type == TextureType::kLightmap) // bilinear filter for lightmaps
    {
        sampler_info->minFilter        = s_vk_tex_filter_options[1].min_filter;
        sampler_info->magFilter        = s_vk_tex_filter_options[1].mag_filter;
        sampler_info->mipmapMode       = s_vk_tex_filter_options[1].mipmap_mode;
        sampler_info->anisotropyEnable = s_vk_tex_filter_options[1].anisotropic;
    }
    else // no mipmaps (UI/Cinematic frames), use point/nearest sampling
    {
        sampler_info->minFilter        = s_vk_tex_filter_options[0].min_filter;
        sampler_info->magFilter        = s_vk_tex_filter_options[0].mag_filter;
        sampler_info->mipmapMode       = s_vk_tex_filter_options[0].mipmap_mode;
        sampler_info->anisotropyEnable = s_vk_tex_filter_options[0].anisotropic;
    }

    if (sampler_info->anisotropyEnable)
    {
        int max_anisotropy = Config::r_max_anisotropy.AsInt();

        if (max_anisotropy > 16)
            max_anisotropy = 16;

        if (max_anisotropy < 1)
            max_anisotropy = 1;

        sampler_info->maxAnisotropy = static_cast<float>(max_anisotropy);
    }
}

///////////////////////////////////////////////////////////////////////////////
// TextureVK:
///////////////////////////////////////////////////////////////////////////////

void TextureVK::Init(const DeviceVK & device, const TextureType type, const bool is_scrap,
                     const ColorRGBA32 * mip_init_data[], const Vec2u16 mip_dimensions[],
                     const uint32_t num_mip_levels, const char * const debug_name)
{
    MRQ2_ASSERT(num_mip_levels >= 1 && num_mip_levels <= TextureImage::kMaxMipLevels);
    MRQ2_ASSERT((mip_dimensions[0].x + mip_dimensions[0].y) != 0);
    MRQ2_ASSERT(mip_init_data[0] != nullptr);
    MRQ2_ASSERT(m_device_vk == nullptr); // Shutdown first

    (void)debug_name; // unused
    (void)is_scrap;   // unused

    //
    // Create optimal tiled target image:
    //
    VkImageCreateInfo image_info{};
    image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType     = VK_IMAGE_TYPE_2D;
    image_info.format        = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width  = mip_dimensions[0].x;
    image_info.extent.height = mip_dimensions[0].y;
    image_info.extent.depth  = 1;
    image_info.mipLevels     = num_mip_levels;
    image_info.arrayLayers   = 1;
    image_info.samples       = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VulkanAllocateImage(device, image_info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m_image_handle, &m_image_mem_handle);

    //
    // Create the image view for our final color texture:
    //
    VkImageViewCreateInfo view_info{};
    view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image                           = m_image_handle;
    view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format                          = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.components.r                    = VK_COMPONENT_SWIZZLE_R;
    view_info.components.g                    = VK_COMPONENT_SWIZZLE_G;
    view_info.components.b                    = VK_COMPONENT_SWIZZLE_B;
    view_info.components.a                    = VK_COMPONENT_SWIZZLE_A;
    view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel   = 0;
    view_info.subresourceRange.levelCount     = num_mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount     = 1;
    VULKAN_CHECK(vkCreateImageView(device.Handle(), &view_info, nullptr, &m_image_view_handle));

    //
    // Lastly, create a sampler object:
    //
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.addressModeU  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.mipLodBias    = 0.0f;
    sampler_info.compareEnable = false;
    sampler_info.compareOp     = VK_COMPARE_OP_ALWAYS;
    sampler_info.minLod        = 0.0f;
    sampler_info.maxLod        = FLT_MAX;
    sampler_info.borderColor   = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    sampler_info.unnormalizedCoordinates = false;
    FilterForTextureType(type, &sampler_info);
    VULKAN_CHECK(vkCreateSampler(device.Handle(), &sampler_info, nullptr, &m_sampler_handle));

    //
    // Upload initial texture pixels:
    //
    TextureUploadVK upload_info{};
    upload_info.texture                = this;
    upload_info.is_scrap               = false; // Transition to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    upload_info.mipmaps.num_mip_levels = num_mip_levels;
    upload_info.mipmaps.mip_init_data  = mip_init_data;
    upload_info.mipmaps.mip_dimensions = mip_dimensions;
    device.UploadContext().CreateTexture(upload_info);

    m_owns_resources = true;
    m_device_vk = &device;

    m_device_vk->SetObjectDebugName(VK_OBJECT_TYPE_IMAGE, m_image_handle, debug_name);
}

void TextureVK::Init(const TextureVK & other)
{
    MRQ2_ASSERT(m_device_vk == nullptr); // Shutdown first

    // Share the other texture resource(s)
    m_device_vk         = other.m_device_vk;
    m_sampler_handle    = other.m_sampler_handle;
    m_image_handle      = other.m_image_handle;
    m_image_view_handle = other.m_image_view_handle;
    m_image_mem_handle  = other.m_image_mem_handle;
    m_owns_resources    = false;
}

void TextureVK::Shutdown()
{
    if (m_device_vk == nullptr)
    {
        return;
    }

    if (m_owns_resources)
    {
        VkDevice device = m_device_vk->Handle();

        if (m_sampler_handle != nullptr)
        {
            vkDestroySampler(device, m_sampler_handle, nullptr);
            m_sampler_handle = nullptr;
        }

        if (m_image_view_handle != nullptr)
        {
            vkDestroyImageView(device, m_image_view_handle, nullptr);
            m_image_view_handle = nullptr;
        }

        if (m_image_handle != nullptr)
        {
            vkDestroyImage(device, m_image_handle, nullptr);
            m_image_handle = nullptr;
        }

        if (m_image_mem_handle != nullptr)
        {
            vkFreeMemory(device, m_image_mem_handle, nullptr);
            m_image_mem_handle = nullptr;
        }

        m_owns_resources = false;
    }

    m_device_vk = nullptr;
}

} // namespace MrQ2
