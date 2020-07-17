//
// BufferD3D12.hpp
//
#pragma once

#include "UtilsD3D12.hpp"

namespace MrQ2
{

class DeviceD3D12;
class GraphicsContextD3D12;

///////////////////////////////////////////////////////////////////////////////

class BufferD3D12
{
public:

    BufferD3D12() = default;

    // Disallow copy.
    BufferD3D12(const BufferD3D12 &) = delete;
    BufferD3D12 & operator=(const BufferD3D12 &) = delete;

    bool InitUntypedBuffer(const DeviceD3D12 & device, const uint32_t size_in_bytes);
    void Shutdown();

    void * Map();
    void Unmap();

protected:

    D12ComPtr<ID3D12Resource> m_resource;
};

///////////////////////////////////////////////////////////////////////////////

class VertexBufferD3D12 final : public BufferD3D12
{
    friend GraphicsContextD3D12;

public:

    bool Init(const DeviceD3D12 & device, const uint32_t buffer_size_in_bytes, const uint32_t vertex_stride_in_bytes);

    uint32_t SizeInBytes()   const { return m_view.SizeInBytes;   }
    uint32_t StrideInBytes() const { return m_view.StrideInBytes; }

private:

    D3D12_VERTEX_BUFFER_VIEW m_view = {};
};

///////////////////////////////////////////////////////////////////////////////

class IndexBufferD3D12 final : public BufferD3D12
{
    friend GraphicsContextD3D12;

public:

    enum IndexFormat : uint32_t
    {
        kFormatUInt16,
        kFormatUInt32,
    };

    bool Init(const DeviceD3D12 & device, const uint32_t buffer_size_in_bytes, const IndexFormat format);

    uint32_t    SizeInBytes()   const { return m_view.SizeInBytes; }
    uint32_t    StrideInBytes() const { return (m_view.Format == DXGI_FORMAT_R16_UINT) ? sizeof(uint16_t) : sizeof(uint32_t); }
    IndexFormat Format()        const { return (m_view.Format == DXGI_FORMAT_R16_UINT) ? kFormatUInt16    : kFormatUInt32;    }

private:

    D3D12_INDEX_BUFFER_VIEW m_view = {};
};

///////////////////////////////////////////////////////////////////////////////

class ConstantBufferD3D12 final : public BufferD3D12
{
    friend GraphicsContextD3D12;

public:

    bool Init(const DeviceD3D12 & device, const uint32_t buffer_size_in_bytes);

    template<typename T>
    void WriteStruct(const T & cbuffer_data)
    {
        MRQ2_ASSERT(sizeof(T) <= SizeInBytes());
        void * cbuffer_upload_mem = Map();
        std::memcpy(cbuffer_upload_mem, &cbuffer_data, sizeof(T));
        Unmap();
    }

    uint32_t SizeInBytes() const { return m_view.SizeInBytes; }

private:

    D3D12_CONSTANT_BUFFER_VIEW_DESC m_view = {};
};

///////////////////////////////////////////////////////////////////////////////

class ScratchConstantBuffersD3D12 final
{
public:

    void Init(const DeviceD3D12 & device, const uint32_t buffer_size_in_bytes)
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
            cbuf.Shutdown();
    }

    ConstantBufferD3D12 & CurrentBuffer()
    {
        MRQ2_ASSERT(m_current_buffer < ArrayLength(m_cbuffers));
        return m_cbuffers[m_current_buffer];
    }

    void MoveToNextFrame()
    {
        m_current_buffer = (m_current_buffer + 1) % kD12NumFrameBuffers;
    }

private:

    uint32_t m_current_buffer{ 0 };
    ConstantBufferD3D12 m_cbuffers[kD12NumFrameBuffers];
};

} // MrQ2
