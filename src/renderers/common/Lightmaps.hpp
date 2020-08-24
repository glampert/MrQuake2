//
// Lightmaps.hpp
//  Classic Quake2 lightmaps.
//
#pragma once

#include "Common.hpp"
#include "Pool.hpp"

// Quake includes
#include "client/ref.h"

namespace MrQ2
{

struct ModelSurface;
class TextureImage;
class TextureStore;
class SpriteBatch;

enum class LightmapFormat : int
{
    kDefault    = 'D',
    kRedChannel = 'R',
    kRGBA       = 'C',
    kInvAlpha   = 'A',
};

constexpr int kLightmapBytesPerPixel = 4; // RGBA
constexpr int kMaxLightmapTextures   = 32;
constexpr int kLightBlockSize        = 34 * 34 * 3;
constexpr float kDLightCutoff        = 64.0f;

// Size in pixels of the lightmap atlases.
constexpr int kLightmapTextureWidth  = 512;
constexpr int kLightmapTextureHeight = 512;

struct LmImageBuffer
{
    ColorRGBA32 pixels[kLightmapTextureWidth * kLightmapTextureHeight];
};

using LmImageBufferPool = Pool<LmImageBuffer, 2>;

/*
===============================================================================

    LightmapManager

===============================================================================
*/
class LightmapManager final
{
public:

    static void Init(TextureStore & tex_store);
    static void Shutdown();
    static void Update();

    static void BeginRegistration();
    static void EndRegistration();

	// Build static surface lightmap.
    static void BeginBuildLightmaps();
    static void CreateSurfaceLightmap(ModelSurface * surf);
    static void FinishBuildLightmaps();

    // Updates dynamic lightmaps/dlights.
    static const TextureImage * UpdateSurfaceLightmap(const ModelSurface * surf, const int lightmap_index, const lightstyle_t * lightstyles,
                                                      const dlight_t * dlights, const int num_dlights, const int frame_num,
                                                      const bool update_surf_cache, const bool dynamic_lightmap);

    // Render each lightmap on screen as an overlay for debugging.
    static void DebugDisplayTextures(SpriteBatch & batch, const int scr_w, const int scr_h);

    // Get static lightmap texture for rendering.
    static const TextureImage * LightmapAtIndex(const size_t index)
    {
        MRQ2_ASSERT(index < ArrayLength(sm_static_lightmaps));
        MRQ2_ASSERT(sm_static_lightmaps[index] != nullptr);
        return sm_static_lightmaps[index];
    }

    // Debug counters.
    static int sm_static_lightmap_updates;
    static int sm_dynamic_lightmap_updates;
    static int sm_num_lightmaps_buffers;

private:

    //
    // Each static lightmap is paired with a dynamic lightmap.
    // Static lightmaps are created from an atlas of tiny light
    // blocks allocated using the sm_allocated_blocks[] map.
    // Dynamic lightmaps are only update for dynamic lights.
    //

    static void ResetBlocks();
    static bool AllocBlock(const int w, const int h, int * x, int * y);
    static void NextLightmapTexture(const bool new_buffers);

    static int sm_lightmap_count;
    static TextureStore * sm_tex_store;

    static const TextureImage * sm_static_lightmaps[kMaxLightmapTextures];
    static const TextureImage * sm_dynamic_lightmaps[kMaxLightmapTextures];

    static LmImageBuffer * sm_static_lightmap_buffers[kMaxLightmapTextures];
    static LmImageBuffer * sm_dynamic_lightmap_buffers[kMaxLightmapTextures];

    static bool sm_dirtied_static_lightmaps[kMaxLightmapTextures];
    static bool sm_dirtied_dynamic_lightmaps[kMaxLightmapTextures];

    static LmImageBufferPool sm_lightmap_buffer_pool;
    static int sm_allocated_blocks[kLightmapTextureWidth];
};

} // MrQ2
