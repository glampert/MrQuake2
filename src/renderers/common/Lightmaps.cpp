//
// Lightmaps.cpp
//  Classic Quake2 lightmaps.
//

#include "Lightmaps.hpp"
#include "TextureStore.hpp"
#include "ModelStructs.hpp"

// Quake includes
#include "common/q_common.h"
#include "common/q_files.h"
#include "client/ref.h"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////

static void AddDynamicLights(float dest_light_block[kLightBlockSize], const ModelSurface * surf, const int num_dlights, const dlight_t * dlights)
{
    // Add dynamic lights contribution to the lightmaps.

    const int smax = (surf->extents[0] >> 4) + 1;
    const int tmax = (surf->extents[1] >> 4) + 1;
    const ModelTexInfo * tex = surf->texinfo;

    for (int lnum = 0; lnum < num_dlights; ++lnum)
    {
        if (!(surf->dlight_bits & (1 << lnum)))
        {
            continue; // not lit by this light
        }

        const dlight_t * dl = &dlights[lnum];
        float frad  = dl->intensity;
        float fdist = Vec3Dot(dl->origin, surf->plane->normal) - surf->plane->dist;
        frad -= std::fabsf(fdist); // rad is now the highest intensity on the plane

        float fminlight = kDLightCutoff; // make configurable?
        if (frad < fminlight)
        {
            continue;
        }

        fminlight = frad - fminlight;

        vec3_t impact;
        for (int i = 0; i < 3; ++i)
        {
            impact[i] = dl->origin[i] - surf->plane->normal[i] * fdist;
        }

        vec2_t local;
        local[0] = Vec3Dot(impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texture_mins[0];
        local[1] = Vec3Dot(impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texture_mins[1];

        int t, s;
        float fsacc, ftacc;
        float * light_block_ptr = dest_light_block;

        for (t = 0, ftacc = 0; t < tmax; t++, ftacc += 16)
        {
            int td = local[1] - ftacc;
            if (td < 0)
                td = -td;

            for (s = 0, fsacc = 0; s < smax; s++, fsacc += 16, light_block_ptr += 3)
            {
                int sd = static_cast<int>(local[0] - fsacc);

                if (sd < 0)
                    sd = -sd;

                if (sd > td)
                    fdist = sd + (td >> 1);
                else
                    fdist = td + (sd >> 1);

                if (fdist < fminlight)
                {
                    light_block_ptr[0] += (frad - fdist) * dl->color[0];
                    light_block_ptr[1] += (frad - fdist) * dl->color[1];
                    light_block_ptr[2] += (frad - fdist) * dl->color[2];
                }
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

static void StoreLightmap(std::uint8_t * dest, const int stride, const int smax, const int tmax, const float light_block[kLightBlockSize])
{
    const auto lightmap_format = LightmapFormat(Config::r_lightmap_format.AsStr()[0]);

    const float * light_block_ptr = light_block;

    for (int i = 0; i < tmax; i++, dest += stride)
    {
        for (int j = 0; j < smax; j++)
        {
            int r = static_cast<int>(light_block_ptr[0]);
            int g = static_cast<int>(light_block_ptr[1]);
            int b = static_cast<int>(light_block_ptr[2]);

            // catch negative lights
            if (r < 0)
                r = 0;
            if (g < 0)
                g = 0;
            if (b < 0)
                b = 0;

            // determine the brightest of the three color components
            int max;
            if (r > g)
                max = r;
            else
                max = g;
            if (b > max)
                max = b;

            // alpha is ONLY used for the mono lightmap case. For this reason
            // we set it to the brightest of the color components so that
            // things don't get too dim.
            int a = max;

            // rescale all the color components if the intensity of the greatest
            // channel exceeds 1.0
            if (max > 255)
            {
                const float t = 255.0f / max;
                r = r * t;
                g = g * t;
                b = b * t;
                a = a * t;
            }

            switch (lightmap_format)
            {
            case LightmapFormat::kDefault:
                break;

            case LightmapFormat::kRedChannel:
                r = a;
                g = b = 0;
                break;

            case LightmapFormat::kRGBA:
                // try faking colored lighting
                a = 255 - ((r + g + b) / 3);
                r *= a / 255.0f;
                g *= a / 255.0f;
                b *= a / 255.0f;
                break;

            case LightmapFormat::kInvAlpha:
                // If we are doing alpha lightmaps we need to set the R, G, and B
                // components to 0 and we need to set alpha to 1-alpha.
                r = g = b = 0;
                a = 255 - a;
                break;

            default:
                GameInterface::Errorf("Invalid lightmap format: %d", int(lightmap_format));
            } // switch

            dest[0] = static_cast<std::uint8_t>(r);
            dest[1] = static_cast<std::uint8_t>(g);
            dest[2] = static_cast<std::uint8_t>(b);
            dest[3] = static_cast<std::uint8_t>(a);

            light_block_ptr += 3;
            dest += 4;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

static void BuildLightmap(std::uint8_t * dest, const int stride,
                          const int frame_num, const float lmap_modulate, const ModelSurface * surf,
                          const int num_dlights, const dlight_t * dlights, const lightstyle_t * lightstyles)
{
    // Combine and scale multiple lightmaps into the floating format in light_block[]
    // then store into the RGBA_U8 texture buffer.

    if (surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP))
    {
        GameInterface::Errorf("BuildLightmap called for non-lit surface!");
    }

    const int smax = (surf->extents[0] >> 4) + 1;
    const int tmax = (surf->extents[1] >> 4) + 1;
    const int size = smax * tmax;

    float light_block[kLightBlockSize] = {};
    if (size > (sizeof(light_block) >> 4))
    {
        GameInterface::Errorf("Bad light block size!");
    }

    // Set to full bright if no lightmap data
    if (surf->samples == nullptr)
    {
        for (int i = 0; i < size * 3; ++i)
        {
            light_block[i] = 255.0f;
        }

        StoreLightmap(dest, stride - (smax << 2), smax, tmax, light_block);
    }
    else
    {
        const std::uint8_t * lightmap = surf->samples;
        vec3_t scale = { 1.0f, 1.0f, 1.0f };

        // Add all the lightmaps
        for (int lmap = 0; lmap < kMaxLightmaps && surf->styles[lmap] != 255; ++lmap)
        {
            // Use modulated lightstyles if we have then, otherwise scale is all 1s.
            if (lightstyles != nullptr)
            {
                for (int i = 0; i < 3; ++i)
                {
                    MRQ2_ASSERT(surf->styles[lmap] < MAX_LIGHTSTYLES);
                    scale[i] = lmap_modulate * lightstyles[surf->styles[lmap]].rgb[i];
                }
            }

            float * light_block_ptr = light_block;
            for (int i = 0; i < size; i++, light_block_ptr += 3)
            {
                light_block_ptr[0] += lightmap[i * 3 + 0] * scale[0];
                light_block_ptr[1] += lightmap[i * 3 + 1] * scale[1];
                light_block_ptr[2] += lightmap[i * 3 + 2] * scale[2];
            }

            lightmap += size * 3; // Skip to next lightmap
        }

        // Add all the dynamic lights
        if ((surf->dlight_frame == frame_num) && (num_dlights > 0))
        {
            AddDynamicLights(light_block, surf, num_dlights, dlights);
        }

        // Put into texture format
        StoreLightmap(dest, stride - (smax << 2), smax, tmax, light_block);
    }
}

///////////////////////////////////////////////////////////////////////////////

static void SetCacheState(ModelSurface * surf, const lightstyle_t * lightstyles)
{
    for (int lmap = 0; lmap < kMaxLightmaps && surf->styles[lmap] != 255; ++lmap)
    {
        MRQ2_ASSERT(surf->styles[lmap] < MAX_LIGHTSTYLES);
        surf->cached_light[lmap] = lightstyles[surf->styles[lmap]].white;
    }
}

///////////////////////////////////////////////////////////////////////////////
// LightmapManager
///////////////////////////////////////////////////////////////////////////////

// Write out each lightmap to a PNG on UploadBlock
constexpr bool kDebugDumpLightmapsToFile = false;

LightmapManager & LightmapManager::Instance()
{
    static LightmapManager s_instance;
    return s_instance;
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::Init(TextureStore & tex_store)
{
    m_tex_store = &tex_store;
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::Shutdown()
{
    for (auto & t : m_textures) t = nullptr;
    m_tex_store = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::BeginRegistration()
{
    // Null out all the textures, they will be recreated on demand.
    for (auto & t : m_textures) t = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::EndRegistration()
{
    // Nothing.
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::BeginBuildLightmaps()
{
    m_current_lightmap_texture = 1; // Index 0 is reserved for the dynamic lightmap.
    Reset();
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::FinishBuildLightmaps()
{
    UploadBlock(false);
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::Reset()
{
    std::memset(m_allocated,       0, sizeof(m_allocated));
    std::memset(m_lightmap_buffer, 0, sizeof(m_lightmap_buffer));
}

///////////////////////////////////////////////////////////////////////////////

bool LightmapManager::AllocBlock(const int w, const int h, int * x, int * y)
{
    int best = kLightmapBlockHeight;

    for (int i = 0; i < kLightmapBlockWidth - w; ++i)
    {
        int j, best2 = 0;

        for (j = 0; j < w; ++j)
        {
            if (m_allocated[i + j] >= best)
            {
                break;
            }
            if (m_allocated[i + j] > best2)
            {
                best2 = m_allocated[i + j];
            }
        }

        if (j == w) // This is a valid spot
        {
            *x = i;
            *y = best = best2;
        }
    }

    if ((best + h) > kLightmapBlockHeight)
    {
        return false;
    }

    for (int i = 0; i < w; ++i)
    {
        m_allocated[*x + i] = best + h;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::UploadBlock(const bool is_dynamic)
{
    MRQ2_ASSERT(m_tex_store != nullptr);
    MRQ2_ASSERT(m_current_lightmap_texture < kMaxLightmapTextures);
    MRQ2_ASSERT(m_current_lightmap_texture >= 1);

    const int texture_index = is_dynamic ? 0 : m_current_lightmap_texture;

    // Allocate on demand
    if (m_textures[texture_index] == nullptr)
    {
        // This will also upload and initialize the lightmap with m_lightmap_buffer
        m_textures[texture_index] = m_tex_store->AllocLightmap(m_lightmap_buffer);
    }
    else
    {
        MRQ2_ASSERT(m_textures[texture_index]->BasePixels() == m_lightmap_buffer);
        MRQ2_ASSERT(m_textures[texture_index]->Width()  == kLightmapBlockWidth);
        MRQ2_ASSERT(m_textures[texture_index]->Height() == kLightmapBlockHeight);
        MRQ2_ASSERT(m_textures[texture_index]->NumMipMapLevels() == 1);

        constexpr uint32_t  num_mip_levels = 1;
        const ColorRGBA32 * mip_init_data[num_mip_levels]  = { m_textures[texture_index]->BasePixels() };
        const Vec2u16       mip_dimensions[num_mip_levels] = { m_textures[texture_index]->MipMapDimensions(0) };

        TextureUpload upload_info{};
        upload_info.texture  = &m_textures[texture_index]->BackendTexture();
        upload_info.is_scrap = true;
        upload_info.mipmaps.num_mip_levels = num_mip_levels;
        upload_info.mipmaps.mip_init_data  = mip_init_data;
        upload_info.mipmaps.mip_dimensions = mip_dimensions;
        m_tex_store->Device().UploadContext().UploadTextureImmediate(upload_info);
    }

    if (kDebugDumpLightmapsToFile)
    {
        DebugDumpToFile();
    }

    if (!is_dynamic)
    {
        // Current texture atlas is full, start to another one.
        ++m_current_lightmap_texture;

        if (m_current_lightmap_texture == kMaxLightmapTextures)
        {
            GameInterface::Errorf("Ran out of lightmap textures! (%i)", kMaxLightmapTextures);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::DebugDumpToFile() const
{
    // Dump the current lightmap to a PNG
    char name[512] = {};
    sprintf_s(name, "lightmaps/%c_lightmap_%d.png", Config::r_lightmap_format.AsStr()[0], m_current_lightmap_texture);
    PNGSaveToFile(name, kLightmapBlockWidth, kLightmapBlockHeight, m_lightmap_buffer);
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::CreateSurfaceLightmap(ModelSurface * surf)
{
    if (surf->flags & (kSurf_DrawSky | kSurf_DrawTurb))
    {
        return;
    }

    const int smax = (surf->extents[0] >> 4) + 1;
    const int tmax = (surf->extents[1] >> 4) + 1;

    if (!AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
    {
        UploadBlock(false);
        Reset();

        if (!AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
        {
            GameInterface::Errorf("Consecutive calls to LightmapManager::AllocBlock(%d,%d) failed", smax, tmax);
        }
    }

    surf->lightmap_texture_num = m_current_lightmap_texture;

    auto * lm_block = reinterpret_cast<std::uint8_t *>(m_lightmap_buffer);
    lm_block += (surf->light_t * kLightmapBlockWidth + surf->light_s) * kLightmapBytesPerPixel;
    const int lm_stride = kLightmapBlockWidth * kLightmapBytesPerPixel;

    const float lightmap_intensity = Config::r_lightmap_intensity.AsFloat();

    // No dynamic lights at this point, just the base lightmaps
    const int frame_num = -1;
    const int num_dlights = 0;
    const dlight_t * dlights = nullptr;

    // Default lightstyles
    lightstyle_t lightstyles[MAX_LIGHTSTYLES];
    for (int i = 0; i < MAX_LIGHTSTYLES; ++i)
    {
        lightstyles[i].rgb[0] = 1.0f;
        lightstyles[i].rgb[1] = 1.0f;
        lightstyles[i].rgb[2] = 1.0f;
        lightstyles[i].white  = 3.0f;
    }

    SetCacheState(surf, lightstyles);
    BuildLightmap(lm_block, lm_stride, frame_num, lightmap_intensity, surf, num_dlights, dlights, lightstyles);
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
