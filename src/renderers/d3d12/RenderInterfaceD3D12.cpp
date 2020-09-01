//
// RenderInterfaceD3D12.cpp
//

#include "RenderInterfaceD3D12.hpp"
#include <dxgidebug.h>
#include <dxgi1_3.h>

namespace MrQ2
{

bool RenderInterfaceD3D12::sm_frame_started{ false };

void RenderInterfaceD3D12::Init(HINSTANCE hInst, WNDPROC wndProc, const int width, const int height, const bool fullscreen, const bool debug)
{
    GameInterface::Printf("**** RenderInterfaceD3D12::Init ****");

    // Window, device and swap-chain setup:
    const auto window_name = debug ? "MrQuake2 (D3D12 Debug)" : "MrQuake2 (D3D12)";
    m_window.Init(window_name, hInst, wndProc, width, height, fullscreen);
    m_device.Init(debug, m_descriptor_heap, m_upload_ctx, m_graphics_ctx, m_swap_chain);
    m_swap_chain.Init(m_device, m_window.WindowHandle(), fullscreen, width, height);

    // Global renderer states setup:
    m_descriptor_heap.Init(m_device);
    m_render_targets.Init(m_device, m_swap_chain, m_descriptor_heap, width, height);
    m_upload_ctx.Init(m_device);
    m_graphics_ctx.Init(m_device, m_swap_chain, m_render_targets);
    RootSignatureD3D12::CreateGlobalRootSignature(m_device);
}

void RenderInterfaceD3D12::Shutdown()
{
    GameInterface::Printf("**** RenderInterfaceD3D12::Shutdown ****");

    const bool debug_check_leaks = m_device.debug_validation;

    RootSignatureD3D12::sm_global.Shutdown();
    m_graphics_ctx.Shutdown();
    m_upload_ctx.Shutdown();
    m_render_targets.Shutdown();
    m_descriptor_heap.Shutdown();
    m_swap_chain.Shutdown();
    m_device.Shutdown();
    m_window.Shutdown();

    // At this point there should be no D3D objects left.
    if (debug_check_leaks)
    {
        D12ComPtr<IDXGIDebug1> debug_interface;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug_interface))))
        {
            debug_interface->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
    }
}

void RenderInterfaceD3D12::BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil)
{
    MRQ2_ASSERT(!sm_frame_started);
    sm_frame_started = true;

    // Flush any textures created by the last level load.
    m_upload_ctx.FlushTextureCreates();

    m_swap_chain.BeginFrame(m_render_targets);
    m_graphics_ctx.BeginFrame(clear_color, clear_depth, clear_stencil);
    m_graphics_ctx.SetViewport(0, 0, RenderWidth(), RenderHeight());
    m_graphics_ctx.SetScissorRect(0, 0, RenderWidth(), RenderHeight());
}

void RenderInterfaceD3D12::EndFrame()
{
    MRQ2_ASSERT(sm_frame_started);
    sm_frame_started = false;

    // Flush any textures created within this frame.
    m_upload_ctx.FlushTextureCreates();

    // Finish any texture uploads that were submitted this fame.
    m_upload_ctx.UpdateCompletedUploads();

    m_graphics_ctx.EndFrame();
    m_swap_chain.EndFrame(m_render_targets);
}

void RenderInterfaceD3D12::WaitForGpu()
{
    m_swap_chain.FullGpuSynch();
}

} // MrQ2
