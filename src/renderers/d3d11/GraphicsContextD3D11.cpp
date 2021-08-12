//
// GraphicsContextD3D11.cpp
//

#include "GraphicsContextD3D11.hpp"
#include "DeviceD3D11.hpp"
#include "SwapChainD3D11.hpp"
#include "BufferD3D11.hpp"
#include "TextureD3D11.hpp"
#include "PipelineStateD3D11.hpp"
#include "ShaderProgramD3D11.hpp"

namespace MrQ2
{

void GraphicsContextD3D11::Init(const DeviceD3D11 & device, const SwapChainD3D11 & swap_chain, const SwapChainRenderTargetsD3D11 & render_targets)
{
    MRQ2_ASSERT(m_device == nullptr);
    MRQ2_ASSERT(device.DeviceContext() != nullptr);

    m_device         = &device;
    m_swap_chain     = &swap_chain;
    m_render_targets = &render_targets;
    m_context        = device.DeviceContext();

    m_current_viewport.MinDepth = 0.0f;
    m_current_viewport.MaxDepth = 1.0f;

    m_gpu_markers_enabled = Config::r_debug_frame_events.IsSet();

    if (m_gpu_markers_enabled)
    {
        ID3DUserDefinedAnnotation * annotations{ nullptr };
        if (SUCCEEDED(m_context->QueryInterface(__uuidof(annotations), (void **)&annotations)))
        {
            m_annotations.Attach(annotations);
        }
        else
        {
            GameInterface::Printf("WARNING: Unable to initialize ID3DUserDefinedAnnotation.");
        }
    }
}

void GraphicsContextD3D11::Shutdown()
{
    m_device         = nullptr;
    m_swap_chain     = nullptr;
    m_render_targets = nullptr;
    m_context        = nullptr;
    m_annotations    = nullptr;
}

void GraphicsContextD3D11::BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil)
{
    m_context->ClearRenderTargetView(m_render_targets->m_framebuffer_rtv.Get(), clear_color);
    m_context->ClearDepthStencilView(m_render_targets->m_depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, clear_depth, clear_stencil);
}

void GraphicsContextD3D11::EndFrame()
{
    m_current_pipeline_state = nullptr;
    m_current_vb             = nullptr;
    m_current_ib             = nullptr;
    m_current_viewport       = {};
    m_current_scissor_rect   = {};
    m_current_topology       = PrimitiveTopologyD3D11::kCount;
    m_depth_range_changed    = false;

    for (auto & cb : m_current_cb)
        cb = nullptr;

    for (auto & tex : m_current_texture)
        tex = nullptr;

    m_current_viewport.MinDepth = 0.0f;
    m_current_viewport.MaxDepth = 1.0f;
}

void GraphicsContextD3D11::SetViewport(const int x, const int y, const int width, const int height)
{
    m_current_viewport.TopLeftX = static_cast<float>(x);
    m_current_viewport.TopLeftY = static_cast<float>(y);
    m_current_viewport.Width    = static_cast<float>(width);
    m_current_viewport.Height   = static_cast<float>(height);
    m_context->RSSetViewports(1, &m_current_viewport);
}

void GraphicsContextD3D11::SetScissorRect(const int x, const int y, const int width, const int height)
{
    m_current_scissor_rect.left   = x;
    m_current_scissor_rect.top    = y;
    m_current_scissor_rect.right  = width;
    m_current_scissor_rect.bottom = height;
    m_context->RSSetScissorRects(1, &m_current_scissor_rect);
}

void GraphicsContextD3D11::SetDepthRange(const float near_val, const float far_val)
{
    m_current_viewport.MinDepth = near_val;
    m_current_viewport.MaxDepth = far_val;
    m_context->RSSetViewports(1, &m_current_viewport);
    m_depth_range_changed = true;
}

void GraphicsContextD3D11::RestoreDepthRange()
{
    if (m_depth_range_changed)
    {
        m_current_viewport.MinDepth = 0.0f;
        m_current_viewport.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &m_current_viewport);
        m_depth_range_changed = false;
    }
}

void GraphicsContextD3D11::SetVertexBuffer(const VertexBufferD3D11 & vb)
{
    if (m_current_vb != vb.m_resource.Get())
    {
        m_current_vb = vb.m_resource.Get();
        const UINT stride = vb.StrideInBytes();
        const UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, &m_current_vb, &stride, &offset);
    }
}

void GraphicsContextD3D11::SetIndexBuffer(const IndexBufferD3D11 & ib)
{
    if (m_current_ib != ib.m_resource.Get())
    {
        m_current_ib = ib.m_resource.Get();
        const DXGI_FORMAT format = (ib.Format() == IndexBufferD3D11::kFormatUInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        const UINT offset = 0;
        m_context->IASetIndexBuffer(m_current_ib, format, offset);
    }
}

void GraphicsContextD3D11::SetConstantBuffer(const ConstantBufferD3D11 & cb, const uint32_t slot)
{
    MRQ2_ASSERT(slot < kCBufferCount);

    if (m_current_cb[slot] != cb.m_resource.Get())
    {
        m_current_cb[slot] = cb.m_resource.Get();
        m_context->VSSetConstantBuffers(slot + kShaderBindingCBuffer0, 1, &m_current_cb[slot]);
        m_context->PSSetConstantBuffers(slot + kShaderBindingCBuffer0, 1, &m_current_cb[slot]);
    }
}

void GraphicsContextD3D11::SetTexture(const TextureD3D11 & texture, const uint32_t slot)
{
    MRQ2_ASSERT(slot < kTextureCount);

    if (m_current_texture[slot] != texture.m_resource.Get())
    {
        m_current_texture[slot] = texture.m_resource.Get();
        m_context->PSSetShaderResources(slot + kShaderBindingTexture0, 1, texture.m_srv.GetAddressOf());
        m_context->PSSetSamplers(slot + kShaderBindingTexture0, 1, texture.m_sampler.GetAddressOf());
    }
}

void GraphicsContextD3D11::SetPipelineState(const PipelineStateD3D11 & pipeline_state)
{
    if (m_current_pipeline_state != &pipeline_state)
    {
        if (!pipeline_state.IsFinalized())
        {
            pipeline_state.Finalize();
        }

        m_current_pipeline_state = &pipeline_state;

        m_context->OMSetDepthStencilState(m_current_pipeline_state->m_ds_state.Get(), 0);
        m_context->OMSetBlendState(m_current_pipeline_state->m_blend_state.Get(), m_current_pipeline_state->m_blend_factor, 0xFFFFFFFF);
        m_context->RSSetState(m_current_pipeline_state->m_rasterizer_state.Get());
        SetPrimitiveTopology(m_current_pipeline_state->m_topology);

        auto * shader = m_current_pipeline_state->m_shader_prog;
        MRQ2_ASSERT(shader != nullptr && shader->IsLoaded());

        m_context->IASetInputLayout(shader->m_vertex_layout.Get());
        m_context->VSSetShader(shader->m_vertex_shader.Get(), nullptr, 0);
        m_context->PSSetShader(shader->m_pixel_shader.Get(), nullptr, 0);
    }
}

static inline D3D_PRIMITIVE_TOPOLOGY PrimitiveTopologyToD3D(const PrimitiveTopologyD3D11 topology)
{
    switch (topology)
    {
    case PrimitiveTopologyD3D11::kTriangleList  : return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case PrimitiveTopologyD3D11::kTriangleStrip : return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case PrimitiveTopologyD3D11::kTriangleFan   : return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; // Converted by the front-end
    case PrimitiveTopologyD3D11::kLineList      : return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    default : GameInterface::Errorf("Bad PrimitiveTopology enum!");
    } // switch
}

void GraphicsContextD3D11::SetPrimitiveTopology(const PrimitiveTopologyD3D11 topology)
{
    if (m_current_topology != topology)
    {
        m_current_topology = topology;
        m_context->IASetPrimitiveTopology(PrimitiveTopologyToD3D(m_current_topology));
    }
}

void GraphicsContextD3D11::Draw(const uint32_t first_vertex, const uint32_t vertex_count)
{
    m_context->Draw(vertex_count, first_vertex);
}

void GraphicsContextD3D11::DrawIndexed(const uint32_t first_index, const uint32_t index_count, const uint32_t base_vertex)
{
    m_context->DrawIndexed(index_count, first_index, base_vertex);
}

void GraphicsContextD3D11::SetAndUpdateConstantBuffer_Internal(const ConstantBufferD3D11 & cb, const uint32_t slot, const void * data, const uint32_t data_size)
{
    MRQ2_ASSERT(slot < kCBufferCount);
    MRQ2_ASSERT(data != nullptr && data_size != 0);
    MRQ2_ASSERT(data_size >= cb.SizeInBytes());
    (void)data_size; // unused

    m_context->UpdateSubresource(cb.m_resource.Get(), 0, nullptr, data, 0, 0);

    SetConstantBuffer(cb, slot);
}

void GraphicsContextD3D11::PushMarker(const wchar_t * name)
{
    if (m_gpu_markers_enabled && m_annotations != nullptr)
    {
        m_annotations->BeginEvent(name);
    }
}

void GraphicsContextD3D11::PopMarker()
{
    if (m_gpu_markers_enabled && m_annotations != nullptr)
    {
        m_annotations->EndEvent();
    }
}

} // MrQ2
