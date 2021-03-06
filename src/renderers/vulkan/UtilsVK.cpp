//
// UtilsVK.cpp
//

#include "UtilsVK.hpp"
#include "DeviceVK.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////

// Printable null-terminated C-string from the VkResult result code.
const char * VulkanResultToString(const VkResult result)
{
#define CASE_(x) case x : return #x
    switch (result)
    {
    CASE_(VK_SUCCESS);
    CASE_(VK_NOT_READY);
    CASE_(VK_TIMEOUT);
    CASE_(VK_EVENT_SET);
    CASE_(VK_EVENT_RESET);
    CASE_(VK_INCOMPLETE);
    CASE_(VK_ERROR_OUT_OF_HOST_MEMORY);
    CASE_(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    CASE_(VK_ERROR_INITIALIZATION_FAILED);
    CASE_(VK_ERROR_DEVICE_LOST);
    CASE_(VK_ERROR_MEMORY_MAP_FAILED);
    CASE_(VK_ERROR_LAYER_NOT_PRESENT);
    CASE_(VK_ERROR_EXTENSION_NOT_PRESENT);
    CASE_(VK_ERROR_FEATURE_NOT_PRESENT);
    CASE_(VK_ERROR_INCOMPATIBLE_DRIVER);
    CASE_(VK_ERROR_TOO_MANY_OBJECTS);
    CASE_(VK_ERROR_FORMAT_NOT_SUPPORTED);
    CASE_(VK_ERROR_FRAGMENTED_POOL);
    CASE_(VK_ERROR_SURFACE_LOST_KHR);
    CASE_(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    CASE_(VK_SUBOPTIMAL_KHR);
    CASE_(VK_ERROR_OUT_OF_DATE_KHR);
    CASE_(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
    CASE_(VK_ERROR_VALIDATION_FAILED_EXT);
    CASE_(VK_ERROR_INVALID_SHADER_NV);
    CASE_(VK_ERROR_OUT_OF_POOL_MEMORY);
    CASE_(VK_ERROR_INVALID_EXTERNAL_HANDLE);
    CASE_(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
    CASE_(VK_ERROR_FRAGMENTATION_EXT);
    CASE_(VK_ERROR_NOT_PERMITTED_EXT);
    CASE_(VK_ERROR_INVALID_DEVICE_ADDRESS_EXT);
    CASE_(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
    default : return "UNKNOWN_VK_RESULT";
    } // switch
#undef CASE_
}

///////////////////////////////////////////////////////////////////////////////

uint32_t VulkanMemoryTypeFromProperties(const DeviceVK & device, const uint32_t type_bits, const VkFlags requirements_mask)
{
    const auto & device_info = device.DeviceInfo();

    // Search mem types to find first index with those properties
    auto bits = type_bits;
    for (uint32_t i = 0; i < device_info.memoryProperties.memoryTypeCount; ++i)
    {
        if (bits & 1)
        {
            // Type is available, does it match user properties?
            if ((device_info.memoryProperties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask)
            {
                return i;
            }
        }
        bits >>= 1;
    }

    // No memory types matched, return failure
    GameInterface::Errorf("Unable to find index for requested memory type %#x, with mask %#x", type_bits, requirements_mask);
}

///////////////////////////////////////////////////////////////////////////////

void VulkanChangeImageLayout(CommandBufferVK & cmd_buff, VkImage image, const VkImageAspectFlags aspect_mask,
                             const VkImageLayout old_image_layout, const VkImageLayout new_image_layout,
                             const uint32_t base_mip_level, const uint32_t mip_level_count,
                             const uint32_t base_layer, const uint32_t layer_count)
{
    MRQ2_ASSERT(image != nullptr);
    MRQ2_ASSERT(cmd_buff.IsInRecordingState());

    VkImageMemoryBarrier image_mem_barrier{};
    image_mem_barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_mem_barrier.oldLayout                       = old_image_layout;
    image_mem_barrier.newLayout                       = new_image_layout;
    image_mem_barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    image_mem_barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    image_mem_barrier.image                           = image;
    image_mem_barrier.subresourceRange.aspectMask     = aspect_mask;
    image_mem_barrier.subresourceRange.baseMipLevel   = base_mip_level;
    image_mem_barrier.subresourceRange.levelCount     = mip_level_count;
    image_mem_barrier.subresourceRange.baseArrayLayer = base_layer;
    image_mem_barrier.subresourceRange.layerCount     = layer_count;

    VkPipelineStageFlags src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (old_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        image_mem_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        image_mem_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        image_mem_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }
    if (old_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        image_mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    if (old_image_layout == VK_IMAGE_LAYOUT_PREINITIALIZED)
    {
        image_mem_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    }
    if (new_image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        image_mem_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    if (new_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        image_mem_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    if (new_image_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        image_mem_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dst_stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    }

    vkCmdPipelineBarrier(
        /* commandBuffer            = */ cmd_buff.Handle(),
        /* srcStageMask             = */ src_stage_mask,
        /* dstStageMask             = */ dst_stage_mask,
        /* dependencyFlags          = */ 0,
        /* memoryBarrierCount       = */ 0,
        /* pMemoryBarriers          = */ nullptr,
        /* bufferMemoryBarrierCount = */ 0,
        /* pBufferMemoryBarriers    = */ nullptr,
        /* imageMemoryBarrierCount  = */ 1,
        /* pImageBarriers           = */ &image_mem_barrier);
}

///////////////////////////////////////////////////////////////////////////////

void VulkanAllocateImage(const DeviceVK & device, const VkImageCreateInfo & image_info,
                         const VkMemoryPropertyFlags memory_properties,
                         VkImage * out_image, VkDeviceMemory * out_image_memory)
{
    MRQ2_ASSERT(out_image != nullptr && out_image_memory != nullptr);
    VULKAN_CHECK(vkCreateImage(device.Handle(), &image_info, nullptr, out_image));

    VkMemoryRequirements mem_requirements{};
    vkGetImageMemoryRequirements(device.Handle(), *out_image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize  = mem_requirements.size;
    alloc_info.memoryTypeIndex = VulkanMemoryTypeFromProperties(device, mem_requirements.memoryTypeBits, memory_properties);
    MRQ2_ASSERT(alloc_info.memoryTypeIndex < UINT32_MAX);

    VULKAN_CHECK(vkAllocateMemory(device.Handle(), &alloc_info, nullptr, out_image_memory));
    VULKAN_CHECK(vkBindImageMemory(device.Handle(), *out_image, *out_image_memory, 0));
}

///////////////////////////////////////////////////////////////////////////////

void VulkanCopyBuffer(CommandBufferVK & cmd_buff,
                      VkBuffer src_buff, VkBuffer dst_buff,
                      const VkDeviceSize size_to_copy,
                      const VkDeviceSize src_offset,
                      const VkDeviceSize dst_offset)
{
    MRQ2_ASSERT(size_to_copy != 0);
    MRQ2_ASSERT(src_buff != nullptr && dst_buff != nullptr);
    MRQ2_ASSERT(cmd_buff.IsInRecordingState());

    VkBufferCopy copy_region{};
    copy_region.srcOffset = src_offset;
    copy_region.dstOffset = dst_offset;
    copy_region.size      = size_to_copy;
    vkCmdCopyBuffer(cmd_buff.Handle(), src_buff, dst_buff, 1, &copy_region);
}

///////////////////////////////////////////////////////////////////////////////

void VulkanAllocateBuffer(const DeviceVK & device, const VkDeviceSize size_bytes, const VkBufferUsageFlags usage,
                          const VkMemoryPropertyFlags memory_properties, VkBuffer * out_buff,
                          VkDeviceMemory * out_buff_memory, VkMemoryRequirements * out_opt_mem_reqs)
{
    MRQ2_ASSERT(size_bytes != 0);
    MRQ2_ASSERT(out_buff != nullptr && out_buff_memory != nullptr);

    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size        = size_bytes;
    buffer_create_info.usage       = usage;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VULKAN_CHECK(vkCreateBuffer(device.Handle(), &buffer_create_info, nullptr, out_buff));

    VkMemoryRequirements mem_requirements{};
    vkGetBufferMemoryRequirements(device.Handle(), *out_buff, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize  = mem_requirements.size;
    alloc_info.memoryTypeIndex = VulkanMemoryTypeFromProperties(device, mem_requirements.memoryTypeBits, memory_properties);
    MRQ2_ASSERT(alloc_info.memoryTypeIndex < UINT32_MAX);

    VULKAN_CHECK(vkAllocateMemory(device.Handle(), &alloc_info, nullptr, out_buff_memory));
    VULKAN_CHECK(vkBindBufferMemory(device.Handle(), *out_buff, *out_buff_memory, 0));

    if (out_opt_mem_reqs != nullptr)
    {
        *out_opt_mem_reqs = mem_requirements;
    }
}

///////////////////////////////////////////////////////////////////////////////
// FenceVK
///////////////////////////////////////////////////////////////////////////////

void FenceVK::Init(const DeviceVK & device, const VkFenceCreateFlags flags)
{
    MRQ2_ASSERT(m_fence_handle == nullptr); // Prevent double init

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = flags;

    VULKAN_CHECK(vkCreateFence(device.Handle(), &fence_create_info, nullptr, &m_fence_handle));
    MRQ2_ASSERT(m_fence_handle != nullptr);

    m_device_vk = &device;
}

void FenceVK::Shutdown()
{
    if (m_fence_handle != nullptr)
    {
        vkDestroyFence(m_device_vk->Handle(), m_fence_handle, nullptr);
        m_fence_handle = nullptr;
    }

    m_device_vk = nullptr;
}

void FenceVK::Reset()
{
    MRQ2_ASSERT(m_fence_handle != nullptr);
    VULKAN_CHECK(vkResetFences(m_device_vk->Handle(), 1, &m_fence_handle));
}

void FenceVK::Wait()
{
    constexpr uint64_t kInfiniteWaitTimeout = UINT64_MAX;

    MRQ2_ASSERT(m_fence_handle != nullptr);
    VULKAN_CHECK(vkWaitForFences(m_device_vk->Handle(), 1, &m_fence_handle, VK_TRUE, kInfiniteWaitTimeout));
}

bool FenceVK::IsSignaled() const
{
    MRQ2_ASSERT(m_fence_handle != nullptr);
    const VkResult res = vkGetFenceStatus(m_device_vk->Handle(), m_fence_handle);

    if (res == VK_SUCCESS)
    {
        return true;
    }
    else if (res == VK_NOT_READY)
    {
        return false;
    }
    else
    {
        GameInterface::Errorf("vkGetFenceStatus() failed with error (%#x): %s", res, VulkanResultToString(res));
    }
}

///////////////////////////////////////////////////////////////////////////////
// CommandBufferPoolVK
///////////////////////////////////////////////////////////////////////////////

void CommandBufferPoolVK::Init(const DeviceVK & device)
{
    MRQ2_ASSERT(m_pool_handle == nullptr); // Prevent double init

    VkCommandPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_create_info.queueFamilyIndex = device.GraphicsQueue().family_index;

    VULKAN_CHECK(vkCreateCommandPool(device.Handle(), &pool_create_info, nullptr, &m_pool_handle));
    MRQ2_ASSERT(m_pool_handle != nullptr);

    m_device_vk = &device;
}

void CommandBufferPoolVK::Shutdown()
{
    if (m_pool_handle != nullptr)
    {
        vkDestroyCommandPool(m_device_vk->Handle(), m_pool_handle, nullptr);
        m_pool_handle = nullptr;
    }

    m_device_vk = nullptr;
}

void CommandBufferPoolVK::Reset()
{
    MRQ2_ASSERT(m_pool_handle != nullptr);
    VULKAN_CHECK(vkResetCommandPool(m_device_vk->Handle(), m_pool_handle, 0));
}

///////////////////////////////////////////////////////////////////////////////
// CommandBufferVK
///////////////////////////////////////////////////////////////////////////////

void CommandBufferVK::Init(const DeviceVK & device, const VkFenceCreateFlags fence_create_flags)
{
    MRQ2_ASSERT(m_cmd_buffer_handle == nullptr); // Prevent double init

    // Buffer pool
    m_cmd_pool.Init(device);

    // Fence
    m_fence.Init(device, fence_create_flags);

    // Command buffer
    VkCommandBufferAllocateInfo cmd_buffer_alloc_info{};
    cmd_buffer_alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_alloc_info.commandPool        = m_cmd_pool.Handle();
    cmd_buffer_alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_alloc_info.commandBufferCount = 1;

    VULKAN_CHECK(vkAllocateCommandBuffers(device.Handle(), &cmd_buffer_alloc_info, &m_cmd_buffer_handle));
    MRQ2_ASSERT(m_cmd_buffer_handle != nullptr);

    m_device_vk   = &device;
    m_state_flags = kNoFlags;
}

void CommandBufferVK::Shutdown()
{
    if (m_cmd_buffer_handle != nullptr)
    {
        vkFreeCommandBuffers(m_device_vk->Handle(), m_cmd_pool.Handle(), 1, &m_cmd_buffer_handle);
        m_cmd_buffer_handle = nullptr;
    }

    m_fence.Shutdown();
    m_cmd_pool.Shutdown();
    m_device_vk = nullptr;
}

void CommandBufferVK::Reset()
{
    MRQ2_ASSERT(m_cmd_buffer_handle != nullptr);
    VULKAN_CHECK(vkResetCommandBuffer(m_cmd_buffer_handle, 0));

    m_fence.Reset();
    m_cmd_pool.Reset();

    m_state_flags = kNoFlags;
}

void CommandBufferVK::BeginRecording()
{
    MRQ2_ASSERT(m_cmd_buffer_handle != nullptr);
    MRQ2_ASSERT(!IsInRecordingState());

    VkCommandBufferBeginInfo cmd_buf_begin_info{};
    cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    VULKAN_CHECK(vkBeginCommandBuffer(m_cmd_buffer_handle, &cmd_buf_begin_info));
    m_state_flags = kFlagRecordingState;
}

void CommandBufferVK::EndRecording()
{
    MRQ2_ASSERT(m_cmd_buffer_handle != nullptr);
    MRQ2_ASSERT(!IsInSubmissionState() && IsInRecordingState());

    VULKAN_CHECK(vkEndCommandBuffer(m_cmd_buffer_handle));
    m_state_flags = kFlagSubmissionState;
}

void CommandBufferVK::Submit()
{
    VkSubmitInfo submit_info{};
    submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers    = &m_cmd_buffer_handle;
    Submit(submit_info);
}

void CommandBufferVK::Submit(const VkSubmitInfo & submit_info)
{
    MRQ2_ASSERT(m_cmd_buffer_handle != nullptr);
    MRQ2_ASSERT(IsInSubmissionState());

    VkQueue gfx_queue = m_device_vk->GraphicsQueue().queue_handle;
    VULKAN_CHECK(vkQueueSubmit(gfx_queue, 1, &submit_info, m_fence.Handle()));
}

void CommandBufferVK::WaitComplete()
{
    MRQ2_ASSERT(!IsInRecordingState());
    m_fence.Wait();
}

///////////////////////////////////////////////////////////////////////////////
// RenderPassVK
///////////////////////////////////////////////////////////////////////////////

void RenderPassVK::Init(const DeviceVK & device, const VkRenderPassCreateInfo & create_info)
{
    MRQ2_ASSERT(m_pass_handle == nullptr); // Prevent double init

    VULKAN_CHECK(vkCreateRenderPass(device.Handle(), &create_info, nullptr, &m_pass_handle));
    MRQ2_ASSERT(m_pass_handle != nullptr);

    m_device_vk = &device;
}

void RenderPassVK::Shutdown()
{
    if (m_pass_handle != nullptr)
    {
        vkDestroyRenderPass(m_device_vk->Handle(), m_pass_handle, nullptr);
        m_pass_handle = nullptr;
    }

    m_device_vk = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// DescriptorSetVK
///////////////////////////////////////////////////////////////////////////////

void DescriptorSetVK::Init(const DeviceVK & device, ArrayBase<const VkDescriptorPoolSize> pool_sizes_and_types, ArrayBase<const VkDescriptorSetLayoutBinding> set_layout_bindings)
{
    MRQ2_ASSERT(m_device_vk == nullptr);
    MRQ2_ASSERT(!pool_sizes_and_types.empty() && !set_layout_bindings.empty());

    VkDescriptorPoolCreateInfo pool_create_info{};
    pool_create_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.maxSets       = 1;
    pool_create_info.poolSizeCount = pool_sizes_and_types.size();
    pool_create_info.pPoolSizes    = pool_sizes_and_types.data();

    VULKAN_CHECK(vkCreateDescriptorPool(device.Handle(), &pool_create_info, nullptr, &m_descriptor_pool_handle));
    MRQ2_ASSERT(m_descriptor_pool_handle != nullptr);

    VkDescriptorSetLayoutCreateInfo layout_create_info{};
    layout_create_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_create_info.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR; // We are using VK_KHR_push_descriptor;
    layout_create_info.bindingCount = set_layout_bindings.size();
    layout_create_info.pBindings    = set_layout_bindings.data();

    VULKAN_CHECK(vkCreateDescriptorSetLayout(device.Handle(), &layout_create_info, nullptr, &m_descriptor_set_layout_handle));
    MRQ2_ASSERT(m_descriptor_set_layout_handle != nullptr);

    VkDescriptorSetAllocateInfo set_alloc_info{};
    set_alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_alloc_info.descriptorPool     = m_descriptor_pool_handle;
    set_alloc_info.descriptorSetCount = 1;
    set_alloc_info.pSetLayouts        = &m_descriptor_set_layout_handle;

    m_device_vk = &device;
}

void DescriptorSetVK::Shutdown()
{
    if (m_device_vk == nullptr)
    {
        return;
    }

    if (m_descriptor_pool_handle != nullptr)
    {
        vkDestroyDescriptorPool(m_device_vk->Handle(), m_descriptor_pool_handle, nullptr);
        m_descriptor_pool_handle = nullptr;
    }

    if (m_descriptor_set_layout_handle != nullptr)
    {
        vkDestroyDescriptorSetLayout(m_device_vk->Handle(), m_descriptor_set_layout_handle, nullptr);
        m_descriptor_set_layout_handle = nullptr;
    }

    m_device_vk = nullptr;
}

} // MrQ2
