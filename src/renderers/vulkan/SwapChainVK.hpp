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

    std::uint32_t    buffer_index{ 0 };
    std::uint32_t    buffer_count{ 0 };
    VkExtent2D       swap_chain_extents{};
    VkSwapchainKHR   swap_chain_handle{ nullptr };
    const DeviceVK * device_vk{ nullptr };

    SwapChainVK() = default;

    // Disallow copy.
    SwapChainVK(const SwapChainVK &) = delete;
    SwapChainVK & operator=(const SwapChainVK &) = delete;

    void Init(const DeviceVK & device, std::uint32_t width, std::uint32_t height, SwapChainRenderTargetsVK & rts);
    void Shutdown();
    void Present();
};

class SwapChainRenderTargetsVK final
{
public:

    int render_target_width{ 0 };
    int render_target_height{ 0 };
    const DeviceVK * device_vk{ nullptr };

    struct DepthBuffer
    {
        VkImage image{ nullptr };
        VkImageView view{ nullptr };
        VkDeviceMemory memory{ nullptr };
    } depth;

    struct FrameBuffer
    {
        VkImage image{ nullptr };
        VkImageView view{ nullptr };
        VkFramebuffer framebuffer_handle{ nullptr };
    } fb[kVkNumFrameBuffers];

    SwapChainRenderTargetsVK() = default;

    // Disallow copy.
    SwapChainRenderTargetsVK(const SwapChainRenderTargetsVK &) = delete;
    SwapChainRenderTargetsVK & operator=(const SwapChainRenderTargetsVK &) = delete;

    void Init(const SwapChainVK & sc);
    void Shutdown();
};

} // MrQ2
