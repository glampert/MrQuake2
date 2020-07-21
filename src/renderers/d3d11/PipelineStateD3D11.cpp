//
// PipelineStateD3D11.cpp
//

#include "PipelineStateD3D11.hpp"
#include "ShaderProgramD3D11.hpp"
#include "DeviceD3D11.hpp"

namespace MrQ2
{

void PipelineStateD3D11::Init(const DeviceD3D11 & device)
{
    MRQ2_ASSERT(m_device == nullptr);
    m_device = &device;
}

void PipelineStateD3D11::Shutdown()
{
    m_device      = nullptr;
    m_shader_prog = nullptr;
}

void PipelineStateD3D11::SetPrimitiveTopology(const PrimitiveTopologyD3D11 topology)
{
    m_topology = topology;
}

void PipelineStateD3D11::SetShaderProgram(const ShaderProgramD3D11 & shader_prog)
{
    if (!shader_prog.m_is_loaded)
    {
        GameInterface::Errorf("PipelineStateD3D11: Trying to set an invalid shader program.");
    }

    m_shader_prog = &shader_prog;
    MRQ2_ASSERT(shader_prog.m_input_layout_count > 0 && shader_prog.m_input_layout_count <= VertexInputLayoutD3D11::kMaxVertexElements);
}

void PipelineStateD3D11::SetDepthTestEnabled(const bool enabled)
{
    if (enabled)
    {
    }
    else
    {
    }
}

void PipelineStateD3D11::SetDepthWritesEnabled(const bool enabled)
{
    if (enabled)
    {
    }
    else
    {
    }
}

void PipelineStateD3D11::SetAlphaBlendingEnabled(const bool enabled)
{
    if (enabled)
    {
        m_blend_factor[0] = m_blend_factor[1] = m_blend_factor[2] = m_blend_factor[3] = 1.0f;
    }
    else
    {
        m_blend_factor[0] = m_blend_factor[1] = m_blend_factor[2] = m_blend_factor[3] = 0.0f;
    }
}

void PipelineStateD3D11::SetCullEnabled(const bool enabled)
{
    if (enabled)
    {
    }
    else
    {
    }
}

void PipelineStateD3D11::Finalize() const
{
/*
    if (m_state == nullptr)
    {
        if (m_shader_prog == nullptr)
        {
            GameInterface::Errorf("PipelineStateD3D11: No shader program has been set!");
        }

        MRQ2_ASSERT(m_device != nullptr);
        D12CHECK(m_device->device->CreateGraphicsPipelineState(&m_pso_desc, IID_PPV_ARGS(&m_state)));
        MRQ2_ASSERT(m_state != nullptr);
    }
*/
}

} // MrQ2
