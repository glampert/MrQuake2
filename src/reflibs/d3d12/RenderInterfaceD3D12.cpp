//
// RenderInterfaceD3D12.cpp
//

#include "RenderInterfaceD3D12.hpp"

namespace MrQ2
{

void RenderInterfaceD3D12::Init(HINSTANCE hInst, WNDPROC wndProc, const int width, const int height, const bool fullscreen, const bool debug)
{
    GameInterface::Printf("**** RenderInterfaceD3D12::Init ****");

    // Window, device and swap-chain setup:
    const auto window_name = debug ? "MrQuake2 (D3D12 Debug)" : "MrQuake2 (D3D12)";
    m_window.Init(window_name, hInst, wndProc, width, height, fullscreen, debug);
    m_device.Init(debug);
    m_swap_chain.Init(m_device, m_window.WindowHandle(), fullscreen, width, height);

    // Global renderer states setup:
    m_descriptor_heap.Init(m_device, kMaxSrvDescriptors, kMaxDsvDescriptors, kMaxRtvDescriptors);
    m_render_targets.Init(m_device, m_swap_chain, m_descriptor_heap, width, height);
    m_upload_ctx.Init(m_device);
    m_graphics_ctx.Init(m_device);
}

void RenderInterfaceD3D12::Shutdown()
{
    GameInterface::Printf("**** RenderInterfaceD3D12::Shutdown ****");

    m_graphics_ctx.Shutdown();
    m_upload_ctx.Shutdown();
    m_render_targets.Shutdown();
    m_descriptor_heap.Shutdown();
    m_swap_chain.Shutdown();
    m_device.Shutdown();
    m_window.Shutdown();
}

void RenderInterfaceD3D12::BeginFrame()
{
    MRQ2_ASSERT(!m_frame_started);
    m_frame_started = true;
}

void RenderInterfaceD3D12::EndFrame()
{
    MRQ2_ASSERT(m_frame_started);
    m_frame_started = false;
}

} // MrQ2
