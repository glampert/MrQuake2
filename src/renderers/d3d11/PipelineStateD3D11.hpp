//
// PipelineStateD3D11.hpp
//
#pragma once

#include "UtilsD3D11.hpp"

namespace MrQ2
{

class DeviceD3D11;
class ShaderProgramD3D11;

class PipelineStateD3D11 final
{
    friend class GraphicsContextD3D11;

public:

    PipelineStateD3D11() = default;

    // Disallow copy.
    PipelineStateD3D11(const PipelineStateD3D11 &) = delete;
    PipelineStateD3D11 & operator=(const PipelineStateD3D11 &) = delete;

    void Init(const DeviceD3D11 & device);
    void Shutdown();

    void SetPrimitiveTopology(const PrimitiveTopologyD3D11 topology);
    void SetShaderProgram(const ShaderProgramD3D11 & shader_prog);

    void SetDepthTestEnabled(const bool enabled);
    void SetDepthWritesEnabled(const bool enabled);
    void SetAlphaBlendingEnabled(const bool enabled);
    void SetCullEnabled(const bool enabled);

    void Finalize() const;
    bool IsFinalized() const { return true; } // TODO

private:

    const DeviceD3D11 *        m_device{ nullptr };
    const ShaderProgramD3D11 * m_shader_prog{ nullptr };
    float                      m_blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    PrimitiveTopologyD3D11     m_topology{ PrimitiveTopologyD3D11::kTriangleList };
};

} // MrQ2
