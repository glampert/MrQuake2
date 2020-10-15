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
    ID3D12Fence * CurrentCmdListExecutedFence() const { return m_cmd_list_executed_fences[m_frame_index].Get(); }
    uint64_t CurrentCmdListExecutedFenceValue() const { return m_frame_count; }

    IDXGISwapChain4 * SwapChain() const { return m_swap_chain.Get(); }
    ID3D12GraphicsCommandList * CmdList() const { return m_command_list.Get(); }

private:

    void InitSyncFence(const DeviceD3D12 & device);
    void InitCmdList(const DeviceD3D12 & device);

    HANDLE                               m_frame_fence_event{ nullptr };
    uint64_t                             m_frame_fence_values[kD12NumFrameBuffers] = {};
    uint64_t                             m_frame_count{ 0 };
    uint32_t                             m_frame_index{ 0 };
    int32_t                              m_back_buffer_index{ -1 };
    D12ComPtr<ID3D12Fence>               m_frame_fence;
    D12ComPtr<ID3D12CommandQueue>        m_command_queue;
    D12ComPtr<ID3D12GraphicsCommandList> m_command_list;
    D12ComPtr<ID3D12CommandAllocator>    m_command_allocators[kD12NumFrameBuffers];
    D12ComPtr<ID3D12Fence>               m_cmd_list_executed_fences[kD12NumFrameBuffers];
    D12ComPtr<IDXGISwapChain4>           m_swap_chain;
};

class SwapChainRenderTargetsD3D12 final
{
    friend class SwapChainD3D12;
    friend class GraphicsContextD3D12;

public:

    SwapChainRenderTargetsD3D12() = default;

    // Disallow copy.
    SwapChainRenderTargetsD3D12(const SwapChainRenderTargetsD3D12 &) = delete;
    SwapChainRenderTargetsD3D12 & operator=(const SwapChainRenderTargetsD3D12 &) = delete;

    void Init(const DeviceD3D12 & device, const SwapChainD3D12 & swap_chain, DescriptorHeapD3D12 & descriptor_heap, const int width, const int height);
    void Shutdown();

    int RenderTargetWidth()  const { return m_render_target_width;  }
    int RenderTargetHeight() const { return m_render_target_height; }

private:

    int m_render_target_width{ 0 };
    int m_render_target_height{ 0 };

    // Framebuffer render targets
    D12ComPtr<ID3D12Resource> m_render_target_resources[kD12NumFrameBuffers];
    DescriptorD3D12           m_render_target_descriptors[kD12NumFrameBuffers] = {};

    // Depth buffer
    D12ComPtr<ID3D12Resource> m_depth_render_target;
    DescriptorD3D12           m_depth_render_target_descriptor{};
};

} // MrQ2
