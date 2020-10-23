//
// RenderInterfaceVK.hpp
//  Main header for the Vulkan back-end.
//
#pragma once

#include "../common/Win32Window.hpp"
#include "BufferVK.hpp"
#include "TextureVK.hpp"
#include "ShaderProgramVK.hpp"
#include "PipelineStateVK.hpp"
#include "GraphicsContextVK.hpp"
#include "UploadContextVK.hpp"
#include "DeviceVK.hpp"
#include "SwapChainVK.hpp"

namespace MrQ2
{

class RenderInterfaceVK final
{
public:

    static constexpr uint32_t kNumFrameBuffers = kVkNumFrameBuffers;

    RenderInterfaceVK() = default;

    // Disallow copy.
    RenderInterfaceVK(const RenderInterfaceVK &) = delete;
    RenderInterfaceVK & operator=(const RenderInterfaceVK &) = delete;

    void Init(HINSTANCE hInst, WNDPROC wndProc, const int width, const int height, const bool fullscreen, const bool debug);
    void Shutdown();

    void BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil);
    void EndFrame();
    void WaitForGpu();

    int RenderWidth() const;
    int RenderHeight() const;
    static bool IsFrameStarted();
    const DeviceVK & Device() const;

private:

    Win32Window              m_window;
    DeviceVK                 m_device;
    SwapChainVK              m_swap_chain;
	SwapChainRenderTargetsVK m_render_targets;
    UploadContextVK          m_upload_ctx;
    GraphicsContextVK        m_graphics_ctx;
    static bool              sm_frame_started;
};

///////////////////////////////////////////////////////////////////////////////

inline int RenderInterfaceVK::RenderWidth() const
{
    return m_render_targets.RenderTargetWidth();
}

inline int RenderInterfaceVK::RenderHeight() const
{
    return m_render_targets.RenderTargetHeight();
}

inline bool RenderInterfaceVK::IsFrameStarted()
{
    return sm_frame_started;
}

inline const DeviceVK & RenderInterfaceVK::Device() const
{
    return m_device;
}

///////////////////////////////////////////////////////////////////////////////

using Buffer                 = BufferVK;
using VertexBuffer           = VertexBufferVK;
using IndexBuffer            = IndexBufferVK;
using ConstantBuffer         = ConstantBufferVK;
using ScratchConstantBuffers = ScratchConstantBuffersVK;
using Texture                = TextureVK;
using TextureUpload          = TextureUploadVK;
using UploadContext          = UploadContextVK;
using VertexInputLayout      = VertexInputLayoutVK;
using ShaderProgram          = ShaderProgramVK;
using PrimitiveTopology      = PrimitiveTopologyVK;
using PipelineState          = PipelineStateVK;
using GraphicsContext        = GraphicsContextVK;
using RenderDevice           = DeviceVK;
using RenderInterface        = RenderInterfaceVK;

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
