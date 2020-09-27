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

void GraphicsContextVK::Init(const DeviceVK & device, const SwapChainVK & swap_chain, const SwapChainRenderTargetsVK & render_targets)
{
}

void GraphicsContextVK::Shutdown()
{
}

void GraphicsContextVK::BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil)
{
}

void GraphicsContextVK::EndFrame()
{
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
