//
// GraphicsContextD3D12.cpp
//

#include "GraphicsContextD3D12.hpp"
#include "SwapChainD3D12.hpp"
#include "BufferD3D12.hpp"
#include "TextureD3D12.hpp"
#include "PipelineStateD3D12.hpp"

namespace MrQ2
{

void GraphicsContextD3D12::Init(const DeviceD3D12 & device, const SwapChainD3D12 & swap_chain, const SwapChainRenderTargetsD3D12 & render_targets)
{
    MRQ2_ASSERT(m_device == nullptr);

    m_device         = &device;
    m_swap_chain     = &swap_chain;
    m_render_targets = &render_targets;
    m_command_list   = swap_chain.command_list.Get();

    m_current_viewport.MinDepth = 0.0f;
    m_current_viewport.MaxDepth = 1.0f;
}

void GraphicsContextD3D12::Shutdown()
{
    m_device         = nullptr;
    m_swap_chain     = nullptr;
    m_render_targets = nullptr;
    m_command_list   = nullptr;
}

void GraphicsContextD3D12::BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil)
{
    auto back_buffer = m_swap_chain->CurrentBackbuffer(*m_render_targets);

    m_command_list->ClearRenderTargetView(back_buffer.descriptor.cpu_handle, clear_color, 0, nullptr);

    m_command_list->ClearDepthStencilView(m_render_targets->depth_render_target_descriptor.cpu_handle,
                                          D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                                          clear_depth, clear_stencil, 0, nullptr);
}

void GraphicsContextD3D12::EndFrame()
{
    m_current_pipeline_state = nullptr;
    m_current_vb             = {};
    m_current_ib             = {};
    m_current_viewport       = {};
    m_current_scissor_rect   = {};
    m_current_topology       = PrimitiveTopologyD3D12::kCount;
    m_depth_range_changed    = false;

    for (auto & cb : m_current_cb)
        cb = {};

    for (auto & tex : m_current_texture)
        tex = {};

    m_current_viewport.MinDepth = 0.0f;
    m_current_viewport.MaxDepth = 1.0f;
}

void GraphicsContextD3D12::SetViewport(const int x, const int y, const int width, const int height)
{
    m_current_viewport.TopLeftX = static_cast<float>(x);
    m_current_viewport.TopLeftY = static_cast<float>(y);
    m_current_viewport.Width    = static_cast<float>(width);
    m_current_viewport.Height   = static_cast<float>(height);
    m_command_list->RSSetViewports(1, &m_current_viewport);
}

void GraphicsContextD3D12::SetScissorRect(const int x, const int y, const int width, const int height)
{
    m_current_scissor_rect.left   = x;
    m_current_scissor_rect.top    = y;
    m_current_scissor_rect.right  = width;
    m_current_scissor_rect.bottom = height;
    m_command_list->RSSetScissorRects(1, &m_current_scissor_rect);
}

void GraphicsContextD3D12::SetDepthRange(const float near_val, const float far_val)
{
    m_current_viewport.MinDepth = near_val;
    m_current_viewport.MaxDepth = far_val;
    m_command_list->RSSetViewports(1, &m_current_viewport);
    m_depth_range_changed = true;
}

void GraphicsContextD3D12::RestoreDepthRange()
{
    if (m_depth_range_changed)
    {
        m_current_viewport.MinDepth = 0.0f;
        m_current_viewport.MaxDepth = 1.0f;
        m_command_list->RSSetViewports(1, &m_current_viewport);
        m_depth_range_changed = false;
    }
}

void GraphicsContextD3D12::SetVertexBuffer(const VertexBufferD3D12 & vb)
{
    if (m_current_vb != vb.m_view.BufferLocation)
    {
        m_current_vb = vb.m_view.BufferLocation;
        m_command_list->IASetVertexBuffers(0, 1, &vb.m_view);
    }
}

void GraphicsContextD3D12::SetIndexBuffer(const IndexBufferD3D12 & ib)
{
    if (m_current_ib != ib.m_view.BufferLocation)
    {
        m_current_ib = ib.m_view.BufferLocation;
        m_command_list->IASetIndexBuffer(&ib.m_view);
    }
}

void GraphicsContextD3D12::SetConstantBuffer(const ConstantBufferD3D12 & cb, const uint32_t slot)
{
    MRQ2_ASSERT(slot < RootSignatureD3D12::kCBufferCount);

    if (m_current_cb[slot] != cb.m_view.BufferLocation)
    {
        m_current_cb[slot] = cb.m_view.BufferLocation;
        m_command_list->SetGraphicsRootConstantBufferView(slot + RootSignatureD3D12::kRootParamIndexCBuffer0, m_current_cb[slot]);
    }
}

void GraphicsContextD3D12::SetTexture(const TextureD3D12 & texture, const uint32_t slot)
{
    MRQ2_ASSERT(slot < RootSignatureD3D12::kTextureCount);

    if (m_current_texture[slot].ptr != texture.m_srv_descriptor.gpu_handle.ptr)
    {
        m_current_texture[slot] = texture.m_srv_descriptor.gpu_handle;
        m_command_list->SetGraphicsRootDescriptorTable(slot + RootSignatureD3D12::kRootParamIndexTexture0, m_current_texture[slot]);
    }
}

void GraphicsContextD3D12::SetPipelineState(const PipelineStateD3D12 & pipeline_state)
{
    if (m_current_pipeline_state != &pipeline_state)
    {
        if (!pipeline_state.IsFinalized())
        {
            pipeline_state.Finalize();
        }

        m_current_pipeline_state = &pipeline_state;
        m_command_list->SetPipelineState(pipeline_state.m_state.Get());

        MRQ2_ASSERT(pipeline_state.m_pso_desc.pRootSignature != nullptr);
        m_command_list->SetGraphicsRootSignature(pipeline_state.m_pso_desc.pRootSignature);

        m_command_list->OMSetBlendFactor(pipeline_state.m_blend_factor);
        SetPrimitiveTopology(pipeline_state.m_topology);
    }
}

static inline D3D_PRIMITIVE_TOPOLOGY PrimitiveTopologyToD3D(const PrimitiveTopologyD3D12 topology)
{
    switch (topology)
    {
    case PrimitiveTopologyD3D12::kTriangleList  : return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case PrimitiveTopologyD3D12::kTriangleStrip : return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case PrimitiveTopologyD3D12::kTriangleFan   : return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; // Converted by the front-end
    default : GameInterface::Errorf("Bad PrimitiveTopology enum!");
    } // switch
}

void GraphicsContextD3D12::SetPrimitiveTopology(const PrimitiveTopologyD3D12 topology)
{
    if (m_current_topology != topology)
    {
        m_current_topology = topology;
        m_command_list->IASetPrimitiveTopology(PrimitiveTopologyToD3D(m_current_topology));
    }
}

void GraphicsContextD3D12::Draw(const uint32_t first_vertex, const uint32_t vertex_count)
{
    const auto instance_count = 1u;
    const auto first_instance = 0u;
    m_command_list->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
}

void GraphicsContextD3D12::SetAndUpdateConstantBuffer_Internal(const ConstantBufferD3D12 & cb, const uint32_t slot, const void * data, const uint32_t data_size)
{
    MRQ2_ASSERT(slot < RootSignatureD3D12::kCBufferCount);
    MRQ2_ASSERT(data != nullptr && data_size != 0);
    MRQ2_ASSERT((data_size % 4) == 0); // Must be a multiple of 4
    MRQ2_ASSERT((data_size / 4) <= RootSignatureD3D12::kMaxInlineRootConstants);
    MRQ2_ASSERT((cb.m_flags & ConstantBufferD3D12::kOptimizeForSingleDraw) != 0);

    const auto num_32bit_values = data_size / 4;
    m_command_list->SetGraphicsRoot32BitConstants(slot + RootSignatureD3D12::kRootParamIndexCBuffer0, num_32bit_values, data, 0);
    (void)cb; // unused
}

} // MrQ2
