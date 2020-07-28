//
// DescriptorHeapD3D12.hpp
//
#pragma once

#include "../common/Array.hpp"
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
        kSampler,
        kTypeCount
    };

    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;
    Type type;
};

class DescriptorHeapD3D12 final
{
public:

    static constexpr uint32_t kMaxSrvDescriptors     = 1024;
    static constexpr uint32_t kMaxDsvDescriptors     = kD12NumFrameBuffers;
    static constexpr uint32_t kMaxRtvDescriptors     = kD12NumFrameBuffers;
    static constexpr uint32_t kMaxSamplerDescriptors = 1024;

    DescriptorHeapD3D12() = default;

    // Disallow copy.
    DescriptorHeapD3D12(const DescriptorHeapD3D12 &) = delete;
    DescriptorHeapD3D12 & operator=(const DescriptorHeapD3D12 &) = delete;

    void Init(const DeviceD3D12 & device);
    void Shutdown();

    DescriptorD3D12 AllocateDescriptor(const DescriptorD3D12::Type type);
    void FreeDescriptor(const DescriptorD3D12 & descriptor);

    ID3D12DescriptorHeap * const * GetHeapAddr(const DescriptorD3D12::Type type) const
    {
        MRQ2_ASSERT(type < DescriptorD3D12::kTypeCount);
        auto & heap = m_heaps[size_t(type)];
        MRQ2_ASSERT(heap.descriptor_heap != nullptr);
        return heap.descriptor_heap.GetAddressOf();
    }

private:

    struct HeapInfo
    {
        D12ComPtr<ID3D12DescriptorHeap> descriptor_heap;
        D3D12_CPU_DESCRIPTOR_HANDLE     cpu_heap_start{};
        D3D12_GPU_DESCRIPTOR_HANDLE     gpu_heap_start{};
        uint32_t                        descriptor_size{ 0 };
        uint32_t                        descriptor_count{ 0 };
        uint32_t                        descriptors_used{ 0 };
        ArrayBase<DescriptorD3D12> *    free_list{}; // Points to one of the FixedSizeArrays below
    };

    HeapInfo m_heaps[DescriptorD3D12::kTypeCount];

    FixedSizeArray<DescriptorD3D12, kMaxSrvDescriptors>     m_free_srv_descriptors;
    FixedSizeArray<DescriptorD3D12, kMaxDsvDescriptors>     m_free_dsv_descriptors;
    FixedSizeArray<DescriptorD3D12, kMaxRtvDescriptors>     m_free_rtv_descriptors;
    FixedSizeArray<DescriptorD3D12, kMaxSamplerDescriptors> m_free_sampler_descriptors;
};

} // MrQ2
