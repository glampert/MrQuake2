//
// SkyBox.cpp
//  SkyBox rendering helper class.
//

#include "SkyBox.hpp"
#include "TextureStore.hpp"
#include "ModelStore.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// SkyBox
///////////////////////////////////////////////////////////////////////////////

constexpr float kSkyPtOnPlaneEpsilon = 0.1f; // Point on plane side epsilon
constexpr int   kSkyMaxClipVerts     = 128;  // For local buffers and such

static const vec3_t s_skyclip[6] = {
    {  1.0f,  1.0f, 0.0f },
    {  1.0f, -1.0f, 0.0f },
    {  0.0f, -1.0f, 1.0f },
    {  0.0f,  1.0f, 1.0f },
    {  1.0f,  0.0f, 1.0f },
    { -1.0f,  0.0f, 1.0f },
};

// 1 = s, 2 = t, 3 = 2048
static const int s_st_to_skyvec[6][3] = {
    {  3, -1,  2 },
    { -3,  1,  2 },
    {  1,  3,  2 },
    { -1, -3,  2 },
    { -2, -1,  3 }, // 0 degrees yaw, look straight up
    {  2, -1, -3 }, // look straight down
};

// s = [0]/[2], t = [1]/[2]
static const int s_skyvec_to_st[6][3] = {
    { -2,  3,  1 },
    {  2,  3, -1 },
    {  1,  3,  2 },
    { -1,  3, -2 },
    { -2, -1,  3 },
    { -2,  1, -3 },
};

///////////////////////////////////////////////////////////////////////////////

SkyBox::SkyBox(TextureStore & tex_store, const char * const name, const float rotate_degrees, const vec3_t axis)
    : m_sky_rotate{ rotate_degrees }
    , m_sky_force_full_draw{ GameInterface::Cvar::Get("r_sky_force_full_draw", "0", 0) }
{
    // Select between TGA or PCX - defaults to TGA (higher quality).
    static auto r_sky_use_pal_textures = GameInterface::Cvar::Get("r_sky_use_pal_textures", "0", CvarWrapper::kFlagArchive);

    strcpy_s(m_sky_name, name);
    Vec3Copy(axis, m_sky_axis);

    if (m_sky_rotate != 0.0f)
    {
        m_sky_min = 1.0f / 256.0f;
        m_sky_max = 255.0f / 256.0f;
    }
    else
    {
        m_sky_min = 1.0f / 512.0f;
        m_sky_max = 511.0f / 512.0f;
    }

    static const char * const suf_names[kNumSides] = { "rt", "bk", "lf", "ft", "up", "dn" };

    for (unsigned i = 0; i < m_sky_images.size(); ++i)
    {
        char pathname[1024];
        if (r_sky_use_pal_textures.IsSet())
        {
            std::snprintf(pathname, sizeof(pathname), "env/%s%s.pcx", m_sky_name, suf_names[i]);
        }
        else
        {
            std::snprintf(pathname, sizeof(pathname), "env/%s%s.tga", m_sky_name, suf_names[i]);
        }

        m_sky_images[i] = tex_store.FindOrLoad(pathname, TextureType::kSky);
        if (m_sky_images[i] == nullptr)
        {
            m_sky_images[i] = tex_store.tex_white2x2;
            GameInterface::Printf("Failed to find or load skybox side %i: '%s'", i, pathname);
        }
    }

    Clear();
}

///////////////////////////////////////////////////////////////////////////////

void SkyBox::AddSkyPolygon(const int verts_count, const vec3_t vecs)
{
    int i, axis;
    const float * vp;
    vec3_t v, av;
    const vec3_t origin = { 0.0f, 0.0f, 0.0f };

    // decide which face it maps to
    Vec3Copy(origin, v);
    for (i = 0, vp = vecs; i < verts_count; i++, vp += 3)
    {
        Vec3Add(vp, v, v);
    }
    av[0] = std::fabs(v[0]);
    av[1] = std::fabs(v[1]);
    av[2] = std::fabs(v[2]);

    if (av[0] > av[1] && av[0] > av[2])
    {
        if (v[0] < 0.0f)
            axis = 1;
        else
            axis = 0;
    }
    else if (av[1] > av[2] && av[1] > av[0])
    {
        if (v[1] < 0.0f)
            axis = 3;
        else
            axis = 2;
    }
    else
    {
        if (v[2] < 0.0f)
            axis = 5;
        else
            axis = 4;
    }

    // project new texture coords
    for (i = 0; i < verts_count; i++, vecs += 3)
    {
        float dv, s, t;

        int j = s_skyvec_to_st[axis][2];
        if (j > 0)
            dv = vecs[j - 1];
        else
            dv = -vecs[-j - 1];

        if (dv < 0.001f)
            continue; // don't divide by zero

        j = s_skyvec_to_st[axis][0];
        if (j < 0)
            s = -vecs[-j - 1] / dv;
        else
            s = vecs[j - 1] / dv;

        j = s_skyvec_to_st[axis][1];
        if (j < 0)
            t = -vecs[-j - 1] / dv;
        else
            t = vecs[j - 1] / dv;

        if (s < m_skybounds_mins[0][axis])
        {
            m_skybounds_mins[0][axis] = s;
        }
        if (t < m_skybounds_mins[1][axis])
        {
            m_skybounds_mins[1][axis] = t;
        }

        if (s > m_skybounds_maxs[0][axis])
        {
            m_skybounds_maxs[0][axis] = s;
        }
        if (t > m_skybounds_maxs[1][axis])
        {
            m_skybounds_maxs[1][axis] = t;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

void SkyBox::ClipSkyPolygon(const int verts_count, vec3_t vecs, const int stage)
{
    if (verts_count > (kSkyMaxClipVerts - 2))
    {
        GameInterface::Errorf("SkyBox::ClipSkyPolygon -> SkyMaxClipVerts exceeded");
    }

    if (stage == 6)
    {
        // fully clipped, so draw it
        AddSkyPolygon(verts_count, vecs);
        return;
    }

    int i;
    const float * v;

    float dists[kSkyMaxClipVerts];
    PlaneSides sides[kSkyMaxClipVerts];

    int new_counts[2];
    vec3_t new_vecs[2][kSkyMaxClipVerts];

    bool front = false;
    bool back  = false;
    const auto norm = s_skyclip[stage];

    for (i = 0, v = vecs; i < verts_count; i++, v += 3)
    {
        const float d = Vec3Dot(v, norm);
        if (d > kSkyPtOnPlaneEpsilon)
        {
            front = true;
            sides[i] = kPlaneSide_Front;
        }
        else if (d < -kSkyPtOnPlaneEpsilon)
        {
            back = true;
            sides[i] = kPlaneSide_Back;
        }
        else
        {
            sides[i] = kPlaneSide_On;
        }
        dists[i] = d;
    }

    if (!front || !back)
    {
        // not clipped
        ClipSkyPolygon(verts_count, vecs, stage + 1);
        return;
    }

    // clip it
    sides[i] = sides[0];
    dists[i] = dists[0];
    Vec3Copy(vecs, vecs + (i * 3));
    new_counts[0] = new_counts[1] = 0;

    for (i = 0, v = vecs; i < verts_count; i++, v += 3)
    {
        switch (sides[i])
        {
        case kPlaneSide_Front:
            Vec3Copy(v, new_vecs[0][new_counts[0]]);
            new_counts[0]++;
            break;
        case kPlaneSide_Back:
            Vec3Copy(v, new_vecs[1][new_counts[1]]);
            new_counts[1]++;
            break;
        case kPlaneSide_On:
            Vec3Copy(v, new_vecs[0][new_counts[0]]);
            new_counts[0]++;
            Vec3Copy(v, new_vecs[1][new_counts[1]]);
            new_counts[1]++;
            break;
        } // switch

        if (sides[i] == kPlaneSide_On || sides[i + 1] == kPlaneSide_On || sides[i + 1] == sides[i])
        {
            continue;
        }

        const float d = dists[i] / (dists[i] - dists[i + 1]);
        for (int j = 0; j < 3; ++j)
        {
            const float e = v[j] + d * (v[j + 3] - v[j]);
            new_vecs[0][new_counts[0]][j] = e;
            new_vecs[1][new_counts[1]][j] = e;
        }
        new_counts[0]++;
        new_counts[1]++;
    }

    // continue
    ClipSkyPolygon(new_counts[0], new_vecs[0][0], stage + 1);
    ClipSkyPolygon(new_counts[1], new_vecs[1][0], stage + 1);
}

///////////////////////////////////////////////////////////////////////////////

void SkyBox::AddSkySurface(const ModelSurface & surf, const vec3_t view_origin)
{
    for (const ModelPoly * p = surf.polys; p != nullptr; p = p->next)
    {
        const int num_triangles = (p->num_verts  - 2);
        const int num_verts     = (num_triangles * 3);

        if (num_verts > kSkyMaxClipVerts)
        {
            GameInterface::Errorf("SkyMaxClipVerts (%i) exceeded! Needed %i", kSkyMaxClipVerts, num_verts);
        }

        vec3_t verts[kSkyMaxClipVerts];
        int verts_count = 0;

        for (int t = 0; t < num_triangles; ++t)
        {
            const ModelTriangle & mdl_tri = p->triangles[t];
            for (int v = 0; v < 3; ++v)
            {
                const PolyVertex & poly_vert = p->vertexes[mdl_tri.vertexes[v]];
                Vec3Sub(poly_vert.position, view_origin, verts[verts_count++]);
            }
        }

        ClipSkyPolygon(verts_count, verts[0], 0);
    }
}

///////////////////////////////////////////////////////////////////////////////

void SkyBox::Clear()
{
    for (int i = 0; i < kNumSides; ++i)
    {
        m_skybounds_mins[0][i] = m_skybounds_mins[1][i] = +9999.0f;
        m_skybounds_maxs[0][i] = m_skybounds_maxs[1][i] = -9999.0f;
    }
}

///////////////////////////////////////////////////////////////////////////////

bool SkyBox::IsAnyPlaneVisible() const
{
    if (m_sky_rotate != 0.0f && !m_sky_force_full_draw.IsSet())
    {
        // check for no sky at all
        int i;
        for (i = 0; i < kNumSides; ++i)
        {
            if (m_skybounds_mins[0][i] < m_skybounds_maxs[0][i] &&
                m_skybounds_mins[1][i] < m_skybounds_maxs[1][i])
            {
                break;
            }
        }

        if (i == kNumSides)
        {
            return false; // nothing visible
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool SkyBox::BuildSkyPlane(const int plane_index, DrawVertex3D out_plane_verts[6], const TextureImage ** out_plane_tex)
{
    const int i = plane_index;
    const int sky_tri_indexes[6] = { 0,1,2, 2,3,0 }; // CW winding
    const int sky_tex_order[kNumSides] = { 0, 2, 1, 3, 4, 5 };

    if (m_sky_rotate != 0.0f || m_sky_force_full_draw.IsSet())
    {
        // Force full sky to draw when rotating
        m_skybounds_mins[0][i] = -1.0f;
        m_skybounds_mins[1][i] = -1.0f;
        m_skybounds_maxs[0][i] =  1.0f;
        m_skybounds_maxs[1][i] =  1.0f;
    }

    if (m_skybounds_mins[0][i] >= m_skybounds_maxs[0][i] ||
        m_skybounds_mins[1][i] >= m_skybounds_maxs[1][i])
    {
        return false;
    }

    DrawVertex3D plane_quad[4];
    MakeSkyVec(m_skybounds_mins[0][i], m_skybounds_mins[1][i], i, plane_quad[0]);
    MakeSkyVec(m_skybounds_mins[0][i], m_skybounds_maxs[1][i], i, plane_quad[1]);
    MakeSkyVec(m_skybounds_maxs[0][i], m_skybounds_maxs[1][i], i, plane_quad[2]);
    MakeSkyVec(m_skybounds_maxs[0][i], m_skybounds_mins[1][i], i, plane_quad[3]);

    // Expand into a triangle
    for (int v = 0; v < 6; ++v)
    {
        out_plane_verts[v] = plane_quad[sky_tri_indexes[v]];
    }
    *out_plane_tex = m_sky_images[sky_tex_order[i]];

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void SkyBox::MakeSkyVec(float s, float t, const int axis, DrawVertex3D & out_vert) const
{
    vec3_t v, b;

    b[0] = s * 2300.0f;
    b[1] = t * 2300.0f;
    b[2] = 2300.0f;

    for (int j = 0; j < 3; ++j)
    {
        const int k = s_st_to_skyvec[axis][j];
        if (k < 0)
        {
            v[j] = -b[-k - 1];
        }
        else
        {
            v[j] = b[k - 1];
        }
    }

    // "Avoid bilerp seam" (copied from original GL code)
    s = (s + 1) * 0.5f;
    t = (t + 1) * 0.5f;

    if (s < m_sky_min)
    {
        s = m_sky_min;
    }
    else if (s > m_sky_max)
    {
        s = m_sky_max;
    }

    if (t < m_sky_min)
    {
        t = m_sky_min;
    }
    else if (t > m_sky_max)
    {
        t = m_sky_max;
    }

    out_vert.position[0] = v[0];
    out_vert.position[1] = v[1];
    out_vert.position[2] = v[2];

    out_vert.uv[0] = s;
    out_vert.uv[1] = 1.0f - t;

    out_vert.rgba[0] = 1.0f;
    out_vert.rgba[1] = 1.0f;
    out_vert.rgba[2] = 1.0f;
    out_vert.rgba[3] = 1.0f;
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
