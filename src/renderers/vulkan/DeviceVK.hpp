//
// DeviceVK.hpp
//
#pragma once

#include "UtilsVK.hpp"
#include <vector>

namespace MrQ2
{

class Win32Window;
class UploadContextVK;
class GraphicsContextVK;
class SwapChainRenderTargetsVK;

class DeviceVK final
{
public:

    DeviceVK() = default;
    ~DeviceVK() { Shutdown(); }

    // Disallow copy.
    DeviceVK(const DeviceVK &) = delete;
    DeviceVK & operator=(const DeviceVK &) = delete;

    void Init(Win32Window & window, UploadContextVK & up_ctx, GraphicsContextVK & gfx_ctx, SwapChainRenderTargetsVK & rts, const bool debug);
    void Shutdown();

    // Public to renderers/common
    UploadContextVK   &        UploadContext()          const { return *m_upload_ctx; }
    GraphicsContextVK &        GraphicsContext()        const { return *m_graphics_ctx; }
    SwapChainRenderTargetsVK & ScRenderTargets()        const { return *m_render_targets; }
    bool                       DebugValidationEnabled() const { return m_debug_validation; }

    // VK public handles
    VkDevice         Handle()              const { return m_device_handle; }
    VkPhysicalDevice PhysDevice()          const { return m_physical_devices[0]; }
    VkSurfaceKHR     RenderSurface()       const { return m_render_surface; }
    VkFormat         RenderSurfaceFormat() const { return m_render_surface_format; }
    const auto &     GraphicsQueue()       const { return m_graphics_queue; }
    const auto &     PresentQueue()        const { return m_present_queue; }
    const auto &     DeviceInfo()          const { return m_device_info; }

    // Extension function pointers
    PFN_vkCmdPushDescriptorSetKHR    pVkCmdPushDescriptorSetKHR{ nullptr };
    PFN_vkCmdBeginDebugUtilsLabelEXT pVkCmdBeginDebugUtilsLabelEXT{ nullptr };
    PFN_vkCmdEndDebugUtilsLabelEXT   pVkCmdEndDebugUtilsLabelEXT{ nullptr };
    PFN_vkSetDebugUtilsObjectNameEXT pVkSetDebugUtilsObjectNameEXT{ nullptr };

    template<typename VkHandleT>
    void SetObjectDebugName(const VkObjectType type, VkHandleT handle, const char * name) const
    {
        if (pVkSetDebugUtilsObjectNameEXT != nullptr)
        {
            VkDebugUtilsObjectNameInfoEXT name_info{};
            name_info.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            name_info.objectType   = type;
            name_info.objectHandle = (uint64_t)handle;
            name_info.pObjectName  = name;
            pVkSetDebugUtilsObjectNameEXT(m_device_handle, &name_info);
        }
    }

private:

    struct LayerProperties
    {
        VkLayerProperties properties{};
        std::vector<VkExtensionProperties> extensions;
    };

    struct DeviceHWInfo
    {
        VkPhysicalDeviceFeatures2 features2{};
        VkPhysicalDeviceProperties2 properties2{};
        VkPhysicalDeviceMemoryProperties memoryProperties{};
    };

    struct DeviceQueueInfo
    {
        VkQueue queue_handle{ nullptr };
        std::int32_t family_index{ -1 };
    };

    void InitInstanceLayerProperties();
    void InitInstanceExtensionProperties(LayerProperties & layer_props);
    void InitInstance();
    void EnumerateDevices();
    void InitSwapChainExtensions(Win32Window & window);
    void InitDevice();
    void InitDebugExtensions();

private:

    UploadContextVK *                    m_upload_ctx{ nullptr };
    GraphicsContextVK *                  m_graphics_ctx{ nullptr };
    SwapChainRenderTargetsVK *           m_render_targets{ nullptr };

    VkInstance                           m_instance_handle{ nullptr };
    VkDevice                             m_device_handle{ nullptr };
    VkSurfaceKHR                         m_render_surface{ nullptr };                    // Render target surface for the rendering window (screen framebuffer).
    VkFormat                             m_render_surface_format{ VK_FORMAT_UNDEFINED }; // Texture format of the rendering surface (the framebuffer).
    bool                                 m_debug_validation{ false };                    // With Vulkan debug validation layers?

    // Queues
    DeviceQueueInfo                      m_present_queue;
    DeviceQueueInfo                      m_graphics_queue;

    // Device info
    std::vector<LayerProperties>         m_instance_layer_properties; // Layers and extensions available for the VK Instance.
    std::vector<VkPhysicalDevice>        m_physical_devices;          // Info about the physical GPUs. We only care about GPU0 for now.
    std::vector<VkQueueFamilyProperties> m_queue_family_properties;   // Info about the graphics, compute and present queues available in the GPU.
    DeviceHWInfo                         m_device_info;
};

} // MrQ2
