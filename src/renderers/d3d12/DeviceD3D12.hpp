//
// DeviceD3D12.hpp
//
#pragma once

#include "UtilsD3D12.hpp"
#include <string>

namespace MrQ2
{

class DescriptorHeapD3D12;
class UploadContextD3D12;
class GraphicsContextD3D12;
class SwapChainD3D12;

class DeviceD3D12 final
{
public:

    // Internal to the D3D12 back end
    D12ComPtr<IDXGIFactory6> factory;
    D12ComPtr<IDXGIAdapter3> adapter;
    D12ComPtr<ID3D12Device5> device;

    SwapChainD3D12       * swap_chain{ nullptr };
    DescriptorHeapD3D12  * descriptor_heap{ nullptr };
    UploadContextD3D12   * upload_ctx{ nullptr };
    GraphicsContextD3D12 * graphics_ctx{ nullptr };

    size_t dedicated_video_memory{ 0 };
    size_t dedicated_system_memory{ 0 };
    size_t shared_system_memory{ 0 };

    bool supports_rtx{ false };     // Does our graphics card support HW RTX ray tracing?
    bool debug_validation{ false }; // With D3D12 debug validation layer?
    std::string adapter_info{};     // Vendor string.

    DeviceD3D12() = default;

    // Disallow copy.
    DeviceD3D12(const DeviceD3D12 &) = delete;
    DeviceD3D12 & operator=(const DeviceD3D12 &) = delete;

    void Init(const bool debug, DescriptorHeapD3D12 & desc_heap, UploadContextD3D12 & up_ctx, GraphicsContextD3D12 & gfx_ctx, SwapChainD3D12 & sc);
    void Shutdown();

    // Public to renderers/common
    UploadContextD3D12   & UploadContext()   const { return *upload_ctx;   }
    GraphicsContextD3D12 & GraphicsContext() const { return *graphics_ctx; }
};

} // MrQ2
