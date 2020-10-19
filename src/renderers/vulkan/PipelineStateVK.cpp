//
// PipelineStateVK.cpp
//

#include "PipelineStateVK.hpp"
#include "ShaderProgramVK.hpp"
#include "DeviceVK.hpp"

namespace MrQ2
{

void PipelineStateVK::Init(const DeviceVK & device)
{
    MRQ2_ASSERT(m_device_vk == nullptr);

    m_device_vk = &device;
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

    if (m_pipeline_layout_handle != nullptr)
    {
        vkDestroyPipelineLayout(m_device_vk->Handle(), m_pipeline_layout_handle, nullptr);
        m_pipeline_layout_handle = nullptr;
    }

    m_device_vk = nullptr;
}

void PipelineStateVK::SetPrimitiveTopology(const PrimitiveTopologyVK topology)
{
}

void PipelineStateVK::SetShaderProgram(const ShaderProgramVK & shader_prog)
{
    //if (!shader_prog.m_is_loaded)
    //{
        //GameInterface::Errorf("PipelineStateVK: Trying to set an invalid shader program.");
    //}
}

void PipelineStateVK::SetDepthTestEnabled(const bool enabled)
{
}

void PipelineStateVK::SetDepthWritesEnabled(const bool enabled)
{
}

void PipelineStateVK::SetAlphaBlendingEnabled(const bool enabled)
{
}

void PipelineStateVK::SetAdditiveBlending(const bool enabled)
{
}

void PipelineStateVK::SetCullEnabled(const bool enabled)
{
}

void PipelineStateVK::Finalize() const
{
    if (IsFinalized())
    {
        return;
    }
}

} // MrQ2
