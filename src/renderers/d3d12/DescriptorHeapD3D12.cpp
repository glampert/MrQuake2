//
// DescriptorHeapD3D12.cpp
//

#include "DescriptorHeapD3D12.hpp"
#include "DeviceD3D12.hpp"

namespace MrQ2
{

void DescriptorHeapD3D12::Init(const DeviceD3D12 & device)
{
    // SRV, DSV, RTV, Sampler
    const D3D12_DESCRIPTOR_HEAP_TYPE d3d_types[]  = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,    D3D12_DESCRIPTOR_HEAP_TYPE_DSV,  D3D12_DESCRIPTOR_HEAP_TYPE_RTV,  D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER };
    const D3D12_DESCRIPTOR_HEAP_FLAGS d3d_flags[] = { D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
    const wchar_t * const debug_names[]           = { L"SRVDescriptorHeap",    L"DSVDescriptorHeap",    L"RTVDescriptorHeap",    L"SamplerDescriptorHeap"    };
    const uint32_t descriptor_counts[]            = { kMaxSrvDescriptors,      kMaxDsvDescriptors,      kMaxRtvDescriptors,      kMaxSamplerDescriptors      };
    ArrayBase<DescriptorD3D12> * free_lists[]     = { &m_free_srv_descriptors, &m_free_dsv_descriptors, &m_free_rtv_descriptors, &m_free_sampler_descriptors };

    for (size_t i = 0; i < DescriptorD3D12::kTypeCount; ++i)
    {
        auto & heap = m_heaps[i];

        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.Type           = d3d_types[i];
        heap_desc.NumDescriptors = descriptor_counts[i];
        heap_desc.Flags          = d3d_flags[i];
        heap_desc.NodeMask       = 1;

        D12CHECK(device.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap.descriptor_heap)));
        D12SetDebugName(heap.descriptor_heap, debug_names[i]);

        heap.cpu_heap_start   = heap.descriptor_heap->GetCPUDescriptorHandleForHeapStart();
        heap.gpu_heap_start   = heap.descriptor_heap->GetGPUDescriptorHandleForHeapStart();
        heap.descriptor_size  = device.device->GetDescriptorHandleIncrementSize(d3d_types[i]);
        heap.descriptor_count = descriptor_counts[i];
        heap.descriptors_used = 0;
        heap.free_list        = free_lists[i];
    }
}

void DescriptorHeapD3D12::Shutdown()
{
    for (auto & heap : m_heaps)
    {
        heap.free_list->clear();
        heap = {};
    }
}

DescriptorD3D12 DescriptorHeapD3D12::AllocateDescriptor(const DescriptorD3D12::Type type)
{
    MRQ2_ASSERT(type < DescriptorD3D12::kTypeCount);

    auto & heap = m_heaps[size_t(type)];
    MRQ2_ASSERT(heap.descriptor_heap != nullptr);
    MRQ2_ASSERT(heap.free_list != nullptr);

    // If we have a free descriptor of this type to recycle, reuse it.
    if (!heap.free_list->empty())
    {
        const DescriptorD3D12 recycled_descriptor = heap.free_list->back();
        MRQ2_ASSERT(recycled_descriptor.type == type);
        heap.free_list->pop_back();
        return recycled_descriptor;
    }

    if (heap.descriptors_used == heap.descriptor_count)
    {
        GameInterface::Errorf("Heap out of descriptors! Max = %u", heap.descriptor_count);
    }

    const DescriptorD3D12 descriptor = { heap.cpu_heap_start, heap.gpu_heap_start, type };

    heap.cpu_heap_start.ptr += heap.descriptor_size;
    heap.gpu_heap_start.ptr += heap.descriptor_size;
    heap.descriptors_used++;

    return descriptor;
}

void DescriptorHeapD3D12::FreeDescriptor(const DescriptorD3D12 & descriptor)
{
    MRQ2_ASSERT(descriptor.type < DescriptorD3D12::kTypeCount);

    auto & heap = m_heaps[size_t(descriptor.type)];
    MRQ2_ASSERT(heap.free_list != nullptr);

    heap.free_list->push_back(descriptor);
}

} // MrQ2
