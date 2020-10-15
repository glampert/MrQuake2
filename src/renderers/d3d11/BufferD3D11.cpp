//
// BufferD3D11.cpp
//

#include "BufferD3D11.hpp"
#include "DeviceD3D11.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// BufferD3D11
///////////////////////////////////////////////////////////////////////////////

void BufferD3D11::InitBufferInternal(const DeviceD3D11 & device, const D3D11_BUFFER_DESC & buffer_desc)
{
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first
    D11CHECK(device.Device()->CreateBuffer(&buffer_desc, nullptr, m_resource.GetAddressOf()));
    m_device = &device;
}

void BufferD3D11::Shutdown()
{
    m_device   = nullptr;
    m_resource = nullptr;
}

void * BufferD3D11::Map()
{
    MRQ2_ASSERT(m_device != nullptr);

    D3D11_MAPPED_SUBRESOURCE mapping_info = {};
    D11CHECK(m_device->DeviceContext()->Map(m_resource.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping_info));
    MRQ2_ASSERT(mapping_info.pData != nullptr);

    return mapping_info.pData;
}

void BufferD3D11::Unmap()
{
    MRQ2_ASSERT(m_device != nullptr);
    m_device->DeviceContext()->Unmap(m_resource.Get(), 0);
}

///////////////////////////////////////////////////////////////////////////////
// VertexBufferD3D11
///////////////////////////////////////////////////////////////////////////////

bool VertexBufferD3D11::Init(const DeviceD3D11 & device, const uint32_t buffer_size_in_bytes, const uint32_t vertex_stride_in_bytes)
{
    MRQ2_ASSERT(buffer_size_in_bytes   != 0);
    MRQ2_ASSERT(vertex_stride_in_bytes != 0);

    D3D11_BUFFER_DESC buffer_desc = {};
    buffer_desc.Usage             = D3D11_USAGE_DYNAMIC; // TODO: Consider exposing the usage and CPU access flags.
    buffer_desc.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
    buffer_desc.ByteWidth         = buffer_size_in_bytes;
    buffer_desc.BindFlags         = D3D11_BIND_VERTEX_BUFFER;

    InitBufferInternal(device, buffer_desc);

    m_size_in_bytes   = buffer_size_in_bytes;
    m_stride_in_bytes = vertex_stride_in_bytes;
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// IndexBufferD3D11
///////////////////////////////////////////////////////////////////////////////

bool IndexBufferD3D11::Init(const DeviceD3D11 & device, const uint32_t buffer_size_in_bytes, const IndexFormat format)
{
    MRQ2_ASSERT(buffer_size_in_bytes != 0);

    D3D11_BUFFER_DESC buffer_desc = {};
    buffer_desc.Usage             = D3D11_USAGE_DYNAMIC;
    buffer_desc.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
    buffer_desc.ByteWidth         = buffer_size_in_bytes;
    buffer_desc.BindFlags         = D3D11_BIND_INDEX_BUFFER;

    InitBufferInternal(device, buffer_desc);

    m_size_in_bytes = buffer_size_in_bytes;
    m_index_format  = format;
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// ConstantBufferD3D11
///////////////////////////////////////////////////////////////////////////////

bool ConstantBufferD3D11::Init(const DeviceD3D11 & device, const uint32_t buffer_size_in_bytes, const Flags flags)
{
    MRQ2_ASSERT(buffer_size_in_bytes != 0);

    D3D11_BUFFER_DESC buffer_desc = {};
    buffer_desc.ByteWidth = buffer_size_in_bytes;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    if (flags & kOptimizeForSingleDraw) // We want to be able to UpdateSubresource with this buffer
    {
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.CPUAccessFlags = 0;
    }
    else // Use Map/Unmap instead
    {
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    }

    InitBufferInternal(device, buffer_desc);

    m_size_in_bytes = buffer_size_in_bytes;
    m_flags = flags;
    return true;
}

} // namespace MrQ2
