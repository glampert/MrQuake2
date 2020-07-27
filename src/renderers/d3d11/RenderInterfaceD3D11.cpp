//
// RenderInterfaceD3D11.cpp
//

#include "RenderInterfaceD3D11.hpp"
#include <dxgidebug.h>
#include <dxgi1_3.h>

namespace MrQ2
{

void RenderInterfaceD3D11::Init(HINSTANCE hInst, WNDPROC wndProc, const int width, const int height, const bool fullscreen, const bool debug)
{
    GameInterface::Printf("**** RenderInterfaceD3D11::Init ****");

    // Window, device and swap-chain setup:
    const auto window_name = debug ? "MrQuake2 (D3D11 Debug)" : "MrQuake2 (D3D11)";
    m_window.Init(window_name, hInst, wndProc, width, height, fullscreen);
    m_swap_chain.Init(m_window.WindowHandle(), fullscreen, width, height, debug);
    m_device.Init(m_swap_chain, debug, m_upload_ctx, m_graphics_ctx);

    // Global renderer states setup:
    m_render_targets.Init(m_swap_chain, width, height);
    m_upload_ctx.Init(m_device);
    m_graphics_ctx.Init(m_device, m_swap_chain, m_render_targets);

    GameInterface::Cmd::RegisterCommand("set_tex_filer", &Texture::ChangeTextureFilterCmd);
}

void RenderInterfaceD3D11::Shutdown()
{
    GameInterface::Printf("**** RenderInterfaceD3D11::Shutdown ****");

    GameInterface::Cmd::RemoveCommand("set_tex_filer");

    const bool debug_check_leaks = m_device.debug_validation;

    m_graphics_ctx.Shutdown();
    m_upload_ctx.Shutdown();
    m_render_targets.Shutdown();
    m_device.Shutdown();
    m_swap_chain.Shutdown();
    m_window.Shutdown();

    // At this point there should be no D3D objects left.
    if (debug_check_leaks)
    {
        D11ComPtr<IDXGIDebug1> debug_interface;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug_interface))))
        {
            debug_interface->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
    }
}

void RenderInterfaceD3D11::BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil)
{
    MRQ2_ASSERT(!m_frame_started);
    m_frame_started = true;

    m_graphics_ctx.BeginFrame(clear_color, clear_depth, clear_stencil);
    m_graphics_ctx.SetViewport(0, 0, RenderWidth(), RenderHeight());
    m_graphics_ctx.SetScissorRect(0, 0, RenderWidth(), RenderHeight());
}

void RenderInterfaceD3D11::EndFrame()
{
    MRQ2_ASSERT(m_frame_started);
    m_frame_started = false;

    m_graphics_ctx.EndFrame();
    m_swap_chain.Present();
}

} // MrQ2
