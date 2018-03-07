//
// RenderWindow.hpp
//  D3D11 rendering window.
//
#pragma once

#include "reflibs/shared/RefShared.hpp"
#include "reflibs/shared/OSWindow.hpp"

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>

namespace MrQ2
{
namespace D3D11
{

using Microsoft::WRL::ComPtr;

/*
===============================================================================

    D3D11 RenderWindow

===============================================================================
*/
class RenderWindow final
    : public OSWindow
{
public:

    bool debug_validation = false;

    ComPtr<ID3D11Device>           device;
    ComPtr<ID3D11DeviceContext>    device_context;
    ComPtr<IDXGISwapChain>         swap_chain;
    ComPtr<ID3D11Texture2D>        framebuffer_texture;
    ComPtr<ID3D11RenderTargetView> framebuffer_rtv;

private:

    void InitRenderWindow() override;
};

} // D3D11
} // MrQ2
