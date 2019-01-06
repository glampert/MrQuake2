//
// MiniImBatch.hpp
//  Simple OpenGL-style immediate mode emulation.
//
#pragma once

#include "RefShared.hpp"

namespace MrQ2
{

/*
===============================================================================

    DrawVertex3D / DrawVertex2D / PrimitiveTopology

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

enum class PrimitiveTopology : std::uint8_t
{
    TriangleList,
    TriangleStrip,
    TriangleFan,
};

struct ModelSurface;

/*
===============================================================================

    MiniImBatch

===============================================================================
*/
class MiniImBatch final
{
public:

    MiniImBatch(DrawVertex3D * const verts_ptr, const int num_verts, const PrimitiveTopology topology)
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

    DrawVertex3D * Increment(const int num_verts)
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

    int NumVerts()  const { return m_num_verts; }
    int UsedVerts() const { return m_used_verts; }
    bool IsValid()  const { return m_verts_ptr != nullptr; }
    PrimitiveTopology Topology() const { return m_topology; }

private:

    REFLIB_NORETURN void OverflowError() const;

    DrawVertex3D *    m_verts_ptr;
    int               m_num_verts;
    int               m_used_verts;
    PrimitiveTopology m_topology;

    // Triangle fan emulation support:
    std::uint8_t      m_tri_fan_vert_count;
    DrawVertex3D      m_tri_fan_first_vert;
    DrawVertex3D      m_tri_fan_last_vert;

    // If set to true, deconstruct PrimitiveTopology::TriangleFan primitives in the
    // MiniImBatch into PrimitiveTopology::TriangleList primitives, to support back-end
    // APIs that are not capable of drawing triangle fans natively.
    static bool       sm_emulated_triangle_fans;
};

} // MrQ2
