//
// PipelineStateD3D12.hpp
//
#pragma once

#include "UtilsD3D12.hpp"

namespace MrQ2
{

class ShaderProgramD3D12;

enum class PrimitiveTopologyD3D12 : uint8_t
{
    kTriangleList,
    kTriangleStrip,
    kTriangleFan
};

class PipelineStateD3D12 final
{
    friend class GraphicsContextD3D12;

public:

    void SetPrimitiveTopology(const PrimitiveTopologyD3D12 topology);
    void SetShaderProgram(const ShaderProgramD3D12 & shader_prog);

    void SetDepthTestEnabled(const bool enabled);
    void SetDepthWritesEnabled(const bool enabled);
    void SetAlphaBlendingEnabled(const bool enabled);

private:

    D12ComPtr<ID3D12PipelineState> m_state;
};

} // MrQ2
