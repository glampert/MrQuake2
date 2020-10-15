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

    SwapChainD3D11() = default;

    // Disallow copy.
    SwapChainD3D11(const SwapChainD3D11 &) = delete;
    SwapChainD3D11 & operator=(const SwapChainD3D11 &) = delete;

    void Init(HWND hWnd, const bool fullscreen, const int width, const int height, const bool debug);
    void Shutdown();
    void Present();

    ID3D11Device * Device() const { return m_device.Get(); }
    ID3D11DeviceContext * DeviceContext() const { return m_context.Get(); }
    IDXGISwapChain * SwapChain() const { return m_swap_chain.Get(); }

private:

    D11ComPtr<ID3D11Device>        m_device;
    D11ComPtr<ID3D11DeviceContext> m_context;
    D11ComPtr<IDXGISwapChain>      m_swap_chain;
};

class SwapChainRenderTargetsD3D11 final
{
    friend class GraphicsContextD3D11;

public:

    SwapChainRenderTargetsD3D11() = default;

    // Disallow copy.
    SwapChainRenderTargetsD3D11(const SwapChainRenderTargetsD3D11 &) = delete;
    SwapChainRenderTargetsD3D11 & operator=(const SwapChainRenderTargetsD3D11 &) = delete;

    void Init(const SwapChainD3D11 & sc, const int width, const int height);
    void Shutdown();

    int RenderTargetWidth()  const { return m_render_target_width;  }
    int RenderTargetHeight() const { return m_render_target_height; }

private:

    int m_render_target_width{ 0 };
    int m_render_target_height{ 0 };

    // Frame buffer:
    D11ComPtr<ID3D11Texture2D>        m_framebuffer_texture;
    D11ComPtr<ID3D11RenderTargetView> m_framebuffer_rtv;

    // Depth/stencil buffer:
    D11ComPtr<ID3D11Texture2D>        m_depth_stencil_texture;
    D11ComPtr<ID3D11DepthStencilView> m_depth_stencil_view;
};

} // MrQ2
