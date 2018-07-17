//
// SkyBox.cpp
//  SkyBox rendering helper class.
//

#include "SkyBox.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// SkyBox
///////////////////////////////////////////////////////////////////////////////

SkyBox::SkyBox(TextureStore & tex_store, const char * const name, const float rotate, const vec3_t axis)
    : m_sky_rotate{ rotate }
{
    // Select between TGA or PCX - defaults to TGA (higher quality).
    auto r_sky_use_pal_textures = GameInterface::Cvar::Get("r_sky_use_pal_textures", "0", CvarWrapper::kFlagArchive);

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

    static const char * const suf_names[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

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
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
