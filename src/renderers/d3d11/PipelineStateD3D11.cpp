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

    // Default states:
    //  Blending: Alpha blending OFF
    //  Rasterizer state: Backface cull ON
    //  Depth-stencil state: Depth test ON, depth write ON, stencil OFF
    m_flags = kDepthTestEnabled | kDepthWriteEnabled | kCullEnabled;
}

void PipelineStateD3D11::Shutdown()
{
    m_device           = nullptr;
    m_shader_prog      = nullptr;
    m_ds_state         = nullptr;
    m_rasterizer_state = nullptr;
    m_blend_state      = nullptr;
    m_flags            = kNoFlags;
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
}

void PipelineStateD3D11::SetDepthTestEnabled(const bool enabled)
{
    if (enabled)
    {
        m_flags |= kDepthTestEnabled;
    }
    else
    {
        m_flags &= ~kDepthTestEnabled;
    }
}

void PipelineStateD3D11::SetDepthWritesEnabled(const bool enabled)
{
    if (enabled)
    {
        m_flags |= kDepthWriteEnabled;
    }
    else
    {
        m_flags &= ~kDepthWriteEnabled;
    }
}

void PipelineStateD3D11::SetAlphaBlendingEnabled(const bool enabled)
{
    if (enabled)
    {
        m_flags |= kAlphaBlendEnabled;
        m_blend_factor[0] = m_blend_factor[1] = m_blend_factor[2] = m_blend_factor[3] = 1.0f;
    }
    else
    {
        m_flags &= ~kAlphaBlendEnabled;
        m_blend_factor[0] = m_blend_factor[1] = m_blend_factor[2] = m_blend_factor[3] = 0.0f;
    }
}

void PipelineStateD3D11::SetCullEnabled(const bool enabled)
{
    if (enabled)
    {
        m_flags |= kCullEnabled;
    }
    else
    {
        m_flags &= ~kCullEnabled;
    }
}

void PipelineStateD3D11::Finalize() const
{
    if (IsFinalized())
    {
        return;
    }

    MRQ2_ASSERT(m_device != nullptr);
    if (m_shader_prog == nullptr)
    {
        GameInterface::Errorf("PipelineStateD3D11: No shader program has been set!");
    }

    // Depth-stencil state object:
    {
        D3D11_DEPTH_STENCIL_DESC ds_desc = {};

        // Stencil test parameters (always OFF):
        ds_desc.StencilEnable                = false;
        ds_desc.StencilReadMask              = 0;
        ds_desc.StencilWriteMask             = 0;
        // Stencil operations if pixel is front-facing:
        ds_desc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
        ds_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
        ds_desc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
        ds_desc.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
        // Stencil operations if pixel is back-facing:
        ds_desc.BackFace.StencilFailOp       = D3D11_STENCIL_OP_KEEP;
        ds_desc.BackFace.StencilDepthFailOp  = D3D11_STENCIL_OP_DECR;
        ds_desc.BackFace.StencilPassOp       = D3D11_STENCIL_OP_KEEP;
        ds_desc.BackFace.StencilFunc         = D3D11_COMPARISON_ALWAYS;

        // Depth test parameters:
        if (m_flags & kDepthTestEnabled)
        {
            ds_desc.DepthEnable = true;
            ds_desc.DepthFunc   = D3D11_COMPARISON_LESS_EQUAL; // Matching ref_gl
        }
        else
        {
            ds_desc.DepthEnable = false;
            ds_desc.DepthFunc   = D3D11_COMPARISON_ALWAYS;
        }

        if (m_flags & kDepthWriteEnabled)
        {
            ds_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        }
        else
        {
            ds_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        }

        D11CHECK(m_device->device->CreateDepthStencilState(&ds_desc, m_ds_state.GetAddressOf()));
    }

    // Rasterizer state object:
    {
        D3D11_RASTERIZER_DESC rs_desc = {};

        rs_desc.FillMode              = D3D11_FILL_SOLID;
        rs_desc.CullMode              = (m_flags & kCullEnabled) ? D3D11_CULL_BACK : D3D11_CULL_NONE;
        rs_desc.FrontCounterClockwise = false;
        rs_desc.DepthBias             = D3D11_DEFAULT_DEPTH_BIAS;
        rs_desc.DepthBiasClamp        = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
        rs_desc.SlopeScaledDepthBias  = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rs_desc.DepthClipEnable       = true;
        rs_desc.ScissorEnable         = true;
        rs_desc.MultisampleEnable     = false;
        rs_desc.AntialiasedLineEnable = false;

        D11CHECK(m_device->device->CreateRasterizerState(&rs_desc, m_rasterizer_state.GetAddressOf()));
    }

    // Blend state object
    {
        // Blend state for the screen text and transparencies:
        D3D11_BLEND_DESC bs_desc = {};

        if (m_flags & kAlphaBlendEnabled)
        {
            bs_desc.RenderTarget[0].BlendEnable           = true;
            bs_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            bs_desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
            bs_desc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
            bs_desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
            bs_desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
            bs_desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
            bs_desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        }
        else
        {
            bs_desc.RenderTarget[0].BlendEnable           = false;
            bs_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            bs_desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
            bs_desc.RenderTarget[0].DestBlend             = D3D11_BLEND_ZERO;
            bs_desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
            bs_desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
            bs_desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
            bs_desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        }

        D11CHECK(m_device->device->CreateBlendState(&bs_desc, m_blend_state.GetAddressOf()));
    }

    m_flags |= kFinalized;
}

} // MrQ2
