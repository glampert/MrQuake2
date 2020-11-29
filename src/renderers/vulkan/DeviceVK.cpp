//
// DeviceVK.cpp
//

// To enable vkCreateWin32SurfaceKHR extensions.
// Defined here before the include so vulkan.h exposes the structure.
#if !defined(VK_USE_PLATFORM_WIN32_KHR)
	#define VK_USE_PLATFORM_WIN32_KHR 1
#endif // !VK_USE_PLATFORM_WIN32_KHR

#include "DeviceVK.hpp"
#include "../common/Win32Window.hpp"

namespace MrQ2
{

void DeviceVK::Init(Win32Window & window, UploadContextVK & up_ctx, GraphicsContextVK & gfx_ctx, SwapChainRenderTargetsVK & rts, const bool debug)
{
    m_upload_ctx       = &up_ctx;
    m_graphics_ctx     = &gfx_ctx;
    m_render_targets   = &rts;
    m_debug_validation = debug;

    InitInstanceLayerProperties();
    InitInstance();
    EnumerateDevices();
    InitSwapChainExtensions(window);
    InitDevice();
    InitDebugExtensions();
}

void DeviceVK::Shutdown()
{
    if (m_device_handle != nullptr)
    {
        vkDestroyDevice(m_device_handle, nullptr);
        m_device_handle = nullptr;
    }

    if (m_render_surface != nullptr)
    {
        vkDestroySurfaceKHR(m_instance_handle, m_render_surface, nullptr);
        m_render_surface = nullptr;
    }

    if (m_instance_handle != nullptr)
    {
        vkDestroyInstance(m_instance_handle, nullptr);
        m_instance_handle = nullptr;
    }

    m_upload_ctx     = nullptr;
    m_graphics_ctx   = nullptr;
    m_render_targets = nullptr;

    m_instance_layer_properties.clear();
    m_physical_devices.clear();
    m_queue_family_properties.clear();
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
    const bool renderdoc = Config::r_renderdoc.IsSet();

    const char * const * instance_layer_names = nullptr;
    std::uint32_t instance_layer_count = 0;

    const char * const * instance_extension_names = nullptr;
    std::uint32_t instance_extension_count = 0;

    if (m_debug_validation)
    {
        if (renderdoc)
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

    if (renderdoc)
    {
        static const char * const s_instance_extension_names_debug_rdoc[] = {
            "VK_KHR_surface",
            "VK_KHR_win32_surface",
            "VK_EXT_debug_utils"
        };
        instance_extension_names = s_instance_extension_names_debug_rdoc;
        instance_extension_count = ArrayLength(s_instance_extension_names_debug_rdoc);
    }
    else
    {
        static const char * const s_instance_extension_names[] = {
            "VK_KHR_surface",
            "VK_KHR_win32_surface"
        };
        instance_extension_names = s_instance_extension_names;
        instance_extension_count = ArrayLength(s_instance_extension_names);
    }

    VkApplicationInfo app_info{};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = "MrQuake2VK";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName        = "MrQuake2VK";
    app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion         = VK_API_VERSION_1_1;

    VkInstanceCreateInfo inst_info{};
    inst_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pApplicationInfo        = &app_info;
    inst_info.enabledLayerCount       = instance_layer_count;
    inst_info.ppEnabledLayerNames     = instance_layer_names;
    inst_info.enabledExtensionCount   = instance_extension_count;
    inst_info.ppEnabledExtensionNames = instance_extension_names;

    VULKAN_CHECK(vkCreateInstance(&inst_info, nullptr, &m_instance_handle));
    MRQ2_ASSERT(m_instance_handle != nullptr);

    GameInterface::Printf("VK Instance created.");
}

void DeviceVK::EnumerateDevices()
{
    std::uint32_t num_devices = 0;
    VULKAN_CHECK(vkEnumeratePhysicalDevices(m_instance_handle, &num_devices, nullptr));
    MRQ2_ASSERT(num_devices >= 1);

    m_physical_devices.resize(num_devices);
    VULKAN_CHECK(vkEnumeratePhysicalDevices(m_instance_handle, &num_devices, m_physical_devices.data()));
    MRQ2_ASSERT(num_devices >= 1);

    std::uint32_t num_queue_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_devices[0], &num_queue_families, nullptr);
    MRQ2_ASSERT(num_queue_families >= 1);

    m_queue_family_properties.resize(num_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_devices[0], &num_queue_families, m_queue_family_properties.data());
    MRQ2_ASSERT(num_queue_families >= 1);

    m_device_info.features2.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    m_device_info.properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

    // NOTE: For now we only care about GPU 0 - no support for multi-GPU systems.
    vkGetPhysicalDeviceFeatures2(m_physical_devices[0], &m_device_info.features2);
    vkGetPhysicalDeviceProperties2(m_physical_devices[0], &m_device_info.properties2);
    vkGetPhysicalDeviceMemoryProperties(m_physical_devices[0], &m_device_info.memoryProperties);

    GameInterface::Printf("Found %u physical GPUs. Using GPU [0]...", num_devices);
    GameInterface::Printf("GPU 0 has %u queues", num_queue_families);
    GameInterface::Printf("GPU 0 name: %s", m_device_info.properties2.properties.deviceName);
}

void DeviceVK::InitSwapChainExtensions(Win32Window & window)
{
    MRQ2_ASSERT(!m_queue_family_properties.empty());
    MRQ2_ASSERT(!m_physical_devices.empty());

    // Construct the surface description:
    VkWin32SurfaceCreateInfoKHR surface_create_info = {};
    surface_create_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hinstance = window.AppInstance();
    surface_create_info.hwnd      = window.WindowHandle();

    VULKAN_CHECK(vkCreateWin32SurfaceKHR(m_instance_handle, &surface_create_info, nullptr, &m_render_surface));
    MRQ2_ASSERT(m_render_surface != nullptr);

    // Iterate over each queue to learn whether it supports presenting:
    std::vector<VkBool32> queues_supporting_present(m_queue_family_properties.size(), VK_FALSE);
    for (std::uint32_t q = 0; q < m_queue_family_properties.size(); ++q)
    {
        VULKAN_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_devices[0], q, m_render_surface, &queues_supporting_present[q]));
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both:
    for (std::uint32_t q = 0; q < m_queue_family_properties.size(); ++q)
    {
        GameInterface::Printf("Queue %u flags: %#x", q, m_queue_family_properties[q].queueFlags);

        if ((m_queue_family_properties[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            if (m_graphics_queue.family_index == -1)
            {
                m_graphics_queue.family_index = static_cast<std::int32_t>(q);
            }

            if (queues_supporting_present[q] == VK_TRUE)
            {
                m_present_queue.family_index  = static_cast<std::int32_t>(q);
                m_graphics_queue.family_index = static_cast<std::int32_t>(q);
                break;
            }
        }
    }

    // If didn't find a queue that supports both graphics and present,
    // then find a separate present queue.
    if (m_present_queue.family_index == -1)
    {
        for (std::uint32_t q = 0; q < m_queue_family_properties.size(); ++q)
        {
            if (queues_supporting_present[q] == VK_TRUE)
            {
                m_present_queue.family_index = static_cast<std::int32_t>(q);
                break;
            }
        }
    }

    // Error if could not find queues that support graphics and present.
    if (m_present_queue.family_index == -1 || m_graphics_queue.family_index == -1)
    {
        GameInterface::Errorf("Could not find a VK queue for graphics and present!");
    }

    // Get the list of VkFormats that are supported:
    std::uint32_t num_formats_supported = 0;
    VULKAN_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_devices[0], m_render_surface, &num_formats_supported, nullptr));
    MRQ2_ASSERT(num_formats_supported >= 1);

    std::vector<VkSurfaceFormatKHR> surface_formats(num_formats_supported);
    VULKAN_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_devices[0], m_render_surface, &num_formats_supported, surface_formats.data()));
    MRQ2_ASSERT(num_formats_supported >= 1);

    GameInterface::Printf("GPU 0 Present Queue family index: %i", m_present_queue.family_index);
    GameInterface::Printf("GPU 0 Graphics Queue family index: %i", m_graphics_queue.family_index);
    GameInterface::Printf("VK render surface formats supported: %u", num_formats_supported);

    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format. Otherwise, at least one
    // supported format will be returned.
    if (num_formats_supported == 1 && surface_formats[0].format == VK_FORMAT_UNDEFINED)
    {
        m_render_surface_format = VK_FORMAT_B8G8R8A8_UNORM;
    }
    else
    {
        m_render_surface_format = surface_formats[0].format;
    }
}

void DeviceVK::InitDevice()
{
    // Dump list of supported extensions
    if (m_debug_validation)
    {
        uint32_t ext_count = 0;
        vkEnumerateDeviceExtensionProperties(PhysDevice(), nullptr, &ext_count, nullptr);
        if (ext_count != 0)
        {
            GameInterface::Printf("------ VK Device extensions available ------");

            std::vector<VkExtensionProperties> extensions;
            extensions.resize(ext_count);

            if (vkEnumerateDeviceExtensionProperties(PhysDevice(), nullptr, &ext_count, extensions.data()) == VK_SUCCESS)
            {
                for (const auto & ext : extensions)
                {
                    GameInterface::Printf("%s", ext.extensionName);
                }
            }

            GameInterface::Printf("--------------------------------------------");
        }
    }

    static const char * const s_device_extension_names[] = {
        "VK_KHR_swapchain",
        "VK_KHR_push_descriptor"
    };

    // Dummy priorities - don't care atm
    const float queue_priorities[] = { 0.0f, 0.0f };

    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueCount       = 1;
    queue_create_info.pQueuePriorities = queue_priorities;
    queue_create_info.queueFamilyIndex = m_graphics_queue.family_index;

    VkDeviceCreateInfo device_create_info      = {};
    device_create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount    = 1;
    device_create_info.pQueueCreateInfos       = &queue_create_info;
    device_create_info.enabledExtensionCount   = ArrayLength(s_device_extension_names);
    device_create_info.ppEnabledExtensionNames = s_device_extension_names;
    device_create_info.pEnabledFeatures        = &m_device_info.features2.features;

    VULKAN_CHECK(vkCreateDevice(m_physical_devices[0], &device_create_info, nullptr, &m_device_handle));
    MRQ2_ASSERT(m_device_handle != nullptr);

    GameInterface::Printf("VK Device created for GPU 0.");

    // Get the GPU queue handles:
    vkGetDeviceQueue(m_device_handle, m_graphics_queue.family_index, 0, &m_graphics_queue.queue_handle);
    MRQ2_ASSERT(m_graphics_queue.queue_handle != nullptr);

    if (m_graphics_queue.family_index == m_present_queue.family_index)
    {
        m_present_queue.queue_handle = m_graphics_queue.queue_handle;
        GameInterface::Printf("Graphics and present queues are the same.");
    }
    else
    {
        vkGetDeviceQueue(m_device_handle, m_present_queue.family_index, 0, &m_present_queue.queue_handle);
        MRQ2_ASSERT(m_present_queue.queue_handle != nullptr);
    }

    pVkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(m_device_handle, "vkCmdPushDescriptorSetKHR");
    if (pVkCmdPushDescriptorSetKHR == nullptr)
    {
        GameInterface::Errorf("Could not get a valid function pointer for vkCmdPushDescriptorSetKHR");
    }
}

void DeviceVK::InitDebugExtensions()
{
    if (Config::r_renderdoc.IsSet())
    {
        // VK_EXT_debug_utils is not part of the core, so function pointers need to be loaded manually
        pVkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(m_device_handle, "vkCmdBeginDebugUtilsLabelEXT");
        pVkCmdEndDebugUtilsLabelEXT   = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(m_device_handle,   "vkCmdEndDebugUtilsLabelEXT");
        pVkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(m_device_handle, "vkSetDebugUtilsObjectNameEXT");
    }
}

} // namespace MrQ2
