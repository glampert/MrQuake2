//
// PipelineStateD3D12.cpp
//

#include "PipelineStateD3D12.hpp"
#include "ShaderProgramD3D12.hpp"
#include "RootSignatureD3D12.hpp"
#include "DeviceD3D12.hpp"

namespace MrQ2
{

void PipelineStateD3D12::Init(const DeviceD3D12 & device)
{
    MRQ2_ASSERT(m_device == nullptr);
    m_device = &device;

    // DEFAULTS:
    // Single render target, triangles topology
    m_pso_desc.NodeMask              = 1;
    m_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    m_pso_desc.SampleMask            = UINT_MAX;
    m_pso_desc.NumRenderTargets      = 1;
    m_pso_desc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    m_pso_desc.DSVFormat             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    m_pso_desc.SampleDesc.Count      = 1;
    m_pso_desc.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE;

    // Blending setup: Alpha blending OFF
    {
        D3D12_BLEND_DESC & desc                    = m_pso_desc.BlendState;
        desc.AlphaToCoverageEnable                 = false;
        desc.RenderTarget[0].BlendEnable           = false;
        desc.RenderTarget[0].SrcBlend              = D3D12_BLEND_ONE;
        desc.RenderTarget[0].DestBlend             = D3D12_BLEND_ZERO;
        desc.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_ZERO;
        desc.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
        desc.RenderTarget[0].LogicOp               = D3D12_LOGIC_OP_NOOP;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    // Rasterizer state: Backface cull ON
    {
        D3D12_RASTERIZER_DESC & desc = m_pso_desc.RasterizerState;
        desc.FillMode                = D3D12_FILL_MODE_SOLID;
        desc.CullMode                = D3D12_CULL_MODE_BACK;
        desc.FrontCounterClockwise   = false;
        desc.DepthBias               = D3D12_DEFAULT_DEPTH_BIAS;
        desc.DepthBiasClamp          = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        desc.SlopeScaledDepthBias    = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        desc.DepthClipEnable         = true;
        desc.MultisampleEnable       = false;
        desc.AntialiasedLineEnable   = false;
        desc.ForcedSampleCount       = 0;
        desc.ConservativeRaster      = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }

    // Depth-stencil state: Depth test ON, depth write ON, stencil OFF
    {
        D3D12_DEPTH_STENCIL_DESC & desc = m_pso_desc.DepthStencilState;
        desc.DepthEnable                = true;
        desc.DepthWriteMask             = D3D12_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc                  = D3D12_COMPARISON_FUNC_LESS_EQUAL; // Matching ref_gl
        desc.StencilEnable              = false;
        desc.FrontFace.StencilFailOp    = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFunc      = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.BackFace                   = desc.FrontFace;
    }

    // All shaders and pipelines share the same Root Signature.
    m_pso_desc.pRootSignature = RootSignatureD3D12::sm_global.root_sig.Get();
    MRQ2_ASSERT(m_pso_desc.pRootSignature != nullptr);
}

void PipelineStateD3D12::Shutdown()
{
    m_device      = nullptr;
    m_state       = nullptr;
    m_shader_prog = nullptr;
    m_pso_desc    = {};
}

void PipelineStateD3D12::SetPrimitiveTopology(const PrimitiveTopologyD3D12 topology)
{
    m_topology = topology;

    if (topology == PrimitiveTopologyD3D12::kLineList)
    {
        m_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    }
    else
    {
        m_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
}

void PipelineStateD3D12::SetShaderProgram(const ShaderProgramD3D12 & shader_prog)
{
    if (!shader_prog.m_is_loaded)
    {
        GameInterface::Errorf("PipelineStateD3D12: Trying to set an invalid shader program.");
    }

    m_shader_prog = &shader_prog;

    MRQ2_ASSERT(shader_prog.m_input_layout_count > 0 && shader_prog.m_input_layout_count <= VertexInputLayoutD3D12::kMaxVertexElements);
    m_pso_desc.InputLayout = { shader_prog.m_input_layout_d3d, shader_prog.m_input_layout_count };

    m_pso_desc.VS = { shader_prog.m_shader_bytecode.vs_blob->GetBufferPointer(), shader_prog.m_shader_bytecode.vs_blob->GetBufferSize() };
    m_pso_desc.PS = { shader_prog.m_shader_bytecode.ps_blob->GetBufferPointer(), shader_prog.m_shader_bytecode.ps_blob->GetBufferSize() };
}

void PipelineStateD3D12::SetDepthTestEnabled(const bool enabled)
{
    if (enabled)
    {
        m_pso_desc.DepthStencilState.DepthEnable = true;
        m_pso_desc.DepthStencilState.DepthFunc   = D3D12_COMPARISON_FUNC_LESS_EQUAL; // Matching ref_gl
    }
    else
    {
        m_pso_desc.DepthStencilState.DepthEnable = false;
        m_pso_desc.DepthStencilState.DepthFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
    }
}

void PipelineStateD3D12::SetDepthWritesEnabled(const bool enabled)
{
    if (enabled)
    {
        m_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    }
    else
    {
        m_pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    }
}

void PipelineStateD3D12::SetAlphaBlendingEnabled(const bool enabled)
{
    if (enabled)
    {
        m_pso_desc.BlendState.RenderTarget[0].BlendEnable    = true;
        m_pso_desc.BlendState.RenderTarget[0].SrcBlend       = D3D12_BLEND_SRC_ALPHA;
        m_pso_desc.BlendState.RenderTarget[0].DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
        m_pso_desc.BlendState.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
        m_pso_desc.BlendState.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_INV_SRC_ALPHA;
        m_pso_desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        m_pso_desc.BlendState.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
        m_pso_desc.BlendState.RenderTarget[0].LogicOp        = D3D12_LOGIC_OP_CLEAR;
        m_blend_factor[0] = m_blend_factor[1] = m_blend_factor[2] = m_blend_factor[3] = 1.0f;
    }
    else
    {
        m_pso_desc.BlendState.RenderTarget[0].BlendEnable    = false;
        m_pso_desc.BlendState.RenderTarget[0].SrcBlend       = D3D12_BLEND_ONE;
        m_pso_desc.BlendState.RenderTarget[0].DestBlend      = D3D12_BLEND_ZERO;
        m_pso_desc.BlendState.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
        m_pso_desc.BlendState.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
        m_pso_desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        m_pso_desc.BlendState.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
        m_pso_desc.BlendState.RenderTarget[0].LogicOp        = D3D12_LOGIC_OP_NOOP;
        m_blend_factor[0] = m_blend_factor[1] = m_blend_factor[2] = m_blend_factor[3] = 0.0f;
    }
}

void PipelineStateD3D12::SetAdditiveBlending(const bool enabled)
{
    if (enabled)
    {
        m_pso_desc.BlendState.RenderTarget[0].SrcBlend  = D3D12_BLEND_ONE;
        m_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    }
    else
    {
        m_pso_desc.BlendState.RenderTarget[0].SrcBlend  = D3D12_BLEND_ONE;
        m_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    }
}

void PipelineStateD3D12::SetCullEnabled(const bool enabled)
{
    if (enabled)
    {
        m_pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    }
    else
    {
        m_pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    }
}

void PipelineStateD3D12::Finalize() const
{
    if (m_state == nullptr)
    {
        if (m_shader_prog == nullptr)
        {
            GameInterface::Errorf("PipelineStateD3D12: No shader program has been set!");
        }

        MRQ2_ASSERT(m_device != nullptr);
        D12CHECK(m_device->Device()->CreateGraphicsPipelineState(&m_pso_desc, IID_PPV_ARGS(&m_state)));
        MRQ2_ASSERT(m_state != nullptr);
    }
}

} // MrQ2
