//
// PipelineStateVK.cpp
//

#include "PipelineStateVK.hpp"
#include "ShaderProgramVK.hpp"
#include "SwapChainVK.hpp"
#include "DeviceVK.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// PipelineStateCreateInfoVK
///////////////////////////////////////////////////////////////////////////////

struct PipelineStateCreateInfoVK final
{
    VkPipelineShaderStageCreateInfo        shader_stages[2] = {}; // VertexShader[0], PixelShader[1]
    VkViewport                             viewport_rect{};
    VkRect2D                               scissor_rect{};
    VkPipelineViewportStateCreateInfo      viewport_state{};
    VkPipelineVertexInputStateCreateInfo   vertex_input_state{};
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
    VkPipelineRasterizationStateCreateInfo rasterizer_state{};
    VkPipelineColorBlendStateCreateInfo    blend_state{};
    VkPipelineColorBlendAttachmentState    blend_attachment_state{};
    VkPipelineMultisampleStateCreateInfo   multi_sampling_state{};
    VkPipelineDepthStencilStateCreateInfo  depth_stencil_state{};
    VkPipelineTessellationStateCreateInfo  tessellation_state{};
    VkPipelineDynamicStateCreateInfo       dynamic_states{};
    const VkDynamicState                   dynamic_state_flags[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkGraphicsPipelineCreateInfo           pipeline_state{};

    void SetDefaults();
};

void PipelineStateCreateInfoVK::SetDefaults()
{
    vertex_input_state.sType                  = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    tessellation_state.sType                  = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

    dynamic_states.sType                      = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_states.dynamicStateCount          = ArrayLength(dynamic_state_flags);
    dynamic_states.pDynamicStates             = dynamic_state_flags;

    // NOTE: Viewport and scissor rect are dynamic states.
    viewport_rect.minDepth                    = 0.0f;
    viewport_rect.maxDepth                    = 1.0f;

    viewport_state.sType                      = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount              = 1;
    viewport_state.pViewports                 = &viewport_rect;
    viewport_state.scissorCount               = 1;
    viewport_state.pScissors                  = &scissor_rect;

    input_assembly_state.sType                = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.topology             = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    rasterizer_state.sType                    = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_state.depthClampEnable         = VK_FALSE;
    rasterizer_state.rasterizerDiscardEnable  = VK_FALSE;
    rasterizer_state.polygonMode              = VK_POLYGON_MODE_FILL;
    rasterizer_state.cullMode                 = VK_CULL_MODE_BACK_BIT;   // Backface culling
    rasterizer_state.frontFace                = VK_FRONT_FACE_CLOCKWISE; // CW
    rasterizer_state.depthBiasEnable          = VK_FALSE;
    rasterizer_state.lineWidth                = 1.0f;

    blend_state.sType                         = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state.logicOpEnable                 = VK_FALSE;
    blend_state.logicOp                       = VK_LOGIC_OP_CLEAR;
    blend_state.attachmentCount               = 1; // Defaults to 1 (a default screen color framebuffer).
    blend_state.pAttachments                  = &blend_attachment_state;
    blend_attachment_state.colorWriteMask     = (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

    multi_sampling_state.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multi_sampling_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    depth_stencil_state.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state.depthTestEnable       = VK_TRUE;
    depth_stencil_state.depthWriteEnable      = VK_TRUE;
    depth_stencil_state.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL; // RH OpenGL-style projection, depth-clear=1
    depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_state.stencilTestEnable     = VK_FALSE;
    depth_stencil_state.minDepthBounds        = 0.0f;
    depth_stencil_state.maxDepthBounds        = 1.0f;

    pipeline_state.sType                      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_state.stageCount                 = ArrayLength(shader_stages);
    pipeline_state.pStages                    = shader_stages;
    pipeline_state.pVertexInputState          = &vertex_input_state;
    pipeline_state.pInputAssemblyState        = &input_assembly_state;
    pipeline_state.pTessellationState         = &tessellation_state;
    pipeline_state.pViewportState             = &viewport_state;
    pipeline_state.pRasterizationState        = &rasterizer_state;
    pipeline_state.pMultisampleState          = &multi_sampling_state;
    pipeline_state.pDepthStencilState         = &depth_stencil_state;
    pipeline_state.pColorBlendState           = &blend_state;
    pipeline_state.pDynamicState              = &dynamic_states;
}

///////////////////////////////////////////////////////////////////////////////
// PipelineStateVK
///////////////////////////////////////////////////////////////////////////////

void PipelineStateVK::Init(const DeviceVK & device)
{
    MRQ2_ASSERT(m_device_vk == nullptr);
    m_device_vk = &device;

    // Default states:
    //  Blending: Alpha blending OFF
    //  Rasterizer state: Backface cull ON
    //  Depth-stencil state: Depth test ON, depth write ON, stencil OFF
    m_flags = kDepthTestEnabled | kDepthWriteEnabled | kCullEnabled;
}

void PipelineStateVK::Shutdown()
{
    if (m_device_vk == nullptr)
    {
        return;
    }

    if (m_pipeline_handle != nullptr)
    {
        vkDestroyPipeline(m_device_vk->Handle(), m_pipeline_handle, nullptr);
        m_pipeline_handle = nullptr;
    }

    m_device_vk   = nullptr;
    m_shader_prog = nullptr;
    m_flags       = kNoFlags;
}

void PipelineStateVK::SetPrimitiveTopology(const PrimitiveTopologyVK topology)
{
    m_topology = topology;
}

void PipelineStateVK::SetShaderProgram(const ShaderProgramVK & shader_prog)
{
    if (!shader_prog.m_vs_handle || !shader_prog.m_ps_handle)
    {
        GameInterface::Errorf("PipelineStateVK: Trying to set an invalid shader program.");
    }
    m_shader_prog = &shader_prog;
}

void PipelineStateVK::SetDepthTestEnabled(const bool enabled)
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

void PipelineStateVK::SetDepthWritesEnabled(const bool enabled)
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

void PipelineStateVK::SetAlphaBlendingEnabled(const bool enabled)
{
    if (enabled)
    {
        m_flags |= kAlphaBlendEnabled;
        // blendConstants = {1,1,1,1}
    }
    else
    {
        m_flags &= ~kAlphaBlendEnabled;
        // blendConstants = {0,0,0,0}
    }
}

void PipelineStateVK::SetAdditiveBlending(const bool enabled)
{
    if (enabled)
    {
        m_flags |= kAdditiveBlending;
    }
    else
    {
        m_flags &= ~kAdditiveBlending;
    }
}

void PipelineStateVK::SetCullEnabled(const bool enabled)
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

void PipelineStateVK::Finalize() const
{
    if (IsFinalized())
    {
        return;
    }

    MRQ2_ASSERT(m_device_vk != nullptr);
    if (m_shader_prog == nullptr)
    {
        GameInterface::Errorf("PipelineStateVK: No shader program has been set!");
    }

    PipelineStateCreateInfoVK pipeline_info;
    pipeline_info.SetDefaults();

    // Depth-stencil states:
    if (m_flags & kDepthTestEnabled)
    {
        pipeline_info.depth_stencil_state.depthTestEnable = VK_TRUE;
        pipeline_info.depth_stencil_state.depthCompareOp  = VK_COMPARE_OP_LESS_OR_EQUAL;
    }
    else
    {
        pipeline_info.depth_stencil_state.depthTestEnable = VK_FALSE;
        pipeline_info.depth_stencil_state.depthCompareOp  = VK_COMPARE_OP_ALWAYS;
    }

    // Depth buffer writers: ON|OFF
    if (m_flags & kDepthWriteEnabled)
    {
        pipeline_info.depth_stencil_state.depthWriteEnable = VK_TRUE;
    }
    else
    {
        pipeline_info.depth_stencil_state.depthWriteEnable = VK_FALSE;
    }

    // Rasterizer states:
    if (m_flags & kCullEnabled)
    {
        pipeline_info.rasterizer_state.cullMode = VK_CULL_MODE_BACK_BIT; // Backface culling
    }
    else
    {
        pipeline_info.rasterizer_state.cullMode = VK_CULL_MODE_NONE;
    }

    // Blend states:
    if (m_flags & kAlphaBlendEnabled)
    {
        const bool additive_blending = (m_flags & kAdditiveBlending);

        pipeline_info.blend_state.blendConstants[0] = 1.0f;
        pipeline_info.blend_state.blendConstants[1] = 1.0f;
        pipeline_info.blend_state.blendConstants[2] = 1.0f;
        pipeline_info.blend_state.blendConstants[3] = 1.0f;

        pipeline_info.blend_attachment_state.blendEnable         = VK_TRUE;
        pipeline_info.blend_attachment_state.srcColorBlendFactor = (additive_blending ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA);
        pipeline_info.blend_attachment_state.dstColorBlendFactor = (additive_blending ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        pipeline_info.blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
        pipeline_info.blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipeline_info.blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        pipeline_info.blend_attachment_state.alphaBlendOp        = VK_BLEND_OP_ADD;
    }
    else
    {
        pipeline_info.blend_state.blendConstants[0] = 0.0f;
        pipeline_info.blend_state.blendConstants[1] = 0.0f;
        pipeline_info.blend_state.blendConstants[2] = 0.0f;
        pipeline_info.blend_state.blendConstants[3] = 0.0f;

        pipeline_info.blend_attachment_state.blendEnable         = VK_FALSE;
        pipeline_info.blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        pipeline_info.blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        pipeline_info.blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
        pipeline_info.blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipeline_info.blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        pipeline_info.blend_attachment_state.alphaBlendOp        = VK_BLEND_OP_ADD;
    }

    // Debug lines or filled triangles?
    if (m_topology == PrimitiveTopologyVK::kLineList)
    {
        pipeline_info.input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    }
    else
    {
        pipeline_info.input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    // Shader stages and vertex input layout:
    pipeline_info.vertex_input_state.pVertexBindingDescriptions      = &m_shader_prog->m_binding_description;
    pipeline_info.vertex_input_state.vertexBindingDescriptionCount   = 1;
    pipeline_info.vertex_input_state.pVertexAttributeDescriptions    = m_shader_prog->m_attribute_descriptions;
    pipeline_info.vertex_input_state.vertexAttributeDescriptionCount = m_shader_prog->m_attribute_count;

    static_assert(ArrayLength(pipeline_info.shader_stages) == ShaderProgramVK::kNumShaderStages);
    m_shader_prog->GetPipelineStages(pipeline_info.shader_stages);
    pipeline_info.pipeline_state.stageCount = ArrayLength(pipeline_info.shader_stages);

    MRQ2_ASSERT(sm_pipeline_layout_handle != nullptr);
    pipeline_info.pipeline_state.layout = sm_pipeline_layout_handle;
    pipeline_info.pipeline_state.renderPass = m_device_vk->ScRenderTargets().MainRenderPassHandle();

    VULKAN_CHECK(vkCreateGraphicsPipelines(m_device_vk->Handle(), nullptr, 1, &pipeline_info.pipeline_state, nullptr, &m_pipeline_handle));
    MRQ2_ASSERT(m_pipeline_handle != nullptr);

    m_flags |= kFinalized;
}

///////////////////////////////////////////////////////////////////////////////
// Global Descriptor Set & Pipeline Layout
///////////////////////////////////////////////////////////////////////////////

// All shaders share the same descriptor set.
VkPipelineLayout PipelineStateVK::sm_pipeline_layout_handle;
DescriptorSetVK  PipelineStateVK::sm_global_descriptor_set;

void PipelineStateVK::InitGlobalDescriptorSet(const DeviceVK & device)
{
    VkDescriptorPoolSize descriptor_pool_sizes[2] = {};
    {
        unsigned i = 0;

        // Constant buffers:
        descriptor_pool_sizes[i].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_pool_sizes[i].descriptorCount = kCBufferCount;
        ++i;

        // Samplers/textures:
        descriptor_pool_sizes[i].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_pool_sizes[i].descriptorCount = kTextureCount;
        ++i;

        MRQ2_ASSERT(i == ArrayLength(descriptor_pool_sizes));
    }

    VkDescriptorSetLayoutBinding descriptor_set_bindings[kCBufferCount + kTextureCount] = {};
    {
		unsigned i = 0;

        // Constant buffers:
        descriptor_set_bindings[i].binding         = 0; // cbuffer PerFrameShaderConstants : register(b0)
        descriptor_set_bindings[i].descriptorCount = 1;
        descriptor_set_bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_set_bindings[i].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        ++i;

        descriptor_set_bindings[i].binding         = 1; // cbuffer PerViewShaderConstants : register(b1)
        descriptor_set_bindings[i].descriptorCount = 1;
        descriptor_set_bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_set_bindings[i].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        ++i;

        descriptor_set_bindings[i].binding         = 2; // cbuffer PerDrawShaderConstants : register(b2)
        descriptor_set_bindings[i].descriptorCount = 1;
        descriptor_set_bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_set_bindings[i].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        ++i;

        // Samplers/textures:
        descriptor_set_bindings[i].binding         = 3; // SamplerState diffuse_sampler: register(s0)
        descriptor_set_bindings[i].descriptorCount = 1;
        descriptor_set_bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_set_bindings[i].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        ++i;

        descriptor_set_bindings[i].binding         = 4; // SamplerState lightmap_sampler : register(s1);
        descriptor_set_bindings[i].descriptorCount = 1;
        descriptor_set_bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_set_bindings[i].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        ++i;

        MRQ2_ASSERT(i == ArrayLength(descriptor_set_bindings));
    }

    sm_global_descriptor_set.Init(device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR, // We are using VK_KHR_push_descriptor
                                  ArrayBase<const VkDescriptorPoolSize>::from_c_array(descriptor_pool_sizes),
                                  ArrayBase<const VkDescriptorSetLayoutBinding>::from_c_array(descriptor_set_bindings));

    // VkPipelineLayout
    const VkDescriptorSetLayout descriptor_set_layouts[] = { sm_global_descriptor_set.LayoutHandle() };

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount         = ArrayLength(descriptor_set_layouts);
    pipeline_layout_info.pSetLayouts            = descriptor_set_layouts;
    pipeline_layout_info.pushConstantRangeCount = 0;       // TODO
    pipeline_layout_info.pPushConstantRanges    = nullptr; // TODO

    VULKAN_CHECK(vkCreatePipelineLayout(device.Handle(), &pipeline_layout_info, nullptr, &sm_pipeline_layout_handle));
    MRQ2_ASSERT(sm_pipeline_layout_handle != nullptr);
}

void PipelineStateVK::ShutdownGlobalDescriptorSet(const DeviceVK & device)
{
    if (sm_pipeline_layout_handle != nullptr)
    {
        vkDestroyPipelineLayout(device.Handle(), sm_pipeline_layout_handle, nullptr);
        sm_pipeline_layout_handle = nullptr;
    }

    sm_global_descriptor_set.Shutdown();
}

} // MrQ2
