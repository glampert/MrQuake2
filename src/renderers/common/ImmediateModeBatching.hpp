//
// ImmediateModeBatching.hpp
//  Simple OpenGL-style immediate mode emulation.
//
#pragma once

#include "Common.hpp"
#include "Memory.hpp"
#include "RenderInterface.hpp"
#include <vector>

namespace MrQ2
{

/*
===============================================================================

    DrawVertex3D / DrawVertex2D

===============================================================================
*/

struct DrawVertex3D
{
    vec3_t position;
    vec2_t uv;
    vec4_t rgba;
};

struct DrawVertex2D
{
    vec4_t xy_uv;
    vec4_t rgba;
};

class TextureImage;
struct ModelSurface;

/*
===============================================================================

    ConstBuffers

===============================================================================
*/
template<typename T>
class ConstBuffers final
{
public:

    T data{};

    void Init(const RenderDevice & device) { m_buffers.Init(device, sizeof(T)); }
    void Shutdown() { m_buffers.Shutdown(); }

    ConstantBuffer & CurrentBuffer() { return m_buffers.CurrentBuffer(); }
    void Upload() { m_buffers.CurrentBuffer().WriteStruct(data); }
    void MoveToNextFrame() { m_buffers.MoveToNextFrame(); }

private:

    ScratchConstantBuffers m_buffers;
};

/*
===============================================================================

    VertexBuffers
    - Multiple mapped vertex buffers helper.

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
        uint32_t       used_verts;
    };

    void Init(const RenderDevice & device, const uint32_t max_verts)
    {
        MRQ2_ASSERT(max_verts != 0);
        m_max_verts = max_verts;

        const uint32_t buffer_size_in_bytes   = sizeof(VertexType) * m_max_verts;
        const uint32_t vertex_stride_in_bytes = sizeof(VertexType);

        for (uint32_t b = 0; b < kNumBuffers; ++b)
        {
            if (!m_vertex_buffers[b].Init(device, buffer_size_in_bytes, vertex_stride_in_bytes))
            {
                GameInterface::Errorf("Failed to create vertex buffer %u", b);
            }
            m_mapped_ptrs[b] = nullptr;
        }

        MemTagsTrackAlloc(buffer_size_in_bytes * kNumBuffers, MemTag::kVertIndexBuffer);
        GameInterface::Printf("VertexBuffers used memory: %s", FormatMemoryUnit(buffer_size_in_bytes * kNumBuffers));
    }

    void Shutdown()
    {
        m_max_verts    = 0;
        m_used_verts   = 0;
        m_buffer_index = 0;

        for (uint32_t b = 0; b < kNumBuffers; ++b)
        {
            m_mapped_ptrs[b] = nullptr;
            m_vertex_buffers[b].Shutdown();
        }
    }

    VertexType * Increment(const uint32_t count)
    {
        MRQ2_ASSERT(count != 0 && count <= m_max_verts);

        VertexType * verts = m_mapped_ptrs[m_buffer_index];
        MRQ2_ASSERT(verts != nullptr);
        MRQ2_ASSERT_ALIGN16(verts);

        verts += m_used_verts;
        m_used_verts += count;

        if (m_used_verts > m_max_verts)
        {
            GameInterface::Errorf("Vertex buffer overflowed! Used=%u, Max=%u. Increase size.", m_used_verts, m_max_verts);
        }

        return verts;
    }

    uint32_t BufferSize() const
    {
        return m_max_verts;
    }

    uint32_t NumVertsRemaining() const
    {
        MRQ2_ASSERT((m_max_verts - m_used_verts) > 0);
        return m_max_verts - m_used_verts;
    }

    uint32_t CurrentPosition() const
    {
        return m_used_verts;
    }

    VertexType * CurrentVertexPtr() const
    {
        return m_mapped_ptrs[m_buffer_index] + m_used_verts;
    }

    void BeginFrame()
    {
        MRQ2_ASSERT(m_used_verts == 0); // Missing End()?

        // Map the current buffer:
        void * const memory = m_vertex_buffers[m_buffer_index].Map();
        if (memory == nullptr)
        {
            GameInterface::Errorf("Failed to map vertex buffer %u", m_buffer_index);
        }

        MRQ2_ASSERT_ALIGN16(memory);
        m_mapped_ptrs[m_buffer_index] = static_cast<VertexType *>(memory);
    }

    DrawBuffer EndFrame()
    {
        MRQ2_ASSERT(m_mapped_ptrs[m_buffer_index] != nullptr); // Missing Begin()?

        VertexBuffer & current_buffer = m_vertex_buffers[m_buffer_index];
        const uint32_t current_position = m_used_verts;

        // Unmap current buffer so we can draw with it:
        current_buffer.Unmap();
        m_mapped_ptrs[m_buffer_index] = nullptr;

        // Move to the next buffer:
        m_buffer_index = (m_buffer_index + 1) % kNumBuffers;
        m_used_verts = 0;

        return { &current_buffer, current_position };
    }

private:

    uint32_t m_max_verts{ 0 };
    uint32_t m_used_verts{ 0 };
    uint32_t m_buffer_index{ 0 };

    VertexType * m_mapped_ptrs[kNumBuffers] = {};
    VertexBuffer m_vertex_buffers[kNumBuffers] = {};
};

/*
===============================================================================

    SpriteBatch
    - 2D immediate mode sprite rendering for UI elements.

===============================================================================
*/
class SpriteBatch final
{
public:

    enum BatchIndex : uint32_t
    {
        kDrawChar,  // Only used to draw console chars
        kDrawPics,  // Used by DrawPic, DrawStretchPic, etc
        kBatchCount // Number of items in the enum - not a valid index.
    };

    void Init(const RenderDevice & device, const uint32_t max_verts);
    void Shutdown();

    void BeginFrame();
    void EndFrame(GraphicsContext & context, const ConstantBuffer & cbuff, const PipelineState & pipeline_state, const TextureImage * const opt_tex_atlas = nullptr);

    DrawVertex2D * Increment(const uint32_t count)
    {
        return m_buffers.Increment(count);
    }

    void PushTriVerts(const DrawVertex2D tri[3])
    {
        DrawVertex2D * verts = Increment(3);
        std::memcpy(verts, tri, sizeof(DrawVertex2D) * 3);
    }

    void PushQuadVerts(const DrawVertex2D quad[4]);
    void PushQuad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, const ColorRGBA32 color);
    void PushQuadTextured(float x, float y, float w, float h, const TextureImage * tex, const ColorRGBA32 color);
    void PushQuadTexturedUVs(float x, float y, float w, float h, float u0, float v0, float u1, float v1, const TextureImage * tex, const ColorRGBA32 color);

private:

    struct DeferredTexQuad
    {
        const TextureImage * tex;
        uint32_t quad_start_vtx;
    };

    using QuadList = std::vector<DeferredTexQuad>;
    using VBuffers = VertexBuffers<DrawVertex2D, RenderInterface::kNumFrameBuffers>;

    QuadList m_deferred_textured_quads;
    VBuffers m_buffers;
};

/*
===============================================================================

    SpriteBatches

===============================================================================
*/
class SpriteBatches final
{
public:

    void Init(const RenderDevice & device);
    void Shutdown();

    void BeginFrame();
    void EndFrame(GraphicsContext & context, const ConstantBuffer & cbuff, const TextureImage * glyphs_texture);

    SpriteBatch & Get(const SpriteBatch::BatchIndex index) { return m_batches[index]; }

private:

    SpriteBatch   m_batches[SpriteBatch::kBatchCount];
    PipelineState m_pipeline_draw_sprites;
    ShaderProgram m_shader_draw_sprites;
};

/*
===============================================================================

    MiniImBatch
    - Immediate mode OpenGL style emulation.

===============================================================================
*/
class MiniImBatch final
{
public:

    MiniImBatch(DrawVertex3D * const verts_ptr, const uint32_t num_verts, const PrimitiveTopology topology)
        : m_verts_ptr{ verts_ptr }
        , m_num_verts{ num_verts }
        , m_used_verts{ 0 }
        , m_topology{ topology }
        , m_tri_fan_vert_count{ 0 }
        , m_tri_fan_first_vert{}
        , m_tri_fan_last_vert{}
    { }

    void Clear()
    {
        m_verts_ptr  = nullptr;
        m_num_verts  = 0;
        m_used_verts = 0;
    }

    DrawVertex3D * Increment(const uint32_t num_verts)
    {
        auto * first_vert = (m_verts_ptr + m_used_verts);
        m_used_verts += num_verts;
        if (m_used_verts > m_num_verts)
        {
            OverflowError();
        }
        return first_vert;
    }

    void SetTriangleFanFirstVertex(const DrawVertex3D & vert)
    {
        if (sm_emulated_triangle_fans)
        {
            m_tri_fan_vert_count = 1;
            m_tri_fan_first_vert = vert;
        }
        else
        {
            PushVertex(vert);
        }
    }

    static void EnableEmulatedTriangleFans(const bool do_enable)
    {
        sm_emulated_triangle_fans = do_enable;
    }

    void PushVertex(const DrawVertex3D & vert);
    void PushModelSurface(const ModelSurface & surf, const vec4_t * opt_color_override = nullptr);

    uint32_t NumVerts()  const { return m_num_verts; }
    uint32_t UsedVerts() const { return m_used_verts; }

    bool IsValid() const { return m_verts_ptr != nullptr; }
    PrimitiveTopology Topology() const { return m_topology; }

private:

    MRQ2_RENDERLIB_NORETURN void OverflowError() const;

    DrawVertex3D *    m_verts_ptr;
    uint32_t          m_num_verts;
    uint32_t          m_used_verts;
    PrimitiveTopology m_topology;

    // Triangle fan emulation support:
    uint8_t           m_tri_fan_vert_count;
    DrawVertex3D      m_tri_fan_first_vert;
    DrawVertex3D      m_tri_fan_last_vert;

    // If set to true, deconstruct PrimitiveTopology::TriangleFan primitives in the
    // MiniImBatch into PrimitiveTopology::TriangleList primitives, to support back-end
    // APIs that are not capable of drawing triangle fans natively.
    static bool       sm_emulated_triangle_fans;
};

} // MrQ2
