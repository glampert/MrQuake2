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
    m_textured_quads.clear();
    m_buffers.Shutdown();
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::EndFrame(GraphicsContext & context, const ConstantBuffer & cbuff, const PipelineState & pipeline_state, const TextureImage * opt_tex_atlas)
{
    const auto draw_buf = m_buffers.EndFrame();

    context.SetPipelineState(pipeline_state);
    context.SetVertexBuffer(*draw_buf.buffer_ptr);
    context.SetConstantBuffer(cbuff, 0);

    // Fast path - one texture for the whole batch:
    if (opt_tex_atlas != nullptr)
    {
        context.SetTexture(opt_tex_atlas->BackendTexture(), 0);

        const auto first_vertex = 0u;
        const auto vertex_count = draw_buf.used_verts;
        context.Draw(first_vertex, vertex_count);
    }
    else // Handle small unique textured draws:
    {
        for (const DeferredTexQuad & d : m_textured_quads)
        {
            context.SetTexture(d.tex->BackendTexture(), 0);

            const auto first_vertex = d.quad_start_vtx;
            const auto vertex_count = 6u; // one quad
            context.Draw(first_vertex, vertex_count);
        }
    }

    // Clear cache for next frame:
    m_textured_quads.clear();
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
    quad[0].position[0] = x;
    quad[0].position[1] = y;
    quad[0].texture_uv[0] = u0;
    quad[0].texture_uv[1] = v0;
    quad[0].rgba[0] = r;
    quad[0].rgba[1] = g;
    quad[0].rgba[2] = b;
    quad[0].rgba[3] = a;
    quad[1].position[0] = x + w;
    quad[1].position[1] = y;
    quad[1].texture_uv[0] = u1;
    quad[1].texture_uv[1] = v0;
    quad[1].rgba[0] = r;
    quad[1].rgba[1] = g;
    quad[1].rgba[2] = b;
    quad[1].rgba[3] = a;
    quad[2].position[0] = x + w;
    quad[2].position[1] = y + h;
    quad[2].texture_uv[0] = u1;
    quad[2].texture_uv[1] = v1;
    quad[2].rgba[0] = r;
    quad[2].rgba[1] = g;
    quad[2].rgba[2] = b;
    quad[2].rgba[3] = a;
    quad[3].position[0] = x;
    quad[3].position[1] = y + h;
    quad[3].texture_uv[0] = u0;
    quad[3].texture_uv[1] = v1;
    quad[3].rgba[0] = r;
    quad[3].rgba[1] = g;
    quad[3].rgba[2] = b;
    quad[3].rgba[3] = a;
    PushQuadVerts(quad);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadTextured(float x, float y, float w, float h, const TextureImage * tex, const ColorRGBA32 color)
{
    MRQ2_ASSERT(tex != nullptr);
    const auto quad_start_vtx = m_buffers.CurrentPosition();
    PushQuad(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color);
    m_textured_quads.push_back({ tex, quad_start_vtx });
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadTexturedUVs(float x, float y, float w, float h, float u0, float v0, float u1, float v1, const TextureImage * tex, const ColorRGBA32 color)
{
    MRQ2_ASSERT(tex != nullptr);
    const auto quad_start_vtx = m_buffers.CurrentPosition();
    PushQuad(x, y, w, h, u0, v0, u1, v1, color);
    m_textured_quads.push_back({ tex, quad_start_vtx });
}

///////////////////////////////////////////////////////////////////////////////
// SpriteBatches
///////////////////////////////////////////////////////////////////////////////

void SpriteBatches::Init(const RenderDevice & device)
{
    m_batches[SpriteBatch::kDrawChar].Init(device, 6 * 6000); // 6 verts per quad (expanded to 2 triangles each)
    m_batches[SpriteBatch::kDrawPics].Init(device, 6 * 128);

    // Shaders
    const VertexInputLayout vertex_input_layout = {
        // DrawVertex2D
        {
            { VertexInputLayout::kVertexPosition,  VertexInputLayout::kFormatFloat2, offsetof(DrawVertex2D, position)   },
            { VertexInputLayout::kVertexTexCoords, VertexInputLayout::kFormatFloat2, offsetof(DrawVertex2D, texture_uv) },
            { VertexInputLayout::kVertexColor,     VertexInputLayout::kFormatFloat4, offsetof(DrawVertex2D, rgba)       },
        }
    };
    if (!m_shader_draw_sprites.LoadFromFile(device, vertex_input_layout, "Draw2D"))
    {
        GameInterface::Errorf("Failed to load Draw2D shader!");
    }

    // PipelineState:
    // - Triangles
    // - Alpha blend ON
    // - Depth test OFF
    // - Depth writes ON
    // - Backface culling OFF
    m_pipeline_draw_sprites.Init(device);
    m_pipeline_draw_sprites.SetPrimitiveTopology(PrimitiveTopology::kTriangleList);
    m_pipeline_draw_sprites.SetShaderProgram(m_shader_draw_sprites);
    m_pipeline_draw_sprites.SetAlphaBlendingEnabled(true);
    m_pipeline_draw_sprites.SetDepthTestEnabled(false);
    m_pipeline_draw_sprites.SetDepthWritesEnabled(true);
    m_pipeline_draw_sprites.SetCullEnabled(false);
    m_pipeline_draw_sprites.Finalize();
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatches::Shutdown()
{
    for (auto & sb : m_batches)
    {
        sb.Shutdown();
    }

    m_pipeline_draw_sprites.Shutdown();
    m_shader_draw_sprites.Shutdown();
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatches::BeginFrame()
{
    m_batches[SpriteBatch::kDrawChar].BeginFrame();
    m_batches[SpriteBatch::kDrawPics].BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatches::EndFrame(GraphicsContext & context, const ConstantBuffer & cbuff, const TextureImage * glyphs_texture)
{
    // Miscellaneous UI sprites
    m_batches[SpriteBatch::kDrawPics].EndFrame(context, cbuff, m_pipeline_draw_sprites);

    // 2D text last so it overlays the console background
    m_batches[SpriteBatch::kDrawChar].EndFrame(context, cbuff, m_pipeline_draw_sprites, glyphs_texture);
}

///////////////////////////////////////////////////////////////////////////////
// MiniImBatch
///////////////////////////////////////////////////////////////////////////////

void MiniImBatch::PushVertex(const DrawVertex3D & vert)
{
    MRQ2_ASSERT(IsValid()); // Clear()ed?

    if (kEmulatedTriangleFans)
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

    const float world_ambient_term = Config::r_world_ambient.AsFloat(); // Modulate with the vertex color

    const ModelPoly & poly   = *surf.polys;
    const int num_triangles  = (poly.num_verts - 2);
    const uint32_t num_verts = (num_triangles * 3);

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

            verts_iter->texture_uv[0] = poly_vert.texture_s;
            verts_iter->texture_uv[1] = poly_vert.texture_t;

            verts_iter->lightmap_uv[0] = poly_vert.lightmap_s;
            verts_iter->lightmap_uv[1] = poly_vert.lightmap_t;

            if (opt_color_override != nullptr)
            {
                Vec4Copy(*opt_color_override, verts_iter->rgba);
            }
            else
            {
                ColorFloats(surf.color, verts_iter->rgba[0], verts_iter->rgba[1], verts_iter->rgba[2], verts_iter->rgba[3]);
            }

            // Scale by world "ambient light" term
            for (int n = 0; n < 4; ++n)
            {
                verts_iter->rgba[n] *= world_ambient_term;
            }

            ++verts_iter;
        }
    }

    MRQ2_ASSERT(verts_iter == (first_vert + num_verts));
}

///////////////////////////////////////////////////////////////////////////////

MRQ2_RENDERLIB_NOINLINE void MiniImBatch::OverflowError() const
{
    GameInterface::Errorf("MiniImBatch overflowed! used_verts=%u, num_verts=%u. "
                          "Increase vertex batch size.", m_used_verts, m_num_verts);
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
