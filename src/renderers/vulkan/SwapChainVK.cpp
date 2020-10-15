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

void SwapChainVK::Init(const DeviceVK & device, uint32_t width, uint32_t height, SwapChainRenderTargetsVK & rts)
{
    MRQ2_ASSERT(width != 0 && height != 0);
    m_device_vk = &device;

    VkSurfaceCapabilitiesKHR surface_capabilities{};
    VULKAN_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.PhysDevice(), device.RenderSurface(), &surface_capabilities));

    uint32_t present_mode_count = 0; // Get the count:
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
        m_swap_chain_extents.width  = width;
        m_swap_chain_extents.height = height;

        Clamp(&m_swap_chain_extents.width,  surface_capabilities.minImageExtent.width,  surface_capabilities.maxImageExtent.width);
        Clamp(&m_swap_chain_extents.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);

        width  = m_swap_chain_extents.width;
        height = m_swap_chain_extents.height;
    }
    else
    {
        // If the surface size is defined, the swap chain size must match.
        m_swap_chain_extents = surface_capabilities.currentExtent;
    }

    GameInterface::Printf("Swap chain extents = {%u,%u}", m_swap_chain_extents.width, m_swap_chain_extents.height);

    // If mailbox mode is available, use it, as is the lowest-latency non-
    // tearing mode. If not, try IMMEDIATE which will usually be available,
    // and is the fastest (though it tears). If not, fall back to FIFO which is
    // always available.
    VkPresentModeKHR swap_chain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < present_mode_count; ++i)
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

    uint32_t num_desired_swap_chain_images = kVkNumFrameBuffers;
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
    swap_chain_create_info.imageExtent.width        = m_swap_chain_extents.width;
    swap_chain_create_info.imageExtent.height       = m_swap_chain_extents.height;
    swap_chain_create_info.preTransform             = pre_transform_flags;
    swap_chain_create_info.compositeAlpha           = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_create_info.imageArrayLayers         = 1;
    swap_chain_create_info.presentMode              = swap_chain_present_mode;
    swap_chain_create_info.oldSwapchain             = nullptr;
    swap_chain_create_info.clipped                  = VK_TRUE;
    swap_chain_create_info.imageColorSpace          = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swap_chain_create_info.imageUsage               = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    swap_chain_create_info.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;

    const uint32_t queue_family_indices[] = {
        static_cast<uint32_t>(device.GraphicsQueue().family_index),
        static_cast<uint32_t>(device.PresentQueue().family_index)
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

    VULKAN_CHECK(vkCreateSwapchainKHR(device.Handle(), &swap_chain_create_info, nullptr, &m_swap_chain_handle));
    MRQ2_ASSERT(m_swap_chain_handle != nullptr);

    VULKAN_CHECK(vkGetSwapchainImagesKHR(device.Handle(), m_swap_chain_handle, &m_frame_buffer_count, nullptr));
    MRQ2_ASSERT(m_frame_buffer_count >= 1);

    std::vector<VkImage> swap_chain_images(m_frame_buffer_count, VkImage(nullptr));
    VULKAN_CHECK(vkGetSwapchainImagesKHR(device.Handle(), m_swap_chain_handle, &m_frame_buffer_count, swap_chain_images.data()));
    MRQ2_ASSERT(m_frame_buffer_count >= 1);

    m_current_cmd_buffer_index = 0;
    GameInterface::Printf("Frame command buffers created.");

    // Temp cmd buffer to transition the swapchain images.
    CommandBufferVK temp_cmd_buffer;
    temp_cmd_buffer.Init(device);
    temp_cmd_buffer.BeginRecording();

    // Create views for the swap chain framebuffer images:
    for (uint32_t i = 0; i < m_frame_buffer_count; ++i)
    {
        VulkanChangeImageLayout(temp_cmd_buffer, swap_chain_images[i], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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

        rts.m_fb[i].image = swap_chain_images[i];
        VULKAN_CHECK(vkCreateImageView(device.Handle(), &view_create_info, nullptr, &rts.m_fb[i].view));
    }
    rts.m_frame_buffer_count = m_frame_buffer_count;

    temp_cmd_buffer.EndRecording();
    temp_cmd_buffer.Submit();
    temp_cmd_buffer.WaitComplete();

    GameInterface::Printf("Swap chain created with %u image buffers.", m_frame_buffer_count);

    // Frame semaphores
    VkSemaphoreCreateInfo sema_create_info{};
    sema_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VULKAN_CHECK(vkCreateSemaphore(device.Handle(), &sema_create_info, nullptr, &m_image_available_semaphore));
    VULKAN_CHECK(vkCreateSemaphore(device.Handle(), &sema_create_info, nullptr, &m_render_finished_semaphore));

    MRQ2_ASSERT(m_image_available_semaphore != nullptr);
    MRQ2_ASSERT(m_render_finished_semaphore != nullptr);

    GameInterface::Printf("Frame semaphores created.");

    // Frame command buffers
    for (CommandBufferVK & buffer : m_cmd_buffers)
    {
        buffer.Init(device, VK_FENCE_CREATE_SIGNALED_BIT);
    }
}

void SwapChainVK::Shutdown()
{
    if (m_device_vk == nullptr)
    {
        return;
    }

    for (CommandBufferVK & buffer : m_cmd_buffers)
    {
        buffer.Shutdown();
    }

    if (m_image_available_semaphore != nullptr)
    {
        vkDestroySemaphore(m_device_vk->Handle(), m_image_available_semaphore, nullptr);
        m_image_available_semaphore = nullptr;
    }

    if (m_render_finished_semaphore != nullptr)
    {
        vkDestroySemaphore(m_device_vk->Handle(), m_render_finished_semaphore, nullptr);
        m_render_finished_semaphore = nullptr;
    }

    if (m_swap_chain_handle != nullptr)
    {
        vkDestroySwapchainKHR(m_device_vk->Handle(), m_swap_chain_handle, nullptr);
        m_swap_chain_handle = nullptr;
    }

    m_device_vk = nullptr;
}

void SwapChainVK::BeginFrame()
{
    constexpr uint64_t kInfiniteWaitTimeout = UINT64_MAX;

    VULKAN_CHECK(vkAcquireNextImageKHR(m_device_vk->Handle(),
                                       m_swap_chain_handle,
                                       kInfiniteWaitTimeout,
                                       m_image_available_semaphore,
                                       nullptr,
                                       &m_frame_buffer_index));

    CommandBufferVK & cmd_buffer = CurrentCmdBuffer();
    cmd_buffer.WaitComplete();
    cmd_buffer.Reset();
    cmd_buffer.BeginRecording();
}

void SwapChainVK::EndFrame()
{
    const VkSemaphore wait_semaphores[]      = { m_image_available_semaphore };
    const VkSemaphore signal_semaphores[]    = { m_render_finished_semaphore };
    const VkSwapchainKHR swap_chains[]       = { m_swap_chain_handle };
    const VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    CommandBufferVK & cmd_buffer = CurrentCmdBuffer();
    VkCommandBuffer submit_buffer = cmd_buffer.Handle();
    cmd_buffer.EndRecording();

    VkQueue present_queue = m_device_vk->PresentQueue().queue_handle;

    VkSubmitInfo submit_info{};
    submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount   = ArrayLength(wait_semaphores);
    submit_info.pWaitSemaphores      = wait_semaphores;
    submit_info.pWaitDstStageMask    = wait_stages;
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &submit_buffer;
    submit_info.signalSemaphoreCount = ArrayLength(signal_semaphores);
    submit_info.pSignalSemaphores    = signal_semaphores;
    cmd_buffer.Submit(submit_info);

    VkPresentInfoKHR present_info{};
    present_info.sType               = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount  = ArrayLength(signal_semaphores);
    present_info.pWaitSemaphores     = signal_semaphores;
    present_info.swapchainCount      = ArrayLength(swap_chains);
    present_info.pSwapchains         = swap_chains;
    present_info.pImageIndices       = &m_frame_buffer_index;
    VULKAN_CHECK(vkQueuePresentKHR(present_queue, &present_info));

    // Next command buffer in the chain
    m_current_cmd_buffer_index = (m_current_cmd_buffer_index + 1) % kVkNumFrameBuffers;
}

///////////////////////////////////////////////////////////////////////////////
// SwapChainRenderTargetsVK
///////////////////////////////////////////////////////////////////////////////

void SwapChainRenderTargetsVK::Init(const DeviceVK & device, const SwapChainVK & sc)
{
    MRQ2_ASSERT(sc.Handle() != nullptr);
    MRQ2_ASSERT(sc.Extents().width != 0 && sc.Extents().height != 0);

    m_render_target_width  = static_cast<int>(sc.Extents().width);
    m_render_target_height = static_cast<int>(sc.Extents().height);
    m_device_vk = &device;

    InitDepthBuffer();
    InitRenderPass();
    InitFramebuffers();
}

void SwapChainRenderTargetsVK::InitDepthBuffer()
{
    // We'll need a temporary command buffer to set up the depth render target.
    CommandBufferVK temp_cmd_buffer;
    temp_cmd_buffer.Init(*m_device_vk);
    temp_cmd_buffer.BeginRecording();

    VkImageCreateInfo image_create_info{};
    const VkFormat kDepthBufferFormat = VK_FORMAT_D24_UNORM_S8_UINT; // 32 bits depth buffer format

    VkFormatProperties fmt_props{};
    vkGetPhysicalDeviceFormatProperties(m_device_vk->PhysDevice(), kDepthBufferFormat, &fmt_props);

    if (fmt_props.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        image_create_info.tiling = VK_IMAGE_TILING_LINEAR;
    }
    else if (fmt_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    }
    else
    {
        // TODO: Try other depth formats if this fails?
        GameInterface::Errorf("Depth format VK_FORMAT_D24_UNORM_S8_UINT not supported!");
    }

    image_create_info.sType                    = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType                = VK_IMAGE_TYPE_2D;
    image_create_info.format                   = kDepthBufferFormat;
    image_create_info.extent.width             = m_render_target_width;
    image_create_info.extent.height            = m_render_target_height;
    image_create_info.extent.depth             = 1;
    image_create_info.mipLevels                = 1;
    image_create_info.arrayLayers              = 1;
    image_create_info.samples                  = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.initialLayout            = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.queueFamilyIndexCount    = 0;
    image_create_info.pQueueFamilyIndices      = nullptr;
    image_create_info.sharingMode              = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.usage                    = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_create_info.flags                    = 0;

    VkImageViewCreateInfo view_info{};
    view_info.sType                            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image                            = nullptr;
    view_info.format                           = kDepthBufferFormat;
    view_info.components.r                     = VK_COMPONENT_SWIZZLE_R;
    view_info.components.g                     = VK_COMPONENT_SWIZZLE_G;
    view_info.components.b                     = VK_COMPONENT_SWIZZLE_B;
    view_info.components.a                     = VK_COMPONENT_SWIZZLE_A;
    view_info.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    view_info.subresourceRange.baseMipLevel    = 0;
    view_info.subresourceRange.levelCount      = 1;
    view_info.subresourceRange.baseArrayLayer  = 0;
    view_info.subresourceRange.layerCount      = 1;
    view_info.viewType                         = VK_IMAGE_VIEW_TYPE_2D;
    view_info.flags                            = 0;

    VkMemoryAllocateInfo mem_alloc_info{};
    mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

    VkDevice device = m_device_vk->Handle();

    VULKAN_CHECK(vkCreateImage(device, &image_create_info, nullptr, &m_depth.image));
    MRQ2_ASSERT(m_depth.image != nullptr);

	VkMemoryRequirements mem_reqs{};
    vkGetImageMemoryRequirements(device, m_depth.image, &mem_reqs);
    mem_alloc_info.allocationSize = mem_reqs.size;

    // Use the memory properties to determine the type of memory required:
    mem_alloc_info.memoryTypeIndex = VulkanMemoryTypeFromProperties(*m_device_vk, mem_reqs.memoryTypeBits, /*requirementsMask=*/0);
    MRQ2_ASSERT(mem_alloc_info.memoryTypeIndex < UINT32_MAX);

    // Allocate the memory:
    VULKAN_CHECK(vkAllocateMemory(device, &mem_alloc_info, nullptr, &m_depth.memory));
    MRQ2_ASSERT(m_depth.memory != nullptr);

    // Bind memory:
    VULKAN_CHECK(vkBindImageMemory(device, m_depth.image, m_depth.memory, 0));

    // Set the image layout to depth stencil optimal:
    VulkanChangeImageLayout(temp_cmd_buffer, m_depth.image, view_info.subresourceRange.aspectMask, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    // And finally create the image view.
    view_info.image = m_depth.image;
    VULKAN_CHECK(vkCreateImageView(device, &view_info, nullptr, &m_depth.view));
    MRQ2_ASSERT(m_depth.view != nullptr);

    temp_cmd_buffer.EndRecording();
    temp_cmd_buffer.Submit();
    temp_cmd_buffer.WaitComplete();

    GameInterface::Printf("Depth buffer created.");
}

void SwapChainRenderTargetsVK::InitRenderPass()
{
    VkAttachmentReference color_reference{};
    color_reference.attachment = 0;
    color_reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_reference{};
    depth_reference.attachment = 1;
    depth_reference.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Need attachments for render target (fb) and depth buffer
    VkAttachmentDescription attachments[2] = {};

    // fb
    attachments[0].format           = m_device_vk->RenderSurfaceFormat();
    attachments[0].samples          = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout      = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[0].flags            = 0;

    // depth
    attachments[1].format           = VK_FORMAT_D24_UNORM_S8_UINT;
    attachments[1].samples          = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].flags            = 0;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &color_reference;
    subpass.pDepthStencilAttachment = &depth_reference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass           = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass           = 0;
    dependency.srcStageMask         = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask        = 0;
    dependency.dstStageMask         = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask        = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = ArrayLength(attachments);
    render_pass_create_info.pAttachments    = attachments;
    render_pass_create_info.subpassCount    = 1;
    render_pass_create_info.pSubpasses      = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies   = &dependency;

    m_main_render_pass.Init(*m_device_vk, render_pass_create_info);
    GameInterface::Printf("Main RenderPass created.");
}

void SwapChainRenderTargetsVK::InitFramebuffers()
{
    MRQ2_ASSERT(m_depth.view != nullptr);                // Depth buffer created first,
    MRQ2_ASSERT(m_main_render_pass.Handle() != nullptr); // and render pass also needed

    VkImageView attachments[2] = {};
    attachments[1] = m_depth.view;

    VkFramebufferCreateInfo fb_create_info{};
    fb_create_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_create_info.renderPass      = m_main_render_pass.Handle();
    fb_create_info.attachmentCount = ArrayLength(attachments); // Include the depth buffer
    fb_create_info.pAttachments    = attachments;
    fb_create_info.width           = m_render_target_width;
    fb_create_info.height          = m_render_target_height;
    fb_create_info.layers          = 1;

    MRQ2_ASSERT(m_frame_buffer_count != 0);
    for (uint32_t i = 0; i < m_frame_buffer_count; ++i)
    {
        attachments[0] = m_fb[i].view;

        VULKAN_CHECK(vkCreateFramebuffer(m_device_vk->Handle(), &fb_create_info, nullptr, &m_fb[i].framebuffer_handle));
        MRQ2_ASSERT(m_fb[i].framebuffer_handle != nullptr);
    }

    GameInterface::Printf("Swap-chain Framebuffers created.");
}

void SwapChainRenderTargetsVK::Shutdown()
{
    if (m_device_vk == nullptr)
    {
        return;
    }

    m_main_render_pass.Shutdown();

    // Clean up the swap-chain image views and FBs.
    // The swap-chain images themselves are owned by the swap-chain.
    for (FrameBuffer & buff : m_fb)
    {
        if (buff.view != nullptr)
        {
            vkDestroyImageView(m_device_vk->Handle(), buff.view, nullptr);
        }
        if (buff.framebuffer_handle != nullptr)
        {
            vkDestroyFramebuffer(m_device_vk->Handle(), buff.framebuffer_handle, nullptr);
        }
        buff = {};
    }

    // The depth buffer owns the views and also the image memory.
    if (m_depth.view != nullptr)
    {
        vkDestroyImageView(m_device_vk->Handle(), m_depth.view, nullptr);
        m_depth.view = nullptr;
    }
    if (m_depth.image != nullptr)
    {
        vkDestroyImage(m_device_vk->Handle(), m_depth.image, nullptr);
        m_depth.image = nullptr;
    }
    if (m_depth.memory != nullptr)
    {
        vkFreeMemory(m_device_vk->Handle(), m_depth.memory, nullptr);
        m_depth.memory = nullptr;
    }

    m_device_vk = nullptr;
}

} // MrQ2
