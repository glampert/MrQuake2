//
// GraphicsContextD3D12.hpp
//
#pragma once

#include "UtilsD3D12.hpp"

namespace MrQ2
{

class DeviceD3D12;
class SwapChainD3D12;
class SwapChainRenderTargetsD3D12;
class TextureD3D12;
class VertexBufferD3D12;
class IndexBufferD3D12;
class ConstantBufferD3D12;
class PipelineStateD3D12;

class GraphicsContextD3D12 final
{
public:

    GraphicsContextD3D12() = default;

    // Disallow copy.
    GraphicsContextD3D12(const GraphicsContextD3D12 &) = delete;
    GraphicsContextD3D12 & operator=(const GraphicsContextD3D12 &) = delete;

    void Init(const DeviceD3D12 & device, const SwapChainD3D12 & swap_chain, const SwapChainRenderTargetsD3D12 & render_targets);
    void Shutdown();

    // Frame start/end
    void BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil);
    void EndFrame();

    // Render states
    void SetViewport(const int x, const int y, const int width, const int height);
    void SetScissorRect(const int x, const int y, const int width, const int height);
    void SetDepthRange(const float near_val, const float far_val);
    void RestoreDepthRange();
    void SetVertexBuffer(const VertexBufferD3D12 & vb);
    void SetIndexBuffer(const IndexBufferD3D12 & ib);
    void SetConstantBuffer(const ConstantBufferD3D12 & cb);
    void SetTexture(const TextureD3D12 & texture);
    void SetPipelineState(const PipelineStateD3D12 & pipeline_state);
    void SetPrimitiveTopology(const PrimitiveTopologyD3D12 topology);

    // Draw calls
    void Draw(const uint32_t first_vertex, const uint32_t vertex_count);

private:

    const DeviceD3D12 *                 m_device{ nullptr };
    const SwapChainD3D12 *              m_swap_chain{ nullptr };
    const SwapChainRenderTargetsD3D12 * m_render_targets{ nullptr };
    ID3D12GraphicsCommandList *         m_command_list{ nullptr };

    // Cached states:
    const PipelineStateD3D12 *          m_current_pipeline_state{ nullptr };
    D3D12_GPU_VIRTUAL_ADDRESS           m_current_vb{};
    D3D12_GPU_VIRTUAL_ADDRESS           m_current_ib{};
    D3D12_GPU_VIRTUAL_ADDRESS           m_current_cb{};
    D3D12_GPU_DESCRIPTOR_HANDLE         m_current_texture_srv{};
    D3D12_VIEWPORT                      m_current_viewport{};
    RECT                                m_current_scissor_rect{};
    PrimitiveTopologyD3D12              m_current_topology{ PrimitiveTopologyD3D12::kCount };
    bool                                m_depth_range_changed{ false };
};

} // MrQ2
