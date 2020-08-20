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

    static LightmapManager & Instance() { return sm_instance; }

    void Init(TextureStore & tex_store);
    void Shutdown();

    void BeginRegistration();
    void EndRegistration();

    void BeginBuildLightmaps();
    void CreateSurfaceLightmap(ModelSurface * surf);
    void FinishBuildLightmaps();

    const TextureImage * LightmapAtIndex(const size_t index) const
    {
        MRQ2_ASSERT(index < ArrayLength(m_textures));
        MRQ2_ASSERT(m_textures[index] != nullptr);
        return m_textures[index];
    }

private:

    bool AllocBlock(const int w, const int h, int * x, int * y);
    void UploadBlock(const bool is_dynamic);
    void Reset();
    void DebugDumpToFile() const;

    int                  m_current_lightmap_texture{ 1 }; // Index 0 is reserved for the dynamic lightmap.
    int                  m_allocated[kLightmapBlockWidth] = {};
    const TextureImage * m_textures[kMaxLightmapTextures] = {};
    TextureStore *       m_tex_store{ nullptr };
    ColorRGBA32          m_lightmap_buffer[kLightmapBlockWidth * kLightmapBlockHeight] = {};

    static LightmapManager sm_instance;
};

} // MrQ2
