//
// MiniImBatch.cpp
//  Simple OpenGL-style immediate mode emulation.
//

#include "MiniImBatch.hpp"
#include "TextureStore.hpp"
#include "ModelStore.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// MiniImBatch
///////////////////////////////////////////////////////////////////////////////

bool MiniImBatch::sm_emulated_triangle_fans = true;

///////////////////////////////////////////////////////////////////////////////

void MiniImBatch::PushVertex(const DrawVertex3D & vert)
{
    FASTASSERT(IsValid()); // Clear()ed?

    if (sm_emulated_triangle_fans)
    {
        if (m_topology != PrimitiveTopology::TriangleFan)
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
    FASTASSERT(IsValid()); // Clear()ed?

    const ModelPoly & poly  = *surf.polys;
    const int num_triangles = (poly.num_verts - 2);
    const int num_verts     = (num_triangles  * 3);

    FASTASSERT(num_triangles > 0);
    FASTASSERT(num_verts <= NumVerts());

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

    FASTASSERT(verts_iter == (first_vert + num_verts));
}

///////////////////////////////////////////////////////////////////////////////

REFLIB_NOINLINE void MiniImBatch::OverflowError() const
{
    GameInterface::Errorf("MiniImBatch overflowed! used_verts=%i, num_verts=%i. "
                          "Increase vertex batch size.", m_used_verts, m_num_verts);
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
