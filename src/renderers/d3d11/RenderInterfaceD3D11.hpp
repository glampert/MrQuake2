//
// RenderInterfaceD3D11.hpp
//  Main header for the Dx11 back-end.
//
#pragma once

#include "../common/Win32Window.hpp"
#include "BufferD3D11.hpp"
#include "TextureD3D11.hpp"
#include "ShaderProgramD3D11.hpp"
#include "PipelineStateD3D11.hpp"
#include "GraphicsContextD3D11.hpp"
#include "UploadContextD3D11.hpp"
#include "DeviceD3D11.hpp"
#include "SwapChainD3D11.hpp"

namespace MrQ2
{

class RenderInterfaceD3D11 final
{
public:

    static constexpr uint32_t kNumFrameBuffers = kD11NumFrameBuffers;

    RenderInterfaceD3D11() = default;

    // Disallow copy.
    RenderInterfaceD3D11(const RenderInterfaceD3D11 &) = delete;
    RenderInterfaceD3D11 & operator=(const RenderInterfaceD3D11 &) = delete;

    void Init(HINSTANCE hInst, WNDPROC wndProc, const int width, const int height, const bool fullscreen, const bool debug);
    void Shutdown();

    void BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil);
    void EndFrame();
    void WaitForGpu() {} // Not required for this backend.

    int RenderWidth() const;
    int RenderHeight() const;
    bool IsFrameStarted() const;
    const DeviceD3D11 & Device() const;

private:

    Win32Window                 m_window;
    DeviceD3D11                 m_device;
    SwapChainD3D11              m_swap_chain;
	SwapChainRenderTargetsD3D11 m_render_targets;
    UploadContextD3D11          m_upload_ctx;
    GraphicsContextD3D11        m_graphics_ctx;
    bool                        m_frame_started{ false };
};

///////////////////////////////////////////////////////////////////////////////

inline int RenderInterfaceD3D11::RenderWidth() const
{
    return m_render_targets.render_target_width;
}

inline int RenderInterfaceD3D11::RenderHeight() const
{
    return m_render_targets.render_target_height;
}

inline bool RenderInterfaceD3D11::IsFrameStarted() const
{
    return m_frame_started;
}

inline const DeviceD3D11 & RenderInterfaceD3D11::Device() const
{
    return m_device;
}

///////////////////////////////////////////////////////////////////////////////

using Buffer                 = BufferD3D11;
using VertexBuffer           = VertexBufferD3D11;
using IndexBuffer            = IndexBufferD3D11;
using ConstantBuffer         = ConstantBufferD3D11;
using ScratchConstantBuffers = ScratchConstantBuffersD3D11;
using Texture                = TextureD3D11;
using TextureUpload          = TextureUploadD3D11;
using UploadContext          = UploadContextD3D11;
using VertexInputLayout      = VertexInputLayoutD3D11;
using ShaderProgram          = ShaderProgramD3D11;
using PrimitiveTopology      = PrimitiveTopologyD3D11;
using PipelineState          = PipelineStateD3D11;
using GraphicsContext        = GraphicsContextD3D11;
using RenderDevice           = DeviceD3D11;
using RenderInterface        = RenderInterfaceD3D11;

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
