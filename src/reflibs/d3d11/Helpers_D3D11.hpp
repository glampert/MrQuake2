//
// Helpers_D3D11.hpp
//  Misc helper classes.
//
#pragma once

#include "RenderWindow_D3D11.hpp"
#include "reflibs/shared/MiniImBatch.hpp"
#include "reflibs/shared/TextureStore.hpp"
#include <DirectXMath.h>
#include <array>

namespace MrQ2
{
namespace D3D11
{

class TextureImageImpl;

/*
===============================================================================

    D3D11 InputLayoutDesc - Input element desc array + count

===============================================================================
*/
struct InputLayoutDesc final
{
    const D3D11_INPUT_ELEMENT_DESC * desc;
    int                              num_elements;
};

/*
===============================================================================

    D3D11 ShaderProgram

===============================================================================
*/
class ShaderProgram final
{
public:

    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader>  ps;
    ComPtr<ID3D11InputLayout>  vertex_layout;

    void LoadFromFxFile(ID3D11Device * device,
                        const wchar_t * filename,
                        const char * vs_entry,
                        const char * ps_entry,
                        const InputLayoutDesc & layout,
                        const bool debug);
};

/*
===============================================================================

    D3D11 DepthStates

===============================================================================
*/
class DepthStates final
{
public:

    ComPtr<ID3D11DepthStencilState> enabled_state;
    ComPtr<ID3D11DepthStencilState> disabled_state;

    void Init(ID3D11Device * device,
              const bool enabled_ztest,
              const D3D11_COMPARISON_FUNC enabled_func,
              const D3D11_DEPTH_WRITE_MASK enabled_write_mask,
              const bool disabled_ztest,
              const D3D11_COMPARISON_FUNC disabled_func,
              const D3D11_DEPTH_WRITE_MASK disabled_write_mask);
};

/*
===============================================================================

    D3D11 VertexBuffers

===============================================================================
*/
template<typename VertexType, int kNumBuffers>
class VertexBuffers final
{
public:

    VertexBuffers() = default;

    struct DrawBuffer
    {
        ID3D11Buffer * buffer_ptr;
        int used_verts;
    };

    void Init(const char * const debug_name, const int max_verts, ID3D11Device * device, ID3D11DeviceContext * context)
    {
        FASTASSERT(device != nullptr && context != nullptr);

        m_num_verts  = max_verts;
        m_debug_name = debug_name;
        m_context    = context;

        D3D11_BUFFER_DESC bd = {};
        bd.Usage             = D3D11_USAGE_DYNAMIC;
        bd.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
        bd.ByteWidth         = sizeof(VertexType) * max_verts;
        bd.BindFlags         = D3D11_BIND_VERTEX_BUFFER;

        for (unsigned b = 0; b < m_vertex_buffers.size(); ++b)
        {
            if (FAILED(device->CreateBuffer(&bd, nullptr, m_vertex_buffers[b].GetAddressOf())))
            {
                GameInterface::Errorf("Failed to create %s vertex buffer %u", debug_name, b);
            }
            m_mapped_ptrs[b] = nullptr;
        }

        MemTagsTrackAlloc(bd.ByteWidth * kNumBuffers, MemTag::kVertIndexBuffer);
        GameInterface::Printf("%s used %s", debug_name, FormatMemoryUnit(bd.ByteWidth * kNumBuffers));
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
        D3D11_MAPPED_SUBRESOURCE mapping_info = {};
        if (FAILED(m_context->Map(m_vertex_buffers[m_buffer_index].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping_info)))
        {
            GameInterface::Errorf("Failed to map %s vertex buffer %i", m_debug_name, m_buffer_index);
        }

        FASTASSERT(mapping_info.pData != nullptr);
        FASTASSERT_ALIGN16(mapping_info.pData);

        m_mapped_ptrs[m_buffer_index] = static_cast<VertexType *>(mapping_info.pData);
    }

    DrawBuffer End()
    {
        FASTASSERT(m_mapped_ptrs[m_buffer_index] != nullptr); // Missing Begin()?

        ID3D11Buffer * const current_buffer = m_vertex_buffers[m_buffer_index].Get();
        const int current_position = m_used_verts;

        // Unmap current buffer so we can draw with it:
        m_context->Unmap(current_buffer, 0);
        m_mapped_ptrs[m_buffer_index] = nullptr;

        // Move to the next buffer:
        m_buffer_index = (m_buffer_index + 1) % kNumBuffers;
        m_used_verts = 0;

        return { current_buffer, current_position };
    }

private:

    int m_num_verts    = 0;
    int m_used_verts   = 0;
    int m_buffer_index = 0;

    ID3D11DeviceContext * m_context    = nullptr;
    const char *          m_debug_name = nullptr;

    std::array<ComPtr<ID3D11Buffer>, kNumBuffers> m_vertex_buffers{};
    std::array<VertexType *,         kNumBuffers> m_mapped_ptrs{};
};

/*
===============================================================================

    D3D11 SpriteBatch

===============================================================================
*/
class SpriteBatch final
{
public:

    static constexpr int kNumSpriteBatchVertexBuffers = 2;

    SpriteBatch() = default;
    void Init(int max_verts);

    void BeginFrame();
    void EndFrame(const ShaderProgram & program, const TextureImageImpl * tex, ID3D11Buffer * cbuffer);

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

    VertexBuffers<DrawVertex2D, kNumSpriteBatchVertexBuffers> m_buffers;

    struct DeferredTexQuad
    {
        int quad_start_vtx;
        const TextureImageImpl * tex;
    };
    std::vector<DeferredTexQuad> m_deferred_textured_quads;
};

} // D3D11
} // MrQ2
