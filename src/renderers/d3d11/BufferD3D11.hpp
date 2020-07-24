//
// BufferD3D11.hpp
//
#pragma once

#include "UtilsD3D11.hpp"

namespace MrQ2
{

class DeviceD3D11;
class GraphicsContextD3D11;

///////////////////////////////////////////////////////////////////////////////

class BufferD3D11
{
public:

    BufferD3D11() = default;

    // Disallow copy.
    BufferD3D11(const BufferD3D11 &) = delete;
    BufferD3D11 & operator=(const BufferD3D11 &) = delete;

    void Shutdown();
    void * Map();
    void Unmap();

protected:

    void InitBufferInternal(const DeviceD3D11 & device, const D3D11_BUFFER_DESC & buffer_desc);

    const DeviceD3D11 *     m_device{ nullptr };
    D11ComPtr<ID3D11Buffer> m_resource;
};

///////////////////////////////////////////////////////////////////////////////

class VertexBufferD3D11 final : public BufferD3D11
{
    friend GraphicsContextD3D11;

public:

    bool Init(const DeviceD3D11 & device, const uint32_t buffer_size_in_bytes, const uint32_t vertex_stride_in_bytes);

    uint32_t SizeInBytes()   const { return m_size_in_bytes; }
    uint32_t StrideInBytes() const { return m_stride_in_bytes; }

private:

    uint32_t m_size_in_bytes{ 0 };
    uint32_t m_stride_in_bytes{ 0 };
};

///////////////////////////////////////////////////////////////////////////////

class IndexBufferD3D11 final : public BufferD3D11
{
    friend GraphicsContextD3D11;

public:

    enum IndexFormat : uint32_t
    {
        kFormatUInt16,
        kFormatUInt32,
    };

    bool Init(const DeviceD3D11 & device, const uint32_t buffer_size_in_bytes, const IndexFormat format);

    uint32_t    SizeInBytes()   const { return m_size_in_bytes; }
    uint32_t    StrideInBytes() const { return (m_index_format == kFormatUInt16) ? sizeof(uint16_t) : sizeof(uint32_t); }
    IndexFormat Format()        const { return m_index_format; }

private:

    uint32_t    m_size_in_bytes{ 0 };
    IndexFormat m_index_format{};
};

///////////////////////////////////////////////////////////////////////////////

class ConstantBufferD3D11 final : public BufferD3D11
{
    friend GraphicsContextD3D11;

public:

    // Buffer is updated, used for a single draw call then discarded (PerDrawShaderConstants).
    enum Flags : uint32_t { kNoFlags = 0, kOptimizeForSingleDraw = (1 << 1) };

    bool Init(const DeviceD3D11 & device, const uint32_t buffer_size_in_bytes, const Flags flags = kNoFlags);

    template<typename T>
    void WriteStruct(const T & cbuffer_data)
    {
        MRQ2_ASSERT(sizeof(T) <= SizeInBytes());
        void * cbuffer_upload_mem = Map();
        std::memcpy(cbuffer_upload_mem, &cbuffer_data, sizeof(T));
        Unmap();
    }

    uint32_t SizeInBytes() const { return m_size_in_bytes; }

private:

    uint32_t m_size_in_bytes{ 0 };
    Flags    m_flags{ kNoFlags };
};

///////////////////////////////////////////////////////////////////////////////

class ScratchConstantBuffersD3D11 final
{
public:

    void Init(const DeviceD3D11 & device, const uint32_t buffer_size_in_bytes)
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

    ConstantBufferD3D11 & CurrentBuffer()
    {
        MRQ2_ASSERT(m_current_buffer < ArrayLength(m_cbuffers));
        return m_cbuffers[m_current_buffer];
    }

    void MoveToNextFrame()
    {
        m_current_buffer = (m_current_buffer + 1) % kD11NumFrameBuffers;
    }

private:

    uint32_t m_current_buffer{ 0 };
    ConstantBufferD3D11 m_cbuffers[kD11NumFrameBuffers];
};

} // MrQ2
