//
// SwapChainD3D11.hpp
//
#pragma once

#include "UtilsD3D11.hpp"

namespace MrQ2
{

class DeviceD3D11;
class SwapChainD3D11;
class SwapChainRenderTargetsD3D11;

class SwapChainD3D11 final
{
public:

    SwapChainD3D11() = default;

    // Disallow copy.
    SwapChainD3D11(const SwapChainD3D11 &) = delete;
    SwapChainD3D11 & operator=(const SwapChainD3D11 &) = delete;

    void Init(const DeviceD3D11 & device, HWND hWnd, const bool fullscreen, const int width, const int height);
    void Shutdown();

    void BeginFrame(const SwapChainRenderTargetsD3D11 & render_targets);
    void EndFrame(const SwapChainRenderTargetsD3D11 & render_targets);
};

class SwapChainRenderTargetsD3D11 final
{
public:

    int render_target_width{ 0 };
    int render_target_height{ 0 };

    SwapChainRenderTargetsD3D11() = default;

    // Disallow copy.
    SwapChainRenderTargetsD3D11(const SwapChainRenderTargetsD3D11 &) = delete;
    SwapChainRenderTargetsD3D11 & operator=(const SwapChainRenderTargetsD3D11 &) = delete;

    void Init(const DeviceD3D11 & device, const SwapChainD3D11 & swap_chain, const int width, const int height);
    void Shutdown();
};

} // MrQ2
