//
// ImmediateModeBatching.cpp
//  Simple OpenGL-style immediate mode emulation.
//

#include "ImmediateModeBatching.hpp"
#include "TextureStore.hpp"
#include "ModelStore.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// SpriteBatch
///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::Init(const RenderDevice & device, const uint32_t max_verts)
{
    m_buffers.Init(device, max_verts);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::Shutdown()
{
    m_deferred_textured_quads.clear();
    m_deferred_textured_quads.shrink_to_fit();
    m_buffers.Shutdown();
}

void SpriteBatch::BeginFrame()
{
    m_buffers.Begin();
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::EndFrame(GraphicsContext & context, const PipelineState & pipeline_state, const TextureImage * const opt_tex_atlas)
{
    const auto draw_buf = m_buffers.End();

    context.SetVertexBuffer(*draw_buf.buffer_ptr);
    context.SetPipelineState(pipeline_state);

    // Fast path - one texture for the whole batch:
    if (opt_tex_atlas != nullptr)
    {
        context.SetTexture(opt_tex_atlas->texture);

        const auto first_vertex = 0u;
        const auto vertex_count = draw_buf.used_verts;
        context.Draw(first_vertex, vertex_count);
    }
    else // Handle small unique textured draws:
    {
        for (const DeferredTexQuad & d : m_deferred_textured_quads)
        {
            context.SetTexture(d.tex->texture);

            const auto first_vertex = d.quad_start_vtx;
            const auto vertex_count = 6u; // one quad
            context.Draw(first_vertex, vertex_count);
        }
    }

    // Clear cache for next frame:
    if (!m_deferred_textured_quads.empty())
    {
        m_deferred_textured_quads.clear();
    }
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadVerts(const DrawVertex2D quad[4])
{
    DrawVertex2D * tri = Increment(6);           // Expand quad into 2 triangles
    const int indexes[6] = { 0, 1, 2, 2, 3, 0 }; // CW winding
    for (int i = 0; i < 6; ++i)
    {
        tri[i] = quad[indexes[i]];
    }
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, const ColorRGBA32 color)
{
    float r, g, b, a;
    ColorFloats(color, r, g, b, a);

    DrawVertex2D quad[4];
    quad[0].xy_uv[0] = x;
    quad[0].xy_uv[1] = y;
    quad[0].xy_uv[2] = u0;
    quad[0].xy_uv[3] = v0;
    quad[0].rgba[0]  = r;
    quad[0].rgba[1]  = g;
    quad[0].rgba[2]  = b;
    quad[0].rgba[3]  = a;
    quad[1].xy_uv[0] = x + w;
    quad[1].xy_uv[1] = y;
    quad[1].xy_uv[2] = u1;
    quad[1].xy_uv[3] = v0;
    quad[1].rgba[0]  = r;
    quad[1].rgba[1]  = g;
    quad[1].rgba[2]  = b;
    quad[1].rgba[3]  = a;
    quad[2].xy_uv[0] = x + w;
    quad[2].xy_uv[1] = y + h;
    quad[2].xy_uv[2] = u1;
    quad[2].xy_uv[3] = v1;
    quad[2].rgba[0]  = r;
    quad[2].rgba[1]  = g;
    quad[2].rgba[2]  = b;
    quad[2].rgba[3]  = a;
    quad[3].xy_uv[0] = x;
    quad[3].xy_uv[1] = y + h;
    quad[3].xy_uv[2] = u0;
    quad[3].xy_uv[3] = v1;
    quad[3].rgba[0]  = r;
    quad[3].rgba[1]  = g;
    quad[3].rgba[2]  = b;
    quad[3].rgba[3]  = a;
    PushQuadVerts(quad);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadTextured(float x, float y, float w, float h, const TextureImage * tex, const ColorRGBA32 color)
{
    MRQ2_ASSERT(tex != nullptr);
    const auto quad_start_vtx = m_buffers.CurrentPosition();
    PushQuad(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color);
    m_deferred_textured_quads.push_back({ tex, quad_start_vtx });
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadTexturedUVs(float x, float y, float w, float h, float u0, float v0, float u1, float v1, const TextureImage * tex, const ColorRGBA32 color)
{
    MRQ2_ASSERT(tex != nullptr);
    const auto quad_start_vtx = m_buffers.CurrentPosition();
    PushQuad(x, y, w, h, u0, v0, u1, v1, color);
    m_deferred_textured_quads.push_back({ tex, quad_start_vtx });
}

///////////////////////////////////////////////////////////////////////////////
// MiniImBatch
///////////////////////////////////////////////////////////////////////////////

bool MiniImBatch::sm_emulated_triangle_fans = true;

///////////////////////////////////////////////////////////////////////////////

void MiniImBatch::PushVertex(const DrawVertex3D & vert)
{
    MRQ2_ASSERT(IsValid()); // Clear()ed?

    if (sm_emulated_triangle_fans)
    {
        if (m_topology != PrimitiveTopology::kTriangleFan)
        {
            *Increment(1) = vert;
        }
        else // Emulated triangle fan
        {
            if (m_tri_fan_vert_count == 3)
            {
                DrawVertex3D * v = Increment(2);
                v[0] = m_tri_fan_first_vert;
                v[1] = m_tri_fan_last_vert;
            }
            else if (m_tri_fan_vert_count == 1)
            {
                *Increment(1) = m_tri_fan_first_vert;
                ++m_tri_fan_vert_count;
            }
            else
            {
                ++m_tri_fan_vert_count;
            }
            *Increment(1) = vert;
        }

        // Save for triangle fan emulation
        m_tri_fan_last_vert = vert;
    }
    else
    {
        *Increment(1) = vert;
    }
}

///////////////////////////////////////////////////////////////////////////////

void MiniImBatch::PushModelSurface(const ModelSurface & surf, const vec4_t * const opt_color_override)
{
    MRQ2_ASSERT(IsValid()); // Clear()ed?

    const ModelPoly & poly  = *surf.polys;
    const int num_triangles = (poly.num_verts - 2);
    const int num_verts     = (num_triangles  * 3);

    MRQ2_ASSERT(num_triangles > 0);
    MRQ2_ASSERT(num_verts <= NumVerts());

    DrawVertex3D * const first_vert = Increment(num_verts);
    DrawVertex3D * verts_iter = first_vert;

    for (int t = 0; t < num_triangles; ++t)
    {
        const ModelTriangle & mdl_tri = poly.triangles[t];

        for (int v = 0; v < 3; ++v)
        {
            const PolyVertex & poly_vert = poly.vertexes[mdl_tri.vertexes[v]];

            verts_iter->position[0] = poly_vert.position[0];
            verts_iter->position[1] = poly_vert.position[1];
            verts_iter->position[2] = poly_vert.position[2];

            verts_iter->uv[0] = poly_vert.texture_s;
            verts_iter->uv[1] = poly_vert.texture_t;

            if (opt_color_override != nullptr)
            {
                Vec4Copy(*opt_color_override, verts_iter->rgba);
            }
            else
            {
                ColorFloats(surf.debug_color,
                            verts_iter->rgba[0],
                            verts_iter->rgba[1],
                            verts_iter->rgba[2],
                            verts_iter->rgba[3]);
            }

            ++verts_iter;
        }
    }

    MRQ2_ASSERT(verts_iter == (first_vert + num_verts));
}

///////////////////////////////////////////////////////////////////////////////

MRQ2_RENDERLIB_NOINLINE void MiniImBatch::OverflowError() const
{
    GameInterface::Errorf("MiniImBatch overflowed! used_verts=%i, num_verts=%i. "
                          "Increase vertex batch size.", m_used_verts, m_num_verts);
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
