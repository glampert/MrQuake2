//
// DeviceVK.cpp
//

#include "DeviceVK.hpp"

namespace MrQ2
{

void DeviceVK::Init(const SwapChainVK & sc, const bool debug, UploadContextVK & up_ctx, GraphicsContextVK & gfx_ctx)
{
    m_upload_ctx       = &up_ctx;
    m_graphics_ctx     = &gfx_ctx;
    m_debug_validation = debug;

    InitInstanceLayerProperties();
    InitInstance();
}

void DeviceVK::Shutdown()
{
    if (m_instance != nullptr)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = nullptr;
    }

    m_upload_ctx   = nullptr;
    m_graphics_ctx = nullptr;
    m_instance_layer_properties.clear();
}

void DeviceVK::InitInstanceLayerProperties()
{
    std::vector<VkLayerProperties> layer_properties;
    std::uint32_t instance_layer_count;
    VkResult res;

    //
    // It's possible, though very rare, that the number of
    // instance layers could change. For example, installing something
    // could include new layers that the loader would pick up
    // between the initial query for the count and the
    // request for VkLayerProperties. The loader indicates that
    // by returning a VK_INCOMPLETE status and will update the
    // the count parameter.
    // The count parameter will be updated with the number of
    // entries loaded into the data pointer - in case the number
    // of layers went down or is smaller than the size given.
    //
    do {
        instance_layer_count = 0;
        res = vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);
        if (res != VK_SUCCESS || instance_layer_count == 0)
        {
            return;
        }

        layer_properties.resize(instance_layer_count);
        res = vkEnumerateInstanceLayerProperties(&instance_layer_count, layer_properties.data());
    } while (res == VK_INCOMPLETE);

    // Now gather the extension list for each instance layer:
    for (std::uint32_t l = 0; l < instance_layer_count; ++l)
    {
        LayerProperties my_layer_props{};
        my_layer_props.properties = layer_properties[l];

        InitInstanceExtensionProperties(my_layer_props);
        GameInterface::Printf("Vulkan layer available: %s", my_layer_props.properties.layerName);

        m_instance_layer_properties.emplace_back(std::move(my_layer_props));
    }
}

void DeviceVK::InitInstanceExtensionProperties(LayerProperties & layer_props)
{
    VkResult res;
    do {
        const char * const layer_name = layer_props.properties.layerName;
        std::uint32_t instance_extension_count = 0;

        res = vkEnumerateInstanceExtensionProperties(layer_name, &instance_extension_count, nullptr);
        if (res != VK_SUCCESS || instance_extension_count == 0)
        {
            return;
        }

        layer_props.extensions.resize(instance_extension_count);
        auto instance_extensions = layer_props.extensions.data();

        res = vkEnumerateInstanceExtensionProperties(layer_name, &instance_extension_count, instance_extensions);
    } while (res == VK_INCOMPLETE);
}

void DeviceVK::InitInstance()
{
    const char * const * instance_layer_names;
    std::uint32_t instance_layer_count;

    if (m_debug_validation)
    {
        if (Config::r_renderdoc.IsSet())
        {
            GameInterface::Printf("Creating VK Instance with debug validation + RenderDoc.");

            static const char * const s_instance_layer_names_debug_rdoc[] = {
                "VK_LAYER_LUNARG_standard_validation",
                "VK_LAYER_KHRONOS_validation",
                "VK_LAYER_RENDERDOC_Capture"
            };
            instance_layer_names = s_instance_layer_names_debug_rdoc;
            instance_layer_count = ArrayLength(s_instance_layer_names_debug_rdoc);
        }
        else
        {
            GameInterface::Printf("Creating VK Instance with debug validation.");

            static const char * const s_instance_layer_names_debug[] = {
                "VK_LAYER_LUNARG_standard_validation",
                "VK_LAYER_KHRONOS_validation"
            };
            instance_layer_names = s_instance_layer_names_debug;
            instance_layer_count = ArrayLength(s_instance_layer_names_debug);
        }
    }
    else
    {
        GameInterface::Printf("Creating VK Instance without validation (Release mode).");
        instance_layer_names = nullptr;
        instance_layer_count = 0;
    }

    static const char * const s_instance_extension_names[] = {
        "VK_KHR_surface",
        "VK_KHR_win32_surface"
    };

    VkApplicationInfo app_info  = {};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = "MrQuake2VK";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName        = "MrQuake2VK";
    app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion         = VK_API_VERSION_1_0;

    VkInstanceCreateInfo inst_info    = {};
    inst_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.flags                   = 0;
    inst_info.pApplicationInfo        = &app_info;
    inst_info.enabledLayerCount       = instance_layer_count;
    inst_info.ppEnabledLayerNames     = instance_layer_names;
    inst_info.enabledExtensionCount   = ArrayLength(s_instance_extension_names);
    inst_info.ppEnabledExtensionNames = s_instance_extension_names;

    VULKAN_CHECK(vkCreateInstance(&inst_info, nullptr, &m_instance));
    MRQ2_ASSERT(m_instance != nullptr);

    GameInterface::Printf("VK Instance created.");
}

} // namespace MrQ2
