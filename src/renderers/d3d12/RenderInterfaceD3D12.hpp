//
// RenderInterfaceD3D12.hpp
//  Main header for the Dx12 back-end.
//
#pragma once

#include "../common/Win32Window.hpp"
#include "BufferD3D12.hpp"
#include "TextureD3D12.hpp"
#include "ShaderProgramD3D12.hpp"
#include "PipelineStateD3D12.hpp"
#include "GraphicsContextD3D12.hpp"
#include "UploadContextD3D12.hpp"
#include "DeviceD3D12.hpp"
#include "SwapChainD3D12.hpp"

namespace MrQ2
{

class RenderInterfaceD3D12 final
{
public:

    static constexpr uint32_t kNumFrameBuffers = kD12NumFrameBuffers;

    RenderInterfaceD3D12() = default;

    // Disallow copy.
    RenderInterfaceD3D12(const RenderInterfaceD3D12 &) = delete;
    RenderInterfaceD3D12 & operator=(const RenderInterfaceD3D12 &) = delete;

    void Init(HINSTANCE hInst, WNDPROC wndProc, const int width, const int height, const bool fullscreen, const bool debug);
    void Shutdown();

    void BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil);
    void EndFrame();
    void WaitForGpu();

    int RenderWidth() const;
    int RenderHeight() const;
    static bool IsFrameStarted();
    const DeviceD3D12 & Device() const;

private:

    Win32Window                 m_window;
    DeviceD3D12                 m_device;
    SwapChainD3D12              m_swap_chain;
	SwapChainRenderTargetsD3D12 m_render_targets;
    DescriptorHeapD3D12         m_descriptor_heap;
    UploadContextD3D12          m_upload_ctx;
    GraphicsContextD3D12        m_graphics_ctx;
    static bool                 sm_frame_started;
};

///////////////////////////////////////////////////////////////////////////////

inline int RenderInterfaceD3D12::RenderWidth() const
{
    return m_render_targets.render_target_width;
}

inline int RenderInterfaceD3D12::RenderHeight() const
{
    return m_render_targets.render_target_height;
}

inline bool RenderInterfaceD3D12::IsFrameStarted()
{
    return sm_frame_started;
}

inline const DeviceD3D12 & RenderInterfaceD3D12::Device() const
{
    return m_device;
}

///////////////////////////////////////////////////////////////////////////////

using Buffer                 = BufferD3D12;
using VertexBuffer           = VertexBufferD3D12;
using IndexBuffer            = IndexBufferD3D12;
using ConstantBuffer         = ConstantBufferD3D12;
using ScratchConstantBuffers = ScratchConstantBuffersD3D12;
using Texture                = TextureD3D12;
using TextureUpload          = TextureUploadD3D12;
using UploadContext          = UploadContextD3D12;
using VertexInputLayout      = VertexInputLayoutD3D12;
using ShaderProgram          = ShaderProgramD3D12;
using PrimitiveTopology      = PrimitiveTopologyD3D12;
using PipelineState          = PipelineStateD3D12;
using GraphicsContext        = GraphicsContextD3D12;
using RenderDevice           = DeviceD3D12;
using RenderInterface        = RenderInterfaceD3D12;

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
