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
}

void PipelineStateVK::Shutdown()
{
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
