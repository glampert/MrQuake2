//
// PipelineStateD3D12.hpp
//
#pragma once

#include "UtilsD3D12.hpp"

namespace MrQ2
{

class DeviceD3D12;
class ShaderProgramD3D12;

class PipelineStateD3D12 final
{
    friend class GraphicsContextD3D12;

public:

    PipelineStateD3D12() = default;

    // Disallow copy.
    PipelineStateD3D12(const PipelineStateD3D12 &) = delete;
    PipelineStateD3D12 & operator=(const PipelineStateD3D12 &) = delete;

    void Init(const DeviceD3D12 & device);
    void Shutdown();

    void SetPrimitiveTopology(const PrimitiveTopologyD3D12 topology);
    void SetShaderProgram(const ShaderProgramD3D12 & shader_prog);

    void SetDepthTestEnabled(const bool enabled);
    void SetDepthWritesEnabled(const bool enabled);
    void SetAlphaBlendingEnabled(const bool enabled);
    void SetAdditiveBlending(const bool enabled);
    void SetCullEnabled(const bool enabled);

    void Finalize() const;
    bool IsFinalized() const { return m_state != nullptr; }

private:

    const DeviceD3D12 *                    m_device{ nullptr };
    mutable D12ComPtr<ID3D12PipelineState> m_state{ nullptr };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC     m_pso_desc{};
    const ShaderProgramD3D12 *             m_shader_prog{ nullptr };
    float                                  m_blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    PrimitiveTopologyD3D12                 m_topology{ PrimitiveTopologyD3D12::kTriangleList };
};

} // MrQ2
