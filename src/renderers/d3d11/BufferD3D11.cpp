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

bool BufferD3D11::InitUntypedBuffer(const DeviceD3D11 & device, const uint32_t size_in_bytes)
{
    //MRQ2_ASSERT(m_resource == nullptr); // Shutdown first
    MRQ2_ASSERT(size_in_bytes != 0);

    return true;
}

void BufferD3D11::Shutdown()
{
    //m_resource = nullptr;
}

void * BufferD3D11::Map()
{
    void * memory = nullptr;
    return memory;
}

void BufferD3D11::Unmap()
{
}

///////////////////////////////////////////////////////////////////////////////
// VertexBufferD3D11
///////////////////////////////////////////////////////////////////////////////

bool VertexBufferD3D11::Init(const DeviceD3D11 & device, const uint32_t buffer_size_in_bytes, const uint32_t vertex_stride_in_bytes)
{
    MRQ2_ASSERT(vertex_stride_in_bytes != 0);
    const bool buffer_ok = InitUntypedBuffer(device, buffer_size_in_bytes);
    if (buffer_ok)
    {
    }
    return buffer_ok;
}

///////////////////////////////////////////////////////////////////////////////
// IndexBufferD3D11
///////////////////////////////////////////////////////////////////////////////

bool IndexBufferD3D11::Init(const DeviceD3D11 & device, const uint32_t buffer_size_in_bytes, const IndexFormat format)
{
    const bool buffer_ok = InitUntypedBuffer(device, buffer_size_in_bytes);
    if (buffer_ok)
    {
    }
    return buffer_ok;
}

///////////////////////////////////////////////////////////////////////////////
// ConstantBufferD3D11
///////////////////////////////////////////////////////////////////////////////

bool ConstantBufferD3D11::Init(const DeviceD3D11 & device, const uint32_t buffer_size_in_bytes, const Flags flags)
{
    const bool buffer_ok = InitUntypedBuffer(device, buffer_size_in_bytes);
    if (buffer_ok)
    {
    }
    return buffer_ok;
}

} // namespace MrQ2
