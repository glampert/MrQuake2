//
// UtilsVK.hpp
//
#pragma once

#include "../common/Common.hpp"
#include <vulkan/vulkan.h>

namespace MrQ2
{

// Internal helper types
class DeviceVK;
class FenceVK;
class CommandBufferPoolVK;
class CommandBufferVK;
class RenderPassVK;

// Triple-buffering
constexpr uint32_t kVkNumFrameBuffers = 3;

enum class PrimitiveTopologyVK : uint8_t
{
    kTriangleList,
    kTriangleStrip,
    kTriangleFan,
    kLineList,

    kCount
};

const char * VulkanResultToString(const VkResult result);

uint32_t VulkanMemoryTypeFromProperties(const DeviceVK & device, const uint32_t type_bits, const VkFlags requirements_mask);

void VulkanChangeImageLayout(CommandBufferVK & cmdBuff, VkImage image, const VkImageAspectFlags aspect_mask,
                             const VkImageLayout old_image_layout, const VkImageLayout new_image_layout,
                             const int base_mip_level = 0, const int mip_level_count = 1,
                             const int base_layer = 0, const int layer_count = 1);

inline void VulkanCheckImpl(const VkResult result, const char * const msg, const char * const file, const int line)
{
    if (result != VK_SUCCESS)
    {
        GameInterface::Errorf("Vulkan Error 0x%x [%s]: %s - %s(%d)", (unsigned)result, VulkanResultToString(result), msg, file, line);
    }
}

#define VULKAN_CHECK(expr) VulkanCheckImpl((expr), #expr, __FILE__, __LINE__)

///////////////////////////////////////////////////////////////////////////////
// Vulkan helper types
///////////////////////////////////////////////////////////////////////////////

class FenceVK final
{
public:

    FenceVK() = default;
    ~FenceVK() { Shutdown(); }

    FenceVK(const FenceVK &) = delete;
    FenceVK & operator=(const FenceVK &) = delete;

    void Init(const DeviceVK & device, const VkFenceCreateFlags flags = 0);
    void Shutdown();
    void Reset();

    void Wait();
    bool IsSignaled() const;

    VkFence Handle() const { return m_fence_handle; }

private:

    const DeviceVK * m_device_vk{ nullptr };
    VkFence          m_fence_handle{ nullptr };
};

class CommandBufferPoolVK final
{
public:

    CommandBufferPoolVK() = default;
    ~CommandBufferPoolVK() { Shutdown(); }

    CommandBufferPoolVK(const CommandBufferPoolVK &) = delete;
    CommandBufferPoolVK & operator=(const CommandBufferPoolVK &) = delete;

    void Init(const DeviceVK & device);
    void Shutdown();
    void Reset();

    VkCommandPool Handle() const { return m_pool_handle; }

private:

    const DeviceVK * m_device_vk{ nullptr };
    VkCommandPool    m_pool_handle{ nullptr };
};

class CommandBufferVK final
{
public:

    CommandBufferVK() = default;
    ~CommandBufferVK() { Shutdown(); }

    CommandBufferVK(const CommandBufferVK &) = delete;
    CommandBufferVK & operator=(const CommandBufferVK &) = delete;

    void Init(const DeviceVK & device, const VkFenceCreateFlags fence_create_flags = 0);
    void Shutdown();
    void Reset();

    // Command recording.
    void BeginRecording();
    void EndRecording();

    bool IsInRecordingState()  const { return (m_state_flags & kFlagRecordingState)  != 0; }
    bool IsInSubmissionState() const { return (m_state_flags & kFlagSubmissionState) != 0; }

    // Submit/execute previously recorded buffer without waiting.
    void Submit();
    void Submit(const VkSubmitInfo & submit_info);

    // Wait on a fence blocking until all commands in the buffer have executed.
    void WaitComplete();

    VkCommandBuffer Handle() const { return m_cmd_buffer_handle; }

private:

    enum Flags : uint32_t
    {
        kNoFlags = 0,
        kFlagRecordingState  = (1 << 1), // Between vkBeginCommandBuffer and vkEndCommandBuffer
        kFlagSubmissionState = (1 << 2), // After vkEndCommandBuffer
    };

    const DeviceVK *    m_device_vk{ nullptr };
    VkCommandBuffer     m_cmd_buffer_handle{ nullptr };
    CommandBufferPoolVK m_cmd_pool;
    FenceVK             m_fence;
    Flags               m_state_flags{ kNoFlags };
};

class RenderPassVK final
{
public:

    RenderPassVK() = default;
    ~RenderPassVK() { Shutdown(); }

    RenderPassVK(const RenderPassVK &) = delete;
    RenderPassVK & operator=(const RenderPassVK &) = delete;

    void Init(const DeviceVK & device, const VkRenderPassCreateInfo & create_info);
    void Shutdown();

    VkRenderPass Handle() const { return m_pass_handle; }

private:

    const DeviceVK * m_device_vk{ nullptr };
    VkRenderPass     m_pass_handle{ nullptr };
};

} // MrQ2
