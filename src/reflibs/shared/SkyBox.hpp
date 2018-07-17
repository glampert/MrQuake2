//
// SkyBox.hpp
//  SkyBox rendering helper class.
//
#pragma once

#include "TextureStore.hpp"
#include <array>

namespace MrQ2
{

/*
===============================================================================

    SkyBox

===============================================================================
*/
class SkyBox final
{
public:

    SkyBox() = default;
    SkyBox(TextureStore & tex_store, const char * name, float rotate, const vec3_t axis);

private:

    std::array<const TextureImage *, 6> m_sky_images = {};
    char m_sky_name[PathName::kNameMaxLen] = {};

    vec3_t m_sky_axis   = {};
    float  m_sky_rotate = 0.0f;
    float  m_sky_min    = 0.0f;
    float  m_sky_max    = 0.0f;
};

} // MrQ2
