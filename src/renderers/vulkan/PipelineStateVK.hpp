//
// PipelineStateVK.hpp
//
#pragma once

#include "UtilsVK.hpp"

namespace MrQ2
{

class DeviceVK;
class ShaderProgramVK;

class PipelineStateVK final
{
    friend class GraphicsContextVK;

public:

    PipelineStateVK() = default;
    ~PipelineStateVK() { Shutdown(); }

    // Disallow copy.
    PipelineStateVK(const PipelineStateVK &) = delete;
    PipelineStateVK & operator=(const PipelineStateVK &) = delete;

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
    bool IsFinalized() const { return false; } // TODO

private:

    const DeviceVK * m_device_vk{ nullptr };
    VkPipelineLayout m_pipeline_layout_handle{ nullptr };
    VkPipeline       m_pipeline_handle{ nullptr };
};

} // MrQ2
