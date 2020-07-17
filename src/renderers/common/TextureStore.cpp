//
// TextureStore.cpp
//  Generic texture/image loading and registration for all render back-ends.
//

#include "TextureStore.hpp"
#include <algorithm>
#include <random>

// Quake includes
#include "common/q_common.h"
#include "common/q_files.h"

// STB Image Write (stbiw)
#define STBIW_ASSERT(expr) MRQ2_ASSERT(expr)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "external/stb/stb_image_write.h"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////

// Verbose debugging
constexpr bool kLogLoadTextures = false;
constexpr bool kLogFindTextures = false;

static const char * const TextureType_Strings[] = {
    "kSkin",
    "kSprite",
    "kWall",
    "kSky",
    "kPic",
};

static_assert(ArrayLength(TextureType_Strings) == unsigned(TextureType::kCount), "Update this if the enum changes!");

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
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::Shutdown()
{
    tex_scrap    = nullptr;
    tex_conchars = nullptr;
    tex_conback  = nullptr;
    tex_backtile = nullptr;
    tex_white2x2 = nullptr;
    tex_debug    = nullptr;
    tex_cinframe = nullptr;

    DestroyAllLoadedTextures();
    m_teximages_cache.shrink_to_fit();
    m_teximages_pool.Drain();
    m_scrap.Shutdown();

    m_registration_num = 0;
    m_scrap_inited     = false;
    m_scrap_dirty      = false;
    m_device           = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::UploadScrapIfNeeded()
{
    if (m_scrap_dirty)
    {
        MRQ2_ASSERT(tex_scrap != nullptr);

        TextureUpload upload_info{};
        upload_info.texture  = &tex_scrap->texture;
        upload_info.pixels   = tex_scrap->pixels;
        upload_info.width    = tex_scrap->width;
        upload_info.height   = tex_scrap->height;
        upload_info.is_scrap = true;

        m_device->UploadContext().UploadTextureImmediate(upload_info);
        m_scrap_dirty = false;
    }
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStore::CreateScrap(const int size, const ColorRGBA32 * pix)
{
    MRQ2_ASSERT(m_device != nullptr);

    TextureImage * new_scrap = m_teximages_pool.Allocate();
    Construct(new_scrap, pix, m_registration_num, TextureType::kPic, /*use_scrap =*/true,
              size, size, Vec2u16{ 0,0 }, Vec2u16{ std::uint16_t(size), std::uint16_t(size) }, "pics/scrap.pcx");

    new_scrap->texture.Init(*m_device, size, size, /*is_scrap =*/true, new_scrap->pixels);
    return new_scrap;
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStore::CreateTexture(const ColorRGBA32 * pix, std::uint32_t regn, TextureType tt, bool use_scrap,
                                           int w, int h, Vec2u16 scrap0, Vec2u16 scrap1, const char * name)
{
    TextureImage * new_tex = m_teximages_pool.Allocate();
    Construct(new_tex, pix, regn, tt, use_scrap, w, h, scrap0, scrap1, name);

    if (use_scrap)
    {
        MRQ2_ASSERT(tex_scrap != nullptr);
        new_tex->texture.Init(tex_scrap->texture);
        m_scrap_dirty = true; // Upload the texture on next opportunity
    }
    else
    {
        MRQ2_ASSERT(m_device != nullptr);
        new_tex->texture.Init(*m_device, w, h, /*is_scrap =*/false, new_tex->pixels);
    }

    return new_tex;
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::DestroyTexture(TextureImage * tex)
{
    tex->texture.Shutdown();
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

void TextureStore::DumpAllLoadedTexturesToFile(const char * const path, const char * const file_type) const
{
    MRQ2_ASSERT(path != nullptr && path[0] != '\0');
    MRQ2_ASSERT(file_type != nullptr && file_type[0] != '\0');

    constexpr int kNComponents = 4; // always RGBA
    char fullname[1024];
    char filename[512];

    if (std::strncmp(file_type, "tga", 3) == 0)
    {
        for (const TextureImage * tex : m_teximages_cache)
        {
            std::snprintf(fullname, sizeof(fullname), "%s/%s.tga", path, tex->name.CStrNoExt(filename));
            GameInterface::FS::CreatePath(fullname);

            if (stbi_write_tga(fullname, tex->width, tex->height, kNComponents, tex->pixels) == 0)
            {
                GameInterface::Printf("Failed to write image '%s'", fullname);
            }
        }
    }
    else if (std::strncmp(file_type, "png", 3) == 0)
    {
        for (const TextureImage * tex : m_teximages_cache)
        {
            std::snprintf(fullname, sizeof(fullname), "%s/%s.png", path, tex->name.CStrNoExt(filename));
            GameInterface::FS::CreatePath(fullname);

            const int stride = tex->width * kNComponents;
            if (stbi_write_png(fullname, tex->width, tex->height, kNComponents, tex->pixels, stride) == 0)
            {
                GameInterface::Printf("Failed to write image '%s'", fullname);
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
    if (!m_scrap_inited)
    {
        m_teximages_cache.push_back(CreateScrap(m_scrap.Size(), m_scrap.pixels.get()));
        m_scrap_inited = true;
    }

    // This texture is generated at runtime
    if (tex_white2x2 == nullptr)
    {
        constexpr int Dim = 2;

        auto * pixels = new(MemTag::kTextures) ColorRGBA32[Dim * Dim];
        std::memset(pixels, 0xFF, sizeof(ColorRGBA32) * (Dim * Dim));

        TextureImage * tex = CreateTexture(pixels, m_registration_num, TextureType::kPic, false,
                                           Dim, Dim, {0,0}, {0,0}, "pics/white2x2.pcx"); // with a fake filename
        m_teximages_cache.push_back(tex);
        tex_white2x2 = tex;
    }

    // Also generated dynamically
    if (tex_debug == nullptr)
    {
        TextureImage * tex = CreateTexture(MakeCheckerPattern(), m_registration_num, TextureType::kPic, false,
                                           kCheckerDim, kCheckerDim, {0,0}, {0,0}, "pics/debug.pcx"); // with a fake filename
        m_teximages_cache.push_back(tex);
        tex_debug = tex;
    }

    // Cinematic frame texture is also a resident buffer
    if (tex_cinframe == nullptr)
    {
        constexpr int Dim = kQuakeCinematicImgSize;

        auto * pixels = new (MemTag::kTextures) ColorRGBA32[Dim * Dim];
        std::memset(pixels, 0, sizeof(ColorRGBA32) * (Dim * Dim));

        TextureImage * tex = CreateTexture(pixels, m_registration_num, TextureType::kPic, false,
                                           Dim, Dim, {0,0}, {0,0}, "pics/cinframe.pcx"); // with a fake filename
        m_teximages_cache.push_back(tex);
        tex_cinframe = tex;
    }

    // Update the registration count for these:
    tex_scrap    = FindOrLoad("scrap",    TextureType::kPic);
    tex_conchars = FindOrLoad("conchars", TextureType::kPic);
    tex_conback  = FindOrLoad("conback",  TextureType::kPic);
    tex_backtile = FindOrLoad("backtile", TextureType::kPic);
    tex_white2x2 = FindOrLoad("white2x2", TextureType::kPic);
    tex_debug    = FindOrLoad("debug",    TextureType::kPic);
    tex_cinframe = FindOrLoad("cinframe", TextureType::kPic);
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::BeginRegistration()
{
    GameInterface::Printf("==== TextureStore::BeginRegistration ====");
    ++m_registration_num;

    // Reference them on every BeginRegistration so they will always have the most current timestamp.
    TouchResidentTextures();
}

///////////////////////////////////////////////////////////////////////////////

void TextureStore::EndRegistration()
{
    GameInterface::Printf("==== TextureStore::EndRegistration ====");

    int num_removed = 0;
    auto remove_pred = [this, &num_removed](TextureImage * tex)
    {
        if (tex->reg_num != m_registration_num)
        {
            DestroyTexture(tex);
            ++num_removed;
            return true;
        }
        return false;
    };

    // "erase_if"
    auto first = m_teximages_cache.begin();
    auto last  = m_teximages_cache.end();
    m_teximages_cache.erase(std::remove_if(first, last, remove_pred), last);

    GameInterface::Printf("Freed %i unused textures.", num_removed);
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
            std::snprintf(s_temp_name, sizeof(s_temp_name), "pics/%s.pcx", tex_name);
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
        if ((name_hash == tex->name.Hash()) && (tt == tex->type))
        {
            tex->reg_num = m_registration_num;
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
    return CreateTexture(pic32, m_registration_num, tt, /*use_scrap =*/false, width, height, {0,0}, {0,0}, name);
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

TextureImage * TextureStore::ScrapTryAlloc8Bit(const Color8 * const pic8, const int width, const int height,
                                               const char * const name, const TextureType tt)
{
    MRQ2_ASSERT(width > 0 && height > 0);
    MRQ2_ASSERT(m_scrap_inited);

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
    return CreateTexture(pic32, m_registration_num, tt, /*use_scrap =*/true, width, height, uv0, uv1, name);
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStore::Common8BitTexSetup(const Color8 * const pic8, const int width, const int height,
                                                const char * const name, const TextureType tt)
{
    MRQ2_ASSERT(width > 0 && height > 0);

    auto * pic32 = new(MemTag::kTextures) ColorRGBA32[width * height];
    UnPalettize8To32(width, height, pic8, sm_global_palette, pic32);

    // Pass ownership of the pixel data
    return CreateTexture(pic32, m_registration_num, tt, /*use_scrap =*/false, width, height, {0,0}, {0,0}, name);
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

} // MrQ2