//
// RenderWindow_D3D12.hpp
//  D3D12 rendering window.
//
#pragma once

#include "reflibs/shared/RefShared.hpp"
#include "reflibs/shared/Win32Window.hpp"

#include <dxgi1_6.h>
#include <d3d12.h>

namespace MrQ2
{
namespace D3D12
{

using Microsoft::WRL::ComPtr;
constexpr uint32_t kNumFrameBuffers = 3; // Triple-buffering

// TODO find a better place for these:

inline uint32_t Dx12Align(uint32_t alignment, uint32_t value)
{
	const uint32_t a = alignment - 1u;
	return (value + a) & ~a;
}

template<typename Dx12Type>
inline void Dx12SetDebugName(Dx12Type& obj_ptr, const wchar_t * const str)
{
	obj_ptr->SetName(str);
}

// TODO: use this for D3D error handling
#define Dx12Check(expr)                                      \
    do                                                       \
    {                                                        \
        if (FAILED(expr))                                    \
        {                                                    \
            GameInterface::Errorf("D3D12 error: %s", #expr); \
        }                                                    \
    } while (0)

/*
===============================================================================

    D3D12 Device & SwapChain helpers

===============================================================================
*/

class DeviceData
{
public:

    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter3> adapter;
    ComPtr<ID3D12Device5> device;
    size_t                dedicated_video_memory  = 0;
    size_t                dedicated_system_memory = 0;
    size_t                shared_system_memory    = 0;
    bool                  supports_rtx            = false; // Does our graphics card support RTX ray tracing?
    std::string           adapter_info;

protected:

    void InitAdapterAndDevice(const bool debug_validation);
};

class SwapChainData
{
public:

    HANDLE                            fence_event = nullptr;
    uint64_t                          fence_values[kNumFrameBuffers] = {};
    uint64_t                          frame_count = 0;
    uint32_t                          frame_index = 0;
    ComPtr<ID3D12Fence>               fence;
    ComPtr<ID3D12CommandQueue>        command_queue;
    ComPtr<ID3D12GraphicsCommandList> gfx_command_list;
    ComPtr<ID3D12CommandAllocator>    command_allocators[kNumFrameBuffers];
    ComPtr<IDXGISwapChain4>           swap_chain;

    void MoveToNextFrame();
    void FullGpuSynch();

protected:

    void InitSwapChain(IDXGIFactory6 * factory, ID3D12Device5 * device, HWND hwnd,
                       const bool fullscreen, const int width, const int height);

    ~SwapChainData();

private:

    void InitSyncFence(ID3D12Device5 * device);
    void InitCmdList(ID3D12Device5 * device);
};

class RenderTargetData
{
public:

    // TODO: Use the DescriptorHeap helper for these as well.

    // Framebuffer render targets
    ComPtr<ID3D12DescriptorHeap> rtv_descriptor_heap; // Render Target View (RTV)
    ComPtr<ID3D12Resource>       render_target_resources[kNumFrameBuffers]   = {};
    D3D12_CPU_DESCRIPTOR_HANDLE  render_target_descriptors[kNumFrameBuffers] = {};

    // Debug buffer
    ComPtr<ID3D12DescriptorHeap> dsv_descriptor_heap; // Depth-Stencil View (DSV) heap
    ComPtr<ID3D12Resource>       depth_render_target;
    D3D12_CPU_DESCRIPTOR_HANDLE  depth_render_target_descriptor = {};

protected:

    void InitRTVs(ID3D12Device5 * device, IDXGISwapChain4 * swap_chain);
};

/*
===============================================================================

    D3D12 RenderWindow

===============================================================================
*/
class RenderWindow final
    : public Win32Window
    , public DeviceData
    , public SwapChainData
    , public RenderTargetData
{
public:

    RenderWindow() = default;
    ~RenderWindow() = default;

private:

    void InitRenderWindow() override;
};

} // D3D12
} // MrQ2
