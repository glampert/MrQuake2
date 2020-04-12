//
// RenderWindow_D3D12.hpp
//  D3D12 rendering window.
//
#pragma once

#include "reflibs/shared/RefShared.hpp"
#include "reflibs/shared/OSWindow.hpp"

#include <dxgi1_6.h>
#include <d3d12.h>

namespace MrQ2
{
namespace D3D12
{

using Microsoft::WRL::ComPtr;
constexpr uint32_t kNumFrameBuffers = 3; // Triple-buffering

/*
===============================================================================

    D3D12 Device & SwapChain helpers

===============================================================================
*/

struct DeviceHelper final
{
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter3> adapter;
    ComPtr<ID3D12Device5> device;
    size_t                dedicated_video_memory  = 0;
    size_t                dedicated_system_memory = 0;
    size_t                shared_system_memory    = 0;
    bool                  supports_rtx            = false; // Does our graphics card support RTX ray tracing?
    std::string           adapter_info;

    void InitAdapterAndDevice(const bool debug_validation);
};

struct SwapChainHelper final
{
    HANDLE                            fence_event = nullptr;
    uint64_t                          fence_values[kNumFrameBuffers] = {};
    uint64_t                          frame_count = 0;
    uint32_t                          frame_index = 0;
    ComPtr<ID3D12Fence>               fence;
    ComPtr<ID3D12CommandQueue>        command_queue;
    ComPtr<ID3D12GraphicsCommandList> gfx_command_list;
    ComPtr<ID3D12CommandAllocator>    command_allocators[kNumFrameBuffers];
    ComPtr<IDXGISwapChain4>           swap_chain;

    void InitSwapChain(IDXGIFactory6 * factory, ID3D12Device5 * device, HWND hwnd,
                       const bool fullscreen, const int width, const int height);

    ~SwapChainHelper();

    void MoveToNextFrame();
    void FullGpuSynch();

private:

    void InitSyncFence(ID3D12Device5 * device);
    void InitCmdList(ID3D12Device5 * device);
};

struct RenderTargets final
{
    ComPtr<ID3D12DescriptorHeap> rtv_descriptor_heap; // Render Target View (RTV) heap
    ComPtr<ID3D12Resource>       render_rarget_resources[kNumFrameBuffers]   = {};
    D3D12_CPU_DESCRIPTOR_HANDLE  render_target_descriptors[kNumFrameBuffers] = {};

    void InitRTVs(ID3D12Device5 * device, IDXGISwapChain4 * swap_chain);
};

/*
===============================================================================

    D3D12 RenderWindow

===============================================================================
*/
class RenderWindow final
    : public OSWindow
{
public:

    DeviceHelper    device_helper;
    SwapChainHelper swap_chain_helper;
    RenderTargets   render_targets;

private:

    void InitRenderWindow() override;
};

} // D3D12
} // MrQ2
