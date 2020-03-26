//
// RenderWindow_D3D12.hpp
//  D3D12 rendering window.
//
#pragma once

#include "reflibs/shared/RefShared.hpp"
#include "reflibs/shared/OSWindow.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>

namespace MrQ2
{
namespace D3D12
{

using Microsoft::WRL::ComPtr;

/*
===============================================================================

    D3D12 RenderWindow

===============================================================================
*/
class RenderWindow final
    : public OSWindow
{
public:

    bool debug_validation = false; // Enable D3D-level debug validation?
    bool supports_rtx     = false; // Does our graphics card support RTX ray tracing?

    ComPtr<IDXGIFactory6>      factory;
    ComPtr<IDXGIAdapter3>      adapter;
    ComPtr<ID3D12Device5>      device;
    ComPtr<IDXGISwapChain4>    swap_chain;
    ComPtr<ID3D12CommandQueue> cmd_queue;
    DXGI_ADAPTER_DESC1         adapter_desc = {};

private:

    void InitRenderWindow() override;
};

} // D3D12
} // MrQ2
