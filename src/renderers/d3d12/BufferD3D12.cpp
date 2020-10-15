//
// BufferD3D12.cpp
//

#include "BufferD3D12.hpp"
#include "DeviceD3D12.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// BufferD3D12
///////////////////////////////////////////////////////////////////////////////

bool BufferD3D12::InitUntypedBuffer(const DeviceD3D12 & device, const uint32_t size_in_bytes)
{
    MRQ2_ASSERT(m_resource == nullptr); // Shutdown first
    MRQ2_ASSERT(size_in_bytes != 0);

    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type                  = D3D12_HEAP_TYPE_UPLOAD; // Mappable buffer.
    heap_props.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask      = 1;
    heap_props.VisibleNodeMask       = 1;

    D3D12_RESOURCE_DESC res_desc     = {};
    res_desc.Dimension               = D3D12_RESOURCE_DIMENSION_BUFFER;
    res_desc.Alignment               = 0; // Must be zero for buffers.
    res_desc.Width                   = size_in_bytes;
    res_desc.Height                  = 1;
    res_desc.DepthOrArraySize        = 1;
    res_desc.MipLevels               = 1;
    res_desc.SampleDesc              = { 1, 0 };
    res_desc.Layout                  = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    res_desc.Format                  = DXGI_FORMAT_UNKNOWN;
    res_desc.Flags                   = D3D12_RESOURCE_FLAG_NONE;

    const D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_GENERIC_READ;

    if (FAILED(device.Device()->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, states, nullptr, IID_PPV_ARGS(&m_resource))))
    {
        GameInterface::Printf("WARNING: CreateCommittedResource failed for new buffer resource!");
        return false;
    }

    return true;
}

void BufferD3D12::Shutdown()
{
    m_resource = nullptr;
}

void * BufferD3D12::Map()
{
    D3D12_RANGE range = {}; // No range specified.
    void * memory = nullptr;
    D12CHECK(m_resource->Map(0, &range, &memory));
    return memory;
}

void BufferD3D12::Unmap()
{
    D3D12_RANGE range = {}; // No range specified.
    m_resource->Unmap(0, &range);
}

///////////////////////////////////////////////////////////////////////////////
// VertexBufferD3D12
///////////////////////////////////////////////////////////////////////////////

bool VertexBufferD3D12::Init(const DeviceD3D12 & device, const uint32_t buffer_size_in_bytes, const uint32_t vertex_stride_in_bytes)
{
    MRQ2_ASSERT(vertex_stride_in_bytes != 0);
    const bool buffer_ok = InitUntypedBuffer(device, buffer_size_in_bytes);
    if (buffer_ok)
    {
        m_view.BufferLocation = m_resource->GetGPUVirtualAddress();
        m_view.SizeInBytes    = buffer_size_in_bytes;
        m_view.StrideInBytes  = vertex_stride_in_bytes;
    }
    return buffer_ok;
}

///////////////////////////////////////////////////////////////////////////////
// IndexBufferD3D12
///////////////////////////////////////////////////////////////////////////////

bool IndexBufferD3D12::Init(const DeviceD3D12 & device, const uint32_t buffer_size_in_bytes, const IndexFormat format)
{
    const bool buffer_ok = InitUntypedBuffer(device, buffer_size_in_bytes);
    if (buffer_ok)
    {
        m_view.BufferLocation = m_resource->GetGPUVirtualAddress();
        m_view.SizeInBytes    = buffer_size_in_bytes;
        m_view.Format         = (format == kFormatUInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    }
    return buffer_ok;
}

///////////////////////////////////////////////////////////////////////////////
// ConstantBufferD3D12
///////////////////////////////////////////////////////////////////////////////

bool ConstantBufferD3D12::Init(const DeviceD3D12 & device, const uint32_t buffer_size_in_bytes, const Flags flags)
{
    const bool buffer_ok = InitUntypedBuffer(device, buffer_size_in_bytes);
    if (buffer_ok)
    {
        m_view.BufferLocation = m_resource->GetGPUVirtualAddress();
        m_view.SizeInBytes    = buffer_size_in_bytes;
        m_flags               = flags;
    }
    return buffer_ok;
}

} // namespace MrQ2
