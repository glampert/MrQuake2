//
// DescriptorHeapD3D12.cpp
//

#include "DescriptorHeapD3D12.hpp"
#include "DeviceD3D12.hpp"

namespace MrQ2
{

void DescriptorHeapD3D12::Init(const DeviceD3D12 & device, const uint32_t num_srv_descriptors, const uint32_t num_dsv_descriptors, const uint32_t num_rtv_descriptors)
{
    MRQ2_ASSERT(num_srv_descriptors != 0);
    MRQ2_ASSERT(num_dsv_descriptors != 0);
    MRQ2_ASSERT(num_rtv_descriptors != 0);

    // SRV, DSV, RTV
    const D3D12_DESCRIPTOR_HEAP_TYPE d3d_types[]  = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,    D3D12_DESCRIPTOR_HEAP_TYPE_DSV,  D3D12_DESCRIPTOR_HEAP_TYPE_RTV  };
    const D3D12_DESCRIPTOR_HEAP_FLAGS d3d_flags[] = { D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, D3D12_DESCRIPTOR_HEAP_FLAG_NONE };
    const wchar_t * const debug_names[]           = { L"SRVDescriptorHeap", L"DSVDescriptorHeap", L"RTVDescriptorHeap" };
    const uint32_t descriptor_counts[]            = { num_srv_descriptors,  num_dsv_descriptors,  num_rtv_descriptors  };

    for (size_t i = 0; i < DescriptorD3D12::kTypeCount; ++i)
    {
        auto & heap = m_heaps[i];

        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.Type           = d3d_types[i];
        heap_desc.NumDescriptors = descriptor_counts[i];
        heap_desc.Flags          = d3d_flags[i];
        heap_desc.NodeMask       = 1;

        D12Check(device.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap.descriptor_heap)));
        D12SetDebugName(heap.descriptor_heap, debug_names[i]);

        heap.cpu_heap_start   = heap.descriptor_heap->GetCPUDescriptorHandleForHeapStart();
        heap.gpu_heap_start   = heap.descriptor_heap->GetGPUDescriptorHandleForHeapStart();
        heap.descriptor_size  = device.device->GetDescriptorHandleIncrementSize(d3d_types[i]);
        heap.descriptor_count = descriptor_counts[i];
    }
}

void DescriptorHeapD3D12::Shutdown()
{
    for (auto & heap : m_heaps)
        heap = {};
}

DescriptorD3D12 DescriptorHeapD3D12::AllocateDescriptor(const DescriptorD3D12::Type type)
{
    auto & heap = m_heaps[size_t(type)];
    MRQ2_ASSERT(heap.descriptor_heap != nullptr);

    if (heap.descriptors_used == heap.descriptor_count)
    {
        GameInterface::Errorf("Heap out of descriptors! Max = %u", heap.descriptor_count);
    }

    DescriptorD3D12 d{};

    d.cpu_handle = heap.cpu_heap_start;
    heap.cpu_heap_start.ptr += heap.descriptor_size;

    d.gpu_handle = heap.gpu_heap_start;
    heap.gpu_heap_start.ptr += heap.descriptor_size;

    heap.descriptors_used++;

    return d;
}

} // MrQ2
