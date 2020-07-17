//
// SkyBox.hpp
//  SkyBox rendering helper class.
//
#pragma once

#include "ImmediateModeBatching.hpp"
#include <array>

namespace MrQ2
{

class TextureStore;
class TextureImage;
struct ModelSurface;

/*
===============================================================================

    SkyBox

===============================================================================
*/
class SkyBox final
{
public:

    static constexpr int kNumSides = 6;

    SkyBox() = default;
    SkyBox(TextureStore & tex_store, const char * name, float rotate_degrees, const vec3_t axis);

    void AddSkySurface(const ModelSurface & surf, const vec3_t view_origin);
    void Clear();

    bool IsAnyPlaneVisible() const;
    bool BuildSkyPlane(int plane_index, DrawVertex3D out_plane_verts[6], const TextureImage ** out_plane_tex);

    float AxisX() const { return m_sky_axis[0]; }
    float AxisY() const { return m_sky_axis[1]; }
    float AxisZ() const { return m_sky_axis[2]; }
    float RotateDegrees() const { return m_sky_rotate; }

private:

    void AddSkyPolygon(int verts_count, const vec3_t vecs);
    void ClipSkyPolygon(int verts_count, vec3_t vecs, int stage);
    void MakeSkyVec(float s, float t, int axis, DrawVertex3D & out_vert) const;

    std::array<const TextureImage *, kNumSides> m_sky_images = {};
    char m_sky_name[PathName::kNameMaxLen] = {};

    vec3_t m_sky_axis   = {};
    float  m_sky_rotate = 0.0f;
    float  m_sky_min    = 0.0f;
    float  m_sky_max    = 0.0f;

    float m_skybounds_mins[2][kNumSides];
    float m_skybounds_maxs[2][kNumSides];

    CvarWrapper m_sky_force_full_draw;
};

} // MrQ2
