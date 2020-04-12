//
// RenderWindow_D3D12.cpp
//  D3D12 rendering window.
//

#include "RenderWindow_D3D12.hpp"
#include "reflibs/shared/RefShared.hpp"

namespace MrQ2
{
namespace D3D12
{

///////////////////////////////////////////////////////////////////////////////
// DeviceHelper
///////////////////////////////////////////////////////////////////////////////

void DeviceHelper::InitAdapterAndDevice(const bool debug_validation)
{
    uint32_t dxgi_factory_flags = 0;

    // Debug layer
    if (debug_validation)
    {
        ComPtr<ID3D12Debug1> debug_interface;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface))))
        {
            debug_interface->EnableDebugLayer();
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
            GameInterface::Printf("Initializing D3D12 with debug layer...");
        }
        else
        {
            GameInterface::Printf("Failed to enable D3D12 debug layer!");
        }
    }

    // Factory
    if (FAILED(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory))))
    {
        GameInterface::Errorf("Failed to create a D3D12 device factory.");
    }

    // Enumerate all available adapters and create the device
    ComPtr<IDXGIAdapter3> temp_adapter;
    for (uint32_t i = 0; factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&temp_adapter)) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        ComPtr<ID3D12Device5> temp_device;
        DXGI_ADAPTER_DESC1 dapter_desc = {};

        FASTASSERT(temp_adapter != nullptr);
        temp_adapter->GetDesc1(&dapter_desc);

        // Remove software emulation
        if ((dapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
            SUCCEEDED(D3D12CreateDevice(temp_adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&temp_device))))
        {
            // Check if the adapter supports ray tracing
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 features = {};
            auto hr = temp_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features, sizeof(features));

            const std::wstring gpu_name{ dapter_desc.Description };
            const bool is_rtx_card = gpu_name.find(L"RTX") != std::wstring::npos;

            if (SUCCEEDED(hr) && is_rtx_card && (features.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0))
            {
                this->supports_rtx = true;
            }

            this->device  = temp_device;
            this->adapter = temp_adapter;

            this->dedicated_video_memory  = dapter_desc.DedicatedVideoMemory;
            this->dedicated_system_memory = dapter_desc.DedicatedSystemMemory;
            this->shared_system_memory    = dapter_desc.SharedSystemMemory;

            this->adapter_info = std::string{ gpu_name.begin(), gpu_name.end() };
            GameInterface::Printf("DXGI Adapter: %s", this->adapter_info.c_str());

            if (debug_validation)
            {
                ComPtr<ID3D12InfoQueue> info_queue;
                if (SUCCEEDED(temp_device->QueryInterface(IID_PPV_ARGS(&info_queue))))
                {
                    // Suppress messages based on their severity level.
                    D3D12_MESSAGE_SEVERITY severities[] =
                    {
                        D3D12_MESSAGE_SEVERITY_INFO,
                    };

                    D3D12_INFO_QUEUE_FILTER filter = {};
                    filter.DenyList.NumSeverities = ARRAYSIZE(severities);
                    filter.DenyList.pSeverityList = severities;

                    info_queue->PushStorageFilter(&filter);
                    info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
                    info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
                }
            }

            // Found a suitable device/adapter.
            break;
        }
        else
        {
            temp_adapter = nullptr;
        }
    }

    if (this->device == nullptr || this->adapter == nullptr)
    {
        GameInterface::Errorf("Failed to create a suitable D3D12 device or adapter!");
    }
    else
    {
        GameInterface::Printf("D3D12 adapter and device created successfully.");
    }
}

///////////////////////////////////////////////////////////////////////////////
// SwapChainHelper
///////////////////////////////////////////////////////////////////////////////

void SwapChainHelper::InitSwapChain(IDXGIFactory6 * factory, ID3D12Device5 * device, HWND hwnd, const bool fullscreen, const int width, const int height)
{
    // Describe and create the swap chain
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.BufferCount = kNumFrameBuffers;
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
    if (FAILED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue))))
    {
        GameInterface::Errorf("Failed to create RenderWindow command queue.");
    }

    ComPtr<IDXGISwapChain1> temp_swapchain;
    if (FAILED(factory->CreateSwapChainForHwnd(command_queue.Get(), hwnd, &sd, p_fs_desc, nullptr, temp_swapchain.GetAddressOf())))
    {
        GameInterface::Errorf("Failed to create a temporary swap chain.");
    }

    // Associate the swap chain with the window
    if (FAILED(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER)))
    {
        GameInterface::Errorf("Failed to make window association.");
    }

    if (FAILED(temp_swapchain->QueryInterface(IID_PPV_ARGS(&swap_chain))))
    {
        GameInterface::Errorf("Failed to query swap chain interface.");
    }

    InitSyncFence(device);
    InitCmdList(device);

    GameInterface::Printf("D3D12 SwapChain created.");
}

///////////////////////////////////////////////////////////////////////////////

void SwapChainHelper::InitSyncFence(ID3D12Device5 * device)
{
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
    {
        GameInterface::Errorf("Failed to create fence.");
    }

    fence_values[frame_index]++;

    fence_event = CreateEventEx(nullptr, L"SwapChainHelperFence", FALSE, EVENT_ALL_ACCESS);
    if (fence_event == nullptr)
    {
        GameInterface::Errorf("Failed to create fence event.");
    }

    GameInterface::Printf("Frame sync fence created.");
}

///////////////////////////////////////////////////////////////////////////////

void SwapChainHelper::InitCmdList(ID3D12Device5 * device)
{
    for (uint32_t i = 0; i < kNumFrameBuffers; ++i)
    {
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[i]))))
        {
            GameInterface::Errorf("Failed to create a command allocator!");
        }
    }

    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[0].Get(), nullptr, IID_PPV_ARGS(&gfx_command_list))) ||
        FAILED(gfx_command_list->Close()))
    {
        GameInterface::Errorf("Failed to create a command list!");
    }

    gfx_command_list->SetName(L"GlobalGfxCommandList");
}

///////////////////////////////////////////////////////////////////////////////

void SwapChainHelper::MoveToNextFrame()
{
    // Schedule a Signal command in the queue
    FASTASSERT(frame_index < kNumFrameBuffers);
    const uint64_t currentFenceValue = fence_values[frame_index];
    command_queue->Signal(fence.Get(), currentFenceValue);

    // Update frame index
    frame_count++;
    frame_index = frame_count % kNumFrameBuffers;

    // If the next frame is not ready to be rendered yet, wait until it is
    if (fence->GetCompletedValue() < fence_values[frame_index])
    {
        fence->SetEventOnCompletion(fence_values[frame_index], fence_event);
        WaitForSingleObjectEx(fence_event, INFINITE, FALSE);
    }

    // Set the fence value for next frame
    fence_values[frame_index] = currentFenceValue + 1;
}

///////////////////////////////////////////////////////////////////////////////

void SwapChainHelper::FullGpuSynch()
{
    for (uint32_t i = 0; i < kNumFrameBuffers; ++i)
    {
        MoveToNextFrame();
    }
}

///////////////////////////////////////////////////////////////////////////////

SwapChainHelper::~SwapChainHelper()
{
    // Make sure all rendering operations are synchronized at this point before we can release the D3D resources.
    FullGpuSynch();

    if (fence_event != nullptr)
    {
        CloseHandle(fence_event);
    }
}

///////////////////////////////////////////////////////////////////////////////
// RenderTargets
///////////////////////////////////////////////////////////////////////////////

void RenderTargets::InitRTVs(ID3D12Device5 * device, IDXGISwapChain4 * swap_chain)
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
    heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NumDescriptors = kNumFrameBuffers;
    heap_desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heap_desc.NodeMask       = 1;

    if (FAILED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&rtv_descriptor_heap))))
    {
        GameInterface::Errorf("RenderTargets - CreateDescriptorHeap failed!");
    }

    const UINT rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart();

    static wchar_t s_buffer_names[kNumFrameBuffers][32];

    for (uint32_t i = 0; i < kNumFrameBuffers; ++i)
    {
        render_target_descriptors[i] = rtv_handle;
        rtv_handle.ptr += rtv_descriptor_size;

        ComPtr<ID3D12Resource> back_buffer;
        if (FAILED(swap_chain->GetBuffer(i, IID_PPV_ARGS(&back_buffer))))
        {
            GameInterface::Errorf("SwapChain GetBuffer %u failed!", i);
        }

        // Debug name displayed in the SDK validation messages.
        swprintf_s(s_buffer_names[i], L"BackBuffer[%u]", i);
        back_buffer->SetName(s_buffer_names[i]);

        device->CreateRenderTargetView(back_buffer.Get(), nullptr, render_target_descriptors[i]);
        render_rarget_resources[i] = back_buffer;
    }
}

///////////////////////////////////////////////////////////////////////////////
// RenderWindow
///////////////////////////////////////////////////////////////////////////////

void RenderWindow::InitRenderWindow()
{
    GameInterface::Printf("D3D12 Setting up the RenderWindow...");

    device_helper.InitAdapterAndDevice(debug_validation);
    swap_chain_helper.InitSwapChain(device_helper.factory.Get(), device_helper.device.Get(), hwnd, fullscreen, width, height);
    render_targets.InitRTVs(device_helper.device.Get(), swap_chain_helper.swap_chain.Get());

    GameInterface::Printf("D3D12 RenderWindow initialized.");
}

///////////////////////////////////////////////////////////////////////////////

} // D3D12
} // MrQ2
