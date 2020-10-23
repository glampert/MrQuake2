//
// PipelineStateVK.hpp
//
#pragma once

#include "UtilsVK.hpp"

namespace MrQ2
{

class DeviceVK;
class ShaderProgramVK;
struct PipelineStateCreateInfoVK;

class PipelineStateVK final
{
    friend class GraphicsContextVK;

public:

    PipelineStateVK() = default;
    ~PipelineStateVK() { Shutdown(); }

    // Disallow copy.
    PipelineStateVK(const PipelineStateVK &) = delete;
    PipelineStateVK & operator=(const PipelineStateVK &) = delete;

    // Movable.
    PipelineStateVK(PipelineStateVK && other);
    PipelineStateVK & operator=(PipelineStateVK && other);

    void Init(const DeviceVK & device);
    void Shutdown();

    void SetPrimitiveTopology(const PrimitiveTopologyVK topology);
    void SetShaderProgram(const ShaderProgramVK & shader_prog);

    void SetDepthTestEnabled(const bool enabled);
    void SetDepthWritesEnabled(const bool enabled);
    void SetAlphaBlendingEnabled(const bool enabled);
    void SetAdditiveBlending(const bool enabled);
    void SetCullEnabled(const bool enabled);

    void Finalize() const;
    bool IsFinalized() const { return (m_flags & kFinalized) != 0; }

    // VK Internal
    static void InitGlobalState(const DeviceVK & device);
    static void ShutdownGlobalState(const DeviceVK & device);

private:

    void Init(const PipelineStateVK & other);
    void CalcSignature() const;
    void MakePipelineStateCreateInfo(PipelineStateCreateInfoVK & pipeline_info) const;
    uint64_t GetSignature() const { return m_signature; }

    enum ShaderBindings : uint32_t
    {
        // Buffers
        kShaderBindingCBuffer0, // PerFrameShaderConstants
        kShaderBindingCBuffer1, // PerViewShaderConstants
        kShaderBindingCBuffer2, // PerDrawShaderConstants

        // Textures/samplers
        kShaderBindingTexture0,
        kShaderBindingTexture1,

        // Internal counts
        kCBufferCount = 3,
        kTextureCount = 2, // BaseTexture and Lightmap

        // Max push constants (one Matrix4x4 worth of data for PerDrawShaderConstants)
        kMaxPushConstantsSizeBytes = sizeof(float) * 16
    };

    enum Flags : uint32_t
    {
        kNoFlags           = 0,
        kFinalized         = (1 << 1),
        kDepthTestEnabled  = (1 << 2),
        kDepthWriteEnabled = (1 << 3),
        kAlphaBlendEnabled = (1 << 4),
        kAdditiveBlending  = (1 << 5),
        kCullEnabled       = (1 << 6),
    };

    const DeviceVK *         m_device_vk{ nullptr };
    const ShaderProgramVK *  m_shader_prog{ nullptr };
    mutable VkPipeline       m_pipeline_handle{ nullptr };
    mutable uint64_t         m_signature{ 0 };
    mutable uint32_t         m_flags{ kNoFlags };
    PrimitiveTopologyVK      m_topology{ PrimitiveTopologyVK::kTriangleList };

    static VkPipelineCache   sm_pipeline_cache_handle;
    static VkPipelineLayout  sm_pipeline_layout_handle;
    static DescriptorSetVK   sm_global_descriptor_set;
};

///////////////////////////////////////////////////////////////////////////////

inline PipelineStateVK::PipelineStateVK(PipelineStateVK && other)
    : m_device_vk{ other.m_device_vk }
    , m_shader_prog{ other.m_shader_prog }
    , m_pipeline_handle{ other.m_pipeline_handle }
    , m_signature{ other.m_signature }
    , m_flags{ other.m_flags }
    , m_topology{ other.m_topology }
{
    other.m_device_vk       = nullptr;
    other.m_shader_prog     = nullptr;
    other.m_pipeline_handle = nullptr;
    other.m_signature       = 0;
    other.m_flags           = kNoFlags;
    other.m_topology        = {};
}

inline PipelineStateVK & PipelineStateVK::operator=(PipelineStateVK && other)
{
    Shutdown();

    m_device_vk             = other.m_device_vk;
    m_shader_prog           = other.m_shader_prog;
    m_pipeline_handle       = other.m_pipeline_handle;
    m_signature             = other.m_signature;
    m_flags                 = other.m_flags;
    m_topology              = other.m_topology;

    other.m_device_vk       = nullptr;
    other.m_shader_prog     = nullptr;
    other.m_pipeline_handle = nullptr;
    other.m_signature       = 0;
    other.m_flags           = kNoFlags;
    other.m_topology        = {};

    return *this;
}

inline void PipelineStateVK::SetPrimitiveTopology(const PrimitiveTopologyVK topology)
{
    m_topology = topology;
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
