//
// SwapChainD3D12.cpp
//

#include "../common/Win32Window.hpp"
#include "SwapChainD3D12.hpp"
#include "DeviceD3D12.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// SwapChainD3D12
///////////////////////////////////////////////////////////////////////////////

void SwapChainD3D12::Init(const DeviceD3D12 & device, HWND hWnd, const bool fullscreen, const int width, const int height)
{
    // Describe and create the swap chain
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.BufferCount = kD12NumFrameBuffers;
    sd.Width       = width;
    sd.Height      = height;
    sd.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.SampleDesc  = { 1, 0 };

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fs_sd = {};
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC * p_fs_desc = nullptr; // Not null if we want to present in full screen
    if (fullscreen)
    {
        p_fs_desc = &fs_sd;
        p_fs_desc->Scaling                 = DXGI_MODE_SCALING_CENTERED;
        p_fs_desc->ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        p_fs_desc->RefreshRate.Numerator   = 60;
        p_fs_desc->RefreshRate.Denominator = 1;
        p_fs_desc->Windowed                = false;
    }

    // CreateSwapChainForHwnd requires a command queue so create one now
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(device.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue))))
    {
        GameInterface::Errorf("Failed to create SwapChain command queue.");
    }

    D12ComPtr<IDXGISwapChain1> temp_swapchain;
    if (FAILED(device.factory->CreateSwapChainForHwnd(command_queue.Get(), hWnd, &sd, p_fs_desc, nullptr, temp_swapchain.GetAddressOf())))
    {
        GameInterface::Errorf("Failed to create a temporary SwapChain.");
    }

    // Associate the swap chain with the window
    if (FAILED(device.factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER)))
    {
        GameInterface::Errorf("Failed to make SwapChain window association.");
    }

    if (FAILED(temp_swapchain->QueryInterface(IID_PPV_ARGS(&swap_chain))))
    {
        GameInterface::Errorf("Failed to query SwapChain interface.");
    }

    InitSyncFence(device);
    InitCmdList(device);

    GameInterface::Printf("D3D12 SwapChain created.");
}

void SwapChainD3D12::Shutdown()
{
    // Make sure all rendering operations are synchronized at this point before we can release the D3D resources.
    FullGpuSynch();

    if (frame_fence_event != nullptr)
    {
        CloseHandle(frame_fence_event);
    }

    frame_fence   = nullptr;
    command_queue = nullptr;
    command_list  = nullptr;

    for (auto & alloc : command_allocators)
        alloc = nullptr;

    for (auto & f : cmd_list_executed_fences)
        f = nullptr;

    swap_chain = nullptr;
}

void SwapChainD3D12::BeginFrame(const SwapChainRenderTargetsD3D12 & render_targets)
{
    MRQ2_ASSERT(back_buffer_index == -1);
    back_buffer_index = swap_chain->GetCurrentBackBufferIndex();
    ID3D12Resource * back_buffer_resource = render_targets.render_target_resources[back_buffer_index].Get();

    // Begin command list
    auto * command_allocator = command_allocators[frame_index].Get();
    D12CHECK(command_list->Reset(command_allocator, nullptr));

    // Set back buffer to render target so we can draw to it
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = back_buffer_resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    command_list->ResourceBarrier(1, &barrier);

    auto back_buffer_descriptor   = render_targets.render_target_descriptors[back_buffer_index];
    auto depth_stencil_descriptor = render_targets.depth_render_target_descriptor;
    command_list->OMSetRenderTargets(1, &back_buffer_descriptor.cpu_handle, false, &depth_stencil_descriptor.cpu_handle);
}

void SwapChainD3D12::EndFrame(const SwapChainRenderTargetsD3D12 & render_targets)
{
    MRQ2_ASSERT(back_buffer_index != -1);
    ID3D12Resource * back_buffer_resource = render_targets.render_target_resources[back_buffer_index].Get();

    // Set back buffer to present
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = back_buffer_resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    command_list->ResourceBarrier(1, &barrier);

    // End command list
    D12CHECK(command_list->Close());

    // Submit
    ID3D12CommandList * cmd_lists_to_execute[] = { command_list.Get() };
    command_queue->ExecuteCommandLists(1, cmd_lists_to_execute);

    // Present(0, 0): without vsync; Present(1, 0): with vsync
    auto present_result = swap_chain->Present(0, 0);

    if (FAILED(present_result))
    {
        GameInterface::Errorf("SwapChain Present failed: %s", Win32Window::ErrorToString(present_result).c_str());
    }

    MoveToNextFrame();
    back_buffer_index = -1; // Outside Begin/EndFrame
}

void SwapChainD3D12::MoveToNextFrame()
{
    // Schedule a Signal command in the queue
    MRQ2_ASSERT(frame_index < kD12NumFrameBuffers);
    const uint64_t current_fence_value = frame_fence_values[frame_index];
    command_queue->Signal(frame_fence.Get(), current_fence_value);

    // Fences checked by the UploadContext
    command_queue->Signal(cmd_list_executed_fences[frame_index].Get(), frame_count);

    // Update frame index
    frame_count++;
    frame_index = frame_count % kD12NumFrameBuffers;

    // If the next frame is not ready to be rendered yet, wait until it is
    if (frame_fence->GetCompletedValue() < frame_fence_values[frame_index])
    {
        frame_fence->SetEventOnCompletion(frame_fence_values[frame_index], frame_fence_event);
        WaitForSingleObjectEx(frame_fence_event, INFINITE, FALSE);
    }

    // Set the fence value for next frame
    frame_fence_values[frame_index] = current_fence_value + 1;
}

void SwapChainD3D12::FullGpuSynch()
{
    for (uint32_t i = 0; i < kD12NumFrameBuffers; ++i)
    {
        MoveToNextFrame();
    }
}

SwapChainD3D12::Backbuffer SwapChainD3D12::CurrentBackbuffer(const SwapChainRenderTargetsD3D12 & render_targets) const
{
    MRQ2_ASSERT(back_buffer_index != -1); // Call only between Begin/EndFrame
    auto descriptor = render_targets.render_target_descriptors[back_buffer_index];
    auto resource   = render_targets.render_target_resources[back_buffer_index].Get();
    return { descriptor, resource };
}

void SwapChainD3D12::InitSyncFence(const DeviceD3D12 & device)
{
    if (FAILED(device.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frame_fence))))
    {
        GameInterface::Errorf("Failed to create SwapChain fence.");
    }

    frame_fence_values[frame_index]++;

    frame_fence_event = CreateEventEx(nullptr, "SwapChainFence", FALSE, EVENT_ALL_ACCESS);
    if (frame_fence_event == nullptr)
    {
        GameInterface::Errorf("Failed to create SwapChain fence event.");
    }

    GameInterface::Printf("SwapChain frame sync fence created.");

    // Fences for UploadContext synchronization
    for (auto & f : cmd_list_executed_fences)
    {
        D12CHECK(device.device->CreateFence(~uint64_t(0), D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&f)));
    }
}

void SwapChainD3D12::InitCmdList(const DeviceD3D12 & device)
{
    for (uint32_t i = 0; i < kD12NumFrameBuffers; ++i)
    {
        if (FAILED(device.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[i]))))
        {
            GameInterface::Errorf("Failed to create a SwapChain command allocator!");
        }
    }

    if (FAILED(device.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[0].Get(), nullptr, IID_PPV_ARGS(&command_list))) ||
        FAILED(command_list->Close()))
    {
        GameInterface::Errorf("Failed to create a SwapChain command list!");
    }

    D12SetDebugName(command_queue, L"SwapChainCmdQueue");
    D12SetDebugName(command_list,  L"SwapChainGfxCmdList");
}

///////////////////////////////////////////////////////////////////////////////
// SwapChainRenderTargetsD3D12
///////////////////////////////////////////////////////////////////////////////

void SwapChainRenderTargetsD3D12::Init(const DeviceD3D12 & device, const SwapChainD3D12 & swap_chain, DescriptorHeapD3D12 & descriptor_heap, const int width, const int height)
{
    render_target_width  = width;
    render_target_height = height;

    // Color framebuffers:
    for (uint32_t i = 0; i < kD12NumFrameBuffers; ++i)
    {
        render_target_descriptors[i] = descriptor_heap.AllocateDescriptor(DescriptorD3D12::kRTV);

        D12ComPtr<ID3D12Resource> back_buffer;
        if (FAILED(swap_chain.swap_chain->GetBuffer(i, IID_PPV_ARGS(&back_buffer))))
        {
            GameInterface::Errorf("SwapChain GetBuffer(%u) failed!", i);
        }

        // Debug name displayed in the SDK validation messages.
        static wchar_t s_rt_buffer_names[kD12NumFrameBuffers][32];
        swprintf_s(s_rt_buffer_names[i], L"SwapChainRenderTarget[%u]", i);
        D12SetDebugName(back_buffer, s_rt_buffer_names[i]);

        device.device->CreateRenderTargetView(back_buffer.Get(), nullptr, render_target_descriptors[i].cpu_handle);
        render_target_resources[i] = back_buffer;
    }

    // Depth buffer:
    {
        depth_render_target_descriptor = descriptor_heap.AllocateDescriptor(DescriptorD3D12::kDSV);

        // Texture resource:
        D3D12_HEAP_PROPERTIES heap_props = {};
        heap_props.Type                  = D3D12_HEAP_TYPE_DEFAULT;
        heap_props.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap_props.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC res_desc     = {};
        res_desc.Dimension               = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        res_desc.Alignment               = 0;
        res_desc.Width                   = width;
        res_desc.Height                  = height;
        res_desc.DepthOrArraySize        = 1;
        res_desc.MipLevels               = 1;
        res_desc.Format                  = DXGI_FORMAT_D24_UNORM_S8_UINT;
        res_desc.SampleDesc.Count        = 1;
        res_desc.SampleDesc.Quality      = 0;
        res_desc.Layout                  = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        res_desc.Flags                   = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clear_value    = {};
        clear_value.Format               = DXGI_FORMAT_D24_UNORM_S8_UINT;
        clear_value.DepthStencil         = { 1.0f, 0 };

        D12CHECK(device.device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value, IID_PPV_ARGS(&depth_render_target)));
        D12SetDebugName(depth_render_target, L"SwapChainDepthTarget");

        device.device->CreateDepthStencilView(depth_render_target.Get(), nullptr, depth_render_target_descriptor.cpu_handle);
    }
}

void SwapChainRenderTargetsD3D12::Shutdown()
{
    for (auto & rt : render_target_resources)
        rt = nullptr;

    for (auto & d : render_target_descriptors)
        d = {};

    depth_render_target = nullptr;
    depth_render_target_descriptor = {};
}

} // MrQ2
