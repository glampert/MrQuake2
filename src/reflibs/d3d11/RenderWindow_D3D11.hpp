//
// RenderWindow_D3D11.hpp
//  D3D11 rendering window.
//
#pragma once

#include "reflibs/shared/RefShared.hpp"
#include "reflibs/shared/OSWindow.hpp"

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_1.h>

namespace MrQ2
{
namespace D3D11
{

using Microsoft::WRL::ComPtr;
constexpr uint32_t kNumFrameBuffers = 2;

/*
===============================================================================

    D3D11 RenderWindow

===============================================================================
*/
class RenderWindow final
    : public OSWindow
{
public:

    // Device/swap-chain:
    ComPtr<ID3D11Device>           device;
    ComPtr<ID3D11DeviceContext>    device_context;
    ComPtr<IDXGISwapChain>         swap_chain;

    // Frame buffer:
    ComPtr<ID3D11Texture2D>        framebuffer_texture;
    ComPtr<ID3D11RenderTargetView> framebuffer_rtv;

    // Depth/stencil buffer:
    ComPtr<ID3D11Texture2D>        depth_stencil_texture;
    ComPtr<ID3D11DepthStencilView> depth_stencil_view;

    // TODO: Multi-buffer rendering (multiple frambuffers/RTVs like in the Dx12 impl, for kNumFrameBuffers).

private:

    void InitRenderWindow() override;
};

} // D3D11
} // MrQ2
