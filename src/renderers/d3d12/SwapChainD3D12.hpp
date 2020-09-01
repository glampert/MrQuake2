//
// SwapChainD3D12.hpp
//
#pragma once

#include "DescriptorHeapD3D12.hpp"

namespace MrQ2
{

class SwapChainD3D12;
class SwapChainRenderTargetsD3D12;

class SwapChainD3D12 final
{
public:

    HANDLE                               frame_fence_event{ nullptr };
    uint64_t                             frame_fence_values[kD12NumFrameBuffers] = {};
    uint64_t                             frame_count{ 0 };
    uint32_t                             frame_index{ 0 };
    int32_t                              back_buffer_index{ -1 };
    D12ComPtr<ID3D12Fence>               frame_fence;
    D12ComPtr<ID3D12CommandQueue>        command_queue;
    D12ComPtr<ID3D12GraphicsCommandList> command_list;
    D12ComPtr<ID3D12CommandAllocator>    command_allocators[kD12NumFrameBuffers];
    D12ComPtr<ID3D12Fence>               cmd_list_executed_fences[kD12NumFrameBuffers];
    D12ComPtr<IDXGISwapChain4>           swap_chain;

    SwapChainD3D12() = default;

    // Disallow copy.
    SwapChainD3D12(const SwapChainD3D12 &) = delete;
    SwapChainD3D12 & operator=(const SwapChainD3D12 &) = delete;

    void Init(const DeviceD3D12 & device, HWND hWnd, const bool fullscreen, const int width, const int height);
    void Shutdown();

    void BeginFrame(const SwapChainRenderTargetsD3D12 & render_targets);
    void EndFrame(const SwapChainRenderTargetsD3D12 & render_targets);

    void MoveToNextFrame();
    void FullGpuSynch();

    struct Backbuffer
    {
        DescriptorD3D12  descriptor;
        ID3D12Resource * resource;
    };

    Backbuffer CurrentBackbuffer(const SwapChainRenderTargetsD3D12 & render_targets) const;
    ID3D12Fence * CurrentCmdListExecutedFence() const { return cmd_list_executed_fences[frame_index].Get(); }
    uint64_t CurrentCmdListExecutedFenceValue() const { return frame_count; }

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
    DescriptorD3D12           depth_render_target_descriptor{};

    SwapChainRenderTargetsD3D12() = default;

    // Disallow copy.
    SwapChainRenderTargetsD3D12(const SwapChainRenderTargetsD3D12 &) = delete;
    SwapChainRenderTargetsD3D12 & operator=(const SwapChainRenderTargetsD3D12 &) = delete;

    void Init(const DeviceD3D12 & device, const SwapChainD3D12 & swap_chain, DescriptorHeapD3D12 & descriptor_heap, const int width, const int height);
    void Shutdown();
};

} // MrQ2
