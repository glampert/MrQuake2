//
// DeviceVK.hpp
//
#pragma once

#include "UtilsVK.hpp"
#include <vector>

namespace MrQ2
{

class SwapChainVK;
class UploadContextVK;
class GraphicsContextVK;

class DeviceVK final
{
public:

    DeviceVK() = default;

    // Disallow copy.
    DeviceVK(const DeviceVK &) = delete;
    DeviceVK & operator=(const DeviceVK &) = delete;

    void Init(const SwapChainVK & sc, const bool debug, UploadContextVK & up_ctx, GraphicsContextVK & gfx_ctx);
    void Shutdown();

    // Public to renderers/common
    UploadContextVK   & UploadContext()   const { return *m_upload_ctx;   }
    GraphicsContextVK & GraphicsContext() const { return *m_graphics_ctx; }

private:

    UploadContextVK *   m_upload_ctx{ nullptr };
    GraphicsContextVK * m_graphics_ctx{ nullptr };
    VkInstance          m_instance{ nullptr };

    // With Vulkan debug validation layers?
    bool m_debug_validation{ false };

    // Layers and extensions available for the VK Instance.
    struct LayerProperties
    {
        VkLayerProperties properties{};
        std::vector<VkExtensionProperties> extensions;
    };
    std::vector<LayerProperties> m_instance_layer_properties;

    void InitInstanceLayerProperties();
    void InitInstanceExtensionProperties(LayerProperties & layer_props);
    void InitInstance();
};

} // MrQ2
