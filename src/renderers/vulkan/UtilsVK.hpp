//
// UtilsVK.hpp
//
#pragma once

#include "../common/Common.hpp"
#include <vulkan/vulkan.h>

namespace MrQ2
{

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

inline void VulkanCheckImpl(const VkResult result, const char * const msg, const char * const file, const int line)
{
    if (result != VK_SUCCESS)
    {
        GameInterface::Errorf("Vulkan Error 0x%x: %s - %s(%d)", (unsigned)result, msg, file, line);
    }
}

#define VULKAN_CHECK(expr) VulkanCheckImpl((expr), #expr, __FILE__, __LINE__)

} // MrQ2
