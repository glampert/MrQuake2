//
// SwapChainVK.cpp
//

#include "SwapChainVK.hpp"
#include "DeviceVK.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// SwapChainVK
///////////////////////////////////////////////////////////////////////////////

void SwapChainVK::Init(const DeviceVK & device, std::uint32_t width, std::uint32_t height, SwapChainRenderTargetsVK & rts)
{
    MRQ2_ASSERT(width != 0 && height != 0);
    this->device_vk = &device;

    VkSurfaceCapabilitiesKHR surface_capabilities{};
    VULKAN_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.PhysDevice(), device.RenderSurface(), &surface_capabilities));

    std::uint32_t present_mode_count = 0; // Get the count:
    VULKAN_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device.PhysDevice(), device.RenderSurface(), &present_mode_count, nullptr));
    MRQ2_ASSERT(present_mode_count >= 1);

    std::vector<VkPresentModeKHR> present_modes(present_mode_count, VkPresentModeKHR(0));
    VULKAN_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device.PhysDevice(), device.RenderSurface(), &present_mode_count, present_modes.data()));
    MRQ2_ASSERT(present_mode_count >= 1);

    // width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
    if (surface_capabilities.currentExtent.width == 0xFFFFFFFF)
    {
        // If the surface size is undefined, size is set to the
        // the window size, but clamped to min/max extents supported.
        swap_chain_extents.width  = width;
        swap_chain_extents.height = height;

        Clamp(&swap_chain_extents.width,  surface_capabilities.minImageExtent.width,  surface_capabilities.maxImageExtent.width);
        Clamp(&swap_chain_extents.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);

        width  = swap_chain_extents.width;
        height = swap_chain_extents.height;
    }
    else
    {
        // If the surface size is defined, the swap chain size must match.
        swap_chain_extents = surface_capabilities.currentExtent;
    }

    GameInterface::Printf("Swap chain extents = {%u,%u}", swap_chain_extents.width, swap_chain_extents.height);

    // If mailbox mode is available, use it, as is the lowest-latency non-
    // tearing mode. If not, try IMMEDIATE which will usually be available,
    // and is the fastest (though it tears). If not, fall back to FIFO which is
    // always available.
    VkPresentModeKHR swap_chain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (std::uint32_t i = 0; i < present_mode_count; ++i)
    {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            swap_chain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
        if ((swap_chain_present_mode != VK_PRESENT_MODE_MAILBOX_KHR) &&
            (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
        {
            swap_chain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    std::uint32_t num_desired_swap_chain_images = kVkNumFrameBuffers;
    if (surface_capabilities.maxImageCount > 0 && num_desired_swap_chain_images > surface_capabilities.maxImageCount)
    {
        num_desired_swap_chain_images = surface_capabilities.maxImageCount;
    }
    GameInterface::Printf("Num swap chain images = %u", num_desired_swap_chain_images);

    VkSurfaceTransformFlagBitsKHR pre_transform_flags;
    if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        pre_transform_flags = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        pre_transform_flags = surface_capabilities.currentTransform;
    }

    VkSwapchainCreateInfoKHR swap_chain_create_info = {};
    swap_chain_create_info.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_chain_create_info.surface                  = device.RenderSurface();
    swap_chain_create_info.minImageCount            = num_desired_swap_chain_images;
    swap_chain_create_info.imageFormat              = device.RenderSurfaceFormat();
    swap_chain_create_info.imageExtent.width        = swap_chain_extents.width;
    swap_chain_create_info.imageExtent.height       = swap_chain_extents.height;
    swap_chain_create_info.preTransform             = pre_transform_flags;
    swap_chain_create_info.compositeAlpha           = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_create_info.imageArrayLayers         = 1;
    swap_chain_create_info.presentMode              = swap_chain_present_mode;
    swap_chain_create_info.oldSwapchain             = nullptr;
    swap_chain_create_info.clipped                  = VK_TRUE;
    swap_chain_create_info.imageColorSpace          = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swap_chain_create_info.imageUsage               = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    swap_chain_create_info.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;

    const std::uint32_t queue_family_indices[] = {
        static_cast<std::uint32_t>(device.GraphicsQueue().family_index),
        static_cast<std::uint32_t>(device.PresentQueue().family_index)
    };

    if (device.GraphicsQueue().family_index != device.PresentQueue().family_index)
    {
        // If the graphics and present queues are from different queue families,
        // we either have to explicitly transfer ownership of images between the
        // queues, or we have to create the swap-chain with imageSharingMode
        // as VK_SHARING_MODE_CONCURRENT
        swap_chain_create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swap_chain_create_info.queueFamilyIndexCount = 2;
        swap_chain_create_info.pQueueFamilyIndices   = queue_family_indices;
    }

    VULKAN_CHECK(vkCreateSwapchainKHR(device.Handle(), &swap_chain_create_info, nullptr, &swap_chain_handle));
    MRQ2_ASSERT(swap_chain_handle != nullptr);

    VULKAN_CHECK(vkGetSwapchainImagesKHR(device.Handle(), swap_chain_handle, &buffer_count, nullptr));
    MRQ2_ASSERT(buffer_count >= 1);

    std::vector<VkImage> swap_chain_images(buffer_count, VK_NULL_HANDLE);
    VULKAN_CHECK(vkGetSwapchainImagesKHR(device.Handle(), swap_chain_handle, &buffer_count, swap_chain_images.data()));
    MRQ2_ASSERT(buffer_count >= 1);

    // Create views for the swap chain framebuffer images:
    for (std::uint32_t i = 0; i < buffer_count; ++i)
    {
        VkImageViewCreateInfo view_create_info           = {};
        view_create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create_info.format                          = device.RenderSurfaceFormat();
        view_create_info.components.r                    = VK_COMPONENT_SWIZZLE_R;
        view_create_info.components.g                    = VK_COMPONENT_SWIZZLE_G;
        view_create_info.components.b                    = VK_COMPONENT_SWIZZLE_B;
        view_create_info.components.a                    = VK_COMPONENT_SWIZZLE_A;
        view_create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        view_create_info.subresourceRange.baseMipLevel   = 0;
        view_create_info.subresourceRange.levelCount     = 1;
        view_create_info.subresourceRange.baseArrayLayer = 0;
        view_create_info.subresourceRange.layerCount     = 1;
        view_create_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        view_create_info.flags                           = 0;
        view_create_info.image                           = swap_chain_images[i];

        rts.fb[i].image = swap_chain_images[i];
        VULKAN_CHECK(vkCreateImageView(device.Handle(), &view_create_info, nullptr, &rts.fb[i].view));
    }

    GameInterface::Printf("Swap chain created with %u image buffers.", buffer_count);
}

void SwapChainVK::Shutdown()
{
    if (device_vk == nullptr)
        return;

    if (swap_chain_handle != nullptr)
    {
        vkDestroySwapchainKHR(device_vk->Handle(), swap_chain_handle, nullptr);
        swap_chain_handle = nullptr;
    }

    device_vk = nullptr;
}

void SwapChainVK::Present()
{
}

///////////////////////////////////////////////////////////////////////////////
// SwapChainRenderTargetsVK
///////////////////////////////////////////////////////////////////////////////

void SwapChainRenderTargetsVK::Init(const SwapChainVK & sc)
{
    MRQ2_ASSERT(sc.swap_chain_handle != nullptr);
    MRQ2_ASSERT(sc.swap_chain_extents.width != 0 && sc.swap_chain_extents.height != 0);

    render_target_width  = static_cast<int>(sc.swap_chain_extents.width);
    render_target_height = static_cast<int>(sc.swap_chain_extents.height);
    device_vk = sc.device_vk;

    // TODO: need a cmdbuffer and a render pass!
}

void SwapChainRenderTargetsVK::Shutdown()
{
    if (device_vk == nullptr)
        return;

    // Clean up the swap-chain image views and FBs.
    // The swap-chain images themselves are owned by the swap-chain.
    for (auto & buff : fb)
    {
        if (buff.view != nullptr)
        {
            vkDestroyImageView(device_vk->Handle(), buff.view, nullptr);
        }
        if (buff.framebuffer_handle != nullptr)
        {
            vkDestroyFramebuffer(device_vk->Handle(), buff.framebuffer_handle, nullptr);
        }
        buff = {};
    }

    // The depth buffer owns the views and also the image memory.
    if (depth.view != nullptr)
    {
        vkDestroyImageView(device_vk->Handle(), depth.view, nullptr);
        depth.view = nullptr;
    }
    if (depth.image != nullptr)
    {
        vkDestroyImage(device_vk->Handle(), depth.image, nullptr);
        depth.image = nullptr;
    }
    if (depth.memory != nullptr)
    {
        vkFreeMemory(device_vk->Handle(), depth.memory, nullptr);
        depth.memory = nullptr;
    }

    device_vk = nullptr;
}

} // MrQ2
