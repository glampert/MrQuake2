//
// SwapChainD3D12.hpp
//
#pragma once

#include "DescriptorHeapD3D12.hpp"

namespace MrQ2
{

class SwapChainD3D12 final
{
public:

    HANDLE                               fence_event{ nullptr };
    uint64_t                             fence_values[kD12NumFrameBuffers] = {};
    uint64_t                             frame_count{ 0 };
    uint32_t                             frame_index{ 0 };
    D12ComPtr<ID3D12Fence>               fence;
    D12ComPtr<ID3D12CommandQueue>        command_queue;
    D12ComPtr<ID3D12GraphicsCommandList> command_list;
    D12ComPtr<ID3D12CommandAllocator>    command_allocators[kD12NumFrameBuffers];
    D12ComPtr<IDXGISwapChain4>           swap_chain;

    void Init(const DeviceD3D12 & device, HWND hWnd, const bool fullscreen, const int width, const int height);
    void Shutdown();

    void MoveToNextFrame();
    void FullGpuSynch();

private:

    void InitSyncFence(const DeviceD3D12 & device);
    void InitCmdList(const DeviceD3D12 & device);
};

class SwapChainRenderTargetsD3D12 final
{
public:

    int render_target_width{ 0 };
    int render_target_height{ 0 };

    // Framebuffer render targets
    D12ComPtr<ID3D12Resource> render_target_resources[kD12NumFrameBuffers];
    DescriptorD3D12           render_target_descriptors[kD12NumFrameBuffers] = {};

    // Depth buffer
    D12ComPtr<ID3D12Resource> depth_render_target;
    DescriptorD3D12           depth_render_target_descriptor = {};

    void Init(const DeviceD3D12 & device, const SwapChainD3D12 & swap_chain, DescriptorHeapD3D12 & descriptor_heap, const int width, const int height);
    void Shutdown();
};

} // MrQ2
