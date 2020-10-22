//
// GraphicsContextVK.hpp
//
#pragma once

#include "PipelineStateVK.hpp"
#include <vector>

namespace MrQ2
{

class DeviceVK;
class SwapChainVK;
class SwapChainRenderTargetsVK;
class TextureVK;
class VertexBufferVK;
class IndexBufferVK;
class ConstantBufferVK;

class GraphicsContextVK final
{
public:

    GraphicsContextVK() = default;
    ~GraphicsContextVK() { Shutdown(); }

    // Disallow copy.
    GraphicsContextVK(const GraphicsContextVK &) = delete;
    GraphicsContextVK & operator=(const GraphicsContextVK &) = delete;

    void Init(const DeviceVK & device, SwapChainVK & swap_chain, const SwapChainRenderTargetsVK & render_targets);
    void Shutdown();

    // Frame start/end
    void BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil);
    void EndFrame();

    // Render states
    void SetViewport(const int x, const int y, const int width, const int height);
    void SetScissorRect(const int x, const int y, const int width, const int height);
    void SetDepthRange(const float near_val, const float far_val);
    void RestoreDepthRange();
    void SetVertexBuffer(const VertexBufferVK & vb);
    void SetIndexBuffer(const IndexBufferVK & ib);
    void SetConstantBuffer(const ConstantBufferVK & cb, const uint32_t slot);
    void SetTexture(const TextureVK & texture, const uint32_t slot);
    void SetPipelineState(const PipelineStateVK & pipeline_state);
    void SetPrimitiveTopology(const PrimitiveTopologyVK topology);

    template<typename T>
    void SetAndUpdateConstantBufferForDraw(const ConstantBufferVK & cb, const uint32_t slot, const T & data)
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

    const DeviceVK *                 m_device_vk{ nullptr };
    SwapChainVK *                    m_swap_chain{ nullptr };
    const SwapChainRenderTargetsVK * m_render_targets{ nullptr };
    CommandBufferVK *                m_command_buffer{ nullptr };
    VkCommandBuffer                  m_command_buffer_handle{ nullptr }; // Cached from m_command_buffer
    std::vector<PipelineStateVK>     m_pipeline_cache;

    // Cached states:
    const PipelineStateVK *          m_current_pipeline_state{ nullptr };
    VkBuffer                         m_current_vb{ nullptr };
    VkBuffer                         m_current_ib{ nullptr };
    VkBuffer                         m_current_cb[PipelineStateVK::kCBufferCount] = {};
    VkImageView                      m_current_texture[PipelineStateVK::kTextureCount] = {};
    VkViewport                       m_current_viewport{};
    VkRect2D                         m_current_scissor_rect{};
    PrimitiveTopologyVK              m_current_topology{ PrimitiveTopologyVK::kCount };
    bool                             m_depth_range_changed{ false };
    bool                             m_gpu_markers_enabled{ false };

    void SetAndUpdateConstantBuffer_Internal(const ConstantBufferVK & cb, const uint32_t slot, const void * data, const uint32_t data_size);
    const PipelineStateVK * FindOrRegisterPipeline(PipelineStateVK * dynamic_pipeline);
};

//
// Debug markers:
//
struct ScopedGpuMarkerVK final
{
    GraphicsContextVK & m_context;

    ScopedGpuMarkerVK(GraphicsContextVK & ctx, const wchar_t * name)
        : m_context{ ctx }
    {
        m_context.PushMarker(name);
    }

    ~ScopedGpuMarkerVK()
    {
        m_context.PopMarker();
    }
};

#define MRQ2_SCOPED_GPU_MARKER(context, name) MrQ2::ScopedGpuMarkerVK MRQ2_CAT_TOKEN(gpu_scope_marker_, __LINE__){ context, MRQ2_MAKE_WIDE_STR(name) }
#define MRQ2_FUNCTION_GPU_MARKER(context)     MrQ2::ScopedGpuMarkerVK MRQ2_CAT_TOKEN(gpu_funct_marker_, __LINE__){ context, MRQ2_MAKE_WIDE_STR(__FUNCTION__) }

#define MRQ2_PUSH_GPU_MARKER(context, name)   context.PushMarker(MRQ2_MAKE_WIDE_STR(name))
#define MRQ2_POP_GPU_MARKER(context)          context.PopMarker()

} // MrQ2
