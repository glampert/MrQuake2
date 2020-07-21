//
// SwapChainD3D11.cpp
//

#include "../common/Win32Window.hpp"
#include "SwapChainD3D11.hpp"
#include "DeviceD3D11.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// SwapChainD3D11
///////////////////////////////////////////////////////////////////////////////

void SwapChainD3D11::Init(const DeviceD3D11 & device, HWND hWnd, const bool fullscreen, const int width, const int height)
{
}

void SwapChainD3D11::Shutdown()
{
}

void SwapChainD3D11::BeginFrame(const SwapChainRenderTargetsD3D11 & render_targets)
{
}

void SwapChainD3D11::EndFrame(const SwapChainRenderTargetsD3D11 & render_targets)
{
}

///////////////////////////////////////////////////////////////////////////////
// SwapChainRenderTargetsD3D11
///////////////////////////////////////////////////////////////////////////////

void SwapChainRenderTargetsD3D11::Init(const DeviceD3D11 & device, const SwapChainD3D11 & swap_chain, const int width, const int height)
{
    render_target_width  = width;
    render_target_height = height;
}

void SwapChainRenderTargetsD3D11::Shutdown()
{
}

} // MrQ2
