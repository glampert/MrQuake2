//
// RenderWindow_D3D12.cpp
//  D3D12 rendering window.
//

#include "RenderWindow_D3D12.hpp"
#include "reflibs/shared/RefShared.hpp"
#include <string>

#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "dxgi.lib")

namespace MrQ2
{
namespace D3D12
{

///////////////////////////////////////////////////////////////////////////////
// RenderWindow
///////////////////////////////////////////////////////////////////////////////

void RenderWindow::InitRenderWindow()
{
    uint32_t dxgi_factory_flags = 0;

    // Debug layer
    if (debug_validation)
    {
        ComPtr<ID3D12Debug1> debug_interface;
        auto hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface));
        if (SUCCEEDED(hr))
        {
            debug_interface->EnableDebugLayer();
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
            GameInterface::Printf("Initializing with debug layer...");
        }
        else
        {
            GameInterface::Printf("Failed to enable D3D12 debug layer!");
        }
    }

    // Factory
    {
        auto hr = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory));
        if (FAILED(hr))
        {
            GameInterface::Errorf("Failed to create a D3D12 device factory.");
        }
    }

    // Enumerate all available adapters and create the device
    ComPtr<IDXGIAdapter3> temp_adapter;
    for (uint32_t i = 0; factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&temp_adapter)) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        ComPtr<ID3D12Device5> temp_device;
        DXGI_ADAPTER_DESC1 temp_adapter_desc = {};

        FASTASSERT(temp_adapter != nullptr);
        temp_adapter->GetDesc1(&temp_adapter_desc);

        // Remove software emulation
        if ((temp_adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
            SUCCEEDED(D3D12CreateDevice(temp_adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&temp_device))))
        {
            // Check if the adapter supports ray tracing
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 features = {};
            auto hr = temp_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features, sizeof(features));

            const std::wstring gpu_name{ temp_adapter_desc.Description };
            const bool is_rtx_card = gpu_name.find(L"RTX") != std::wstring::npos;

            if (SUCCEEDED(hr) && is_rtx_card && (features.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0))
            {
                supports_rtx = true;
            }

            this->adapter      = temp_adapter;
            this->device       = temp_device;
            this->adapter_desc = temp_adapter_desc;

            const std::string adapter_info{ gpu_name.begin(), gpu_name.end() };
            GameInterface::Printf("DXGI Adapter: %s", adapter_info.c_str());

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

    // Describe and create the swap chain
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.BufferCount = 2;
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
    if (FAILED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&cmd_queue))))
    {
        GameInterface::Errorf("Failed to create RenderWindow command queue.");
    }

    ComPtr<IDXGISwapChain1> temp_swapchain;
    if (FAILED(factory->CreateSwapChainForHwnd(cmd_queue.Get(), hwnd, &sd, p_fs_desc, nullptr, temp_swapchain.GetAddressOf())))
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

    GameInterface::Printf("D3D12 RenderWindow initialized.");
}

///////////////////////////////////////////////////////////////////////////////

} // D3D12
} // MrQ2
