//
// Helpers_D3D12.hpp
//  Misc helper classes.
//
#pragma once

#include "RenderWindow_D3D12.hpp"
#include "reflibs/shared/MiniImBatch.hpp"
#include "reflibs/shared/TextureStore.hpp"
#include "reflibs/shared/d3d/D3DShader.hpp"
#include <DirectXMath.h>
#include <array>

namespace MrQ2
{
namespace D3D12
{

class TextureImageImpl;

/*
===============================================================================

    D3D12 ShaderProgram

===============================================================================
*/
struct ShaderProgram final
{
    ComPtr<ID3D12RootSignature> rootsig;
    D3DShader::Blobs            shader_bytecode;

    void LoadFromFxFile(const wchar_t * filename, const char * vs_entry, const char * ps_entry, const bool debug);
    void CreateRootSignature(ID3D12Device5 * device, const D3D12_ROOT_SIGNATURE_DESC & rootsig_desc);
};

/*
===============================================================================

    D3D12 Buffer

===============================================================================
*/
struct Buffer
{
    ComPtr<ID3D12Resource> resource;

    bool Init(ID3D12Device5 * device, const uint32_t size_in_bytes);
    void * Map();
    void Unmap();
};

struct VertexBuffer final : public Buffer
{
    D3D12_VERTEX_BUFFER_VIEW view = {};

    bool Init(ID3D12Device5 * device, const uint32_t buffer_size_in_bytes, const uint32_t vertex_stride_in_bytes)
    {
        const bool buffer_ok = Buffer::Init(device, buffer_size_in_bytes);
        if (buffer_ok)
        {
            view.BufferLocation = resource->GetGPUVirtualAddress();
            view.SizeInBytes    = buffer_size_in_bytes;
            view.StrideInBytes  = vertex_stride_in_bytes;
        }
        return buffer_ok;
    }
};

/*
===============================================================================

    D3D12 VertexBuffers

===============================================================================
*/
template<typename VertexType, uint32_t kNumBuffers>
class VertexBuffers final
{
public:

    VertexBuffers() = default;

    struct DrawBuffer
    {
        VertexBuffer * buffer_ptr;
        int            used_verts;
    };

    void Init(ID3D12Device5 * device, const char * const debug_name, const int max_verts)
    {
        FASTASSERT(device != nullptr);

        m_num_verts  = max_verts;
        m_debug_name = debug_name;

        const uint32_t buffer_size_in_bytes   = sizeof(VertexType) * max_verts;
        const uint32_t vertex_stride_in_bytes = sizeof(VertexType);

        for (unsigned b = 0; b < m_vertex_buffers.size(); ++b)
        {
            if (!m_vertex_buffers[b].Init(device, buffer_size_in_bytes, vertex_stride_in_bytes))
            {
                GameInterface::Errorf("Failed to create %s vertex buffer %u", debug_name, b);
            }
            m_mapped_ptrs[b] = nullptr;
        }

        MemTagsTrackAlloc(buffer_size_in_bytes * kNumBuffers, MemTag::kVertIndexBuffer);
        GameInterface::Printf("%s used %s", debug_name, FormatMemoryUnit(buffer_size_in_bytes * kNumBuffers));
    }

    VertexType * Increment(const int count)
    {
        FASTASSERT(count > 0 && count <= m_num_verts);

        VertexType * verts = m_mapped_ptrs[m_buffer_index];
        FASTASSERT(verts != nullptr);
        FASTASSERT_ALIGN16(verts);

        verts += m_used_verts;
        m_used_verts += count;

        if (m_used_verts > m_num_verts)
        {
            GameInterface::Errorf("%s vertex buffer overflowed! used_verts=%i, num_verts=%i. "
                                  "Increase size.", m_debug_name, m_used_verts, m_num_verts);
        }

        return verts;
    }

    int BufferSize() const
    {
        return m_num_verts;
    }

    int NumVertsRemaining() const
    {
        FASTASSERT((m_num_verts - m_used_verts) > 0);
        return m_num_verts - m_used_verts;
    }

    int CurrentPosition() const
    {
        return m_used_verts;
    }

    VertexType * CurrentVertexPtr() const
    {
        return m_mapped_ptrs[m_buffer_index] + m_used_verts;
    }

    void Begin()
    {
        FASTASSERT(m_used_verts == 0); // Missing End()?

        // Map the current buffer:
        void * const memory = m_vertex_buffers[m_buffer_index].Map();
        if (memory == nullptr)
        {
            GameInterface::Errorf("Failed to map %s vertex buffer %i", m_debug_name, m_buffer_index);
        }

        FASTASSERT_ALIGN16(memory);
        m_mapped_ptrs[m_buffer_index] = static_cast<VertexType *>(memory);
    }

    DrawBuffer End()
    {
        FASTASSERT(m_mapped_ptrs[m_buffer_index] != nullptr); // Missing Begin()?

        VertexBuffer & current_buffer = m_vertex_buffers[m_buffer_index];
        const int current_position = m_used_verts;

        // Unmap current buffer so we can draw with it:
        current_buffer.Unmap();
        m_mapped_ptrs[m_buffer_index] = nullptr;

        // Move to the next buffer:
        m_buffer_index = (m_buffer_index + 1) % kNumBuffers;
        m_used_verts = 0;

        return { &current_buffer, current_position };
    }

private:

    int m_num_verts    = 0;
    int m_used_verts   = 0;
    int m_buffer_index = 0;

    std::array<VertexBuffer, kNumBuffers> m_vertex_buffers{};
    std::array<VertexType *, kNumBuffers> m_mapped_ptrs{};

    const char * m_debug_name = nullptr;
};

/*
===============================================================================

    D3D12 SpriteBatch

===============================================================================
*/
class SpriteBatch final
{
public:

    SpriteBatch() = default;
    void Init(ID3D12Device5 * device, int max_verts);

    void BeginFrame();
    void EndFrame(const ShaderProgram & program, const TextureImageImpl * tex);

    DrawVertex2D * Increment(const int count) { return m_buffers.Increment(count); }

    void PushTriVerts(const DrawVertex2D tri[3]);
    void PushQuadVerts(const DrawVertex2D quad[4]);

    void PushQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  const DirectX::XMFLOAT4A & color);

    void PushQuadTextured(float x, float y, float w, float h,
                          const TextureImage * tex, const DirectX::XMFLOAT4A & color);

    void PushQuadTexturedUVs(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                             const TextureImage * tex, const DirectX::XMFLOAT4A & color);

    // Disallow copy.
    SpriteBatch(const SpriteBatch &) = delete;
    SpriteBatch & operator=(const SpriteBatch &) = delete;

private:

    VertexBuffers<DrawVertex2D, kNumFrameBuffers> m_buffers;

    struct DeferredTexQuad
    {
        int quad_start_vtx;
        const TextureImageImpl * tex;
    };
    std::vector<DeferredTexQuad> m_deferred_textured_quads;
};

} // D3D12
} // MrQ2
