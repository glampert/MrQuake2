//
// ModelLoad.cpp
//  Loaders for the Quake 2 model file formats.
//

#include "ModelStore.hpp"
#include "TextureStore.hpp"
#include "Lightmaps.hpp"
#include "ImmediateModeBatching.hpp"

// Quake includes
#include "common/q_common.h"
#include "common/q_files.h"

namespace MrQ2
{

// d*_t structures are on-disk representation
// m*_t structures are in-memory representation
// c*_t are structures reused from the collision code

///////////////////////////////////////////////////////////////////////////////
// Local helpers:
///////////////////////////////////////////////////////////////////////////////

// Verbose debugging
constexpr bool kVerboseModelLoading = true;

// Round integer to its next power of two (using the classic bit twiddling technique).
static unsigned RoundNextPoT(unsigned v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

// Helper to offset into the model file data based on lump offset.
template<typename T>
static const T * GetDataPtr(const void * const mdl_data, const lump_t & l)
{
    auto * ptr = (static_cast<const std::uint8_t *>(mdl_data) + l.fileofs);
    return reinterpret_cast<const T *>(ptr);
}

constexpr unsigned Megabytes(unsigned n) { return (n * 1024 * 1024); }

///////////////////////////////////////////////////////////////////////////////
// BRUSH MODELS (WORLD MAP):
///////////////////////////////////////////////////////////////////////////////

namespace BMod
{

static void LoadVertexes(ModelInstance & mdl, const void * const mdl_data, const lump_t & l)
{
    const auto * in = GetDataPtr<dvertex_t>(mdl_data, l);
    if (l.filelen % sizeof(*in))
    {
        GameInterface::Errorf("BMod::LoadVertexes: Funny lump size in '%s'", mdl.name.CStr());
    }

    const int count = l.filelen / sizeof(*in);
    auto * out = mdl.hunk.AllocBlockOfType<ModelVertex>(count);

    mdl.data.vertexes     = out;
    mdl.data.num_vertexes = count;

    for (int i = 0; i < count; ++i, ++in, ++out)
    {
        out->position[0] = in->point[0];
        out->position[1] = in->point[1];
        out->position[2] = in->point[2];
    }
}

///////////////////////////////////////////////////////////////////////////////

static void LoadEdges(ModelInstance & mdl, const void * const mdl_data, const lump_t & l)
{
    const auto * in = GetDataPtr<dedge_t>(mdl_data, l);
    if (l.filelen % sizeof(*in))
    {
        GameInterface::Errorf("BMod::LoadEdges: Funny lump size in '%s'", mdl.name.CStr());
    }

    const int count = l.filelen / sizeof(*in);
    auto * out = mdl.hunk.AllocBlockOfType<ModelEdge>(count + 1);

    mdl.data.edges     = out;
    mdl.data.num_edges = count;

    for (int i = 0; i < count; ++i, ++in, ++out)
    {
        out->v[0] = in->v[0];
        out->v[1] = in->v[1];
    }
}

///////////////////////////////////////////////////////////////////////////////

static void LoadSurfEdges(ModelInstance & mdl, const void * const mdl_data, const lump_t & l)
{
    const int * in = GetDataPtr<int>(mdl_data, l);
    if (l.filelen % sizeof(*in))
    {
        GameInterface::Errorf("BMod::LoadSurfEdges: Funny lump size in '%s'", mdl.name.CStr());
    }

    const int count = l.filelen / sizeof(*in);
    if (count < 1 || count >= MAX_MAP_SURFEDGES)
    {
        GameInterface::Errorf("BMod::LoadSurfEdges: Bad surf edges count in '%s': %i", mdl.name.CStr(), count);
    }

    int * out = mdl.hunk.AllocBlockOfType<int>(count);

    mdl.data.surf_edges     = out;
    mdl.data.num_surf_edges = count;

    for (int i = 0; i < count; ++i)
    {
        out[i] = in[i];
    }
}

///////////////////////////////////////////////////////////////////////////////

static void LoadLighting(ModelInstance & mdl, const void * const mdl_data, const lump_t & l)
{
    if (l.filelen <= 0)
    {
        if (kVerboseModelLoading)
        {
            GameInterface::Printf("No light data for brush model '%s'", mdl.name.CStr());
        }

        mdl.data.light_data = nullptr;
        return;
    }

    mdl.data.light_data = mdl.hunk.AllocBlockOfType<std::uint8_t>(l.filelen);
    std::memcpy(mdl.data.light_data, GetDataPtr<std::uint8_t>(mdl_data, l), l.filelen);
}

///////////////////////////////////////////////////////////////////////////////

static void LoadPlanes(ModelInstance & mdl, const void * const mdl_data, const lump_t & l)
{
    const auto * in = GetDataPtr<dplane_t>(mdl_data, l);
    if (l.filelen % sizeof(*in))
    {
        GameInterface::Errorf("BMod::LoadPlanes: Funny lump size in '%s'", mdl.name.CStr());
    }

    const int count = l.filelen / sizeof(*in);
    auto * out  = mdl.hunk.AllocBlockOfType<cplane_t>(count * 2);

    mdl.data.planes     = out;
    mdl.data.num_planes = count;

    for (int i = 0; i < count; ++i, ++in, ++out)
    {
        int bits = 0;
        for (int j = 0; j < 3; ++j)
        {
            out->normal[j] = in->normal[j];
            if (out->normal[j] < 0.0f)
            {
                bits |= (1 << j); // Negative vertex normals will set a bit
            }
        }
        out->dist = in->dist;
        out->type = in->type;
        out->signbits = bits;
    }
}

///////////////////////////////////////////////////////////////////////////////

static void LoadTexInfo(TextureStore & tex_store, ModelInstance & mdl, const void * const mdl_data, const lump_t & l)
{
    const auto * in = GetDataPtr<textureinfo_t>(mdl_data, l);
    if (l.filelen % sizeof(*in))
    {
        GameInterface::Errorf("BMod::LoadTexInfo: Funny lump size in '%s'", mdl.name.CStr());
    }

    const int count = l.filelen / sizeof(*in);
    auto * out = mdl.hunk.AllocBlockOfType<ModelTexInfo>(count);

    mdl.data.texinfos     = out;
    mdl.data.num_texinfos = count;

    for (int i = 0; i < count; ++i, ++in, ++out)
    {
        for (int j = 0; j < 8; ++j)
        {
            out->vecs[0][j] = in->vecs[0][j];
        }

        out->flags = in->flags;
        const int next = in->nexttexinfo;

        if (next > 0)
        {
            out->next = mdl.data.texinfos + next;
        }
        else
        {
            out->next = nullptr;
        }

        char name[1024];
        std::snprintf(name, sizeof(name), "textures/%s.wal", in->texture);
        out->teximage = tex_store.FindOrLoad(name, TextureType::kWall);

        // Warn and set a dummy texture if we failed to load:
        if (out->teximage == nullptr)
        {
            out->teximage = tex_store.tex_white2x2;
            GameInterface::Printf("WARNING: Failed to load wall texture '%s'", name);
        }
    }

    // Count animation frames:
    for (int i = 0; i < count; ++i)
    {
        out = &mdl.data.texinfos[i];
        out->num_frames = 1;

        for (const auto * step = out->next; step && step != out; step = step->next)
        {
            out->num_frames++;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

static void CalcSurfaceExtents(const ModelInstance & mdl, ModelSurface & surf)
{
    float mins[2];
    float maxs[2];
    mins[0] = mins[1] = 999999;
    maxs[0] = maxs[1] = -99999;

    const ModelTexInfo * const tex = surf.texinfo;
    MRQ2_ASSERT(tex != nullptr);

    for (int i = 0; i < surf.num_edges; ++i)
    {
        const ModelVertex * v;
        const int e = mdl.data.surf_edges[surf.first_edge + i];
        if (e >= 0)
        {
            v = &mdl.data.vertexes[mdl.data.edges[e].v[0]];
        }
        else
        {
            v = &mdl.data.vertexes[mdl.data.edges[-e].v[1]];
        }

        for (int j = 0; j < 2; ++j)
        {
            const float val = (v->position[0] * tex->vecs[j][0] +
                               v->position[1] * tex->vecs[j][1] +
                               v->position[2] * tex->vecs[j][2] +
                               tex->vecs[j][3]);

            if (val < mins[j]) { mins[j] = val; }
            if (val > maxs[j]) { maxs[j] = val; }
        }
    }

    int bmins[2];
    int bmaxs[2];
    for (int i = 0; i < 2; ++i)
    {
        bmins[i] = static_cast<int>(std::floor(mins[i] / 16.0f));
        bmaxs[i] = static_cast<int>(std::ceil(maxs[i] / 16.0f));

        surf.texture_mins[i] = static_cast<std::int16_t>(bmins[i] * 16);
        surf.extents[i] = static_cast<std::int16_t>((bmaxs[i] - bmins[i]) * 16);
    }
}

///////////////////////////////////////////////////////////////////////////////

// Computing the normal of an arbitrary polygon is as simple as taking the
// cross product or each pair of vertexes, from the first to the last and
// wrapping around back to the first one if needed. A more detailed mathematical
// explanation of why this works can be found at: http://www.iquilezles.org/www/articles/areas/areas.htm
static void ComputePolygonNormal(const ModelPoly & poly, vec3_t normal)
{
    vec3_t cross, p0, p1;
    Vec3Zero(normal);
    for (int v = 0; v < poly.num_verts; ++v)
    {
        const int v_next = (v + 1) % poly.num_verts;

        Vec3Copy(poly.vertexes[v].position, p0);
        Vec3Copy(poly.vertexes[v_next].position, p1);

        Vec3Cross(p0, p1, cross);
        Vec3Add(normal, cross, normal);
    }
    Vec3Normalize(normal);
}

///////////////////////////////////////////////////////////////////////////////

static int NextActive(int x, const int num_verts, const bool * const active)
{
    for (;;)
    {
        if (++x == num_verts)
        {
            x = 0;
        }
        if (active[x])
        {
            return x;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

static int PrevActive(int x, const int num_verts, const bool * const active)
{
    for (;;)
    {
        if (--x == -1)
        {
            x = num_verts - 1;
        }
        if (active[x])
        {
            return x;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

static bool TestTriangle(const int pi1, const int pi2, const int pi3,
                         const vec3_t p1, const vec3_t p2, const vec3_t p3, const vec3_t normal,
                         const bool * const active, const ModelPoly & poly, const float epsilon)
{
    vec3_t n1, n2, n3, pv;
    vec3_t temp0, temp1, temp2;
    bool result = false;

    Vec3Sub(p2, p1, temp0);
    Vec3Sub(p3, p1, temp1);

    Vec3Normalize(temp0);
    Vec3Cross(normal, temp0, n1);

    if (Vec3Dot(n1, temp1) > epsilon)
    {
        Vec3Sub(p3, p2, temp0);
        Vec3Sub(p1, p3, temp1);

        Vec3Normalize(temp0);
        Vec3Normalize(temp1);

        Vec3Cross(normal, temp0, n2);
        Vec3Cross(normal, temp1, n3);

        result = true;
        for (int v = 0; v < poly.num_verts; ++v)
        {
            // Look for other vertexes inside the triangle:
            if (active[v] && v != pi1 && v != pi2 && v != pi3)
            {
                Vec3Copy(poly.vertexes[v].position, pv);

                Vec3Sub(pv, p1, temp0);
                Vec3Sub(pv, p2, temp1);
                Vec3Sub(pv, p3, temp2);

                Vec3Normalize(temp0);
                Vec3Normalize(temp1);
                Vec3Normalize(temp2);

                if (Vec3Dot(n1, temp0) > -epsilon &&
                    Vec3Dot(n2, temp1) > -epsilon &&
                    Vec3Dot(n3, temp2) > -epsilon)
                {
                    result = false;
                    break;
                }
            }
        }
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////

constexpr float kTriangulationEpsilon  = 0.001f;
constexpr int   kTriangulationMaxVerts = 128; // Per polygon

///////////////////////////////////////////////////////////////////////////////

// Algorithm used below is an "Ear clipping"-based triangulation algorithm, adapted
// from sample code presented in the "Mathematics for 3D Game Programming and Computer Graphics"
// book by Eric Lengyel (available at http://www.mathfor3dgameprogramming.com/code/Listing9.2.cpp).
static void TriangulatePolygon(ModelPoly & poly)
{
    // Already a triangle or an broken polygon.
    if (poly.num_verts <= 3)
    {
        if (poly.num_verts == 3)
        {
            if (poly.triangles == nullptr)
            {
                GameInterface::Errorf("Null triangle list in polygon!");
            }
            poly.triangles->vertexes[0] = 0;
            poly.triangles->vertexes[1] = 1;
            poly.triangles->vertexes[2] = 2;
        }
        else
        {
            // Broken polygons will be ignored by the rendering code.
            GameInterface::Printf("WARNING: Broken polygon found in brush model!");
        }
        return;
    }

    const int num_verts = poly.num_verts;
    const int num_triangles = num_verts - 2;

    // Just make it bigger if you ever hit this. We only require 1 byte per entry.
    if (num_verts > kTriangulationMaxVerts)
    {
        GameInterface::Errorf("kTriangulationMaxVerts (%i) exceeded!", kTriangulationMaxVerts);
    }

    // We need a normal to properly judge the winding of the triangles.
    vec3_t normal;
    ComputePolygonNormal(poly, normal);

    int start = 0;
    int p1 = 0;
    int p2 = 1;
    int m1 = num_verts - 1;
    int m2 = num_verts - 2;
    bool last_positive = false;

    int triangles_done = 0;
    ModelTriangle * tris_ptr = poly.triangles;

    vec3_t temp0, temp1;
    vec3_t vp1, vp2, vm1, vm2;

    // BSP polygons are generally small, under 20 verts or so.
    // We can get away with a local stack buffer and avoid a malloc.
    bool active[kTriangulationMaxVerts];
    for (int i = 0; i < num_verts; ++i)
    {
        active[i] = true;
    }

    auto EmitTriangle = [&triangles_done, &num_triangles, &tris_ptr](int v0, int v1, int v2)
    {
        if (triangles_done == num_triangles)
        {
            GameInterface::Errorf("BMod::TriangulatePolygon: Triangle list overflowed!");
        }

        tris_ptr->vertexes[0] = v0;
        tris_ptr->vertexes[1] = v1;
        tris_ptr->vertexes[2] = v2;

        ++tris_ptr;
        ++triangles_done;
    };

    // Triangulation loop:
    for (;;)
    {
        if (p2 == m2)
        {
            // Only three vertexes remain. We're done.
            EmitTriangle(m1, p1, p2);
            break;
        }

        Vec3Copy(poly.vertexes[p1].position, vp1);
        Vec3Copy(poly.vertexes[p2].position, vp2);
        Vec3Copy(poly.vertexes[m1].position, vm1);
        Vec3Copy(poly.vertexes[m2].position, vm2);

        // Determine whether vp1, vp2, and vm1 form a valid triangle:
        bool positive = TestTriangle(p1, p2, m1, vp2, vm1, vp1, normal, active, poly, kTriangulationEpsilon);

        // Determine whether vm1, vm2, and vp1 form a valid triangle:
        bool negative = TestTriangle(m1, m2, p1, vp1, vm2, vm1, normal, active, poly, kTriangulationEpsilon);

        // If both triangles are valid, choose the one having the larger smallest angle.
        if (positive && negative)
        {
            Vec3Sub(vp2, vm1, temp0);
            Vec3Sub(vm2, vm1, temp1);
            Vec3Normalize(temp0);
            Vec3Normalize(temp1);
            const float pDot = Vec3Dot(temp0, temp1);

            Vec3Sub(vm2, vp1, temp0);
            Vec3Sub(vp2, vp1, temp1);
            Vec3Normalize(temp0);
            Vec3Normalize(temp1);
            const float mDot = Vec3Dot(temp0, temp1);

            if (std::fabs(pDot - mDot) < kTriangulationEpsilon)
            {
                if (last_positive) { positive = false; }
                else               { negative = false; }
            }
            else
            {
                if (pDot < mDot)   { negative = false; }
                else               { positive = false; }
            }
        }

        if (positive)
        {
            // Output the triangle m1, p1, p2:
            active[p1] = false;
            EmitTriangle(m1, p1, p2);
            p1 = NextActive(p1, num_verts, active);
            p2 = NextActive(p2, num_verts, active);
            last_positive = true;
            start = -1;
        }
        else if (negative)
        {
            // Output the triangle m2, m1, p1:
            active[m1] = false;
            EmitTriangle(m2, m1, p1);
            m1 = PrevActive(m1, num_verts, active);
            m2 = PrevActive(m2, num_verts, active);
            last_positive = false;
            start = -1;
        }
        else // Not a valid triangle yet.
        {
            if (start == -1)
            {
                start = p2;
            }
            else if (p2 == start)
            {
                // Exit if we've gone all the way around the
                // polygon without finding a valid triangle.
                break;
            }

            // Advance working set of vertexes:
            m2 = m1;
            m1 = p1;
            p1 = p2;
            p2 = NextActive(p2, num_verts, active);
        }
    }

    // TODO|FIXME? I don't think this is a hard error...
    // We should be outputting at most num_verts - 2 triangles
    // but the algorithm might still fail to produce that many tris.
    // better to keep a num_triangles member in the polygon struct instead!
    if (triangles_done != num_triangles)
    {
        GameInterface::Printf("WARNING - BMod::TriangulatePolygon: Unexpected triangle count!");
    }
}

///////////////////////////////////////////////////////////////////////////////

static void BuildPolygonFromSurface(ModelInstance & mdl, ModelSurface & surf)
{
    MRQ2_ASSERT(mdl.data.vertexes != nullptr);
    MRQ2_ASSERT(mdl.data.edges != nullptr && mdl.data.surf_edges != nullptr);

    const ModelVertex * const verts = mdl.data.vertexes;
    const ModelEdge * const edges = mdl.data.edges;
    const int * const surf_edges = mdl.data.surf_edges;

    const int num_verts = surf.num_edges;
    const int num_triangles = num_verts - 2;

    auto * poly = mdl.hunk.AllocBlockOfType<ModelPoly>(1);
    poly->next  = surf.polys;
    surf.polys  = poly;

    poly->num_verts = num_verts;
    poly->vertexes  = mdl.hunk.AllocBlockOfType<PolyVertex>(num_verts);
    poly->triangles = mdl.hunk.AllocBlockOfType<ModelTriangle>(num_triangles);

    vec3_t total = {};

    // Reconstruct the polygon from edges:
    for (int i = 0; i < num_verts; ++i)
    {
        float s, t;
        const float * vec;
        const ModelEdge * other_edge;

        const int index = surf_edges[surf.first_edge + i];
        if (index > 0)
        {
            other_edge = &edges[index];
            vec = verts[other_edge->v[0]].position;
        }
        else
        {
            other_edge = &edges[-index];
            vec = verts[other_edge->v[1]].position;
        }

        s = Vec3Dot(vec, surf.texinfo->vecs[0]) + surf.texinfo->vecs[0][3];
        s /= surf.texinfo->teximage->Width();

        t = Vec3Dot(vec, surf.texinfo->vecs[1]) + surf.texinfo->vecs[1][3];
        t /= surf.texinfo->teximage->Height();

        // Vertex position:
        Vec3Add(total, vec, total);
        Vec3Copy(vec, poly->vertexes[i].position);

        // Color texture tex coordinates:
        poly->vertexes[i].texture_s = s;
        poly->vertexes[i].texture_t = t;

        // Lightmap texture coordinates:
        s = Vec3Dot(vec, surf.texinfo->vecs[0]) + surf.texinfo->vecs[0][3];
        s -= surf.texture_mins[0];
        s += surf.light_s * 16;
        s += 8;
        s /= kLightmapTextureWidth * 16;

        t = Vec3Dot(vec, surf.texinfo->vecs[1]) + surf.texinfo->vecs[1][3];
        t -= surf.texture_mins[1];
        t += surf.light_t * 16;
        t += 8;
        t /= kLightmapTextureHeight * 16;

        poly->vertexes[i].lightmap_s = s;
        poly->vertexes[i].lightmap_t = t;
    }

    // We need triangles to draw with modern renderers.
    TriangulatePolygon(*poly);
}

///////////////////////////////////////////////////////////////////////////////

static void BoundPoly(const int num_verts, const float * const verts, vec3_t mins, vec3_t maxs)
{
    mins[0] = mins[1] = mins[2] = +9999;
    maxs[0] = maxs[1] = maxs[2] = -9999;

    const float * v = verts;
    for (int i = 0; i < num_verts; ++i)
    {
        for (int j = 0; j < 3; ++j, ++v)
        {
            if (*v < mins[j]) mins[j] = *v;
            if (*v > maxs[j]) maxs[j] = *v;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

static void SubdividePolygon(ModelInstance & mdl, ModelSurface & surf, const int num_verts, float * verts)
{
    if (num_verts > (kSubdivideSize - 4))
    {
        GameInterface::Errorf("BMod::SubdividePolygon -> Too many verts (%i)", num_verts);
    }

    vec3_t mins, maxs;
    BoundPoly(num_verts, verts, mins, maxs);

    float  dist[kSubdivideSize];
    vec3_t front[kSubdivideSize];
    vec3_t back[kSubdivideSize];

    for (int i = 0; i < 3; ++i)
    {
        float m = (mins[i] + maxs[i]) * 0.5f;
        m = kSubdivideSize * std::floor(m / kSubdivideSize + 0.5f);

        if (maxs[i] - m < 8.0f) continue;
        if (m - mins[i] < 8.0f) continue;

        // cut it
        int j;
        float * v = verts + i;
        for (j = 0; j < num_verts; ++j, v += 3)
        {
            dist[j] = *v - m;
        }

        // wrap cases
        dist[j] = dist[0];
        v -= i;
        Vec3Copy(verts, v);

        int f = 0, b = 0;
        v = verts;
        for (j = 0; j < num_verts; ++j, v += 3)
        {
            if (dist[j] >= 0.0f)
            {
                Vec3Copy(v, front[f]);
                ++f;
            }
            if (dist[j] <= 0.0f)
            {
                Vec3Copy(v, back[b]);
                ++b;
            }
            if (dist[j] == 0.0f || dist[j + 1] == 0.0f)
            {
                continue;
            }
            if ((dist[j] > 0.0f) != (dist[j + 1] > 0.0f))
            {
                // clip point
                const float frac = dist[j] / (dist[j] - dist[j + 1]);
                for (int k = 0; k < 3; ++k)
                {
                    front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
                }
                ++f;
                ++b;
            }
        }

        SubdividePolygon(mdl, surf, f, front[0]);
        SubdividePolygon(mdl, surf, b, back[0]);
        return;
    }

    auto * poly = mdl.hunk.AllocBlockOfType<ModelPoly>(1);
    poly->next  = surf.polys;
    surf.polys  = poly;

    poly->num_verts = num_verts + 2; // add a point in the center to help keep warp valid
    poly->vertexes  = mdl.hunk.AllocBlockOfType<PolyVertex>(poly->num_verts);
    poly->triangles = nullptr; // NOTE: will not be allocated for the warped water polygons

    vec3_t total  = {};
    float total_s = 0.0f;
    float total_t = 0.0f;

    for (int i = 0; i < num_verts; ++i, verts += 3)
    {
        Vec3Copy(verts, poly->vertexes[i + 1].position);

        const float s = Vec3Dot(verts, surf.texinfo->vecs[0]);
        const float t = Vec3Dot(verts, surf.texinfo->vecs[1]);
        total_s += s;
        total_t += t;

        Vec3Add(total, verts, total);

        poly->vertexes[i + 1].texture_s = s;
        poly->vertexes[i + 1].texture_t = t;
    }

    Vec3Scale(total, (1.0f / num_verts), poly->vertexes[0].position);
    poly->vertexes[0].texture_s = total_s / num_verts;
    poly->vertexes[0].texture_t = total_t / num_verts;

    // copy first vertex to last
    std::memcpy(&poly->vertexes[num_verts + 1], &poly->vertexes[1], sizeof(poly->vertexes[0]));
}

///////////////////////////////////////////////////////////////////////////////

// Breaks a polygon up along axial 'kSubdivideSize' (64) unit boundaries
// so that turbulent and sky warps can be done reasonably.
static void SubdivideSurface(ModelInstance & mdl, ModelSurface & surf)
{
    vec3_t verts[kSubdivideSize];
    int verts_count = 0;

    // Convert edges back to a normal polygon:
    for (int i = 0; i < surf.num_edges; ++i)
    {
        const int lindex = mdl.data.surf_edges[surf.first_edge + i];
        const float * vec;

        if (lindex > 0)
        {
            vec = mdl.data.vertexes[mdl.data.edges[lindex].v[0]].position;
        }
        else
        {
            vec = mdl.data.vertexes[mdl.data.edges[-lindex].v[1]].position;
        }

        if (verts_count >= kSubdivideSize)
        {
            GameInterface::Errorf("BMod::SubdivideSurface -> Max verts exceeded!");
        }

        Vec3Copy(vec, verts[verts_count++]);
    }

    SubdividePolygon(mdl, surf, verts_count, verts[0]);
}

///////////////////////////////////////////////////////////////////////////////

static void LoadFaces(ModelInstance & mdl, const void * const mdl_data, const lump_t & l)
{
    MRQ2_ASSERT(mdl.data.planes   != nullptr); // load first!
    MRQ2_ASSERT(mdl.data.texinfos != nullptr);

    const auto * in = GetDataPtr<dface_t>(mdl_data, l);
    if (l.filelen % sizeof(*in))
    {
        GameInterface::Errorf("BMod::LoadFaces: Funny lump size in %s", mdl.name.CStr());
    }

    const int count = l.filelen / sizeof(*in);
    auto * out = mdl.hunk.AllocBlockOfType<ModelSurface>(count);

    mdl.data.surfaces     = out;
    mdl.data.num_surfaces = count;

    LightmapManager::BeginBuildLightmaps();

    for (int surf_num = 0; surf_num < count; ++surf_num, ++in, ++out)
    {
        out->first_edge = in->firstedge;
        out->num_edges  = in->numedges;
        out->color      = Config::r_surf_use_debug_color.IsSet() ? RandomDebugColor() : ColorRGBA32{ 0xFFFFFFFF };
        out->flags      = 0;
        out->polys      = nullptr;

        // Default it to not ligthmapped.
        out->lightmap_texture_num = -1;

        const int plane_num = in->planenum;
        const int side = in->side;
        if (side)
        {
            out->flags |= kSurf_PlaneBack;
        }
        out->plane = mdl.data.planes + plane_num;

        const int tex_num = in->texinfo;
        if (tex_num < 0 || tex_num >= mdl.data.num_texinfos)
        {
            GameInterface::Errorf("BMod::LoadFaces: Bad texinfo number: %i", tex_num);
        }
        out->texinfo = mdl.data.texinfos + tex_num;

        //
        // Fill out->texture_mins[] and out->extents[]:
        //
        CalcSurfaceExtents(mdl, *out);

        //
        // Lighting info:
        //
        int i;
        for (i = 0; i < kMaxLightmaps; ++i)
        {
            out->styles[i] = in->styles[i];
        }

        i = in->lightofs;
        if (i == -1)
        {
            out->samples = nullptr;
        }
        else
        {
            MRQ2_ASSERT(mdl.data.light_data != nullptr);
            out->samples = mdl.data.light_data + i;
        }

        //
        // Water simulated surfaces:
        //
        if (out->texinfo->flags & SURF_WARP)
        {
            out->flags |= kSurf_DrawTurb;
            for (i = 0; i < 2; ++i)
            {
                out->extents[i] = 16384;
                out->texture_mins[i] = -8192;
            }

            SubdivideSurface(mdl, *out); // Cut up polygon for warps
        }

        //
        // Create lightmaps:
        //
        if (!(out->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)))
        {
            LightmapManager::CreateSurfaceLightmap(out);
        }

        //
        // Regular opaque surface:
        //
        if (!(out->texinfo->flags & SURF_WARP))
        {
            BuildPolygonFromSurface(mdl, *out);
        }
    }

    LightmapManager::FinishBuildLightmaps();
}

///////////////////////////////////////////////////////////////////////////////

static void LoadMarkSurfaces(ModelInstance & mdl, const void * const mdl_data, const lump_t & l)
{
    MRQ2_ASSERT(mdl.data.surfaces != nullptr); // load first!

    const auto * in = GetDataPtr<std::int16_t>(mdl_data, l);
    if (l.filelen % sizeof(*in))
    {
        GameInterface::Errorf("BMod::LoadMarkSurfaces: Funny lump size in '%s'", mdl.name.CStr());
    }

    const int count = l.filelen / sizeof(*in);
    ModelSurface ** out = mdl.hunk.AllocBlockOfType<ModelSurface *>(count);

    mdl.data.mark_surfaces     = out;
    mdl.data.num_mark_surfaces = count;

    for (int i = 0; i < count; ++i)
    {
        const int j = in[i];
        if (j < 0 || j >= mdl.data.num_surfaces)
        {
            GameInterface::Errorf("BMod::LoadMarkSurfaces: Bad surface number: %i", j);
        }
        out[i] = mdl.data.surfaces + j;
    }
}

///////////////////////////////////////////////////////////////////////////////

static void LoadVisibility(ModelInstance & mdl, const void * const mdl_data, const lump_t & l)
{
    if (l.filelen <= 0)
    {
        if (kVerboseModelLoading)
        {
            GameInterface::Printf("No vis data for brush model '%s'", mdl.name.CStr());
        }

        mdl.data.vis = nullptr;
        return;
    }

    mdl.data.vis = static_cast<dvis_t *>(mdl.hunk.AllocBlock(l.filelen));
    std::memcpy(mdl.data.vis, GetDataPtr<std::uint8_t>(mdl_data, l), l.filelen);
}

///////////////////////////////////////////////////////////////////////////////

static void LoadLeafs(ModelInstance & mdl, const void * const mdl_data, const lump_t & l)
{
    MRQ2_ASSERT(mdl.data.mark_surfaces != nullptr); // load first!

    const auto * in = GetDataPtr<dleaf_t>(mdl_data, l);
    if (l.filelen % sizeof(*in))
    {
        GameInterface::Errorf("BMod::LoadLeafs: Funny lump size in '%s'", mdl.name.CStr());
    }

    const int count = l.filelen / sizeof(*in);
    auto * out = mdl.hunk.AllocBlockOfType<ModelLeaf>(count);

    mdl.data.leafs     = out;
    mdl.data.num_leafs = count;

    for (int i = 0; i < count; ++i, ++in, ++out)
    {
        for (int j = 0; j < 3; ++j)
        {
            out->minmaxs[j] = in->mins[j];
            out->minmaxs[j + 3] = in->maxs[j];
        }

        out->contents = in->contents;
        out->cluster  = in->cluster;
        out->area     = in->area;

        out->first_mark_surface = mdl.data.mark_surfaces + in->firstleafface;
        out->num_mark_surfaces = in->numleaffaces;
    }
}

///////////////////////////////////////////////////////////////////////////////

static void SetParentRecursive(ModelNode * node, ModelNode * parent)
{
    node->parent = parent;
    if (node->contents != -1)
    {
        return;
    }

    SetParentRecursive(node->children[0], node);
    SetParentRecursive(node->children[1], node);
}

///////////////////////////////////////////////////////////////////////////////

static void LoadNodes(ModelInstance & mdl, const void * const mdl_data, const lump_t & l)
{
    MRQ2_ASSERT(mdl.data.planes != nullptr); // load first!
    MRQ2_ASSERT(mdl.data.leafs  != nullptr);

    const auto * in = GetDataPtr<dnode_t>(mdl_data, l);
    if (l.filelen % sizeof(*in))
    {
        GameInterface::Errorf("BMod::LoadNodes: Funny lump size in '%s'", mdl.name.CStr());
    }

    const int count = l.filelen / sizeof(*in);
    auto * out = mdl.hunk.AllocBlockOfType<ModelNode>(count);

    mdl.data.nodes     = out;
    mdl.data.num_nodes = count;

    for (int i = 0; i < count; ++i, ++in, ++out)
    {
        for (int j = 0; j < 3; ++j)
        {
            out->minmaxs[j] = in->mins[j];
            out->minmaxs[j + 3] = in->maxs[j];
        }

        int p = in->planenum;
        out->plane = mdl.data.planes + p;

        out->first_surface = in->firstface;
        out->num_surfaces  = in->numfaces;
        out->contents      = -1; // differentiate from leafs

        for (int j = 0; j < 2; ++j)
        {
            p = in->children[j];
            if (p >= 0)
            {
                out->children[j] = mdl.data.nodes + p;
            }
            else
            {
                out->children[j] = reinterpret_cast<ModelNode *>(mdl.data.leafs + (-1 - p));
            }
        }
    }

    SetParentRecursive(mdl.data.nodes, nullptr); // Also sets nodes and leafs
}

///////////////////////////////////////////////////////////////////////////////

static float RadiusFromBounds(const vec3_t mins, const vec3_t maxs)
{
    vec3_t corner;
    for (int i = 0; i < 3; ++i)
    {
        const float abs_min = std::fabs(mins[i]);
        const float abs_max = std::fabs(maxs[i]);
        corner[i] = (abs_min > abs_max) ? abs_min : abs_max;
    }
    return Vec3Length(corner);
}

///////////////////////////////////////////////////////////////////////////////

static void LoadSubModels(ModelInstance & mdl, const void * const mdl_data, const lump_t & l)
{
    const auto * in = GetDataPtr<dmodel_t>(mdl_data, l);
    if (l.filelen % sizeof(*in))
    {
        GameInterface::Errorf("BMod::LoadSubmodels: Funny lump size in '%s'", mdl.name.CStr());
    }

    const int count = l.filelen / sizeof(*in);
    auto * out = mdl.hunk.AllocBlockOfType<SubModelInfo>(count);

    mdl.data.submodels     = out;
    mdl.data.num_submodels = count;

    for (int i = 0; i < count; ++i, ++in, ++out)
    {
        for (int j = 0; j < 3; ++j)
        {
            // spread the mins/maxs by a unit
            out->mins[j]   = in->mins[j] - 1;
            out->maxs[j]   = in->maxs[j] + 1;
            out->origin[j] = in->origin[j];
        }

        out->radius     = RadiusFromBounds(out->mins, out->maxs);
        out->head_node  = in->headnode;
        out->first_face = in->firstface;
        out->num_faces  = in->numfaces;
    }
}

///////////////////////////////////////////////////////////////////////////////

static void SetUpSubModels(ModelStore & mdl_store, ModelInstance & mdl)
{
    const int num_submodels = mdl.data.num_submodels;
    for (int i = 0; i < num_submodels; ++i)
    {
        const SubModelInfo & submodel = mdl.data.submodels[i];
        ModelInstance * inline_mdl = mdl_store.InlineModelAt(i);

        inline_mdl->data = mdl.data;
        inline_mdl->data.first_model_surface = submodel.first_face;
        inline_mdl->data.num_model_surfaces  = submodel.num_faces;
        inline_mdl->data.first_node          = submodel.head_node;

        if (inline_mdl->data.first_node >= mdl.data.num_nodes)
        {
            GameInterface::Errorf("Inline model %i has bad first_node!", i);
        }

        Vec3Copy(submodel.maxs, inline_mdl->data.maxs);
        Vec3Copy(submodel.mins, inline_mdl->data.mins);
        inline_mdl->data.radius = submodel.radius;

        if (i == 0)
        {
            mdl.data = inline_mdl->data;
        }

        inline_mdl->data.num_leafs = submodel.vis_leafs;
    }
}

} // BMod

///////////////////////////////////////////////////////////////////////////////

void ModelStore::LoadBrushModel(TextureStore & tex_store, ModelInstance & mdl, const void * const mdl_data, const int mdl_data_len)
{
    MRQ2_ASSERT(mdl_data != nullptr);
    MRQ2_ASSERT(mdl_data_len > 0);
    (void)mdl_data_len;

    // Allocate the block we are going to expand the data into:
    const unsigned hunk_size = Megabytes(16); // 16MB is the original size used by Quake 2
    MRQ2_ASSERT(hunk_size >= unsigned(mdl_data_len));
    mdl.hunk.Init(hunk_size, MemTag::kWorldModel);

    // Check header version/id:
    const auto * header = static_cast<const dheader_t *>(mdl_data);
    if (header->version != BSPVERSION)
    {
        GameInterface::Errorf("LoadBrushModel: '%s' has wrong version number (%i should be %i)",
                              mdl.name.CStr(), header->version, BSPVERSION);
    }
    if (header->ident != IDBSPHEADER)
    {
        GameInterface::Errorf("LoadBrushModel: '%s' bad file ident!", mdl.name.CStr());
    }

    // Load file contents into the in-memory model structure:
    BMod::LoadVertexes(mdl, mdl_data, header->lumps[LUMP_VERTEXES]);
    BMod::LoadEdges(mdl, mdl_data, header->lumps[LUMP_EDGES]);
    BMod::LoadSurfEdges(mdl, mdl_data, header->lumps[LUMP_SURFEDGES]);
    BMod::LoadLighting(mdl, mdl_data, header->lumps[LUMP_LIGHTING]);
    BMod::LoadPlanes(mdl, mdl_data, header->lumps[LUMP_PLANES]);
    BMod::LoadTexInfo(tex_store, mdl, mdl_data, header->lumps[LUMP_TEXINFO]);
    BMod::LoadFaces(mdl, mdl_data, header->lumps[LUMP_FACES]);
    BMod::LoadMarkSurfaces(mdl, mdl_data, header->lumps[LUMP_LEAFFACES]);
    BMod::LoadVisibility(mdl, mdl_data, header->lumps[LUMP_VISIBILITY]);
    BMod::LoadLeafs(mdl, mdl_data, header->lumps[LUMP_LEAFS]);
    BMod::LoadNodes(mdl, mdl_data, header->lumps[LUMP_NODES]);
    BMod::LoadSubModels(mdl, mdl_data, header->lumps[LUMP_MODELS]);
    BMod::SetUpSubModels(*this, mdl);
    mdl.data.num_frames = 2; // regular and alternate animation

    // Make sure all images are referenced now:
    for (int i = 0; i < mdl.data.num_texinfos; ++i)
    {
        if (mdl.data.texinfos[i].teximage == nullptr)
        {
            GameInterface::Errorf("Null texture at %i for model '%s'! Should have been loaded...", i, mdl.name.CStr());
        }

        auto * tex = const_cast<TextureImage *>(mdl.data.texinfos[i].teximage);
        tex->m_reg_num = tex_store.RegistrationNum();
    }

    // Vertex/Index buffer setup:
    if (kUseVertexAndIndexBuffers)
    {
        const float world_ambient_term = Config::r_world_ambient.AsFloat(); // Modulate with the vertex color

        const int num_surfaces = mdl.data.num_surfaces;
        const ModelSurface * const surfaces = mdl.data.surfaces;

        int vertex_count = 0;
        int index_count  = 0;

        for (int s = 0; s < num_surfaces; ++s)
        {
            const ModelSurface & surf = surfaces[s];

            for (const ModelPoly * poly = surf.polys; poly != nullptr; poly = poly->next)
            {
                if (poly->triangles == nullptr) // Only indexed polygons
                {
                    MRQ2_ASSERT(surf.texinfo->flags & SURF_WARP);
                    continue;
                }

                const int num_triangles = (poly->num_verts - 2);
                MRQ2_ASSERT(num_triangles > 0);

                vertex_count += poly->num_verts;
                index_count  += num_triangles * 3;
            }
        }

        MRQ2_ASSERT(vertex_count > 0);
        MRQ2_ASSERT(index_count  > 0);

        auto & device = tex_store.Device();

        mdl.vb.Init(device, vertex_count * sizeof(DrawVertex3D),  sizeof(DrawVertex3D));
        mdl.ib.Init(device, index_count  * sizeof(std::uint16_t), IndexBuffer::kFormatUInt16);

        // Add the vertex and index buffer to the memory statistics
        MemTagsTrackAlloc(vertex_count * sizeof(DrawVertex3D),  MemTag::kVertIndexBuffer);
        MemTagsTrackAlloc(index_count  * sizeof(std::uint16_t), MemTag::kVertIndexBuffer);

        auto * vertex_iter = static_cast<DrawVertex3D  *>(mdl.vb.Map());
        auto * index_iter  = static_cast<std::uint16_t *>(mdl.ib.Map());

        int vertex_buffer_offset = 0;
        int index_buffer_offset  = 0;

        for (int s = 0; s < num_surfaces; ++s)
        {
            const ModelSurface & surf = surfaces[s];

            for (ModelPoly * poly = surf.polys; poly != nullptr; poly = poly->next)
            {
                if (poly->triangles == nullptr) // Only indexed polygons
                {
                    MRQ2_ASSERT(surf.texinfo->flags & SURF_WARP);
                    poly->index_buffer = {};
                    continue;
                }

                // Vertex Buffer
                for (int v = 0; v < poly->num_verts; ++v)
                {
                    const PolyVertex & poly_vert = poly->vertexes[v];

                    vertex_iter->position[0] = poly_vert.position[0];
                    vertex_iter->position[1] = poly_vert.position[1];
                    vertex_iter->position[2] = poly_vert.position[2];

                    vertex_iter->texture_uv[0] = poly_vert.texture_s;
                    vertex_iter->texture_uv[1] = poly_vert.texture_t;

                    vertex_iter->lightmap_uv[0] = poly_vert.lightmap_s;
                    vertex_iter->lightmap_uv[1] = poly_vert.lightmap_t;

                    ColorFloats(surf.color, vertex_iter->rgba[0], vertex_iter->rgba[1], vertex_iter->rgba[2], vertex_iter->rgba[3]);

                    // Scale by world "ambient light" term
                    for (int n = 0; n < 4; ++n)
                    {
                        vertex_iter->rgba[n] *= world_ambient_term;
                    }

                    ++vertex_iter;
                }

                // Index Buffer
                const int num_triangles = (poly->num_verts - 2);
                MRQ2_ASSERT(num_triangles > 0);

                poly->index_buffer.first_index = index_buffer_offset;
                poly->index_buffer.index_count = num_triangles * 3;
                poly->index_buffer.base_vertex = vertex_buffer_offset;

                for (int t = 0; t < num_triangles; ++t)
                {
                    const ModelTriangle & mdl_tri = poly->triangles[t];
                    for (int v = 0; v < 3; ++v)
                    {
                        *index_iter++ = mdl_tri.vertexes[v];
                    }
                }

                index_buffer_offset += num_triangles * 3;
                MRQ2_ASSERT(index_buffer_offset <= index_count);

                vertex_buffer_offset += poly->num_verts;
                MRQ2_ASSERT(vertex_buffer_offset <= vertex_count);
            }
        }

        mdl.ib.Unmap();
        mdl.vb.Unmap();
    }

    if (kVerboseModelLoading)
    {
        GameInterface::Printf("New brush model '%s' loaded.", mdl.name.CStr());
    }
}

///////////////////////////////////////////////////////////////////////////////
// SPRITE MODELS:
///////////////////////////////////////////////////////////////////////////////

void ModelStore::LoadSpriteModel(TextureStore & tex_store, ModelInstance & mdl, const void * const mdl_data, const int mdl_data_len)
{
    MRQ2_ASSERT(mdl_data != nullptr);
    MRQ2_ASSERT(mdl_data_len > 0);

    // Allocate the block we are going to expand the data into:
    const unsigned hunk_size = RoundNextPoT(mdl_data_len);
    MRQ2_ASSERT(hunk_size >= unsigned(mdl_data_len));
    mdl.hunk.Init(hunk_size, MemTag::kSpriteModel);

    const auto * p_sprite_in = static_cast<const dsprite_t *>(mdl_data);
    auto * p_sprite_out = static_cast<dsprite_t *>(mdl.hunk.AllocBlock(mdl_data_len));

    p_sprite_out->ident     = p_sprite_in->ident;
    p_sprite_out->version   = p_sprite_in->version;
    p_sprite_out->numframes = p_sprite_in->numframes;

    if (p_sprite_out->version != SPRITE_VERSION)
    {
        GameInterface::Errorf("Sprite %s has wrong version number (%i should be %i)",
                              mdl.name.CStr(), p_sprite_out->version, SPRITE_VERSION);
    }
    if (p_sprite_out->numframes > MAX_MD2SKINS)
    {
        GameInterface::Errorf("Sprite %s has too many frames (%i > %i)",
                              mdl.name.CStr(), p_sprite_out->numframes, MAX_MD2SKINS);
    }

    for (int i = 0; i < p_sprite_out->numframes; ++i)
    {
        p_sprite_out->frames[i].width    = p_sprite_in->frames[i].width;
        p_sprite_out->frames[i].height   = p_sprite_in->frames[i].height;
        p_sprite_out->frames[i].origin_x = p_sprite_in->frames[i].origin_x;
        p_sprite_out->frames[i].origin_y = p_sprite_in->frames[i].origin_y;

        // Reference the texture images:
        std::memcpy(p_sprite_out->frames[i].name, p_sprite_in->frames[i].name, MAX_SKINNAME);
        mdl.data.skins[i] = tex_store.FindOrLoad(p_sprite_out->frames[i].name, TextureType::kSprite);
    }
    mdl.data.num_frames = p_sprite_in->numframes;

    if (kVerboseModelLoading)
    {
        GameInterface::Printf("New sprite model '%s' loaded.", mdl.name.CStr());
    }
}

///////////////////////////////////////////////////////////////////////////////
// ALIAS MD2 MODELS:
///////////////////////////////////////////////////////////////////////////////

void ModelStore::LoadAliasMD2Model(TextureStore & tex_store, ModelInstance & mdl, const void * const mdl_data, const int mdl_data_len)
{
    MRQ2_ASSERT(mdl_data != nullptr);
    MRQ2_ASSERT(mdl_data_len > 0);

    // Allocate the block we are going to expand the data into:
    const unsigned hunk_size = RoundNextPoT(mdl_data_len);
    MRQ2_ASSERT(hunk_size >= unsigned(mdl_data_len));
    mdl.hunk.Init(hunk_size, MemTag::kAliasModel);

    const auto * p_mdl_data_in = static_cast<const dmdl_t *>(mdl_data);
    if (p_mdl_data_in->version != ALIAS_VERSION)
    {
        GameInterface::Errorf("Model '%s' has wrong version number (%i should be %i)",
                              mdl.name.CStr(), p_mdl_data_in->version, ALIAS_VERSION);
    }

    auto * p_header_out = static_cast<dmdl_t *>(mdl.hunk.AllocBlock(p_mdl_data_in->ofs_end));
    *p_header_out = *p_mdl_data_in;

    //
    // Validate header fields:
    //
    if (p_header_out->skinheight > kMaxMD2SkinHeight)
    {
        GameInterface::Errorf("Model '%s' has a skin taller than %i.", mdl.name.CStr(), kMaxMD2SkinHeight);
    }
    if (p_header_out->num_xyz <= 0)
    {
        GameInterface::Errorf("Model '%s' has no vertices!", mdl.name.CStr());
    }
    if (p_header_out->num_xyz > MAX_VERTS)
    {
        GameInterface::Errorf("Model '%s' has too many vertices!", mdl.name.CStr());
    }
    if (p_header_out->num_st <= 0)
    {
        GameInterface::Errorf("Model '%s' has no st vertices!", mdl.name.CStr());
    }
    if (p_header_out->num_tris <= 0)
    {
        GameInterface::Errorf("Model '%s' has no triangles!", mdl.name.CStr());
    }
    if (p_header_out->num_frames <= 0)
    {
        GameInterface::Errorf("Model '%s' has no frames!", mdl.name.CStr());
    }

    //
    // S and T texture coordinates:
    //
    const auto * p_st_in = (const dstvert_t *)((const std::uint8_t *)p_mdl_data_in + p_header_out->ofs_st);
    auto * p_st_out = (dstvert_t *)((std::uint8_t *)p_header_out + p_header_out->ofs_st);

    for (int i = 0; i < p_header_out->num_st; ++i)
    {
        p_st_out[i].s = p_st_in[i].s;
        p_st_out[i].t = p_st_in[i].t;
    }

    //
    // Triangle lists:
    //
    const auto * p_tris_in = (const dtriangle_t *)((const std::uint8_t *)p_mdl_data_in + p_header_out->ofs_tris);
    auto * p_tris_out = (dtriangle_t *)((std::uint8_t *)p_header_out + p_header_out->ofs_tris);

    for (int i = 0; i < p_header_out->num_tris; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            p_tris_out[i].index_xyz[j] = p_tris_in[i].index_xyz[j];
            p_tris_out[i].index_st[j]  = p_tris_in[i].index_st[j];
        }
    }

    //
    // Animation frames:
    //
    for (int i = 0; i < p_header_out->num_frames; ++i)
    {
        const auto * p_frame_in = (const daliasframe_t *)((const std::uint8_t *)p_mdl_data_in +
                                                          p_header_out->ofs_frames +
                                                          i * p_header_out->framesize);

        auto * p_frame_out = (daliasframe_t *)((std::uint8_t *)p_header_out +
                                               p_header_out->ofs_frames +
                                               i * p_header_out->framesize);

        std::memcpy(p_frame_out->name, p_frame_in->name, sizeof(p_frame_out->name));

        for (int j = 0; j < 3; ++j)
        {
            p_frame_out->scale[j]     = p_frame_in->scale[j];
            p_frame_out->translate[j] = p_frame_in->translate[j];
        }

        std::memcpy(p_frame_out->verts, p_frame_in->verts, p_header_out->num_xyz * sizeof(dtrivertx_t));
    }

    //
    // The GL Cmds:
    //
    const auto * p_cmds_in = (const int *)((const std::uint8_t *)p_mdl_data_in + p_header_out->ofs_glcmds);
    auto * p_cmds_out = (int *)((std::uint8_t *)p_header_out + p_header_out->ofs_glcmds);

    for (int i = 0; i < p_header_out->num_glcmds; ++i)
    {
        p_cmds_out[i] = p_cmds_in[i];
    }

    // Set defaults for these:
    mdl.data.mins[0] = -32.0f;
    mdl.data.mins[1] = -32.0f;
    mdl.data.mins[2] = -32.0f;
    mdl.data.maxs[0] =  32.0f;
    mdl.data.maxs[1] =  32.0f;
    mdl.data.maxs[2] =  32.0f;
    mdl.data.num_frames = p_header_out->num_frames;

    //
    // Register all skins:
    //
    std::memcpy((char *)p_header_out + p_header_out->ofs_skins,
                (const char *)p_mdl_data_in + p_header_out->ofs_skins,
                p_header_out->num_skins * MAX_SKINNAME);

    for (int i = 0; i < p_header_out->num_skins; ++i)
    {
        const char * p_skin_name = (const char *)p_header_out + p_header_out->ofs_skins + (i * MAX_SKINNAME);
        mdl.data.skins[i] = tex_store.FindOrLoad(p_skin_name, TextureType::kSkin);
    }

    if (kVerboseModelLoading)
    {
        GameInterface::Printf("New alias model '%s' loaded.", mdl.name.CStr());
    }
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
