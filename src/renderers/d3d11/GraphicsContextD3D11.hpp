//
// GraphicsContextD3D11.hpp
//
#pragma once

#include "UtilsD3D11.hpp"

namespace MrQ2
{

class DeviceD3D11;
class SwapChainD3D11;
class SwapChainRenderTargetsD3D11;
class TextureD3D11;
class VertexBufferD3D11;
class IndexBufferD3D11;
class ConstantBufferD3D11;
class PipelineStateD3D11;

class GraphicsContextD3D11 final
{
public:

    GraphicsContextD3D11() = default;

    // Disallow copy.
    GraphicsContextD3D11(const GraphicsContextD3D11 &) = delete;
    GraphicsContextD3D11 & operator=(const GraphicsContextD3D11 &) = delete;

    void Init(const DeviceD3D11 & device, const SwapChainD3D11 & swap_chain, const SwapChainRenderTargetsD3D11 & render_targets);
    void Shutdown();

    // Frame start/end
    void BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil);
    void EndFrame();

    // Render states
    void SetViewport(const int x, const int y, const int width, const int height);
    void SetScissorRect(const int x, const int y, const int width, const int height);
    void SetDepthRange(const float near_val, const float far_val);
    void RestoreDepthRange();
    void SetVertexBuffer(const VertexBufferD3D11 & vb);
    void SetIndexBuffer(const IndexBufferD3D11 & ib);
    void SetConstantBuffer(const ConstantBufferD3D11 & cb, const uint32_t slot);
    void SetTexture(const TextureD3D11 & texture, const uint32_t slot);
    void SetPipelineState(const PipelineStateD3D11 & pipeline_state);
    void SetPrimitiveTopology(const PrimitiveTopologyD3D11 topology);

    template<typename T>
    void SetAndUpdateConstantBufferForDraw(const ConstantBufferD3D11 & cb, const uint32_t slot, const T & data)
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

    // These match the D3D12 global RootSignature
    static constexpr uint32_t kCBufferSlotCount = 3;
    static constexpr uint32_t kTextureSlotCount = 1;

    const DeviceD3D11 *                  m_device{ nullptr };
    const SwapChainD3D11 *               m_swap_chain{ nullptr };
    const SwapChainRenderTargetsD3D11 *  m_render_targets{ nullptr };
    ID3D11DeviceContext *                m_context{ nullptr };
    D11ComPtr<ID3DUserDefinedAnnotation> m_annotations{ nullptr };

    // Cached states:
    const PipelineStateD3D11 *           m_current_pipeline_state{ nullptr };
    const VertexBufferD3D11 *            m_current_vb{ nullptr };
    const IndexBufferD3D11 *             m_current_ib{ nullptr };
    const ConstantBufferD3D11 *          m_current_cb[kCBufferSlotCount] = {};
    const TextureD3D11 *                 m_current_texture[kTextureSlotCount] = {};
    D3D11_VIEWPORT                       m_current_viewport{};
    RECT                                 m_current_scissor_rect{};
    PrimitiveTopologyD3D11               m_current_topology{ PrimitiveTopologyD3D11::kCount };
    bool                                 m_depth_range_changed{ false };
    bool                                 m_gpu_markers_enabled{ false };

    void SetAndUpdateConstantBuffer_Internal(const ConstantBufferD3D11 & cb, const uint32_t slot, const void * data, const uint32_t data_size);
};

//
// Debug markers:
//
struct ScopedGpuMarkerD3D11 final
{
    GraphicsContextD3D11 & m_context;

    ScopedGpuMarkerD3D11(GraphicsContextD3D11 & ctx, const wchar_t * name)
        : m_context{ ctx }
    {
        m_context.PushMarker(name);
    }

    ~ScopedGpuMarkerD3D11()
    {
        m_context.PopMarker();
    }
};

#define MRQ2_SCOPED_GPU_MARKER(context, name) MrQ2::ScopedGpuMarkerD3D11 MRQ2_CAT_TOKEN(gpu_scope_marker_, __LINE__){ context, MRQ2_MAKE_WIDE_STR(name) }
#define MRQ2_FUNCTION_GPU_MARKER(context)     MrQ2::ScopedGpuMarkerD3D11 MRQ2_CAT_TOKEN(gpu_funct_marker_, __LINE__){ context, MRQ2_MAKE_WIDE_STR(__FUNCTION__) }

#define MRQ2_PUSH_GPU_MARKER(context, name)   context.PushMarker(MRQ2_MAKE_WIDE_STR(name))
#define MRQ2_POP_GPU_MARKER(context)          context.PopMarker()

} // MrQ2
