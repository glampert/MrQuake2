//
// GraphicsContextD3D12.hpp
//
#pragma once

#include "UtilsD3D12.hpp"
#include "RootSignatureD3D12.hpp"

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
    void SetConstantBuffer(const ConstantBufferD3D12 & cb, const uint32_t slot);
    void SetTexture(const TextureD3D12 & texture, const uint32_t slot);
    void SetPipelineState(const PipelineStateD3D12 & pipeline_state);
    void SetPrimitiveTopology(const PrimitiveTopologyD3D12 topology);

    template<typename T>
    void SetAndUpdateConstantBufferForDraw(const ConstantBufferD3D12 & cb, const uint32_t slot, const T & data)
    {
        SetAndUpdateConstantBuffer_Internal(cb, slot, &data, sizeof(T));
    }

    // Draw calls
    void Draw(const uint32_t first_vertex, const uint32_t vertex_count);
    void DrawIndexed(const uint32_t first_index, const uint32_t index_count, const uint32_t base_vertex);

    // Debug markers
    void PushMarker(const wchar_t * name);
    void PopMarker();

private:

    const DeviceD3D12 *                 m_device{ nullptr };
    const SwapChainD3D12 *              m_swap_chain{ nullptr };
    const SwapChainRenderTargetsD3D12 * m_render_targets{ nullptr };
    ID3D12GraphicsCommandList *         m_command_list{ nullptr };

    // Cached states:
    const PipelineStateD3D12 *          m_current_pipeline_state{ nullptr };
    D3D12_GPU_VIRTUAL_ADDRESS           m_current_vb{};
    D3D12_GPU_VIRTUAL_ADDRESS           m_current_ib{};
    D3D12_GPU_VIRTUAL_ADDRESS           m_current_cb[RootSignatureD3D12::kCBufferCount] = {};
    D3D12_GPU_DESCRIPTOR_HANDLE         m_current_texture[RootSignatureD3D12::kTextureCount] = {};
    D3D12_VIEWPORT                      m_current_viewport{};
    RECT                                m_current_scissor_rect{};
    PrimitiveTopologyD3D12              m_current_topology{ PrimitiveTopologyD3D12::kCount };
    bool                                m_depth_range_changed{ false };
    bool                                m_gpu_markers_enabled{ false };

    void SetAndUpdateConstantBuffer_Internal(const ConstantBufferD3D12 & cb, const uint32_t slot, const void * data, const uint32_t data_size);
};

//
// Debug markers:
//
struct ScopedGpuMarkerD3D12 final
{
    GraphicsContextD3D12 & m_context;

    ScopedGpuMarkerD3D12(GraphicsContextD3D12 & ctx, const wchar_t * name)
        : m_context{ ctx }
    {
        m_context.PushMarker(name);
    }

    ~ScopedGpuMarkerD3D12()
    {
        m_context.PopMarker();
    }
};

#define MRQ2_SCOPED_GPU_MARKER(context, name) MrQ2::ScopedGpuMarkerD3D12 MRQ2_CAT_TOKEN(gpu_scope_marker_, __LINE__){ context, MRQ2_MAKE_WIDE_STR(name) }
#define MRQ2_FUNCTION_GPU_MARKER(context)     MrQ2::ScopedGpuMarkerD3D12 MRQ2_CAT_TOKEN(gpu_funct_marker_, __LINE__){ context, MRQ2_MAKE_WIDE_STR(__FUNCTION__) }

#define MRQ2_PUSH_GPU_MARKER(context, name)   context.PushMarker(MRQ2_MAKE_WIDE_STR(name))
#define MRQ2_POP_GPU_MARKER(context)          context.PopMarker()

} // MrQ2
