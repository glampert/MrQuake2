//
// TextureStore.cpp
//  Generic texture/image loading and registration for all render back-ends.
//

#include "TextureStore.hpp"
#include "Lightmaps.hpp"
#include <algorithm>
#include <random>

// Quake includes
#include "common/q_common.h"
#include "common/q_files.h"

///////////////////////////////////////////////////////////////////////////////
// STB memory hooks
///////////////////////////////////////////////////////////////////////////////

#define Stb_AllocMagic_64(a, b, c, d, e, f, g, h)                                                    \
    (((uint64_t)(a) << 0)  | ((uint64_t)(b) << 8)  | ((uint64_t)(c) << 16) | ((uint64_t)(d) << 24) | \
     ((uint64_t)(e) << 32) | ((uint64_t)(f) << 40) | ((uint64_t)(g) << 48) | ((uint64_t)(h) << 56))

struct Stb_AllocHeader
{
    static constexpr uint64_t kAlignment = 16;
    static constexpr uint64_t kMagic = Stb_AllocMagic_64('S', 'T', 'B', 'I', 'M', 'A', 'G', 'E');

    uint64_t size;
    uint64_t magic;
};

static inline void * Stb_MemAlloc(const size_t size_bytes)
{
    const size_t alloc_size = size_bytes + sizeof(Stb_AllocHeader) + Stb_AllocHeader::kAlignment;
    static_assert((sizeof(Stb_AllocHeader) % Stb_AllocHeader::kAlignment) == 0);

    void * memory = MrQ2::MemAllocTracked(alloc_size, MrQ2::MemTag::kTextures);
    void * aligned_ptr = MrQ2::AlignPtr(memory, Stb_AllocHeader::kAlignment);

    auto * header = static_cast<Stb_AllocHeader *>(aligned_ptr);
    header->size  = alloc_size;
    header->magic = Stb_AllocHeader::kMagic;

    void * user_memory = header + 1;
    return user_memory;
}

static inline void Stb_MemFree(const void * ptr)
{
    if (ptr != nullptr)
    {
        auto * header = reinterpret_cast<const Stb_AllocHeader *>(static_cast<const uint8_t *>(ptr) - sizeof(Stb_AllocHeader));

        MRQ2_ASSERT(header->size != 0);
        MRQ2_ASSERT(header->magic == Stb_AllocHeader::kMagic);

        MrQ2::MemFreeTracked(header, header->size, MrQ2::MemTag::kTextures);
    }
}

static inline void * Stb_MemReAlloc(const void * old_ptr, const size_t old_size, const size_t new_size)
{
    if (old_ptr == nullptr)
    {
        return Stb_MemAlloc(new_size);
    }

    auto * header = reinterpret_cast<const Stb_AllocHeader *>(static_cast<const uint8_t *>(old_ptr) - sizeof(Stb_AllocHeader));

    MRQ2_ASSERT(header->size != 0);
    MRQ2_ASSERT(header->magic == Stb_AllocHeader::kMagic);
    MRQ2_ASSERT(old_size == (header->size - sizeof(Stb_AllocHeader) - Stb_AllocHeader::kAlignment));

    void * new_ptr = Stb_MemAlloc(new_size);
    std::memcpy(new_ptr, old_ptr, old_size);
    Stb_MemFree(old_ptr);

    return new_ptr;
}

///////////////////////////////////////////////////////////////////////////////
// STB Image (stbi)
///////////////////////////////////////////////////////////////////////////////

#define STBI_ASSERT(expr) MRQ2_ASSERT(expr)
#define STBI_MALLOC(size) Stb_MemAlloc(size)
#define STBI_REALLOC_SIZED(ptr, oldsz, newsz) Stb_MemReAlloc(ptr, oldsz, newsz)
#define STBI_FREE(ptr) Stb_MemFree(ptr)
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "external/stb/stb_image.h"

///////////////////////////////////////////////////////////////////////////////
// STB Image Write (stbiw)
///////////////////////////////////////////////////////////////////////////////

#define STBIW_ASSERT(expr) MRQ2_ASSERT(expr)
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "external/stb/stb_image_write.h"

///////////////////////////////////////////////////////////////////////////////
// STB Image Resize (stbir)
///////////////////////////////////////////////////////////////////////////////

#define STBIR_ASSERT(expr) MRQ2_ASSERT(expr)
#define STBIR_MALLOC(size, context) Stb_MemAlloc(size)
#define STBIR_FREE(ptr, context) Stb_MemFree(ptr)
#define STB_IMAGE_RESIZE_STATIC
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "external/stb/stb_image_resize.h"

///////////////////////////////////////////////////////////////////////////////

namespace MrQ2
{

// Verbose debugging
constexpr bool kLogLoadTextures = false;
constexpr bool kLogFindTextures = false;

const char * const TextureFilterOptionNames[kNumTextureFilterOptions] = {
    "nearest",
    "bilinear",
    "trilinear",
    "anisotropic",
};

static const char * const TextureType_Strings[] = {
    "Skin",
    "Sprite",
    "Wall",
    "Sky",
    "Pic",
    "Lightmap",
};

static_assert(ArrayLength(TextureType_Strings) == unsigned(TextureType::kCount), "Update this if the enum changes!");

///////////////////////////////////////////////////////////////////////////////
// TextureImage
///////////////////////////////////////////////////////////////////////////////

static void MipDebugBorder(const uint32_t mip, const uint32_t w, const uint32_t h, uint8_t * pixels)
{
    // Add a one pixel colored border around each mip
    auto * rgba_pixels = reinterpret_cast<ColorRGBA32 *>(pixels);
    const ColorRGBA32 c = DebugColorsTable[mip];
    size_t x, y;

    // top row
    for (x = 0, y = 0; x < w; ++x)
    {
        rgba_pixels[x + (y * w)] = c;
    }

    // bottom row
    for (x = 0, y = h - 1; x < w; ++x)
    {
        rgba_pixels[x + (y * w)] = c;
    }

    // left column
    for (x = 0, y = 0; y < h; ++y)
    {
        rgba_pixels[x + (y * w)] = c;
    }

    // right column
    for (x = w - 1, y = 0; y < h; ++y)
    {
        rgba_pixels[x + (y * w)] = c;
    }
}

///////////////////////////////////////////////////////////////////////////////

void TextureImage::GenerateMipMaps()
{
    const bool no_mipmaps    = Config::r_no_mipmaps.IsSet();
    const bool debug_mipmaps = Config::r_debug_mipmaps.IsSet();

    if (no_mipmaps)
    {
        return;
    }

    if (Width() == 1 && Height() == 1)
    {
        // If the base surface happens to be a 1x1 pixels image, then we can't subdivide it any further.
        // A 2x2 image can still generate one 1x1 mipmap level.
        return;
    }

    // All sub-surface mipmaps will be allocated in a contiguous
    // block of memory. Align the start of each portion belonging
    // to a surface to 16 bytes.
    constexpr uint32_t alignment = 16;

    // Initial image is the base surface (mipmap level = 0).
    // Always use the initial image to generate all mipmaps
    // to avoid propagating errors.
    const uint32_t initial_width  = Width();
    const uint32_t initial_height = Height();
    const uint8_t * const base_image_pixels = m_mip_levels.base_pixels;

    if (debug_mipmaps)
    {
        // Add the debug border to the base texture as well.
        MipDebugBorder(0, initial_width, initial_height, const_cast<uint8_t *>(base_image_pixels));
    }

    uint32_t target_width  = initial_width;
    uint32_t target_height = initial_height;
    uint32_t mipmap_count  = 1; // Mip 0 is the initial image.
    uint32_t mipmap_memory = 0;

    // First, figure out how much memory to allocate.
    // Stop when any of the dimensions reach 1.
    while (mipmap_count != kMaxMipLevels)
    {
        target_width  = std::max(1u, target_width  / 2);
        target_height = std::max(1u, target_height / 2);

        mipmap_memory += ((target_width * target_height * kBytesPerPixel) + alignment);
        mipmap_count++;

        if (target_width == 1 && target_height == 1)
        {
            break;
        }
    }

    // Allocate exact memory needed:
    uint8_t * const mipmap_pixels = static_cast<uint8_t *>(MemAllocTracked(mipmap_memory, MemTag::kTextures));

    // Mipmap surface generation:
    uint8_t * mip_data_start_ptr = mipmap_pixels;
    target_width  = initial_width;
    target_height = initial_height;
    mipmap_count  = 1;

    while (mipmap_count != kMaxMipLevels)
    {
        target_width  = std::max(1u, target_width  / 2);
        target_height = std::max(1u, target_height / 2);

        stbir_resize_uint8(
            // Source:
            base_image_pixels,
            initial_width,
            initial_height,
            0,
            // Destination:
            mip_data_start_ptr,
            target_width,
            target_height,
            0,
            // NumChannels (RGBA):
            4);

        if (debug_mipmaps) // Add a debug border to each mipmap
        {
            MipDebugBorder(mipmap_count, target_width, target_height, mip_data_start_ptr);
        }

        m_mip_levels.offsets_to_mip_pixels[mipmap_count] = static_cast<uint32_t>(mip_data_start_ptr - mipmap_pixels);
        m_mip_levels.dimensions[mipmap_count].x = static_cast<uint16_t>(target_width);
        m_mip_levels.dimensions[mipmap_count].y = static_cast<uint16_t>(target_height);

        mip_data_start_ptr += (target_width * target_height * kBytesPerPixel);
        mip_data_start_ptr = static_cast<uint8_t *>(AlignPtr(mip_data_start_ptr, alignment));
        mipmap_count++;

        if (target_width == 1 && target_height == 1)
        {
            break;
        }
    }

    m_mip_levels.num_levels = mipmap_count;
    m_mip_levels.mip_memory = mipmap_memory;
    m_mip_levels.mip_pixels = mipmap_pixels;
}

///////////////////////////////////////////////////////////////////////////////
// TextureStore
///////////////////////////////////////////////////////////////////////////////

ColorRGBA32 TextureStore::sm_cinematic_palette[256];

///////////////////////////////////////////////////////////////////////////////

void TextureStore::Init(const RenderDevice & device)
{
    MRQ2_ASSERT(m_device == nullptr);
    m_device = &device;

    m_teximages_cache.reserve(kTexturePoolSize);
    MemTagsTrackAlloc(m_teximages_cache.capacity() * sizeof(TextureImage *), MemTag::kTextures);

    // Load the default resident textures now
    TouchResidentTextures();

    GameInterface::Printf("TextureStore initialized.");

    LightmapManager::Init(*this);
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::Shutdown()
{
    LightmapManager::Shutdown();

    tex_scrap    = nullptr;
    tex_conchars = nullptr;
    tex_conback  = nullptr;
    tex_backtile = nullptr;
    tex_white2x2 = nullptr;
    tex_debug    = nullptr;
    tex_cinframe = nullptr;
    tex_particle = nullptr;

    DestroyAllLoadedTextures();
    m_teximages_cache.shrink_to_fit();
    m_teximages_pool.Drain();
    m_scrap.Shutdown();

    m_registration_num = 0;
    m_scrap_dirty      = false;
    m_device           = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::UploadScrapIfNeeded()
{
    if (m_scrap_dirty)
    {
        MRQ2_ASSERT(tex_scrap != nullptr);

        constexpr uint32_t  num_mip_levels = 1;
        const ColorRGBA32 * mip_init_data[num_mip_levels]  = { tex_scrap->BasePixels() };
        const Vec2u16       mip_dimensions[num_mip_levels] = { tex_scrap->MipMapDimensions(0) };

        TextureUpload upload_info{};
        upload_info.texture  = &tex_scrap->m_texture;
        upload_info.is_scrap = true;
        upload_info.mipmaps.num_mip_levels = num_mip_levels;
        upload_info.mipmaps.mip_init_data  = mip_init_data;
        upload_info.mipmaps.mip_dimensions = mip_dimensions;
        m_device->UploadContext().UploadTextureImmediate(upload_info);

        m_scrap_dirty = false;
    }
}

///////////////////////////////////////////////////////////////////////////////

const TextureImage * TextureStore::AllocLightmap(const ColorRGBA32* pixels, const uint32_t w, const uint32_t h, const char * name)
{
    MRQ2_ASSERT((w + h) > 0);
    MRQ2_ASSERT(pixels != nullptr);
    MRQ2_ASSERT(name != nullptr);
    MRQ2_ASSERT(m_device != nullptr);

    // Create a one mip level texture tagged as a scrap image.
    TextureImage * new_lightmap = m_teximages_pool.Allocate();
    ::new(new_lightmap) TextureImage{ pixels, m_registration_num, TextureType::kLightmap, /*scrap =*/true, w, h, {}, {}, name };

    constexpr uint32_t  num_mip_levels = 1;
    const ColorRGBA32 * mip_init_data[num_mip_levels]  = { new_lightmap->BasePixels() };
    const Vec2u16       mip_dimensions[num_mip_levels] = { new_lightmap->MipMapDimensions(0) };

    new_lightmap->m_texture.Init(*m_device, TextureType::kLightmap, /*is_scrap =*/true, mip_init_data, mip_dimensions, num_mip_levels, new_lightmap->Name().CStr());
    m_teximages_cache.push_back(new_lightmap);

    return new_lightmap;
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStore::CreateCinematicTexture()
{
    MRQ2_ASSERT(m_device != nullptr);

    constexpr uint32_t Dims = kQuakeCinematicImgSize;
    auto * pixels = new(MemTag::kTextures) ColorRGBA32[Dims * Dims];
    std::memset(pixels, 0, sizeof(ColorRGBA32) * (Dims * Dims));

    // Create a one mip level texture tagged as a scrap image.
    TextureImage * new_cinframe = m_teximages_pool.Allocate();
    ::new(new_cinframe) TextureImage{ pixels, m_registration_num, TextureType::kPic, /*scrap =*/true, Dims, Dims, {}, {}, "pics/cinframe.pcx" };

    constexpr uint32_t  num_mip_levels = 1;
    const ColorRGBA32 * mip_init_data[num_mip_levels]  = { new_cinframe->BasePixels() };
    const Vec2u16       mip_dimensions[num_mip_levels] = { new_cinframe->MipMapDimensions(0) };

    new_cinframe->m_texture.Init(*m_device, TextureType::kPic, /*is_scrap =*/true, mip_init_data, mip_dimensions, num_mip_levels, new_cinframe->Name().CStr());
    return new_cinframe;
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStore::CreateScrapTexture(const uint32_t size, const ColorRGBA32 * pixels)
{
    MRQ2_ASSERT(m_device != nullptr);

    TextureImage * new_scrap = m_teximages_pool.Allocate();
    ::new(new_scrap) TextureImage{ pixels, m_registration_num, TextureType::kPic, /*scrap =*/true,
                                   size, size, Vec2u16{0,0}, Vec2u16{ std::uint16_t(size), std::uint16_t(size) },
                                   "pics/scrap.pcx" };

    constexpr uint32_t  num_mip_levels = 1;
    const ColorRGBA32 * mip_init_data[num_mip_levels]  = { new_scrap->BasePixels() };
    const Vec2u16       mip_dimensions[num_mip_levels] = { new_scrap->MipMapDimensions(0) };

    new_scrap->m_texture.Init(*m_device, TextureType::kPic, /*is_scrap =*/true, mip_init_data, mip_dimensions, num_mip_levels, new_scrap->Name().CStr());
    return new_scrap;
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStore::CreateTexture(const ColorRGBA32 * pixels, uint32_t reg_num, TextureType tt, bool from_scrap,
                                           uint32_t w, uint32_t h, Vec2u16 scrap0, Vec2u16 scrap1, const char * const name)
{
    TextureImage * new_tex = m_teximages_pool.Allocate();
    ::new(new_tex) TextureImage{ pixels, reg_num, tt, from_scrap, w, h, scrap0, scrap1, name };

    if (from_scrap)
    {
        MRQ2_ASSERT(tex_scrap != nullptr);
        new_tex->m_texture.Init(tex_scrap->m_texture);
        m_scrap_dirty = true; // Upload the scrap texture on next opportunity
    }
    else
    {
        MRQ2_ASSERT(m_device != nullptr);

        if (new_tex->SupportsMipMaps())
        {
            new_tex->GenerateMipMaps();
        }

        const uint32_t num_mip_levels = new_tex->NumMipMapLevels();
        MRQ2_ASSERT(num_mip_levels >= 1 && num_mip_levels <= TextureImage::kMaxMipLevels);

        const ColorRGBA32 * mip_init_data[TextureImage::kMaxMipLevels] = {};
        Vec2u16 mip_dimensions[TextureImage::kMaxMipLevels] = {};

        for (uint32_t mip = 0; mip < num_mip_levels; ++mip)
        {
            mip_init_data[mip]  = new_tex->MipMapPixels(mip);
            mip_dimensions[mip] = new_tex->MipMapDimensions(mip);
        }

        new_tex->m_texture.Init(*m_device, tt, /*is_scrap =*/false, mip_init_data, mip_dimensions, num_mip_levels, name);
    }

    return new_tex;
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::DestroyTexture(TextureImage * tex)
{
    Destroy(tex);
    m_teximages_pool.Deallocate(tex);
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::DestroyAllLoadedTextures()
{
    // Unconditionally free all textures
    for (TextureImage * tex : m_teximages_cache)
    {
        DestroyTexture(tex);
    }
    m_teximages_cache.clear();
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::DumpAllLoadedTexturesToFile(const char * const path, const char * const file_type, const bool dump_mipmaps) const
{
    MRQ2_ASSERT(path != nullptr && path[0] != '\0');
    MRQ2_ASSERT(file_type != nullptr && file_type[0] != '\0');

    char fullname[1024] = {};
    char filename[512] = {};

    if (std::strncmp(file_type, "tga", 3) == 0)
    {
        for (const TextureImage * tex : m_teximages_cache)
        {
            sprintf_s(fullname, "%s/%s.tga", path, tex->Name().CStrNoExt(filename));
            GameInterface::FS::CreatePath(fullname);

            if (!TGASaveToFile(fullname, tex->Width(), tex->Height(), tex->BasePixels()))
            {
                GameInterface::Printf("Failed to write image '%s'", fullname);
            }

            if (dump_mipmaps && tex->HasMipMaps())
            {
                for (uint32_t mip = 1; mip < tex->NumMipMapLevels(); ++mip)
                {
                    sprintf_s(fullname, "%s/%s_mip%u.tga", path, tex->Name().CStrNoExt(filename), mip);
                    if (!TGASaveToFile(fullname, tex->Width(mip), tex->Height(mip), tex->MipMapPixels(mip)))
                    {
                        GameInterface::Printf("Failed to write image '%s'", fullname);
                    }
                }
            }
        }
    }
    else if (std::strncmp(file_type, "png", 3) == 0)
    {
        for (const TextureImage * tex : m_teximages_cache)
        {
            sprintf_s(fullname, "%s/%s.png", path, tex->Name().CStrNoExt(filename));
            GameInterface::FS::CreatePath(fullname);

            if (!PNGSaveToFile(fullname, tex->Width(), tex->Height(), tex->BasePixels()))
            {
                GameInterface::Printf("Failed to write image '%s'", fullname);
            }

            if (dump_mipmaps && tex->HasMipMaps())
            {
                for (uint32_t mip = 1; mip < tex->NumMipMapLevels(); ++mip)
                {
                    sprintf_s(fullname, "%s/%s_mip%u.png", path, tex->Name().CStrNoExt(filename), mip);
                    if (!PNGSaveToFile(fullname, tex->Width(mip), tex->Height(mip), tex->MipMapPixels(mip)))
                    {
                        GameInterface::Printf("Failed to write image '%s'", fullname);
                    }
                }
            }
        }
    }
    else
    {
        GameInterface::Printf("Invalid file type '%s'", file_type);
    }
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::SetCinematicPaletteFromRaw(const std::uint8_t * const palette)
{
    // Set default game palette:
    if (palette == nullptr)
    {
        std::memcpy(sm_cinematic_palette, sm_global_palette, sizeof(ColorRGBA32) * kQuakePaletteSize);
    }
    else // Copy custom palette with alpha override:
    {
        std::uint8_t * dest = reinterpret_cast<std::uint8_t *>(sm_cinematic_palette);
        for (int i = 0; i < kQuakePaletteSize; ++i)
        {
            dest[(i * 4) + 0] = palette[(i * 3) + 0];
            dest[(i * 4) + 1] = palette[(i * 3) + 1];
            dest[(i * 4) + 2] = palette[(i * 3) + 2];
            dest[(i * 4) + 3] = 0xFF;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

constexpr int kCheckerDim     = 64;
constexpr int kCheckerSquares = 4;
constexpr int kCheckerSize    = kCheckerDim / kCheckerSquares;

// Generate a black and pink checkerboard pattern for the debug texture.
// Caller handles lifetime of the allocated buffer.
static ColorRGBA32 * MakeCheckerPattern()
{
    const ColorRGBA32 colors[] = {
        BytesToColor(255, 100, 255, 255), // pink
        BytesToColor(0, 0, 0, 255)        // black
    };

    auto * pixels = new(MemTag::kTextures) ColorRGBA32[kCheckerDim * kCheckerDim];

    for (int y = 0; y < kCheckerDim; ++y)
    {
        for (int x = 0; x < kCheckerDim; ++x)
        {
            const auto color_index = ((y / kCheckerSize) + (x / kCheckerSize)) % 2;
            pixels[x + (y * kCheckerDim)] = colors[color_index];
        }
    }

    return pixels;
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::TouchResidentTextures()
{
    // Create the scrap texture if needed
    if (!m_scrap.IsInitialised())
    {
        m_scrap.Init();
        m_teximages_cache.push_back(CreateScrapTexture(m_scrap.Size(), m_scrap.pixels));
    }

    // This texture is generated at runtime
    if (tex_white2x2 == nullptr)
    {
        constexpr uint32_t Dims = 2;

        auto * pixels = new(MemTag::kTextures) ColorRGBA32[Dims * Dims];
        std::memset(pixels, 0xFF, sizeof(ColorRGBA32) * (Dims * Dims));

        TextureImage * tex = CreateTexture(pixels, m_registration_num, TextureType::kPic, false,
                                           Dims, Dims, {}, {}, "pics/white2x2.pcx"); // with a fake filename
        m_teximages_cache.push_back(tex);
        tex_white2x2 = tex;
    }

    // Also generated dynamically
    if (tex_debug == nullptr)
    {
        TextureImage * tex = CreateTexture(MakeCheckerPattern(), m_registration_num, TextureType::kPic, false,
                                           kCheckerDim, kCheckerDim, {}, {}, "pics/debug.pcx"); // with a fake filename
        m_teximages_cache.push_back(tex);
        tex_debug = tex;
    }

    // Cinematic frame texture is also a resident buffer
    if (tex_cinframe == nullptr)
    {
        TextureImage * tex = CreateCinematicTexture();
        m_teximages_cache.push_back(tex);
        tex_cinframe = tex;
    }

    // Little dot for particles (8x8 white/alpha texture)
    if (tex_particle == nullptr)
    {
        int w = 0, h = 0;
        ColorRGBA32 * pixels = nullptr;

        if (Config::r_hd_particles.IsSet())
        {
            if (!PNGLoadFromFile("MrQ2/particle.png", &pixels, &w, &h))
            {
                GameInterface::Errorf("Failed to load high quality particle texture 'MrQ2/particle.png'");
            }
        }
        else // Classic Quake2 dot texture
        {
            constexpr int Dims = 8;
            const uint8_t dot_texture[Dims][Dims] = {
                { 0, 0, 0, 0, 0, 0, 0, 0 },
                { 0, 0, 1, 1, 0, 0, 0, 0 },
                { 0, 1, 1, 1, 1, 0, 0, 0 },
                { 0, 1, 1, 1, 1, 0, 0, 0 },
                { 0, 0, 1, 1, 0, 0, 0, 0 },
                { 0, 0, 0, 0, 0, 0, 0, 0 },
                { 0, 0, 0, 0, 0, 0, 0, 0 },
                { 0, 0, 0, 0, 0, 0, 0, 0 },
            };

            w = h = Dims;
            pixels = new(MemTag::kTextures) ColorRGBA32[Dims * Dims];

            for (int x = 0; x < Dims; ++x)
            {
                for (int y = 0; y < Dims; ++y)
                {
                    pixels[x + (y * Dims)] = BytesToColor(255, 255, 255, dot_texture[x][y] * 255);
                }
            }
        }

        TextureImage * tex = CreateTexture(pixels, m_registration_num, TextureType::kPic, false, w, h, {}, {}, "pics/particle.pcx");
        m_teximages_cache.push_back(tex);
        tex_particle = tex;
    }

    // Update the registration count for these:
    tex_scrap    = FindOrLoad("scrap",    TextureType::kPic);
    tex_conchars = FindOrLoad("conchars", TextureType::kPic);
    tex_conback  = FindOrLoad("conback",  TextureType::kPic);
    tex_backtile = FindOrLoad("backtile", TextureType::kPic);
    tex_white2x2 = FindOrLoad("white2x2", TextureType::kPic);
    tex_debug    = FindOrLoad("debug",    TextureType::kPic);
    tex_cinframe = FindOrLoad("cinframe", TextureType::kPic);
    tex_particle = FindOrLoad("particle", TextureType::kPic);
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::BeginRegistration(const char * const map_name)
{
    GameInterface::Printf("==== TextureStore::BeginRegistration ====");
    ++m_registration_num;

    // Reference them on every BeginRegistration so they will always have the most current timestamp.
    TouchResidentTextures();

    LightmapManager::BeginRegistration(map_name);
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::EndRegistration()
{
    GameInterface::Printf("==== TextureStore::EndRegistration ====");

    LightmapManager::EndRegistration();

    int num_textures_removed = 0;
    int num_lmaps_removed = 0;

    auto RemovePred = [this, &num_textures_removed, &num_lmaps_removed](TextureImage * tex)
    {
        if (tex->m_reg_num != m_registration_num)
        {
            if (tex->Type() == TextureType::kLightmap)
                ++num_lmaps_removed;

            DestroyTexture(tex);
            ++num_textures_removed;
            return true;
        }
        return false;
    };

    // "erase_if"
    auto first = m_teximages_cache.begin();
    auto last  = m_teximages_cache.end();
    m_teximages_cache.erase(std::remove_if(first, last, RemovePred), last);

    GameInterface::Printf("Freed %i unused textures (%i lightmaps).", num_textures_removed, num_lmaps_removed);
}

///////////////////////////////////////////////////////////////////////////////

const char * TextureStore::NameFixup(const char * const in, const TextureType tt)
{
    MRQ2_ASSERT(in != nullptr && in[0] != '\0');

    const char * tex_name = in;
    static char s_temp_name[1024];

    if (tt == TextureType::kPic)
    {
        // This is the same logic used by ref_gl.
        // If the name doesn't start with a path separator, its just the base filename,
        // like "conchars", otherwise the full file path is specified in the 'name' string.
        if (tex_name[0] != '/' && tex_name[0] != '\\')
        {
            sprintf_s(s_temp_name, "pics/%s.pcx", tex_name);
            tex_name = s_temp_name; // Override with local string
        }
        else
        {
            tex_name++; // Skip over path separator
        }
    }

    return tex_name;
}

///////////////////////////////////////////////////////////////////////////////

const TextureImage * TextureStore::Find(const char * const name, const TextureType tt)
{
    MRQ2_ASSERT(tt != TextureType::kCount);
    const char * tex_name = NameFixup(name, tt);

    if (kLogFindTextures)
    {
        GameInterface::Printf("TextureStore::Find('%s', %s)",
                              tex_name, TextureType_Strings[unsigned(tt)]);
    }

    // At least "X.ext"
    MRQ2_ASSERT(std::strlen(tex_name) >= 5);

    // Compare by hash, much cheaper.
    const std::uint32_t name_hash = PathName::CalcHash(tex_name);

    for (TextureImage * tex : m_teximages_cache)
    {
        // If name and type match, we are done.
        if ((name_hash == tex->Name().Hash()) && (tt == tex->Type()))
        {
            tex->m_reg_num = m_registration_num;
            return tex;
        }
    }

    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

const TextureImage * TextureStore::FindOrLoad(const char * const name, const TextureType tt)
{
    // Lookup the cache first:
    const TextureImage * tex = Find(name, tt);

    // Load 'n cache new if not found:
    if (tex == nullptr)
    {
        const char * tex_name = NameFixup(name, tt);
        const auto name_len = std::strlen(tex_name);

        if (kLogLoadTextures)
        {
            GameInterface::Printf("TextureStore::FindOrLoad('%s', %s)",
                                  tex_name, TextureType_Strings[unsigned(tt)]);
        }

        TextureImage * new_tex;
        if (std::strcmp(tex_name + name_len - 4, ".pcx") == 0)
        {
            new_tex = LoadPCXImpl(tex_name, tt);
        }
        else if (std::strcmp(tex_name + name_len - 4, ".wal") == 0)
        {
            new_tex = LoadWALImpl(tex_name);
        }
        else if (std::strcmp(tex_name + name_len - 4, ".tga") == 0)
        {
            new_tex = LoadTGAImpl(tex_name, tt);
        }
        else
        {
            GameInterface::Printf("WARNING: Unable to find image '%s' - unsupported file extension", tex_name);
            new_tex = nullptr;
        }

        if (new_tex != nullptr)
        {
            m_teximages_cache.push_back(new_tex);
        }
        tex = new_tex;
    }

    return tex;
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStore::LoadPCXImpl(const char * const name, const TextureType tt)
{
    TextureImage * tex = nullptr;
    Color8 * pic8;
    int width, height;

    if (!PCXLoadFromFile(name, &pic8, &width, &height, nullptr))
    {
        GameInterface::Printf("WARNING: Can't load PCX pic for '%s'", name);
        return nullptr;
    }

    // Try placing small images in the scrap atlas:
    if (tt == TextureType::kPic)
    {
        constexpr int kMaxSizeForScrapPlacement = 64; // in pixels, w & h
        if (width <= kMaxSizeForScrapPlacement && height <= kMaxSizeForScrapPlacement)
        {
            tex = ScrapTryAlloc8Bit(pic8, width, height, name, tt);
        }
    }

    // Atlas full or image too big, create a standalone texture:
    if (tex == nullptr)
    {
        tex = Common8BitTexSetup(pic8, width, height, name, tt);
    }

    // The palettized pcx image is no longer needed.
    MemFreeTracked(pic8, (width * height), MemTag::kTextures);
    return tex;
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStore::LoadTGAImpl(const char * const name, const TextureType tt)
{
    ColorRGBA32 * pic32;
    int width, height;

    if (!TGALoadFromFile(name, &pic32, &width, &height))
    {
        GameInterface::Printf("WARNING: Can't load TGA texture for '%s'", name);
        return nullptr;
    }

    // TGAs are always expanded to RGBA, so no extra conversion is needed.
    return CreateTexture(pic32, m_registration_num, tt, /*from_scrap =*/false, width, height, {0,0}, {0,0}, name);
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStore::LoadWALImpl(const char * const name)
{
    GameInterface::FS::ScopedFile file{ name };
    if (!file.IsLoaded())
    {
        GameInterface::Printf("WARNING: Can't load WAL texture for '%s'", name);
        return nullptr;
    }

    auto * wall = static_cast<const miptex_t *>(file.data_ptr);

    const int width  = wall->width;
    const int height = wall->height;
    const int offset = wall->offsets[0];
    auto * pic8 = reinterpret_cast<const Color8 *>(wall) + offset;

    return Common8BitTexSetup(pic8, width, height, name, TextureType::kWall);
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStore::ScrapTryAlloc8Bit(const Color8 * const pic8, const int width, const int height, const char * const name, const TextureType tt)
{
    MRQ2_ASSERT(width > 0 && height > 0);
    MRQ2_ASSERT(m_scrap.IsInitialised());

    // Adding a 2 pixels padding border around each side to avoid sampling artifacts.
    const int padded_width  = width  + 2;
    const int padded_height = height + 2;

    int sx = 0;
    int sy = 0;
    int best = m_scrap.Size();

    // Try to find a good fit in the atlas:
    for (int i = 0; i < m_scrap.Size() - padded_width; ++i)
    {
        int j, best2 = 0;
        for (j = 0; j < padded_width; ++j)
        {
            if (m_scrap.allocated[i + j] >= best)
            {
                break;
            }
            if (m_scrap.allocated[i + j] > best2)
            {
                best2 = m_scrap.allocated[i + j];
            }
        }

        if (j == padded_width) // This is a valid spot
        {
            sx = i;
            sy = best = best2;
        }
    }

    // No more room.
    if ((best + padded_height) > m_scrap.Size())
    {
        return nullptr;
    }

    // Managed to allocate, update the scrap:
    for (int i = 0; i < padded_width; ++i)
    {
        m_scrap.allocated[sx + i] = best + padded_height;
    }

    // Expand pic to RGBA:
    auto * pic32 = new(MemTag::kTextures) ColorRGBA32[width * height];
    UnPalettize8To32(width, height, pic8, sm_global_palette, pic32);

    // Copy the pixels into the scrap block:
    int k = 0;
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j, ++k)
        {
            m_scrap.pixels[(sy + i) * m_scrap.Size() + sx + j] = pic32[k];
        }
    }

    Vec2u16 uv0, uv1;
    uv0.x = std::uint16_t(sx);
    uv0.y = std::uint16_t(sy);
    uv1.x = std::uint16_t(sx + width);
    uv1.y = std::uint16_t(sy + height);

    // Pass ownership of the pixel data
    return CreateTexture(pic32, m_registration_num, tt, /*from_scrap =*/true, width, height, uv0, uv1, name);
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStore::Common8BitTexSetup(const Color8 * const pic8, const int width, const int height, const char * const name, const TextureType tt)
{
    MRQ2_ASSERT(width > 0 && height > 0);

    auto * pic32 = new(MemTag::kTextures) ColorRGBA32[width * height];
    UnPalettize8To32(width, height, pic8, sm_global_palette, pic32);

    // Pass ownership of the pixel data
    return CreateTexture(pic32, m_registration_num, tt, /*from_scrap =*/false, width, height, {0,0}, {0,0}, name);
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::UnPalettize8To32(const int width, const int height, const Color8 * const pic8in,
                                    const ColorRGBA32 * const palette, ColorRGBA32 * const pic32out)
{
    const int pixel_count = width * height;
    for (int i = 0; i < pixel_count; ++i)
    {
        int p = pic8in[i];
        pic32out[i] = palette[p];

        // Transparency algorithm adapted from GL_Upload8 in ref_gl/gl_image.c
        if (p == 255)
        {
            // Transparent, so scan around for another
            // color to avoid alpha fringes
            if (i > width && pic8in[i - width] != 255)
            {
                p = pic8in[i - width];
            }
            else if (i < pixel_count - width && pic8in[i + width] != 255)
            {
                p = pic8in[i + width];
            }
            else if (i > 0 && pic8in[i - 1] != 255)
            {
                p = pic8in[i - 1];
            }
            else if (i < pixel_count - 1 && pic8in[i + 1] != 255)
            {
                p = pic8in[i + 1];
            }
            else
            {
                p = 0;
            }

            // Copy RGB components:
            reinterpret_cast<Color8 *>(&pic32out[i])[0] = reinterpret_cast<const Color8 *>(&palette[p])[0];
            reinterpret_cast<Color8 *>(&pic32out[i])[1] = reinterpret_cast<const Color8 *>(&palette[p])[1];
            reinterpret_cast<Color8 *>(&pic32out[i])[2] = reinterpret_cast<const Color8 *>(&palette[p])[2];
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// PCX image loading helpers:
///////////////////////////////////////////////////////////////////////////////

// 'pic' and 'palette' can be null if you want to ignore one or the other.
// If 'palette' is not null, it must point to an array of 256 32bit integers.
// Mostly adapted from Q2 ref_gl/gl_image.c
bool PCXLoadFromMemory(const char * const filename, const Color8 * data, const int data_len,
                       Color8 ** pic, int * width, int * height, ColorRGBA32 * palette)
{
    const pcx_t * pcx        = reinterpret_cast<const pcx_t *>(data);
    const int manufacturer   = pcx->manufacturer;
    const int version        = pcx->version;
    const int encoding       = pcx->encoding;
    const int bits_per_pixel = pcx->bits_per_pixel;
    const int xmax           = pcx->xmax;
    const int ymax           = pcx->ymax;

    // Validate the image:
    if (manufacturer != 0x0A || version != 5 || encoding != 1 ||
        bits_per_pixel != 8  || xmax >= 640  || ymax >= 480)
    {
        GameInterface::Printf("Bad PCX file %s. Invalid header value(s)!", filename);
        return false;
    }

    // Get the palette:
    if (palette != nullptr)
    {
        constexpr int kPalSizeBytes = 768;
        std::uint8_t temp_pal[kPalSizeBytes];
        std::memcpy(temp_pal, reinterpret_cast<const std::uint8_t *>(pcx) + data_len - kPalSizeBytes, kPalSizeBytes);

        // Adjust the palette and fix byte order if needed:
        ColorRGBA32 c;
        int i, r, g, b;
        for (i = 0; i < 256; ++i)
        {
            r = temp_pal[(i * 3) + 0];
            g = temp_pal[(i * 3) + 1];
            b = temp_pal[(i * 3) + 2];
            c = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
            palette[i] = c;
        }
        palette[255] &= 0xFFFFFF; // 255 is transparent
    }

    if (width != nullptr)
    {
        *width = xmax + 1;
    }
    if (height != nullptr)
    {
        *height = ymax + 1;
    }

    if (pic == nullptr)
    {
        // Caller just wanted the palette.
        return true;
    }

    // Now alloc and read in the pixel data:
    auto * pix = new(MemTag::kTextures) Color8[(xmax + 1) * (ymax + 1)];
    *pic = pix;

    // Skip the header:
    data = &pcx->data;

    // Decode the RLE packets:
    int run_length;
    for (int y = 0; y <= ymax; y++, pix += xmax + 1)
    {
        for (int x = 0; x <= xmax;)
        {
            int data_byte = *data++;
            if ((data_byte & 0xC0) == 0xC0)
            {
                run_length = data_byte & 0x3F;
                data_byte  = *data++;
            }
            else
            {
                run_length = 1;
            }

            while (run_length-- > 0)
            {
                pix[x++] = data_byte;
            }
        }
    }

    if ((data - reinterpret_cast<const Color8 *>(pcx)) > data_len)
    {
        MemFreeTracked(*pic, (xmax + 1) * (ymax + 1), MemTag::kTextures);
        *pic = nullptr;

        GameInterface::Printf("PCX image %s was malformed!", filename);
        return false;
    }
    else
    {
        return true;
    }
}

///////////////////////////////////////////////////////////////////////////////

// Mostly adapted from Q2 ref_gl/gl_image.c
bool PCXLoadFromFile(const char * const filename, Color8 ** pic, int * width, int * height, ColorRGBA32 * palette)
{
    GameInterface::FS::ScopedFile file{ filename };
    if (!file.IsLoaded())
    {
        GameInterface::Printf("Bad PCX file '%s'", filename);
        return false;
    }

    auto * data_ptr = static_cast<const Color8 *>(file.data_ptr);
    const int data_len = file.length;

    return PCXLoadFromMemory(filename, data_ptr, data_len, pic, width, height, palette);
}

///////////////////////////////////////////////////////////////////////////////
// TGA image loading helpers:
///////////////////////////////////////////////////////////////////////////////

struct TGAFileHeader
{
    std::uint8_t  id_length;
    std::uint8_t  colormap_type;
    std::uint8_t  image_type;
    std::uint16_t colormap_index;
    std::uint16_t colormap_length;
    std::uint8_t  colormap_size;
    std::uint16_t x_origin;
    std::uint16_t y_origin;
    std::uint16_t width;
    std::uint16_t height;
    std::uint8_t  pixel_size;
    std::uint8_t  attributes;
};

///////////////////////////////////////////////////////////////////////////////

// Mostly adapted from LoadTGA, ref_gl/gl_image.c
// Note: Output image is always RGBA 32bits.
bool TGALoadFromFile(const char * const filename, ColorRGBA32 ** pic, int * width, int * height)
{
    TGAFileHeader targa_header;
    int column, row;
    Color8 * pixbuf;
    alignas(std::uint32_t) std::uint8_t tmp[2];

    *pic = nullptr;

    GameInterface::FS::ScopedFile file{ filename };
    if (!file.IsLoaded())
    {
        GameInterface::Printf("Bad TGA file '%s'", filename);
        return false;
    }

    auto * buffer = static_cast<const std::uint8_t *>(file.data_ptr);

    const std::uint8_t * buf_p = buffer;
    targa_header.id_length     = *buf_p++;
    targa_header.colormap_type = *buf_p++;
    targa_header.image_type    = *buf_p++;

    tmp[0] = buf_p[0];
    tmp[1] = buf_p[1];
    targa_header.colormap_index = *reinterpret_cast<const std::uint16_t *>(tmp);
    buf_p += 2;

    tmp[0] = buf_p[0];
    tmp[1] = buf_p[1];
    targa_header.colormap_length = *reinterpret_cast<const std::uint16_t *>(tmp);
    buf_p += 2;

    targa_header.colormap_size = *buf_p++;

    targa_header.x_origin = *reinterpret_cast<const std::uint16_t *>(buf_p); buf_p += 2;
    targa_header.y_origin = *reinterpret_cast<const std::uint16_t *>(buf_p); buf_p += 2;
    targa_header.width    = *reinterpret_cast<const std::uint16_t *>(buf_p); buf_p += 2;
    targa_header.height   = *reinterpret_cast<const std::uint16_t *>(buf_p); buf_p += 2;

    targa_header.pixel_size = *buf_p++;
    targa_header.attributes = *buf_p++;

    if (targa_header.image_type != 2 && targa_header.image_type != 10)
    {
        GameInterface::Printf("TGALoadFromFile: Only type 2 and 10 TARGA RGB images supported! %s", filename);
        return false;
    }

    if (targa_header.colormap_type != 0 || (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
    {
        GameInterface::Printf("TGALoadFromFile: Only 32 or 24 bit images supported (no colormaps)! %s", filename);
        return false;
    }

    const int columns     = targa_header.width;
    const int rows        = targa_header.height;
    const int pixel_count = columns * rows;

    if (width != nullptr)
    {
        *width = columns;
    }
    if (height != nullptr)
    {
        *height = rows;
    }

    *pic = new(MemTag::kTextures) ColorRGBA32[pixel_count];
    auto * targa_rgba = reinterpret_cast<Color8 *>(*pic);

    if (targa_header.id_length != 0)
    {
        buf_p += targa_header.id_length; // skip TARGA image comment
    }

    if (targa_header.image_type == 2) // Uncompressed, RGB images
    {
        for (row = rows - 1; row >= 0; --row)
        {
            pixbuf = targa_rgba + row * columns * 4;
            for (column = 0; column < columns; ++column)
            {
                Color8 red, green, blue, alpha;
                switch (targa_header.pixel_size)
                {
                case 24 :
                    blue      = *buf_p++;
                    green     = *buf_p++;
                    red       = *buf_p++;
                    *pixbuf++ = red;
                    *pixbuf++ = green;
                    *pixbuf++ = blue;
                    *pixbuf++ = 255;
                    break;

                case 32 :
                    blue      = *buf_p++;
                    green     = *buf_p++;
                    red       = *buf_p++;
                    alpha     = *buf_p++;
                    *pixbuf++ = red;
                    *pixbuf++ = green;
                    *pixbuf++ = blue;
                    *pixbuf++ = alpha;
                    break;

                default:
                    MRQ2_ASSERT(false);
                    return false;
                } // switch
            }
        }
    }
    else if (targa_header.image_type == 10) // Run-length encoded RGB images
    {
        Color8 red, green, blue, alpha;
        std::uint8_t packetHeader, packetSize, j;

        for (row = rows - 1; row >= 0; --row)
        {
            pixbuf = targa_rgba + row * columns * 4;
            for (column = 0; column < columns;)
            {
                packetHeader = *buf_p++;
                packetSize = 1 + (packetHeader & 0x7F);

                if (packetHeader & 0x80) // Run-length packet
                {
                    switch (targa_header.pixel_size)
                    {
                    case 24 :
                        blue  = *buf_p++;
                        green = *buf_p++;
                        red   = *buf_p++;
                        alpha = 255;
                        break;

                    case 32 :
                        blue  = *buf_p++;
                        green = *buf_p++;
                        red   = *buf_p++;
                        alpha = *buf_p++;
                        break;

                    default:
                        MRQ2_ASSERT(false);
                        return false;
                    } // switch

                    for (j = 0; j < packetSize; ++j)
                    {
                        *pixbuf++ = red;
                        *pixbuf++ = green;
                        *pixbuf++ = blue;
                        *pixbuf++ = alpha;

                        ++column;
                        if (column == columns) // run spans across rows
                        {
                            column = 0;
                            if (row > 0)
                            {
                                --row;
                            }
                            else
                            {
                                goto BREAKOUT;
                            }
                            pixbuf = targa_rgba + row * columns * 4;
                        }
                    }
                }
                else // Non run-length packet
                {
                    for (j = 0; j < packetSize; ++j)
                    {
                        switch (targa_header.pixel_size)
                        {
                        case 24 :
                            blue      = *buf_p++;
                            green     = *buf_p++;
                            red       = *buf_p++;
                            *pixbuf++ = red;
                            *pixbuf++ = green;
                            *pixbuf++ = blue;
                            *pixbuf++ = 255;
                            break;

                        case 32 :
                            blue      = *buf_p++;
                            green     = *buf_p++;
                            red       = *buf_p++;
                            alpha     = *buf_p++;
                            *pixbuf++ = red;
                            *pixbuf++ = green;
                            *pixbuf++ = blue;
                            *pixbuf++ = alpha;
                            break;

                        default:
                            MRQ2_ASSERT(false);
                            return false;
                        } // switch

                        ++column;
                        if (column == columns) // pixel packet run spans across rows
                        {
                            column = 0;
                            if (row > 0)
                            {
                                --row;
                            }
                            else
                            {
                                goto BREAKOUT;
                            }
                            pixbuf = targa_rgba + row * columns * 4;
                        }
                    }
                }
            }
        BREAKOUT:
            ;
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// PNG loader using STBI
///////////////////////////////////////////////////////////////////////////////

bool PNGLoadFromFile(const char * filename, ColorRGBA32 ** pic, int * width, int * height)
{
    *pic    = nullptr;
    *width  = 0;
    *height = 0;

    GameInterface::FS::ScopedFile file{ filename };
    if (!file.IsLoaded())
    {
        GameInterface::Printf("Bad PNG file '%s'", filename);
        return false;
    }

    int n = 0;
    stbi_uc * img_data = stbi_load_from_memory(reinterpret_cast<stbi_uc const *>(file.data_ptr), file.length, width, height, &n, TextureImage::kBytesPerPixel);
    if (img_data == nullptr)
    {
        GameInterface::Printf("stbi_load_from_memory('%s') failed!", filename);
        return false;
    }

    const size_t pixel_count = (*width) * (*height);
    *pic = new(MemTag::kTextures) ColorRGBA32[pixel_count];

    std::memcpy(*pic, img_data, pixel_count * sizeof(ColorRGBA32));
    stbi_image_free(img_data);

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// Debug colors
///////////////////////////////////////////////////////////////////////////////

// Built-in colors for misc debug rendering.
const ColorRGBA32 DebugColorsTable[kNumDebugColors] = {
    //              R     G     B     A
    BytesToColor(   0,    0,  255,  255 ), // Blue
    BytesToColor( 165,   42,   42,  255 ), // Brown
    BytesToColor( 127,   31,    0,  255 ), // Copper
    BytesToColor(   0,  255,  255,  255 ), // Cyan
    BytesToColor(   0,    0,  139,  255 ), // DarkBlue
    BytesToColor( 255,  215,    0,  255 ), // Gold
    BytesToColor( 128,  128,  128,  255 ), // Gray
    BytesToColor(   0,  255,    0,  255 ), // Green
    BytesToColor( 195,  223,  223,  255 ), // Ice
    BytesToColor( 173,  216,  230,  255 ), // LightBlue
    BytesToColor( 175,  175,  175,  255 ), // LightGray
    BytesToColor( 135,  206,  250,  255 ), // LightSkyBlue
    BytesToColor( 210,  105,   30,  255 ), // Lime
    BytesToColor( 255,    0,  255,  255 ), // Magenta
    BytesToColor( 128,    0,    0,  255 ), // Maroon
    BytesToColor( 128,  128,    0,  255 ), // Olive
    BytesToColor( 255,  165,    0,  255 ), // Orange
    BytesToColor( 255,  192,  203,  255 ), // Pink
    BytesToColor( 128,    0,  128,  255 ), // Purple
    BytesToColor( 255,    0,    0,  255 ), // Red
    BytesToColor( 192,  192,  192,  255 ), // Silver
    BytesToColor(   0,  128,  128,  255 ), // Teal
    BytesToColor( 238,  130,  238,  255 ), // Violet
    BytesToColor( 255,  255,  255,  255 ), // White
    BytesToColor( 255,  255,    0,  255 ), // Yellow
};

ColorRGBA32 NextDebugColor()
{
    static unsigned s_next_color = 0;
    s_next_color = (s_next_color + 1) % kNumDebugColors;
    return DebugColorsTable[s_next_color];
}

ColorRGBA32 RandomDebugColor()
{
    static std::default_random_engine s_generator;
    static std::uniform_int_distribution<int> s_distribution(0, kNumDebugColors - 1);

    const unsigned color_index = s_distribution(s_generator);
    MRQ2_ASSERT(color_index < kNumDebugColors);

    return DebugColorsTable[color_index];
}

///////////////////////////////////////////////////////////////////////////////
// STB Image writers
///////////////////////////////////////////////////////////////////////////////

bool TGASaveToFile(const char * filename, int width, int height, const ColorRGBA32 * pixels)
{
    if (stbi_write_tga(filename, width, height, TextureImage::kBytesPerPixel, pixels) == 0)
    {
        return false;
    }
    return true;
}

bool PNGSaveToFile(const char * filename, int width, int height, const ColorRGBA32 * pixels)
{
    const int stride = width * TextureImage::kBytesPerPixel;
    if (stbi_write_png(filename, width, height, TextureImage::kBytesPerPixel, pixels, stride) == 0)
    {
        return false;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
