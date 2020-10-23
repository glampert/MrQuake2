//
// RenderInterfaceVK.cpp
//

#include "RenderInterfaceVK.hpp"

namespace MrQ2
{

bool RenderInterfaceVK::sm_frame_started{ false };

void RenderInterfaceVK::Init(HINSTANCE hInst, WNDPROC wndProc, const int width, const int height, const bool fullscreen, const bool debug)
{
    GameInterface::Printf("**** RenderInterfaceVK::Init ****");

    // Window, device and swap-chain setup:
    const auto window_name = debug ? "MrQuake2 (Vulkan Debug)" : "MrQuake2 (Vulkan)";
    m_window.Init(window_name, hInst, wndProc, width, height, fullscreen);
    m_device.Init(m_window, m_upload_ctx, m_graphics_ctx, m_render_targets, debug);
    m_swap_chain.Init(m_device, width, height, m_render_targets);

    // Global renderer states setup:
    m_render_targets.Init(m_device, m_swap_chain);
    m_upload_ctx.Init(m_device, m_swap_chain);
    m_graphics_ctx.Init(m_device, m_swap_chain, m_render_targets);
    PipelineStateVK::InitGlobalState(m_device);
}

void RenderInterfaceVK::Shutdown()
{
    GameInterface::Printf("**** RenderInterfaceVK::Shutdown ****");

    PipelineStateVK::ShutdownGlobalState(m_device);
    m_graphics_ctx.Shutdown();
    m_upload_ctx.Shutdown();
    m_render_targets.Shutdown();
    m_swap_chain.Shutdown();
    m_device.Shutdown();
    m_window.Shutdown();
}

void RenderInterfaceVK::BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil)
{
    MRQ2_ASSERT(!sm_frame_started);
    sm_frame_started = true;

    // Flush any textures created by the last level load.
    m_upload_ctx.FlushTextureCreates();

    m_swap_chain.BeginFrame();
    m_graphics_ctx.BeginFrame(clear_color, clear_depth, clear_stencil);
    m_graphics_ctx.SetViewport(0, 0, RenderWidth(), RenderHeight());
    m_graphics_ctx.SetScissorRect(0, 0, RenderWidth(), RenderHeight());
}

void RenderInterfaceVK::EndFrame()
{
    MRQ2_ASSERT(sm_frame_started);
    sm_frame_started = false;

    // Finish the main VK RenderPass.
    m_graphics_ctx.EndFrame();

    // Flush any textures created within this frame.
    m_upload_ctx.FlushTextureCreates();

    // Finish any texture uploads that were submitted this fame.
    m_upload_ctx.UpdateCompletedUploads();

    m_swap_chain.EndFrame();
}

void RenderInterfaceVK::WaitForGpu()
{
    vkDeviceWaitIdle(m_device.Handle());
}

} // MrQ2
