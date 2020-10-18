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

void BufferVK::InitBufferInternal(const DeviceVK & device, const uint32_t buffer_size_in_bytes, const VkBufferUsageFlags buffer_usage)
{
    MRQ2_ASSERT(m_device_vk == nullptr);
    MRQ2_ASSERT(buffer_size_in_bytes != 0);

    const auto memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VulkanAllocateBuffer(device, buffer_size_in_bytes, buffer_usage, memory_flags, &m_buffer_handle, &m_buffer_mem_handle);

    m_device_vk    = &device;
    m_buffer_size  = buffer_size_in_bytes;
    m_buffer_usage = buffer_usage;
}

void BufferVK::Shutdown()
{
    if (m_device_vk == nullptr)
    {
        return;
    }

    if (m_buffer_mem_handle != nullptr)
    {
        vkFreeMemory(m_device_vk->Handle(), m_buffer_mem_handle, nullptr);
        m_buffer_mem_handle = nullptr;
    }

    if (m_buffer_handle != nullptr)
    {
        vkDestroyBuffer(m_device_vk->Handle(), m_buffer_handle, nullptr);
        m_buffer_handle = nullptr;
    }

    m_device_vk = nullptr;
}

void * BufferVK::Map()
{
    // Map the staging buffer.
    void * memory = nullptr;
    VULKAN_CHECK(vkMapMemory(m_device_vk->Handle(), m_buffer_mem_handle, 0, m_buffer_size, 0, &memory));
    return memory;
}

void BufferVK::Unmap()
{
    vkUnmapMemory(m_device_vk->Handle(), m_buffer_mem_handle);
}

///////////////////////////////////////////////////////////////////////////////
// VertexBufferVK
///////////////////////////////////////////////////////////////////////////////

bool VertexBufferVK::Init(const DeviceVK & device, const uint32_t buffer_size_in_bytes, const uint32_t vertex_stride_in_bytes)
{
    MRQ2_ASSERT(vertex_stride_in_bytes != 0);
    InitBufferInternal(device, buffer_size_in_bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_stride_in_bytes = vertex_stride_in_bytes;
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// IndexBufferVK
///////////////////////////////////////////////////////////////////////////////

bool IndexBufferVK::Init(const DeviceVK & device, const uint32_t buffer_size_in_bytes, const IndexFormat format)
{
    InitBufferInternal(device, buffer_size_in_bytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    m_index_format = format;
    return true;
}

///////////////////////////////////////////////////////////////////////////////
// ConstantBufferVK
///////////////////////////////////////////////////////////////////////////////

bool ConstantBufferVK::Init(const DeviceVK & device, const uint32_t buffer_size_in_bytes, const Flags flags)
{
    InitBufferInternal(device, buffer_size_in_bytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_flags = flags;
    return true;
}

} // namespace MrQ2
