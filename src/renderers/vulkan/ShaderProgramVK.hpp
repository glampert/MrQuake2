//
// ShaderProgramVK.hpp
//
#pragma once

#include "UtilsVK.hpp"
#include <string>

namespace MrQ2
{

class DeviceVK;
class PipelineStateVK;
class GraphicsContextVK;

struct VertexInputLayoutVK final
{
    enum ElementType : uint8_t
    {
        kInvalidElementType = 0,

        kVertexPosition,
        kVertexTexCoords,
        kVertexLmCoords,
        kVertexColor,

        kElementTypeCount
    };

    enum ElementFormat : uint8_t
    {
        kInvalidElementFormat = 0,

        kFormatFloat2,
        kFormatFloat3,
        kFormatFloat4,

        kElementFormatCount
    };

    static constexpr uint32_t kMaxVertexElements = 4;

    struct VertexElement
    {
        ElementType   type;
        ElementFormat format;
        uint32_t      offset;
    } elements[kMaxVertexElements];
};

class ShaderProgramVK final
{
    friend PipelineStateVK;
    friend GraphicsContextVK;

public:

    ShaderProgramVK() = default;
    ~ShaderProgramVK() { Shutdown(); }

    // Disallow copy.
    ShaderProgramVK(const ShaderProgramVK &) = delete;
    ShaderProgramVK & operator=(const ShaderProgramVK &) = delete;

    // Defaults to VS_main/PS_main and debug if the Device has debug validation on.
    bool LoadFromFile(const DeviceVK & device,
                      const VertexInputLayoutVK & input_layout,
                      const char * filename);

    bool LoadFromFile(const DeviceVK & device,
                      const VertexInputLayoutVK & input_layout,
                      const char * filename,
                      const char * vs_entry, const char * ps_entry,
                      const bool debug);

    void Shutdown();

private:

    static constexpr int kNumShaderStages = 2; // Vertex[0] and Pixel[1] shaders
    void GetPipelineStages(VkPipelineShaderStageCreateInfo out_stages[kNumShaderStages]) const;

    const DeviceVK * m_device_vk{ nullptr };
    VkShaderModule   m_vs_handle{ nullptr };
    VkShaderModule   m_ps_handle{ nullptr };
    std::string      m_ps_entry{};
    std::string      m_vs_entry{};
    std::string      m_filename{};
    bool             m_debug_mode{ false };

    VkVertexInputBindingDescription   m_binding_description = {};
    VkVertexInputAttributeDescription m_attribute_descriptions[VertexInputLayoutVK::kElementTypeCount] = {};
    uint32_t                          m_attribute_count{ 0 };
};

} // MrQ2
