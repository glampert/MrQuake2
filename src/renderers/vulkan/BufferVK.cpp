//
// BufferVK.cpp
//

#include "BufferVK.hpp"
#include "DeviceVK.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// BufferVK
///////////////////////////////////////////////////////////////////////////////

void BufferVK::Shutdown()
{
}

void * BufferVK::Map()
{
    static char dummy_buffer[1024*1024];
    return dummy_buffer;
}

void BufferVK::Unmap()
{
}

///////////////////////////////////////////////////////////////////////////////
// VertexBufferVK
///////////////////////////////////////////////////////////////////////////////

bool VertexBufferVK::Init(const DeviceVK & device, const uint32_t buffer_size_in_bytes, const uint32_t vertex_stride_in_bytes)
{
    MRQ2_ASSERT(buffer_size_in_bytes   != 0);
    MRQ2_ASSERT(vertex_stride_in_bytes != 0);

    // TODO

    m_size_in_bytes   = buffer_size_in_bytes;
    m_stride_in_bytes = vertex_stride_in_bytes;
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// IndexBufferVK
///////////////////////////////////////////////////////////////////////////////

bool IndexBufferVK::Init(const DeviceVK & device, const uint32_t buffer_size_in_bytes, const IndexFormat format)
{
    MRQ2_ASSERT(buffer_size_in_bytes != 0);

    // TODO

    m_size_in_bytes = buffer_size_in_bytes;
    m_index_format  = format;
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// ConstantBufferVK
///////////////////////////////////////////////////////////////////////////////

bool ConstantBufferVK::Init(const DeviceVK & device, const uint32_t buffer_size_in_bytes, const Flags flags)
{
    MRQ2_ASSERT(buffer_size_in_bytes != 0);

    // TODO

    m_size_in_bytes = buffer_size_in_bytes;
    m_flags = flags;
    return true;
}

} // namespace MrQ2
