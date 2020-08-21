//
// Lightmaps.hpp
//  Classic Quake2 lightmaps.
//
#pragma once

#include "Common.hpp"

namespace MrQ2
{

struct ModelSurface;
class TextureImage;
class TextureStore;

enum class LightmapFormat : int
{
    kDefault    = 'D',
    kRedChannel = 'R',
    kRGBA       = 'C',
    kInvAlpha   = 'A',
};

constexpr int kLightmapBytesPerPixel = 4; // RGBA
constexpr int kMaxLightmapTextures   = 128;
constexpr int kLightBlockSize        = 34 * 34 * 3;
constexpr float kDLightCutoff        = 64.0f;

// Size in pixels of the lightmap atlases.
constexpr int kLightmapBlockWidth    = 128;
constexpr int kLightmapBlockHeight   = 128;

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

    static void BeginRegistration();
    static void EndRegistration();

    static void BeginBuildLightmaps();
    static void CreateSurfaceLightmap(ModelSurface * surf);
    static void FinishBuildLightmaps();

    static const TextureImage * LightmapAtIndex(const size_t index)
    {
        MRQ2_ASSERT(index < ArrayLength(sm_textures));
        MRQ2_ASSERT(sm_textures[index] != nullptr);
        return sm_textures[index];
    }

private:

    static bool AllocBlock(const int w, const int h, int * x, int * y);
    static void UploadBlock(const bool is_dynamic);
    static void Reset();
    static void DebugDumpToFile();

    static TextureStore *       sm_tex_store;
    static int                  sm_current_lightmap_texture;
    static int                  sm_allocated[kLightmapBlockWidth];
    static const TextureImage * sm_textures[kMaxLightmapTextures];
    static ColorRGBA32          sm_lightmap_buffer[kLightmapBlockWidth * kLightmapBlockHeight];
};

} // MrQ2
