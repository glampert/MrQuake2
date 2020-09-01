//
// Lightmaps.cpp
//  Classic Quake2 lightmaps.
//

#include "Lightmaps.hpp"
#include "TextureStore.hpp"
#include "ModelStructs.hpp"
#include "ImmediateModeBatching.hpp"
#include "OptickProfiler.hpp"

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
        GameInterface::Errorf("Bad lightmap block size!");
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
        vec3_t scale = {};

        // Add all the lightmaps
        for (int lmap = 0; lmap < kMaxLightmaps && surf->styles[lmap] != 255; ++lmap)
        {
            for (int i = 0; i < 3; ++i)
            {
                MRQ2_ASSERT(surf->styles[lmap] < MAX_LIGHTSTYLES);
                scale[i] = lmap_modulate * lightstyles[surf->styles[lmap]].rgb[i];
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

static inline void SetSurfaceCachedLightingInfo(ModelSurface * surf, const lightstyle_t * lightstyles)
{
    for (int lmap = 0; lmap < kMaxLightmaps && surf->styles[lmap] != 255; ++lmap)
    {
        MRQ2_ASSERT(surf->styles[lmap] < MAX_LIGHTSTYLES);
        surf->cached_light[lmap] = lightstyles[surf->styles[lmap]].white;
    }
}

///////////////////////////////////////////////////////////////////////////////

static inline void ClearTexture(LmImageBuffer * buff)
{
    // Clear to white
    std::memset(buff, 0xFF, sizeof(LmImageBuffer));
}

///////////////////////////////////////////////////////////////////////////////
// LightmapManager
///////////////////////////////////////////////////////////////////////////////

// Global instance data
int                  LightmapManager::sm_static_lightmap_updates{ 0 };
int                  LightmapManager::sm_dynamic_lightmap_updates{ 0 };
int                  LightmapManager::sm_num_lightmaps_buffers{ 0 };
int                  LightmapManager::sm_lightmap_count{ 0 };
TextureStore *       LightmapManager::sm_tex_store{ nullptr };
const TextureImage * LightmapManager::sm_static_lightmaps[kMaxLightmapTextures] = {};
const TextureImage * LightmapManager::sm_dynamic_lightmaps[kMaxLightmapTextures] = {};
LmImageBuffer *      LightmapManager::sm_static_lightmap_buffers[kMaxLightmapTextures] = {};
LmImageBuffer *      LightmapManager::sm_dynamic_lightmap_buffers[kMaxLightmapTextures] = {};
bool                 LightmapManager::sm_dirtied_static_lightmaps[kMaxLightmapTextures] = {};
bool                 LightmapManager::sm_dirtied_dynamic_lightmaps[kMaxLightmapTextures] = {};
LmImageBufferPool    LightmapManager::sm_lightmap_buffer_pool{ MemTag::kLightmaps };
int                  LightmapManager::sm_allocated_blocks[kLightmapTextureWidth] = {};

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::Init(TextureStore & tex_store)
{
    sm_tex_store = &tex_store;
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::Shutdown()
{
    for (int lmap = 0; lmap < kMaxLightmapTextures; ++lmap)
    {
        sm_static_lightmaps[lmap]  = nullptr;
        sm_dynamic_lightmaps[lmap] = nullptr;

        sm_static_lightmap_buffers[lmap]  = nullptr;
        sm_dynamic_lightmap_buffers[lmap] = nullptr;
    }

    sm_tex_store = nullptr;
    sm_lightmap_count = 0;
    sm_lightmap_buffer_pool.Drain();
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::Update()
{
    OPTICK_EVENT();

    auto UploadLightmap = [](const TextureImage * lightmap_tex)
    {
        const ColorRGBA32 * pixels[] = { lightmap_tex->BasePixels() };
        const Vec2u16 dimensions = lightmap_tex->MipMapDimensions(0);

        TextureUpload upload_info{};
        upload_info.texture  = &lightmap_tex->BackendTexture();
        upload_info.is_scrap = true;
        upload_info.mipmaps.num_mip_levels = 1;
        upload_info.mipmaps.mip_init_data  = pixels;
        upload_info.mipmaps.mip_dimensions = &dimensions;
        sm_tex_store->Device().UploadContext().UploadTexture(upload_info);
    };

    sm_dynamic_lightmap_updates = 0;
    sm_static_lightmap_updates  = 0;
    sm_num_lightmaps_buffers    = sm_lightmap_buffer_pool.BlockCount() * sm_lightmap_buffer_pool.PoolGranularity();

    for (int lmap = 0; lmap < sm_lightmap_count; ++lmap)
    {
        if (sm_dirtied_dynamic_lightmaps[lmap])
        {
            sm_dirtied_dynamic_lightmaps[lmap] = false;

            auto * dynamic_lightmap_tex = sm_dynamic_lightmaps[lmap];
            UploadLightmap(dynamic_lightmap_tex);

            ++sm_dynamic_lightmap_updates;
        }

        if (sm_dirtied_static_lightmaps[lmap])
        {
            sm_dirtied_static_lightmaps[lmap] = false;

            auto * static_lightmap_tex = sm_static_lightmaps[lmap];
            UploadLightmap(static_lightmap_tex);

            ++sm_static_lightmap_updates;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::BeginRegistration(const char * const map_name)
{
    static PathName s_current_map;

    bool is_level_reload = false;
    if (PathName{ map_name }.Hash() == s_current_map.Hash())
    {
        is_level_reload = true;
    }

    s_current_map = PathName{ map_name };

    // If we're reloading the same level we can preserve the existing lightmaps because the world model will be reused.
    if (!is_level_reload)
    {
        // Null out all the textures, they will be recreated on demand.
        for (int lmap = 0; lmap < sm_lightmap_count; ++lmap)
        {
            sm_static_lightmaps[lmap]  = nullptr;
            sm_dynamic_lightmaps[lmap] = nullptr;

            if (sm_static_lightmap_buffers[lmap] != nullptr)
            {
                sm_lightmap_buffer_pool.Deallocate(sm_static_lightmap_buffers[lmap]);
                sm_static_lightmap_buffers[lmap] = nullptr;
            }

            if (sm_dynamic_lightmap_buffers[lmap] != nullptr)
            {
                sm_lightmap_buffer_pool.Deallocate(sm_dynamic_lightmap_buffers[lmap]);
                sm_dynamic_lightmap_buffers[lmap] = nullptr;
            }
        }

        sm_lightmap_count = 0;
    }
    else // Just update the registration number so the textures are not freed.
    {
        for (int lmap = 0; lmap < sm_lightmap_count; ++lmap)
        {
            if (sm_static_lightmaps[lmap] != nullptr)
            {
                auto * tex = const_cast<TextureImage *>(sm_static_lightmaps[lmap]);
                tex->m_reg_num = sm_tex_store->RegistrationNum();
            }

            if (sm_dynamic_lightmaps[lmap] != nullptr)
            {
                auto * tex = const_cast<TextureImage *>(sm_dynamic_lightmaps[lmap]);
                tex->m_reg_num = sm_tex_store->RegistrationNum();
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::EndRegistration()
{
    // Nothing.
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::BeginBuildLightmaps()
{
    MRQ2_ASSERT(sm_tex_store != nullptr);

    sm_lightmap_count = 0;

    // Start lightmap[0]
    sm_static_lightmap_buffers[0]  = sm_lightmap_buffer_pool.Allocate();
    sm_dynamic_lightmap_buffers[0] = sm_lightmap_buffer_pool.Allocate();

    ClearTexture(sm_static_lightmap_buffers[0]);
    ClearTexture(sm_dynamic_lightmap_buffers[0]);

    ResetBlocks();
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::FinishBuildLightmaps()
{
    NextLightmapTexture(false);
    ResetBlocks();
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::ResetBlocks()
{
    std::memset(sm_allocated_blocks, 0, sizeof(sm_allocated_blocks));
}

///////////////////////////////////////////////////////////////////////////////

bool LightmapManager::AllocBlock(const int w, const int h, int * x, int * y)
{
    int best = kLightmapTextureHeight;

    for (int i = 0; i < kLightmapTextureWidth - w; ++i)
    {
        int j, best2 = 0;

        for (j = 0; j < w; ++j)
        {
            if (sm_allocated_blocks[i + j] >= best)
            {
                break;
            }
            if (sm_allocated_blocks[i + j] > best2)
            {
                best2 = sm_allocated_blocks[i + j];
            }
        }

        if (j == w) // This is a valid spot
        {
            *x = i;
            *y = best = best2;
        }
    }

    if ((best + h) > kLightmapTextureHeight)
    {
        return false;
    }

    for (int i = 0; i < w; ++i)
    {
        sm_allocated_blocks[*x + i] = best + h;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::NextLightmapTexture(const bool new_buffers)
{
    MRQ2_ASSERT(sm_tex_store != nullptr);
    MRQ2_ASSERT(sm_lightmap_count >= 0 && sm_lightmap_count < kMaxLightmapTextures);

    MRQ2_ASSERT(sm_static_lightmaps[sm_lightmap_count]  == nullptr);
    MRQ2_ASSERT(sm_dynamic_lightmaps[sm_lightmap_count] == nullptr);

    MRQ2_ASSERT(sm_static_lightmap_buffers[sm_lightmap_count]  != nullptr);
    MRQ2_ASSERT(sm_dynamic_lightmap_buffers[sm_lightmap_count] != nullptr);

    char name[256];

    // Allocate textures on demand.
    // This will also upload and initialize the lightmaps with their CPU-side buffers.

    // Static
    sprintf_s(name, "static_lightmap_%d", sm_lightmap_count);
    sm_static_lightmaps[sm_lightmap_count] = sm_tex_store->AllocLightmap(sm_static_lightmap_buffers[sm_lightmap_count]->pixels, kLightmapTextureWidth, kLightmapTextureHeight, name);

    // Dynamic
    sprintf_s(name, "dyn_lightmap_%d", sm_lightmap_count);
    sm_dynamic_lightmaps[sm_lightmap_count] = sm_tex_store->AllocLightmap(sm_dynamic_lightmap_buffers[sm_lightmap_count]->pixels, kLightmapTextureWidth, kLightmapTextureHeight, name);

    // Current lightmap texture is full, start another one.
    if ((++sm_lightmap_count) == kMaxLightmapTextures)
    {
        GameInterface::Errorf("Ran out of lightmap textures! (%i)", kMaxLightmapTextures);
    }

    if (new_buffers)
    {
        sm_static_lightmap_buffers[sm_lightmap_count]  = sm_lightmap_buffer_pool.Allocate();
        sm_dynamic_lightmap_buffers[sm_lightmap_count] = sm_lightmap_buffer_pool.Allocate();

        ClearTexture(sm_static_lightmap_buffers[sm_lightmap_count]);
        ClearTexture(sm_dynamic_lightmap_buffers[sm_lightmap_count]);
    }
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
        NextLightmapTexture(true);
        ResetBlocks();

        if (!AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
        {
            GameInterface::Errorf("Consecutive calls to LightmapManager::AllocBlock(%i,%i) failed", smax, tmax);
        }
    }

    surf->lightmap_texture_num = sm_lightmap_count;

    auto * lm_block = reinterpret_cast<std::uint8_t *>(sm_static_lightmap_buffers[sm_lightmap_count]->pixels);
    MRQ2_ASSERT(lm_block != nullptr);

    lm_block += (surf->light_t * kLightmapTextureWidth + surf->light_s) * kLightmapBytesPerPixel;
    const int lm_stride = kLightmapTextureWidth * kLightmapBytesPerPixel;

    const float lightmap_intensity = Config::r_lightmap_intensity.AsFloat();

    // No dynamic lights at this point, just the base lightmaps
    const int frame_num      = -1;
    const int num_dlights    =  0;
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

    BuildLightmap(lm_block, lm_stride, frame_num, lightmap_intensity, surf, num_dlights, dlights, lightstyles);
    SetSurfaceCachedLightingInfo(surf, lightstyles);
}

///////////////////////////////////////////////////////////////////////////////

const TextureImage * LightmapManager::UpdateSurfaceLightmap(const ModelSurface * surf, const int lightmap_index, const lightstyle_t * lightstyles,
                                                            const dlight_t * dlights, const int num_dlights, const int frame_num,
                                                            const bool update_surf_cache, const bool dynamic_lightmap)
{
    MRQ2_ASSERT(lightmap_index >= 0 && lightmap_index < sm_lightmap_count);
    const float lightmap_intensity = Config::r_lightmap_intensity.AsFloat();

    const int smax = (surf->extents[0] >> 4) + 1;
    const int tmax = (surf->extents[1] >> 4) + 1;

    auto * lm_block = reinterpret_cast<std::uint8_t *>(dynamic_lightmap ? sm_dynamic_lightmap_buffers[lightmap_index]->pixels : sm_static_lightmap_buffers[lightmap_index]->pixels);
    MRQ2_ASSERT(lm_block != nullptr);

    lm_block += (surf->light_t * kLightmapTextureWidth + surf->light_s) * kLightmapBytesPerPixel;
    const int lm_stride = kLightmapTextureWidth * kLightmapBytesPerPixel;

    BuildLightmap(lm_block, lm_stride, frame_num, lightmap_intensity, surf, num_dlights, dlights, lightstyles);

    if (update_surf_cache)
    {
        SetSurfaceCachedLightingInfo(const_cast<ModelSurface *>(surf), lightstyles);
    }

    if (dynamic_lightmap)
    {
        sm_dirtied_dynamic_lightmaps[lightmap_index] = true;
        return sm_dynamic_lightmaps[lightmap_index];
    }
    else
    {
        sm_dirtied_static_lightmaps[lightmap_index] = true;
        return sm_static_lightmaps[lightmap_index];
    }
}

///////////////////////////////////////////////////////////////////////////////

void LightmapManager::DebugDisplayTextures(SpriteBatch & batch, const int scr_w, const int scr_h)
{
    const float scale = 1.0f;
    float x = 5.0f;
    float y = 65.0f;

    auto Background = [&]()
    {
        batch.PushQuadTextured(x, y, (scr_w - 10.0f), (kLightmapTextureHeight + 5.0f) * scale,
                               sm_tex_store->tex_white2x2, ColorRGBA32{ 0xFFFFFFFF });
        x += 5.0f;
        y += 5.0f;
    };

    auto TexturedQuad = [&](const TextureImage * tex, const bool is_last)
    {
        const float w = tex->Width()  * scale;
        const float h = tex->Height() * scale;

        batch.PushQuadTextured(x, y, w, h, tex, ColorRGBA32{ 0xFFFFFFFF });

        x += w + 1.0f;
        if (!is_last && (x + w + 1.0f) >= scr_w) // Wrap around if next one would be clipped
        {
            x = 5.0f;
            y += h + 6.0f;
            Background();
        }
    };

    Background();

    for (int lmap = 0; lmap < sm_lightmap_count; ++lmap)
    {
        auto * tex = sm_dynamic_lightmaps[lmap];
        TexturedQuad(tex, lmap == (sm_lightmap_count - 1));
    }

    for (int lmap = 0; lmap < sm_lightmap_count; ++lmap)
    {
        auto * tex = sm_static_lightmaps[lmap];
        TexturedQuad(tex, lmap == (sm_lightmap_count - 1));
    }
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
