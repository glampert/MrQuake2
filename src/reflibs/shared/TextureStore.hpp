//
// TextureStore.hpp
//  Generic texture/image loading and registration for all render back-ends.
//
#pragma once

#include "RefShared.hpp"
#include "Memory.hpp"

#include <vector>
#include <memory>

namespace MrQ2
{

// Real width/height of a cinematic frame.
constexpr int kQuakeCinematicImgSize = 256;

// Size in entries (u32s) of the game palettes.
constexpr int kQuakePaletteSize = 256;

/*
===============================================================================

    TextureType - type tag for textures/images (used internally by Quake2)

===============================================================================
*/
enum class TextureType : std::uint8_t
{
    kSkin,   // Usually PCX
    kSprite, // Usually PCX
    kWall,   // Custom WALL format (miptex_t)
    kSky,    // PCX or TGA
    kPic,    // Usually PCX

    // Number of items in the enum - not a valid texture type.
    kCount
};

/*
===============================================================================

    TextureImage

===============================================================================
*/
class TextureImage
{
public:

    const ColorRGBA32 * pixels;     // Pointer to heap memory with image pixels (or into scrap atlas).
    std::uint32_t       reg_num;    // Registration num, so we know if currently referenced by the level being played.
    const TextureType   type;       // Types of textures used by Quake.
    const bool          from_scrap; // True if allocated from the scrap atlas.
    const int           width;      // Width in pixels.
    const int           height;     // Height in pixels.
    const Vec2u16       scrap_uv0;  // Offsets into the scrap if this is allocate from the scrap, zero otherwise.
    const Vec2u16       scrap_uv1;  // If not zero, this is a scrap image. In such case, use these instead of width & height.
    const PathName      name;       // Texture filename/unique id.

    TextureImage(const ColorRGBA32 * const pix, const std::uint32_t regn, const TextureType tt, const bool use_scrap,
                 const int w, const int h, const Vec2u16 scrap0, const Vec2u16 scrap1, const char * const tex_name)
        : pixels{ pix }
        , reg_num{ regn }
        , type{ tt }
        , from_scrap{ use_scrap }
        , width{ w }
        , height{ h }
        , scrap_uv0{ scrap0 }
        , scrap_uv1{ scrap1 }
        , name{ tex_name }
    {
    }

    // Disallow copy.
    TextureImage(const TextureImage &) = delete;
    TextureImage & operator=(const TextureImage &) = delete;

protected:

    // The back-end renderer uses non-virtual inheritance.
    ~TextureImage()
    {
        if (!from_scrap && pixels != nullptr)
        {
            MemFreeTracked(pixels, (width * height * 4), MemTag::kTextures);
        }
    }
};

/*
===============================================================================

    TextureStore

===============================================================================
*/
class TextureStore
{
public:

    static constexpr int kTexturePoolSize = 1024;

    TextureStore();
    virtual ~TextureStore();

    // Registration sequence:
    virtual void BeginRegistration();
    virtual void EndRegistration();
    std::uint32_t RegistrationNum() const { return m_registration_num; }

    // Texture cache:
    const TextureImage * Find(const char * name, TextureType tt);       // Must be in cache, null otherwise
    const TextureImage * FindOrLoad(const char * name, TextureType tt); // Load if necessary

    // Dumps all loaded textures to the correct paths, creating dirs as needed.
    void DumpAllLoadedTexturesToFile(const char * path, const char * file_type) const;

    // Resident textures:
    static constexpr int kScrapSize   = 512; // - width & height
    const TextureImage * tex_scrap    = nullptr;
    const TextureImage * tex_conchars = nullptr;
    const TextureImage * tex_conback  = nullptr;
    const TextureImage * tex_backtile = nullptr;
    const TextureImage * tex_white2x2 = nullptr;
    const TextureImage * tex_cinframe = nullptr;

    // Global palette access.
    static ColorRGBA32 * CinematicPalette()          { return sm_cinematic_palette; }
    static const ColorRGBA32 * GlobalPalette()       { return sm_global_palette;    }
    static ColorRGBA32 ColorForIndex(const Color8 c) { return sm_global_palette[c]; }

    // Nullptr sets the global palette.
    static void SetCinematicPaletteFromRaw(const std::uint8_t * palette);

protected:

    // Back-end hooks:
    virtual TextureImage * CreateScrap(int size, const ColorRGBA32 * pix) = 0;
    virtual TextureImage * CreateTexture(const ColorRGBA32 * pix, std::uint32_t regn, TextureType tt, bool use_scrap,
                                         int w, int h, Vec2u16 scrap0, Vec2u16 scrap1, const char * name) = 0;
    virtual void DestroyTexture(TextureImage * tex) = 0;

    // So the back end can cleanup on exit.
    void DestroyAllLoadedTextures();

    // Reference all the default 'tex_' TextureImages and create the scrap (if needed).
    void TouchResidentTextures();

private:

    static const char * NameFixup(const char * in, TextureType tt);

    TextureImage * LoadPCXImpl(const char * name, TextureType tt);
    TextureImage * LoadTGAImpl(const char * name, TextureType tt);
    TextureImage * LoadWALImpl(const char * name);

    TextureImage * ScrapTryAlloc8Bit(const Color8 * pic8, int width, int height,
                                     const char * name, TextureType tt);

    TextureImage * Common8BitTexSetup(const Color8 * pic8, int width, int height,
                                      const char * name, TextureType tt);

    static void UnPalettize8To32(int width, int height, const Color8 * pic8in,
                                 const ColorRGBA32 * palette, ColorRGBA32 * pic32out);

private:

    // Palette used to expand the 8bits textures to RGBA32.
    // Imported from the colormap.pcx file with imgdump.
    static const ColorRGBA32 sm_global_palette[256];

    // Palette provided by the game to expand 8bit cinematic frames.
    static ColorRGBA32 sm_cinematic_palette[256];

    // Scrap allocation - AKA texture atlas.
    // Useful to group small textures into a larger one,
    // reducing the number of texture switches when rendering.
    struct ScrapAtlas
    {
        std::unique_ptr<int[]>         allocated; // Allocated space map
        std::unique_ptr<ColorRGBA32[]> pixels;    // RGBA pixels

        ScrapAtlas() // Allocate zero-initialized arrays
            : allocated{ new(MemTag::kTextures) int[kScrapSize]{} }
            , pixels{ new(MemTag::kTextures) ColorRGBA32[kScrapSize * kScrapSize]{} }
        {}

        constexpr static int Size() { return kScrapSize; }
    };

    // Scrap texture atlas to group small textures 
    ScrapAtlas m_scrap;
    bool m_scrap_inited = false;

    // Loaded textures cache
    std::uint32_t m_registration_num = 0;
    std::vector<TextureImage *> m_teximages_cache;
};

// ============================================================================

bool TGALoadFromFile(const char * filename, ColorRGBA32 ** pic, int * width, int * height);
bool PCXLoadFromFile(const char * filename, Color8 ** pic, int * width, int * height, ColorRGBA32 * palette);

// Packed color format is 0xAABBGGRR

inline constexpr ColorRGBA32 BytesToColor(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    return ((ColorRGBA32(a) << 24) | (ColorRGBA32(b) << 16) | (ColorRGBA32(g) << 8) | r);
}

inline void ColorBytes(const ColorRGBA32 c, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b, std::uint8_t& a)
{
    r = std::uint8_t( c & 0x000000FF);
    g = std::uint8_t((c & 0x0000FF00) >> 8);
    b = std::uint8_t((c & 0x00FF0000) >> 16);
    a = std::uint8_t((c & 0xFF000000) >> 24);
}

constexpr int kNumDebugColors = 25;
extern const ColorRGBA32 DebugColorsTable[kNumDebugColors];

ColorRGBA32 NextDebugColor();   // Sequential color from table starting at 0 and wrapping around
ColorRGBA32 RandomDebugColor(); // Randomized color from table index between 0..kNumDebugColors-1

// ============================================================================

} // MrQ2
