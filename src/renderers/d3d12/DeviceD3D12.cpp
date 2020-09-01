//
// DeviceD3D12.cpp
//

#include "DeviceD3D12.hpp"

namespace MrQ2
{

void DeviceD3D12::Init(const bool debug, DescriptorHeapD3D12 & desc_heap, UploadContextD3D12 & up_ctx, GraphicsContextD3D12 & gfx_ctx, SwapChainD3D12 & sc)
{
    debug_validation = debug;
    uint32_t dxgi_factory_flags = 0;

    // Debug layer
    if (debug_validation)
    {
        D12ComPtr<ID3D12Debug1> debug_interface;
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
    D12ComPtr<IDXGIAdapter3> temp_adapter;
    for (uint32_t i = 0; factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&temp_adapter)) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        D12ComPtr<ID3D12Device5> temp_device;
        DXGI_ADAPTER_DESC1 dapter_desc = {};

        MRQ2_ASSERT(temp_adapter != nullptr);
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
                D12ComPtr<ID3D12InfoQueue> info_queue;
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

    swap_chain      = &sc;
    descriptor_heap = &desc_heap;
    upload_ctx      = &up_ctx;
    graphics_ctx    = &gfx_ctx;
}

void DeviceD3D12::Shutdown()
{
    device  = nullptr;
    adapter = nullptr;
    factory = nullptr;
    adapter_info.clear();

    descriptor_heap = nullptr;
    upload_ctx      = nullptr;
    graphics_ctx    = nullptr;
}

} // MrQ2
