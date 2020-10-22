//
// BufferVK.hpp
//
#pragma once

#include "UtilsVK.hpp"

namespace MrQ2
{

class DeviceVK;

///////////////////////////////////////////////////////////////////////////////

class BufferVK
{
public:

    BufferVK() = default;

    // Disallow copy.
    BufferVK(const BufferVK &) = delete;
    BufferVK & operator=(const BufferVK &) = delete;

    void Shutdown();
    void * Map();
    void Unmap();

    uint32_t SizeInBytes() const { return m_buffer_size; }
    VkBuffer Handle()      const { return m_buffer_handle; }

protected:

    ~BufferVK() { Shutdown(); }
    void InitBufferInternal(const DeviceVK & device, const uint32_t buffer_size_in_bytes,
                            const VkBufferUsageFlags buffer_usage, VkMemoryRequirements * out_opt_mem_reqs = nullptr);

    const DeviceVK *   m_device_vk{ nullptr };
    VkBuffer           m_buffer_handle{ nullptr };
    VkDeviceMemory     m_buffer_mem_handle{ nullptr };
    uint32_t           m_buffer_size{ 0 };
    VkBufferUsageFlags m_buffer_usage{ 0 };
};

///////////////////////////////////////////////////////////////////////////////

class VertexBufferVK final : public BufferVK
{
public:

    bool Init(const DeviceVK & device, const uint32_t buffer_size_in_bytes, const uint32_t vertex_stride_in_bytes);

    uint32_t StrideInBytes() const { return m_stride_in_bytes; }

private:

    uint32_t m_stride_in_bytes{ 0 };
};

///////////////////////////////////////////////////////////////////////////////

class IndexBufferVK final : public BufferVK
{
public:

    enum IndexFormat : uint32_t
    {
        kFormatUInt16,
        kFormatUInt32,
    };

    bool Init(const DeviceVK & device, const uint32_t buffer_size_in_bytes, const IndexFormat format);

    uint32_t    StrideInBytes() const { return (m_index_format == kFormatUInt16) ? sizeof(uint16_t) : sizeof(uint32_t); }
    VkIndexType TypeVK()        const { return (m_index_format == kFormatUInt16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32; }
    IndexFormat Format()        const { return m_index_format; }

private:

    IndexFormat m_index_format{};
};

///////////////////////////////////////////////////////////////////////////////

class ConstantBufferVK final : public BufferVK
{
    friend class GraphicsContextVK;

public:

    // Buffer is updated, used for a single draw call then discarded (PerDrawShaderConstants).
    enum Flags : uint32_t { kNoFlags = 0, kOptimizeForSingleDraw = (1 << 1) };

    bool Init(const DeviceVK & device, const uint32_t buffer_size_in_bytes, const Flags flags = kNoFlags);

    template<typename T>
    void WriteStruct(const T & cbuffer_data)
    {
        MRQ2_ASSERT(sizeof(T) <= SizeInBytes());
        void * cbuffer_upload_mem = Map();
        std::memcpy(cbuffer_upload_mem, &cbuffer_data, sizeof(T));
        Unmap();
    }

private:

    Flags m_flags{ kNoFlags };
};

///////////////////////////////////////////////////////////////////////////////

class ScratchConstantBuffersVK final
{
public:

    void Init(const DeviceVK & device, const uint32_t buffer_size_in_bytes)
    {
        for (auto & cbuf : m_cbuffers)
        {
            const bool buffer_ok = cbuf.Init(device, buffer_size_in_bytes);
            MRQ2_ASSERT(buffer_ok);
        }
    }

    void Shutdown()
    {
        m_current_buffer = 0;
        for (auto & cbuf : m_cbuffers)
        {
            cbuf.Shutdown();
        }
    }

    ConstantBufferVK & CurrentBuffer()
    {
        MRQ2_ASSERT(m_current_buffer < ArrayLength(m_cbuffers));
        return m_cbuffers[m_current_buffer];
    }

    void MoveToNextFrame()
    {
        m_current_buffer = (m_current_buffer + 1) % kVkNumFrameBuffers;
    }

private:

    uint32_t m_current_buffer{ 0 };
    ConstantBufferVK m_cbuffers[kVkNumFrameBuffers];
};

} // MrQ2
