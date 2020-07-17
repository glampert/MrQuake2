//
// DescriptorHeapD3D12.hpp
//
#pragma once

#include "UtilsD3D12.hpp"

namespace MrQ2
{

class DeviceD3D12;

struct DescriptorD3D12 final
{
    enum Type : uint32_t
    {
        kSRV, // Shader Resource View
        kDSV, // Depth-Stencil View
        kRTV, // RenderTarget View
        kTypeCount
    };

    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;
    Type type;
};

class DescriptorHeapD3D12 final
{
public:

    DescriptorHeapD3D12() = default;

    // Disallow copy.
    DescriptorHeapD3D12(const DescriptorHeapD3D12 &) = delete;
    DescriptorHeapD3D12 & operator=(const DescriptorHeapD3D12 &) = delete;

    void Init(const DeviceD3D12 & device, const uint32_t num_srv_descriptors, const uint32_t num_dsv_descriptors, const uint32_t num_rtv_descriptors);
    void Shutdown();

    DescriptorD3D12 AllocateDescriptor(const DescriptorD3D12::Type type);

private:

    struct HeapInfo
    {
        D12ComPtr<ID3D12DescriptorHeap> descriptor_heap;
        D3D12_CPU_DESCRIPTOR_HANDLE     cpu_heap_start{};
        D3D12_GPU_DESCRIPTOR_HANDLE     gpu_heap_start{};
        uint32_t                        descriptor_size{ 0 };
        uint32_t                        descriptor_count{ 0 };
        uint32_t                        descriptors_used{ 0 };
    };
    HeapInfo m_heaps[DescriptorD3D12::kTypeCount];
};

} // MrQ2
