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

// Useful links:
// https://computergraphics.stackexchange.com/questions/4422/directx-openglvulkan-concepts-mapping-chart
// https://zeux.io/2020/02/27/writing-an-efficient-vulkan-renderer/

namespace MrQ2
{

void GraphicsContextVK::Init(const DeviceVK & device, SwapChainVK & swap_chain, const SwapChainRenderTargetsVK & render_targets)
{
    MRQ2_ASSERT(m_device_vk == nullptr);

    m_device_vk      = &device;
    m_swap_chain     = &swap_chain;
    m_render_targets = &render_targets;

    m_current_viewport.minDepth = 0.0f;
    m_current_viewport.maxDepth = 1.0f;

    m_gpu_markers_enabled = Config::r_debug_frame_events.IsSet();
    m_pipeline_cache.reserve(16);
}

void GraphicsContextVK::Shutdown()
{
    m_pipeline_cache.clear();

    m_device_vk             = nullptr;
    m_swap_chain            = nullptr;
    m_render_targets        = nullptr;
    m_command_buffer        = nullptr;
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
    m_command_buffer             = nullptr;
    m_command_buffer_handle      = nullptr;

    // Reset frame states
    m_current_pipeline_state     = nullptr;
    m_current_vb                 = nullptr;
    m_current_ib                 = nullptr;
    m_current_viewport           = {};
    m_current_viewport.minDepth  = 0.0f;
    m_current_viewport.maxDepth  = 1.0f;
    m_current_scissor_rect       = {};
    m_current_topology           = PrimitiveTopologyVK::kCount;
    m_depth_range_changed        = false;

    for (auto & cb : m_current_cb)
        cb = nullptr;

    for (auto & tex : m_current_texture)
        tex = nullptr;
}

void GraphicsContextVK::SetViewport(const int x, const int y, const int width, const int height)
{
    // NOTE: Invert the Y axis (assume we have VK_KHR_Maintenance1, which should be always true for Vulkan 1.1).
    (void)y;

    m_current_viewport.x      =  static_cast<float>(x);
    m_current_viewport.y      =  static_cast<float>(height);
    m_current_viewport.width  =  static_cast<float>(width);
    m_current_viewport.height = -static_cast<float>(height);

    vkCmdSetViewport(m_command_buffer_handle, 0, 1, &m_current_viewport);
}

void GraphicsContextVK::SetScissorRect(const int x, const int y, const int width, const int height)
{
    m_current_scissor_rect.offset.x      = x;
    m_current_scissor_rect.offset.y      = y;
    m_current_scissor_rect.extent.width  = width;
    m_current_scissor_rect.extent.height = height;

    vkCmdSetScissor(m_command_buffer_handle, 0, 1, &m_current_scissor_rect);
}

void GraphicsContextVK::SetDepthRange(const float near_val, const float far_val)
{
    m_current_viewport.minDepth = near_val;
    m_current_viewport.maxDepth = far_val;
    m_depth_range_changed       = true;

    vkCmdSetViewport(m_command_buffer_handle, 0, 1, &m_current_viewport);
}

void GraphicsContextVK::RestoreDepthRange()
{
    if (m_depth_range_changed)
    {
        m_current_viewport.minDepth = 0.0f;
        m_current_viewport.maxDepth = 1.0f;
        m_depth_range_changed       = false;

        vkCmdSetViewport(m_command_buffer_handle, 0, 1, &m_current_viewport);
    }
}

void GraphicsContextVK::SetVertexBuffer(const VertexBufferVK & vb)
{
    MRQ2_ASSERT(vb.Handle() != nullptr);

    if (vb.Handle() != m_current_vb)
    {
        m_current_vb = vb.Handle();

        const VkBuffer buffers[] = { m_current_vb };
        const VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(m_command_buffer_handle, 0, 1, buffers, offsets);
    }
}

void GraphicsContextVK::SetIndexBuffer(const IndexBufferVK & ib)
{
    MRQ2_ASSERT(ib.Handle() != nullptr);

    if (ib.Handle() != m_current_ib)
    {
        m_current_ib = ib.Handle();
        vkCmdBindIndexBuffer(m_command_buffer_handle, m_current_ib, 0, ib.TypeVK());
    }
}

void GraphicsContextVK::SetConstantBuffer(const ConstantBufferVK & cb, const uint32_t slot)
{
    MRQ2_ASSERT(cb.Handle() != nullptr);
    MRQ2_ASSERT(slot < PipelineStateVK::kCBufferCount);

    if (m_current_cb[slot] != cb.Handle())
    {
        m_current_cb[slot] = cb.Handle();

        VkDescriptorBufferInfo descriptor_buffer_info{};
        descriptor_buffer_info.buffer = m_current_cb[slot];
        descriptor_buffer_info.range  = cb.SizeInBytes();
        descriptor_buffer_info.offset = 0;

        VkWriteDescriptorSet descriptor_set_writes{};
        descriptor_set_writes.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_set_writes.dstBinding      = slot + PipelineStateVK::kShaderBindingCBuffer0;
        descriptor_set_writes.descriptorCount = 1;
        descriptor_set_writes.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_set_writes.pBufferInfo     = &descriptor_buffer_info;

        m_device_vk->pVkCmdPushDescriptorSetKHR(m_command_buffer_handle, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineStateVK::sm_pipeline_layout_handle, 0, 1, &descriptor_set_writes);
    }
}

void GraphicsContextVK::SetAndUpdateConstantBuffer_Internal(const ConstantBufferVK & cb, const uint32_t slot, const void * data, const uint32_t data_size)
{
    // Similarly to D3D12 where we use the RootSignature inline constants.

    MRQ2_ASSERT(cb.Handle() != nullptr);
    MRQ2_ASSERT(slot < PipelineStateVK::kCBufferCount);
    MRQ2_ASSERT(data != nullptr && data_size != 0);
    MRQ2_ASSERT(data_size <= PipelineStateVK::kMaxPushConstantsSizeBytes);
    MRQ2_ASSERT((cb.m_flags & ConstantBufferVK::kOptimizeForSingleDraw) != 0);

    vkCmdPushConstants(m_command_buffer_handle, PipelineStateVK::sm_pipeline_layout_handle, VK_SHADER_STAGE_VERTEX_BIT, 0, data_size, data);

    // unused
    (void)cb;
    (void)slot;
}

void GraphicsContextVK::SetTexture(const TextureVK & texture, const uint32_t slot)
{
    MRQ2_ASSERT(texture.Handle() != nullptr);
    MRQ2_ASSERT(slot < PipelineStateVK::kTextureCount);

    if (m_current_texture[slot] != texture.ViewHandle())
    {
        m_current_texture[slot] = texture.ViewHandle();

        VkDescriptorImageInfo descriptor_image_info{};
        descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        descriptor_image_info.imageView   = m_current_texture[slot];
        descriptor_image_info.sampler     = texture.SamplerHandle();

        VkWriteDescriptorSet descriptor_set_writes{};
        descriptor_set_writes.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_set_writes.dstBinding      = slot + PipelineStateVK::kShaderBindingTexture0;
        descriptor_set_writes.descriptorCount = 1;
        descriptor_set_writes.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_set_writes.pImageInfo      = &descriptor_image_info;

        m_device_vk->pVkCmdPushDescriptorSetKHR(m_command_buffer_handle, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineStateVK::sm_pipeline_layout_handle, 0, 1, &descriptor_set_writes);
    }
}

void GraphicsContextVK::SetPipelineState(const PipelineStateVK & pipeline_state)
{
    if (m_current_pipeline_state != &pipeline_state)
    {
        if (!pipeline_state.IsFinalized())
        {
            pipeline_state.Finalize();
        }

        m_current_pipeline_state = &pipeline_state;
        vkCmdBindPipeline(m_command_buffer_handle, VK_PIPELINE_BIND_POINT_GRAPHICS, m_current_pipeline_state->m_pipeline_handle);
        SetPrimitiveTopology(m_current_pipeline_state->m_topology);
    }
}

void GraphicsContextVK::SetPrimitiveTopology(const PrimitiveTopologyVK topology)
{
    MRQ2_ASSERT(m_current_pipeline_state != nullptr);

    if (m_current_topology != topology)
    {
        m_current_topology = topology;

        //
        // We're not able to dynamically set the primitive topology individually
        // without VK_EXT_extended_dynamic_state/vkCmdSetPrimitiveTopologyEXT extension
        // which doesn't seem to be widely supported yet, so create a new dynamic pipeline
        // with the same properties of whatever its the current one but with the desired
        // primitive topology.
        //
        if (m_current_topology != m_current_pipeline_state->m_topology)
        {
            PipelineStateVK dynamic_pipeline;
            dynamic_pipeline.Init(*m_current_pipeline_state);
            dynamic_pipeline.SetPrimitiveTopology(m_current_topology);
            dynamic_pipeline.CalcSignature();

            auto * cached_pipeline = FindOrRegisterPipeline(&dynamic_pipeline);
            MRQ2_ASSERT(cached_pipeline != nullptr);

            m_current_pipeline_state = cached_pipeline;
            vkCmdBindPipeline(m_command_buffer_handle, VK_PIPELINE_BIND_POINT_GRAPHICS, m_current_pipeline_state->m_pipeline_handle);
        }
    }
}

const PipelineStateVK * GraphicsContextVK::FindOrRegisterPipeline(PipelineStateVK * dynamic_pipeline)
{
    for (const PipelineStateVK & p : m_pipeline_cache)
    {
        if (p.GetSignature() == dynamic_pipeline->GetSignature())
        {
            return &p;
        }
    }

    dynamic_pipeline->Finalize();
    m_pipeline_cache.emplace_back(std::move(*dynamic_pipeline));
    return &m_pipeline_cache.back();
}

void GraphicsContextVK::Draw(const uint32_t first_vertex, const uint32_t vertex_count)
{
    const auto instance_count = 1u;
    const auto first_instance = 0u;
    vkCmdDraw(m_command_buffer_handle, vertex_count, instance_count, first_vertex, first_instance);
}

void GraphicsContextVK::DrawIndexed(const uint32_t first_index, const uint32_t index_count, const uint32_t base_vertex)
{
    const auto instance_count = 1u;
    const auto first_instance = 0u;
    vkCmdDrawIndexed(m_command_buffer_handle, index_count, instance_count, first_index, base_vertex, first_instance);
}

void GraphicsContextVK::PushMarker(const char * name)
{
    if (m_gpu_markers_enabled && m_device_vk->pVkCmdBeginDebugUtilsLabelEXT)
    {
        VkDebugUtilsLabelEXT label{};
        label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name;
        label.color[0] = 0.7f;
        label.color[1] = 0.0f;
        label.color[2] = 0.0f;
        label.color[3] = 1.0f;
        m_device_vk->pVkCmdBeginDebugUtilsLabelEXT(m_command_buffer_handle, &label);
    }
}

void GraphicsContextVK::PopMarker()
{
    if (m_gpu_markers_enabled && m_device_vk->pVkCmdEndDebugUtilsLabelEXT)
    {
        m_device_vk->pVkCmdEndDebugUtilsLabelEXT(m_command_buffer_handle);
    }
}

} // MrQ2
