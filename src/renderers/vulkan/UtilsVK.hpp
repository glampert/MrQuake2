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

// Clamp the input value between min and max (inclusive range).
template<typename T>
inline void Clamp(T * inOutVal, const T minVal, const T maxVal)
{
    if      ((*inOutVal) < minVal) { (*inOutVal) = minVal; }
    else if ((*inOutVal) > maxVal) { (*inOutVal) = maxVal; }
}

// Printable null-terminated C-string from the VkResult result code.
const char * VulkanResultToString(const VkResult result);

inline void VulkanCheckImpl(const VkResult result, const char * const msg, const char * const file, const int line)
{
    if (result != VK_SUCCESS)
    {
        GameInterface::Errorf("Vulkan Error 0x%x [%s]: %s - %s(%d)", (unsigned)result, VulkanResultToString(result), msg, file, line);
    }
}

#define VULKAN_CHECK(expr) VulkanCheckImpl((expr), #expr, __FILE__, __LINE__)

} // MrQ2
