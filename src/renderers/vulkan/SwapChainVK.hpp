//
// SwapChainVK.hpp
//
#pragma once

#include "UtilsVK.hpp"

namespace MrQ2
{

class DeviceVK;
class SwapChainRenderTargetsVK;

class SwapChainVK final
{
public:

    SwapChainVK() = default;
    ~SwapChainVK() { Shutdown(); }

    // Disallow copy.
    SwapChainVK(const SwapChainVK &) = delete;
    SwapChainVK & operator=(const SwapChainVK &) = delete;

    void Init(const DeviceVK & device, uint32_t width, uint32_t height, SwapChainRenderTargetsVK & rts);
    void Shutdown();
    void Present();

    VkSwapchainKHR     Handle()  const { return m_swap_chain_handle;  }
    const VkExtent2D & Extents() const { return m_swap_chain_extents; }

private:

    const DeviceVK * m_device_vk{ nullptr };
    uint32_t         m_buffer_index{ 0 };
    uint32_t         m_buffer_count{ 0 };
    VkExtent2D       m_swap_chain_extents{};
    VkSwapchainKHR   m_swap_chain_handle{ nullptr };
    VkSemaphore      m_image_available_semaphore{ nullptr };
    VkSemaphore      m_render_finished_semaphore{ nullptr };
};

class SwapChainRenderTargetsVK final
{
public:

    friend SwapChainVK;

    SwapChainRenderTargetsVK() = default;
    ~SwapChainRenderTargetsVK() { Shutdown(); }

    // Disallow copy.
    SwapChainRenderTargetsVK(const SwapChainRenderTargetsVK &) = delete;
    SwapChainRenderTargetsVK & operator=(const SwapChainRenderTargetsVK &) = delete;

    void Init(const DeviceVK & device, const SwapChainVK & sc);
    void Shutdown();

    int RenderTargetWidth()  const { return m_render_target_width;  }
    int RenderTargetHeight() const { return m_render_target_height; }

private:

    void InitDepthBuffer();
    void InitRenderPass();
    void InitFramebuffers();

    const DeviceVK * m_device_vk{ nullptr };
    RenderPassVK     m_main_render_pass;

    int m_render_target_width{ 0 };
    int m_render_target_height{ 0 };

    struct DepthBuffer
    {
        VkImage        image{ nullptr };
        VkImageView    view{ nullptr };
        VkDeviceMemory memory{ nullptr };
    } m_depth;

    struct FrameBuffer
    {
        VkImage        image{ nullptr };
        VkImageView    view{ nullptr };
        VkFramebuffer  framebuffer_handle{ nullptr };
    } m_fb[kVkNumFrameBuffers];
};

} // MrQ2
