//
// DeviceD3D12.hpp
//
#pragma once

#include "UtilsD3D12.hpp"
#include <string>

namespace MrQ2
{

class SwapChainD3D12;
class UploadContextD3D12;
class GraphicsContextD3D12;
class DescriptorHeapD3D12;

class DeviceD3D12 final
{
    friend class SwapChainD3D12;
    friend class UploadContextD3D12;
    friend class GraphicsContextD3D12;

public:

    DeviceD3D12() = default;

    // Disallow copy.
    DeviceD3D12(const DeviceD3D12 &) = delete;
    DeviceD3D12 & operator=(const DeviceD3D12 &) = delete;

    void Init(const bool debug, DescriptorHeapD3D12 & desc_heap, UploadContextD3D12 & up_ctx, GraphicsContextD3D12 & gfx_ctx, SwapChainD3D12 & sc);
    void Shutdown();

    bool DebugValidationEnabled() const { return m_debug_validation; }
    ID3D12Device5 * Device() const { return m_device.Get(); }

    // Public to renderers/common
    UploadContextD3D12   & UploadContext()   const { return *m_upload_ctx; }
    GraphicsContextD3D12 & GraphicsContext() const { return *m_graphics_ctx; }
    DescriptorHeapD3D12  & DescriptorHeap()  const { return *m_descriptor_heap; }

private:

    // Internal to the D3D12 back end
    D12ComPtr<IDXGIFactory6> m_factory;
    D12ComPtr<IDXGIAdapter3> m_adapter;
    D12ComPtr<ID3D12Device5> m_device;

    SwapChainD3D12       * m_swap_chain{ nullptr };
    DescriptorHeapD3D12  * m_descriptor_heap{ nullptr };
    UploadContextD3D12   * m_upload_ctx{ nullptr };
    GraphicsContextD3D12 * m_graphics_ctx{ nullptr };

    size_t m_dedicated_video_memory{ 0 };
    size_t m_dedicated_system_memory{ 0 };
    size_t m_shared_system_memory{ 0 };

    bool m_supports_rtx{ false };     // Does our graphics card support HW RTX ray tracing?
    bool m_debug_validation{ false }; // With D3D12 debug validation layer?
    std::string m_adapter_info{};     // Vendor string.
};

} // MrQ2
