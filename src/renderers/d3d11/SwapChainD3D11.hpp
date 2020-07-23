//
// SwapChainD3D11.hpp
//
#pragma once

#include "UtilsD3D11.hpp"

namespace MrQ2
{

class SwapChainD3D11 final
{
public:

    D11ComPtr<ID3D11Device>        device;
    D11ComPtr<ID3D11DeviceContext> context;
    D11ComPtr<IDXGISwapChain>      swap_chain;

    SwapChainD3D11() = default;

    // Disallow copy.
    SwapChainD3D11(const SwapChainD3D11 &) = delete;
    SwapChainD3D11 & operator=(const SwapChainD3D11 &) = delete;

    void Init(HWND hWnd, const bool fullscreen, const int width, const int height, const bool debug);
    void Shutdown();
    void Present();
};

class SwapChainRenderTargetsD3D11 final
{
public:

    // Frame buffer:
    D11ComPtr<ID3D11Texture2D>        framebuffer_texture;
    D11ComPtr<ID3D11RenderTargetView> framebuffer_rtv;

    // Depth/stencil buffer:
    D11ComPtr<ID3D11Texture2D>        depth_stencil_texture;
    D11ComPtr<ID3D11DepthStencilView> depth_stencil_view;

    int render_target_width{ 0 };
    int render_target_height{ 0 };

    SwapChainRenderTargetsD3D11() = default;

    // Disallow copy.
    SwapChainRenderTargetsD3D11(const SwapChainRenderTargetsD3D11 &) = delete;
    SwapChainRenderTargetsD3D11 & operator=(const SwapChainRenderTargetsD3D11 &) = delete;

    void Init(const SwapChainD3D11 & sc, const int width, const int height);
    void Shutdown();
};

} // MrQ2
