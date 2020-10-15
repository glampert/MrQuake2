//
// GraphicsContextVK.cpp
//

#include "GraphicsContextVK.hpp"
#include "DeviceVK.hpp"
#include "SwapChainVK.hpp"
#include "BufferVK.hpp"
#include "TextureVK.hpp"
#include "PipelineStateVK.hpp"
#include "ShaderProgramVK.hpp"

// https://computergraphics.stackexchange.com/questions/4422/directx-openglvulkan-concepts-mapping-chart

namespace MrQ2
{

void GraphicsContextVK::Init(const DeviceVK & device, SwapChainVK & swap_chain, const SwapChainRenderTargetsVK & render_targets)
{
    m_device_vk = &device;
    m_swap_chain = &swap_chain;
    m_render_targets = &render_targets;
}

void GraphicsContextVK::Shutdown()
{
    m_device_vk = nullptr;
    m_swap_chain = nullptr;
    m_render_targets = nullptr;
    m_command_buffer = nullptr;
    m_command_buffer_handle = nullptr;
}

void GraphicsContextVK::BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil)
{
    m_command_buffer = &m_swap_chain->CurrentCmdBuffer();
    MRQ2_ASSERT(m_command_buffer->IsInRecordingState());
    m_command_buffer_handle = m_command_buffer->Handle();

    const auto fb_index = m_swap_chain->CurrentFrameBufferIdx();

    VkClearValue clear_values[2] = {}; // color & depth
	clear_values[0].color.float32[0]     = clear_color[0];
	clear_values[0].color.float32[1]     = clear_color[1];
	clear_values[0].color.float32[2]     = clear_color[2];
	clear_values[0].color.float32[3]     = clear_color[3];
    clear_values[1].depthStencil.depth   = clear_depth;
    clear_values[1].depthStencil.stencil = clear_stencil;

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass               = m_render_targets->MainRenderPassHandle();
    render_pass_info.framebuffer              = m_render_targets->FrameBufferHandle(fb_index);
    render_pass_info.renderArea.extent.width  = m_render_targets->RenderTargetWidth();
    render_pass_info.renderArea.extent.height = m_render_targets->RenderTargetHeight();
    render_pass_info.clearValueCount          = 2; // Number of attachments in the pass
    render_pass_info.pClearValues             = clear_values;

    vkCmdBeginRenderPass(m_command_buffer_handle, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

void GraphicsContextVK::EndFrame()
{
    MRQ2_ASSERT(m_command_buffer->IsInRecordingState());
    MRQ2_ASSERT(m_command_buffer_handle == m_command_buffer->Handle());

    vkCmdEndRenderPass(m_command_buffer_handle);

    // No calls outside Begin/EndFrame
    m_command_buffer = nullptr;
    m_command_buffer_handle = nullptr;
}

void GraphicsContextVK::SetViewport(const int x, const int y, const int width, const int height)
{
}

void GraphicsContextVK::SetScissorRect(const int x, const int y, const int width, const int height)
{
}

void GraphicsContextVK::SetDepthRange(const float near_val, const float far_val)
{
}

void GraphicsContextVK::RestoreDepthRange()
{
}

void GraphicsContextVK::SetVertexBuffer(const VertexBufferVK & vb)
{
}

void GraphicsContextVK::SetIndexBuffer(const IndexBufferVK & ib)
{
}

void GraphicsContextVK::SetConstantBuffer(const ConstantBufferVK & cb, const uint32_t slot)
{
}

void GraphicsContextVK::SetTexture(const TextureVK & texture, const uint32_t slot)
{
}

void GraphicsContextVK::SetPipelineState(const PipelineStateVK & pipeline_state)
{
}

void GraphicsContextVK::SetPrimitiveTopology(const PrimitiveTopologyVK topology)
{
}

void GraphicsContextVK::Draw(const uint32_t first_vertex, const uint32_t vertex_count)
{
}

void GraphicsContextVK::DrawIndexed(const uint32_t first_index, const uint32_t index_count, const uint32_t base_vertex)
{
}

void GraphicsContextVK::PushMarker(const wchar_t * name)
{
}

void GraphicsContextVK::PopMarker()
{
}

} // MrQ2
