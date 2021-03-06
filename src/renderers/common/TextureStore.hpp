//
// TextureStore.hpp
//  Generic texture/image loading and registration for all render back-ends.
//
#pragma once

#include "Common.hpp"
#include "Pool.hpp"
#include "Array.hpp"
#include "RenderInterface.hpp"

namespace MrQ2
{

// Real width/height of a cinematic frame.
constexpr int kQuakeCinematicImgSize = 256;

// Size in entries (u32s) of the game palettes.
constexpr int kQuakePaletteSize = 256;

// Defined by ModelStructs.hpp, but we only need a forward decl here.
struct ModelSurface;

/*
===============================================================================

    TextureType - type tag for textures/images (used internally by Quake2)

===============================================================================
*/
enum class TextureType : std::uint8_t
{
    kSkin,   // Usually PCX          (mipmaps=yes)
    kSprite, // Usually PCX          (mipmaps=yes)
    kWall,   // WALL/miptex_t format (mipmaps=yes)
    kSky,    // PCX or TGA           (mipmaps=yes)
    kPic,    // Usually PCX          (mipmaps=no)
    kLightmap,

    // Number of items in the enum - not a valid texture type.
    kCount
};

/*
===============================================================================

    TextureImage

===============================================================================
*/
class TextureImage final
{
    friend class TextureStore;
    friend class ModelStore;
    friend class LightmapManager;

public:

    static constexpr int kMaxMipLevels  = 8; // Level 0 is the base texture, 7 mipmaps in total.
    static constexpr int kBytesPerPixel = 4; // All textures are RGBA_U8

    // Disallow copy.
    TextureImage(const TextureImage &) = delete;
    TextureImage & operator=(const TextureImage &) = delete;

    ~TextureImage()
    {
        m_texture.Shutdown();

        // Memory is owned by the TextureImage unless it is using the scrap atlas (or a lightmap atlas).
        if (!m_is_scrap_image)
        {
            if (m_mip_levels.base_pixels != nullptr)
            {
                MemFreeTracked(m_mip_levels.base_pixels, m_mip_levels.base_memory, MemTag::kTextures);
            }

            if (m_mip_levels.mip_pixels != nullptr)
            {
                MemFreeTracked(m_mip_levels.mip_pixels, m_mip_levels.mip_memory, MemTag::kTextures);
            }
        }
    }

    const PathName & Name() const { return m_name; }
    const Texture  & BackendTexture() const { return m_texture; }
    TextureType Type() const { return m_type; }

    // Scrap atlas
    bool IsScrapImage() const { return m_is_scrap_image; }
    Vec2u16 ScrapUV0()  const { return m_scrap_coords.uv0; }
    Vec2u16 ScrapUV1()  const { return m_scrap_coords.uv1; }

    // Draw by texture linked list used by the world renderer
    void SetDrawChainPtr(const ModelSurface * p) const { m_draw_chain = p; }
    const ModelSurface * DrawChainPtr() const { return m_draw_chain; }

    // Mipmaps
    bool SupportsMipMaps() const { return m_type < TextureType::kPic; }
    bool HasMipMaps() const { return m_mip_levels.num_levels > 1; }
    uint32_t NumMipMapLevels() const { return m_mip_levels.num_levels; }

    const ColorRGBA32 * BasePixels() const
    {
        return reinterpret_cast<const ColorRGBA32 *>(m_mip_levels.base_pixels);
    }

    const ColorRGBA32 * MipMapPixels(const uint32_t mip_level) const
    {
        MRQ2_ASSERT(mip_level < m_mip_levels.num_levels);
        if (mip_level == 0)
        {
            return reinterpret_cast<const ColorRGBA32 *>(m_mip_levels.base_pixels);
        }
        else
        {
            MRQ2_ASSERT(m_mip_levels.mip_pixels != nullptr);
            return reinterpret_cast<const ColorRGBA32 *>(m_mip_levels.mip_pixels + m_mip_levels.offsets_to_mip_pixels[mip_level]);
        }
    }

    Vec2u16 MipMapDimensions(const uint32_t mip_level) const
    {
        MRQ2_ASSERT(mip_level < m_mip_levels.num_levels);
        return m_mip_levels.dimensions[mip_level];
    }

    int Width(const uint32_t mip_level = 0) const
    {
        MRQ2_ASSERT(mip_level < m_mip_levels.num_levels);
        return m_mip_levels.dimensions[mip_level].x;
    }

    int Height(const uint32_t mip_level = 0) const
    {
        MRQ2_ASSERT(mip_level < m_mip_levels.num_levels);
        return m_mip_levels.dimensions[mip_level].y;
    }

    int OriginalWidth() const
    {
        MRQ2_ASSERT(!m_is_scrap_image);
        return m_original_dimensions.x;
    }

    int OriginalHeight() const
    {
        MRQ2_ASSERT(!m_is_scrap_image);
        return m_original_dimensions.y;
    }

private:

    // Initialize with a single mipmap level (level 0)
    TextureImage(const ColorRGBA32 * const mip0_pixels, const uint32_t registration_number, const TextureType type, const bool scrap,
                 const uint32_t mip0_width, const uint32_t mip0_height, const Vec2u16 scrap_uv0, const Vec2u16 scrap_uv1, const char * const tex_name)
        : m_name{ tex_name }
        , m_reg_num{ registration_number }
        , m_type{ type }
        , m_is_scrap_image{ scrap }
    {
        MRQ2_ASSERT(mip0_width  <= UINT16_MAX);
        MRQ2_ASSERT(mip0_height <= UINT16_MAX);

        m_mip_levels.num_levels      = 1;
        m_mip_levels.base_memory     = (mip0_width * mip0_height * kBytesPerPixel);
        m_mip_levels.base_pixels     = reinterpret_cast<const uint8_t *>(mip0_pixels); // NOTE: Takes ownership of the memory
        m_mip_levels.dimensions[0].x = static_cast<uint16_t>(mip0_width);
        m_mip_levels.dimensions[0].y = static_cast<uint16_t>(mip0_height);

        if (m_is_scrap_image)
        {
            m_scrap_coords.uv0 = scrap_uv0;
            m_scrap_coords.uv1 = scrap_uv1;
        }
        else
        {
            m_scrap_coords = {};
            m_original_dimensions.x = static_cast<uint16_t>(mip0_width);
            m_original_dimensions.y = static_cast<uint16_t>(mip0_height);
        }
    }

    void SetHDOverrideOriginalSize(const uint32_t original_w, const uint32_t original_h)
    {
        MRQ2_ASSERT(!m_is_scrap_image);
        m_is_hd_override = true;
        m_original_dimensions.x = static_cast<uint16_t>(original_w);
        m_original_dimensions.y = static_cast<uint16_t>(original_h);
    }

    void GenerateMipMaps();

private:

    struct MipLevels
    {
        uint32_t        num_levels;
        uint32_t        base_memory;
        uint32_t        mip_memory;
        const uint8_t * base_pixels; // Pixels for mip level 0 (the base level / original image)
        const uint8_t * mip_pixels;  // Memory for any additional mip levels. offsets_to_mip_pixels[1..num_levels] points to the beginning of each.
        Vec2u16         dimensions[kMaxMipLevels];
        uint32_t        offsets_to_mip_pixels[kMaxMipLevels];
    };

    struct ScrapCoords
    {
        Vec2u16 uv0;
        Vec2u16 uv1;
    };

    const PathName               m_name;                    // Texture filename/unique id (must be the first field - game code assumes this).
    MipLevels                    m_mip_levels{};            // Dimensions and offsets for each mipmap level. Always at least one.
    mutable const ModelSurface * m_draw_chain{ nullptr };   // For sort-by-texture world drawing.
    uint32_t                     m_reg_num{ 0 };            // Registration number, so we know if currently referenced by the level being played.
    const TextureType            m_type;                    // Types of textures used by Quake.
    const bool                   m_is_scrap_image;          // True if allocated from the scrap atlas.
    bool                         m_is_hd_override{ false }; // True if this texture was replaced by a higher quality override (m_original_dimensions contain the size of the original low-res image).
    union {
        Vec2u16                  m_original_dimensions;     // If not a scrap image reuse this to store the original mip0 width/height in case this image is an HD replacement.
        ScrapCoords              m_scrap_coords;            // Offsets into the scrap if this is allocate from the scrap, zero otherwise.
    };
    Texture                      m_texture;                 // Back-end renderer low-level texture object.
};

/*
===============================================================================

    TextureStore

===============================================================================
*/
class TextureStore final
{
public:

    static constexpr int kTexturePoolSize = 1024; // - in TextureImages
    static constexpr int kScrapSize       = 256;  // - width & height

    void Init(const RenderDevice & device);
    void Shutdown();

    void UploadScrapIfNeeded();
    bool ScrapIsDirty() const { return m_scrap_dirty; }
    const RenderDevice & Device() const { return *m_device; }

    // Registration sequence:
    void BeginRegistration(const char * const map_name);
    void EndRegistration();
    std::uint32_t RegistrationNum() const { return m_registration_num; }

    // Texture cache:
    const TextureImage * Find(const char * name, TextureType tt);       // Must be in cache, null otherwise
    const TextureImage * FindOrLoad(const char * name, TextureType tt); // Load if necessary

    // Lightmaps
    const TextureImage * AllocLightmap(const ColorRGBA32* pixels, const uint32_t w, const uint32_t h, const char * name);

    // Dumps all loaded textures to the correct paths, creating dirs as needed.
    void DumpAllLoadedTexturesToFile(const char * path, const char * file_type, bool dump_mipmaps) const;

    // TextureStore iteration:
    auto begin() { return m_teximages_cache.begin(); }
    auto end()   { return m_teximages_cache.end();   }

    // Resident textures:
    const TextureImage * tex_scrap        = nullptr;
    const TextureImage * tex_conchars     = nullptr;
    const TextureImage * tex_conback      = nullptr;
    const TextureImage * tex_backtile     = nullptr;
    const TextureImage * tex_white2x2     = nullptr;
    const TextureImage * tex_debug        = nullptr;
    const TextureImage * tex_cinframe     = nullptr;
    const TextureImage * tex_particle_dot = nullptr;
    const TextureImage * tex_particle_hd  = nullptr;

    // Global palette access.
    static ColorRGBA32 * CinematicPalette()          { return sm_cinematic_palette; }
    static const ColorRGBA32 * GlobalPalette()       { return sm_global_palette;    }
    static ColorRGBA32 ColorForIndex(const Color8 c) { return sm_global_palette[c]; }

    // palette=nullptr sets back the global palette.
    static void SetCinematicPaletteFromRaw(const std::uint8_t * palette);

private:

    TextureImage * CreateCinematicTexture();
    TextureImage * CreateScrapTexture(const uint32_t size, const ColorRGBA32 * pixels);
    TextureImage * CreateTexture(const ColorRGBA32 * pixels, uint32_t reg_num, TextureType tt, bool from_scrap,
                                 uint32_t w, uint32_t h, Vec2u16 scrap0, Vec2u16 scrap1, const char * name);

    void DestroyTexture(TextureImage * tex);
    void DestroyAllLoadedTextures();

    // Reference all the default 'tex_' TextureImages and create the scrap (if needed).
    void TouchResidentTextures();

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
        int * allocated{ nullptr };      // Allocated space map
        ColorRGBA32 * pixels{ nullptr }; // RGBA pixels

        void Init()
        {
            // Allocate zero-initialized arrays

            size_t size = sizeof(int) * kScrapSize;
            allocated = (int *)MemAllocTracked(size, MemTag::kTextures);
            std::memset(allocated, 0, size);

            size = sizeof(ColorRGBA32) * (kScrapSize * kScrapSize);
            pixels = (ColorRGBA32 *)MemAllocTracked(size, MemTag::kTextures);
            std::memset(pixels, 0, size);
        }

        void Shutdown()
        {
            size_t size = sizeof(int) * kScrapSize;
            MemFreeTracked(allocated, size, MemTag::kTextures);
            allocated = nullptr;

            size = sizeof(ColorRGBA32) * (kScrapSize * kScrapSize);
            MemFreeTracked(pixels, size, MemTag::kTextures);
            pixels = nullptr;
        }

        bool IsInitialised() const  { return allocated != nullptr; }
        constexpr static int Size() { return kScrapSize; }
    };

    const RenderDevice * m_device{ nullptr };

    // Scrap texture atlas to group small textures
    ScrapAtlas m_scrap;
    bool m_scrap_dirty{ false };

    // Loaded textures cache
    std::uint32_t m_registration_num{ 0 };
    Pool<TextureImage, kTexturePoolSize> m_teximages_pool{ MemTag::kTextures };
    FixedSizeArray<TextureImage *, kTexturePoolSize> m_teximages_cache;
};

// ============================================================================

bool TGALoadFromFile(const char * filename, ColorRGBA32 ** pic, int * width, int * height);
bool PNGLoadFromFile(const char * filename, ColorRGBA32 ** pic, int * width, int * height);
bool JPGLoadFromFile(const char * filename, ColorRGBA32 ** pic, int * width, int * height);
bool PCXLoadFromFile(const char * filename, Color8 ** pic, int * width, int * height, ColorRGBA32 * palette);

bool TGASaveToFile(const char * filename, int width, int height, const ColorRGBA32 * pixels);
bool PNGSaveToFile(const char * filename, int width, int height, const ColorRGBA32 * pixels);

constexpr int kNumTextureFilterOptions = 4;
extern const char * const TextureFilterOptionNames[kNumTextureFilterOptions];

// Packed color format is 0xAABBGGRR

inline constexpr ColorRGBA32 BytesToColor(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    return ((ColorRGBA32(a) << 24) | (ColorRGBA32(b) << 16) | (ColorRGBA32(g) << 8) | r);
}

inline void ColorBytes(const ColorRGBA32 c, std::uint8_t & r, std::uint8_t & g, std::uint8_t & b, std::uint8_t & a)
{
    r = std::uint8_t( c & 0x000000FF);
    g = std::uint8_t((c & 0x0000FF00) >> 8);
    b = std::uint8_t((c & 0x00FF0000) >> 16);
    a = std::uint8_t((c & 0xFF000000) >> 24);
}

inline void ColorFloats(const ColorRGBA32 c, float & r, float & g, float & b, float & a)
{
    std::uint8_t bR, bG, bB, bA;
    ColorBytes(c, bR, bG, bB, bA);
    r = bR / 255.0f;
    g = bG / 255.0f;
    b = bB / 255.0f;
    a = bA / 255.0f;
}

constexpr int kNumDebugColors = 25;
extern const ColorRGBA32 DebugColorsTable[kNumDebugColors];

ColorRGBA32 NextDebugColor();   // Sequential color from table starting at 0 and wrapping around
ColorRGBA32 RandomDebugColor(); // Randomized color from table index between 0..kNumDebugColors-1

// ============================================================================

} // MrQ2
