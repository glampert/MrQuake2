//
// ShaderProgramVK.cpp
//

#include "ShaderProgramVK.hpp"
#include "DeviceVK.hpp"

#include <cstdio>
#include <vector>

namespace MrQ2
{

// Path from the project root where to find shaders for this renderer.
// NOTE: For Vulkan we're actually loading a precompiled binary shader
// so we have to point to the bin folder where the build results are outputted.
#if defined(_DEBUG)
	static const char VulkanShadersPath[] = "bin\\x64\\Debug\\SpirV";
#else
	static const char VulkanShadersPath[] = "bin\\x64\\Release\\SpirV";
#endif

using ShaderBytecode = std::vector<uint32_t>;

static bool LoadBinaryShader(const char * const filename, ShaderBytecode * spirv_bytecode)
{
    FILE * file = nullptr;
    auto result = fopen_s(&file, filename, "rb");

    if (result != 0 || file == nullptr)
    {
        GameInterface::Printf("Failed to open shader binary file '%s' for reading.", filename);
        return false;
    }

    fseek(file, 0, SEEK_END);
    const auto file_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    if ((file_length <= 0) || ((file_length % 4) != 0))
    {
        GameInterface::Printf("Shader binary file size in invalid: %zd", file_length);
        fclose(file);
        return false;
    }

    const size_t size_in_u32s = file_length / 4;
    spirv_bytecode->resize(size_in_u32s);

    const size_t num_read = fread(spirv_bytecode->data(), 4, size_in_u32s, file);
    fclose(file);

    return (num_read == size_in_u32s);
}

static VkShaderModule CreateShaderModule(const DeviceVK & device, const ShaderBytecode & spirv_bytecode)
{
    VkShaderModuleCreateInfo shader_module_info{};
    shader_module_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_info.codeSize = (spirv_bytecode.size() * 4); // size is in bytes!
    shader_module_info.pCode    = spirv_bytecode.data();

    VkShaderModule shader_module = nullptr;
    const VkResult result = vkCreateShaderModule(device.Handle(), &shader_module_info, nullptr, &shader_module);

    if (result != VK_SUCCESS || shader_module == nullptr)
    {
        GameInterface::Printf("Failed to create shader module: (0x%x) %s", (unsigned)result, VulkanResultToString(result));
        return nullptr;
    }

    return shader_module;
}

///////////////////////////////////////////////////////////////////////////////
// ShaderProgramVK
///////////////////////////////////////////////////////////////////////////////

bool ShaderProgramVK::LoadFromFile(const DeviceVK & device, const VertexInputLayoutVK & input_layout, const char * filename)
{
    return LoadFromFile(device, input_layout, filename, "VS_main", "PS_main", device.DebugValidationEnabled());
}

bool ShaderProgramVK::LoadFromFile(const DeviceVK & device, const VertexInputLayoutVK & input_layout,
                                   const char * filename, const char * vs_entry, const char * ps_entry, const bool debug)
{
    MRQ2_ASSERT(filename != nullptr && filename[0] != '\0');
    MRQ2_ASSERT(m_device_vk == nullptr);

    char full_shader_path[1024] = {};
    ShaderBytecode vs_bytecode;
    ShaderBytecode ps_bytecode;

    sprintf_s(full_shader_path, "%s\\%s.spv.vs", VulkanShadersPath, filename);
    if (!LoadBinaryShader(full_shader_path, &vs_bytecode))
    {
        return false;
    }

    sprintf_s(full_shader_path, "%s\\%s.spv.ps", VulkanShadersPath, filename);
    if (!LoadBinaryShader(full_shader_path, &ps_bytecode))
    {
        return false;
    }

    m_vs_handle = CreateShaderModule(device, vs_bytecode);
    if (m_vs_handle == nullptr)
    {
        return false;
    }

    m_ps_handle = CreateShaderModule(device, ps_bytecode);
    if (m_ps_handle == nullptr)
    {
        return false;
    }

    m_binding_description.binding   = 0;
    m_binding_description.stride    = 0;
    m_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    const uint32_t vk_element_sizes[] = { 0, sizeof(float) * 2, sizeof(float) * 3, sizeof(float) * 4 };
    static_assert(ArrayLength(vk_element_sizes) == VertexInputLayoutVK::kElementFormatCount);

    const VkFormat vk_element_formats[] = { VK_FORMAT_UNDEFINED, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT };
    static_assert(ArrayLength(vk_element_formats) == VertexInputLayoutVK::kElementFormatCount);

    for (const auto & element : input_layout.elements)
    {
        if (element.type   == VertexInputLayoutVK::kInvalidElementType ||
            element.format == VertexInputLayoutVK::kInvalidElementFormat)
        {
            continue;
        }

        m_attribute_descriptions[m_attribute_count].location = m_attribute_count;
        m_attribute_descriptions[m_attribute_count].binding  = 0;
        m_attribute_descriptions[m_attribute_count].format   = vk_element_formats[element.format];
        m_attribute_descriptions[m_attribute_count].offset   = element.offset;

        m_binding_description.stride += vk_element_sizes[element.format];
        ++m_attribute_count;
    }

    m_vs_entry   = vs_entry;
    m_ps_entry   = ps_entry;
    m_filename   = filename;
    m_debug_mode = debug;
    m_device_vk  = &device;

    return true;
}

void ShaderProgramVK::Shutdown()
{
    if (m_device_vk == nullptr)
    {
        return;
    }

    if (m_vs_handle != nullptr)
    {
        vkDestroyShaderModule(m_device_vk->Handle(), m_vs_handle, nullptr);
        m_vs_handle = nullptr;
    }

    if (m_ps_handle != nullptr)
    {
        vkDestroyShaderModule(m_device_vk->Handle(), m_ps_handle, nullptr);
        m_ps_handle = nullptr;
    }

    m_device_vk = nullptr;
    m_attribute_count = 0;
}

void ShaderProgramVK::GetPipelineStages(VkPipelineShaderStageCreateInfo out_stages[kNumShaderStages]) const
{
    // VS:
    out_stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    out_stages[0].pNext  = nullptr;
    out_stages[0].flags  = 0;
    out_stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    out_stages[0].module = m_vs_handle;
    out_stages[0].pName  = m_vs_entry.c_str();
    out_stages[0].pSpecializationInfo = nullptr;

    // PS (AKA Fragment Shader):
    out_stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    out_stages[1].pNext  = nullptr;
    out_stages[1].flags  = 0;
    out_stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    out_stages[1].module = m_ps_handle;
    out_stages[1].pName  = m_ps_entry.c_str();
    out_stages[1].pSpecializationInfo = nullptr;
}

} // MrQ2
