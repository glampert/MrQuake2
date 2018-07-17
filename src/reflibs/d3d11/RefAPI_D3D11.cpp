//
// RefAPI_D3D11.cpp
//  Exposes GetRefAPI as the DLL entry point for Quake and the function pointers
//  required by refexport_t. Sets up the D3D11 refresh DLL.
//

#include "Renderer.hpp"

// Quake includes
#include "client/ref.h"
#include "client/vid.h"

// Refresh common lib
#pragma comment(lib, "RefShared.lib")

///////////////////////////////////////////////////////////////////////////////
// Game => Renderer interface
///////////////////////////////////////////////////////////////////////////////

namespace MrQ2
{
namespace D3D11
{

static int InitRefresh(void * hInstance, void * wndproc, int fullscreen)
{
    auto vid_mode = GameInterface::Cvar::Get("vid_mode", "6", CvarWrapper::kFlagArchive);
    auto vid_width = GameInterface::Cvar::Get("vid_width", "1024", CvarWrapper::kFlagArchive);
    auto vid_height = GameInterface::Cvar::Get("vid_height", "768", CvarWrapper::kFlagArchive);
    auto r_renderdoc = GameInterface::Cvar::Get("r_renderdoc", "0", CvarWrapper::kFlagArchive);

    int w, h;
    if (!GameInterface::Video::GetModeInfo(w, h, vid_mode.AsInt()))
    {
        // An invalid vid_mode (i.e.: -1) uses the explicit size
        w = vid_width.AsInt();
        h = vid_height.AsInt();
    }

    #if defined(DEBUG) || defined(_DEBUG)
    constexpr bool debug_validation = true;
    #else // DEBUG
    constexpr bool debug_validation = false;
    #endif // DEBUG

    if (r_renderdoc.IsSet())
    {
        RenderDocUtils::Initialize();
    }

    CreateRendererInstance();
    g_Renderer->Init("MrQuake2 (D3D11)", (HINSTANCE)hInstance, (WNDPROC)wndproc, w, h, !!fullscreen, debug_validation);
    return true;
}

///////////////////////////////////////////////////////////////////////////////

static void ShutdownRefresh()
{
    DestroyRendererInstance();
    RenderDocUtils::Shutdown();
    GameInterface::Shutdown();
}

///////////////////////////////////////////////////////////////////////////////

static void AppActivate(int activate)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void BeginRegistration(const char * map_name)
{
    GameInterface::Printf("**** D3D11::BeginRegistration ****");

    g_Renderer->ViewState()->BeginRegistration();
    g_Renderer->TexStore()->BeginRegistration();
    g_Renderer->MdlStore()->BeginRegistration(map_name);

    MemTagsPrintAll();
}

///////////////////////////////////////////////////////////////////////////////

static void EndRegistration()
{
    GameInterface::Printf("**** D3D11::EndRegistration ****");

    g_Renderer->MdlStore()->EndRegistration();
    g_Renderer->TexStore()->EndRegistration();
    g_Renderer->TexStore()->UploadScrapIfNeeded();

    MemTagsPrintAll();
}

///////////////////////////////////////////////////////////////////////////////

static model_s * RegisterModel(const char * name)
{
    // Returned as an opaque handle.
    return (model_s *)g_Renderer->MdlStore()->FindOrLoad(name, ModelType::kAny);
}

///////////////////////////////////////////////////////////////////////////////

static image_s * RegisterSkin(const char * name)
{
    // Returned as an opaque handle.
    return (image_s *)g_Renderer->TexStore()->FindOrLoad(name, TextureType::kSkin);
}

///////////////////////////////////////////////////////////////////////////////

static image_s * RegisterPic(const char * name)
{
    // Returned as an opaque handle.
    return (image_s *)g_Renderer->TexStore()->FindOrLoad(name, TextureType::kPic);
}

///////////////////////////////////////////////////////////////////////////////

static void SetSky(const char * name, float rotate, vec3_t axis)
{
    g_Renderer->ViewState()->Sky() = SkyBox(*g_Renderer->TexStore(), name, rotate, axis);
}

///////////////////////////////////////////////////////////////////////////////

static void DrawGetPicSize(int * w, int * h, const char * name)
{
    // This can be called outside Begin/End frame.
    const TextureImage * tex = g_Renderer->TexStore()->FindOrLoad(name, TextureType::kPic);
    if (tex == nullptr)
    {
        GameInterface::Printf("WARNING: Can't find or load pic: '%s'", name);
        *w = *h = -1;
        return;
    }

    *w = tex->width;
    *h = tex->height;
}

///////////////////////////////////////////////////////////////////////////////

static void DrawPic(int x, int y, const char * name)
{
    FASTASSERT(g_Renderer->FrameStarted());

    const TextureImage * tex = g_Renderer->TexStore()->FindOrLoad(name, TextureType::kPic);
    if (tex == nullptr)
    {
        GameInterface::Printf("WARNING: Can't find or load pic: '%s'", name);
        return;
    }

    const auto fx = float(x);
    const auto fy = float(y);
    const auto fw = float(tex->width);
    const auto fh = float(tex->height);

    if (tex->from_scrap)
    {
        const auto u0 = float(tex->scrap_uv0.x) / TextureStore::kScrapSize;
        const auto v0 = float(tex->scrap_uv0.y) / TextureStore::kScrapSize;
        const auto u1 = float(tex->scrap_uv1.x) / TextureStore::kScrapSize;
        const auto v1 = float(tex->scrap_uv1.y) / TextureStore::kScrapSize;

        g_Renderer->SBatch(SpriteBatch::kDrawPics)->PushQuadTexturedUVs(
            fx, fy, fw, fh, u0, v0, u1, v1, static_cast<const TextureImageImpl *>(tex), Renderer::kColorWhite);
    }
    else
    {
        g_Renderer->SBatch(SpriteBatch::kDrawPics)->PushQuadTextured(
            fx, fy, fw, fh, static_cast<const TextureImageImpl *>(tex), Renderer::kColorWhite);
    }
}

///////////////////////////////////////////////////////////////////////////////

static void DrawStretchPic(int x, int y, int w, int h, const char * name)
{
    FASTASSERT(g_Renderer->FrameStarted());

    const TextureImage * tex = g_Renderer->TexStore()->FindOrLoad(name, TextureType::kPic);
    if (tex == nullptr)
    {
        GameInterface::Printf("WARNING: Can't find or load pic: '%s'", name);
        return;
    }

    const auto fx = float(x);
    const auto fy = float(y);
    const auto fw = float(w);
    const auto fh = float(h);

    if (tex->from_scrap)
    {
        const auto u0 = float(tex->scrap_uv0.x) / TextureStore::kScrapSize;
        const auto v0 = float(tex->scrap_uv0.y) / TextureStore::kScrapSize;
        const auto u1 = float(tex->scrap_uv1.x) / TextureStore::kScrapSize;
        const auto v1 = float(tex->scrap_uv1.y) / TextureStore::kScrapSize;

        g_Renderer->SBatch(SpriteBatch::kDrawPics)->PushQuadTexturedUVs(
            fx, fy, fw, fh, u0, v0, u1, v1, static_cast<const TextureImageImpl *>(tex), Renderer::kColorWhite);
    }
    else
    {
        g_Renderer->SBatch(SpriteBatch::kDrawPics)->PushQuadTextured(
            fx, fy, fw, fh, static_cast<const TextureImageImpl *>(tex), Renderer::kColorWhite);
    }
}

///////////////////////////////////////////////////////////////////////////////

static void DrawChar(int x, int y, int c)
{
    FASTASSERT(g_Renderer->FrameStarted());

    // Draws one 8*8 console graphic character with 0 being transparent.
    // It can be clipped to the top of the screen to allow the console
    // to be smoothly scrolled off. Based on Draw_Char() from ref_gl.
    constexpr int kGlyphSize = 8;
    constexpr int kGlyphTextureSize = 128;
    constexpr float kGlyphUVScale = float(kGlyphSize) / float(kGlyphTextureSize);

    c &= 255;

    if ((c & 127) == ' ')
    {
        return; // Whitespace
    }
    if (y <= -kGlyphSize)
    {
        return; // Totally off screen
    }

    const int row = c >> 4;
    const int col = c & 15;
    const float frow = row * kGlyphUVScale;
    const float fcol = col * kGlyphUVScale;

    g_Renderer->SBatch(SpriteBatch::kDrawChar)->PushQuad(
        float(x),
        float(y),
        kGlyphSize,
        kGlyphSize,
        fcol,
        frow,
        fcol + kGlyphUVScale,
        frow + kGlyphUVScale,
        Renderer::kColorWhite);
}

///////////////////////////////////////////////////////////////////////////////

static void DrawTileClear(int x, int y, int w, int h, const char * name)
{
    FASTASSERT(g_Renderer->FrameStarted());
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void DrawFill(int x, int y, int w, int h, int c)
{
    FASTASSERT(g_Renderer->FrameStarted());

    const ColorRGBA32 color = TextureStore::ColorForIndex(c & 0xFF);
    const std::uint8_t r = (color & 0xFF);
    const std::uint8_t g = (color >> 8)  & 0xFF;
    const std::uint8_t b = (color >> 16) & 0xFF;

    constexpr float scale = 1.0f / 255.0f;
    const DirectX::XMFLOAT4A normalized_color{ r * scale, g * scale, b * scale, 1.0f };

    auto * dummy_tex = static_cast<const TextureImageImpl *>(g_Renderer->TexStore()->tex_white2x2);

    g_Renderer->SBatch(SpriteBatch::kDrawPics)->PushQuadTextured(
        float(x), float(y),
        float(w), float(h),
        dummy_tex,
        normalized_color);
}

///////////////////////////////////////////////////////////////////////////////

static void DrawFadeScreen()
{
    // was 0.8 on ref_gl Draw_FadeScreen
    constexpr float fade_alpha = 0.5f;

    FASTASSERT(g_Renderer->FrameStarted());

    // Use a dummy white texture as base
    auto * dummy_tex = static_cast<const TextureImageImpl *>(g_Renderer->TexStore()->tex_white2x2);

    // Full screen quad with alpha
    g_Renderer->SBatch(SpriteBatch::kDrawPics)->PushQuadTextured(
        0.0f, 0.0f, g_Renderer->Width(), g_Renderer->Height(), 
        dummy_tex, DirectX::XMFLOAT4A{ 0.0f, 0.0f, 0.0f, fade_alpha });
}

///////////////////////////////////////////////////////////////////////////////

static void DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const qbyte * data)
{
    FASTASSERT(g_Renderer->FrameStarted());

    //
    // This function is only used by Quake2 to draw the cinematic frames, nothing else,
    // so it could have a better name... We'll optimize for that and assume this is not
    // a generic "draw pixels" kind of function.
    //

    const TextureImage * const cin_tex = g_Renderer->TexStore()->tex_cinframe;
    FASTASSERT(cin_tex != nullptr);

    ColorRGBA32 * const cinematic_buffer = const_cast<ColorRGBA32 *>(cin_tex->pixels);
    FASTASSERT(cinematic_buffer != nullptr);

    const ColorRGBA32 * const cinematic_palette = TextureStore::CinematicPalette();
    float hscale;
    int num_rows;

    if (rows <= kQuakeCinematicImgSize)
    {
        hscale = 1.0f;
        num_rows = rows;
    }
    else
    {
        hscale = float(rows) / float(kQuakeCinematicImgSize);
        num_rows = kQuakeCinematicImgSize;
    }

    // Good idea to clear the buffer first, in case the
    // following upsampling doesn't fill the whole thing.
    for (int p = 0; p < (kQuakeCinematicImgSize * kQuakeCinematicImgSize); ++p)
    {
        //                        0xAABBGGRR
        const ColorRGBA32 black = 0xFF000000;
        cinematic_buffer[p] = black;
    }

    // Upsample to fill our 256*256 cinematic buffer.
    // This is based on the algorithm applied by ref_gl.
    for (int i = 0; i < num_rows; ++i)
    {
        const int row = static_cast<int>(i * hscale);
        if (row > rows)
        {
            break;
        }

        const qbyte * source = &data[cols * row];
        ColorRGBA32 * dest = &cinematic_buffer[i * kQuakeCinematicImgSize];

        const int fracstep = (cols * 65536 / kQuakeCinematicImgSize);
        int frac = fracstep >> 1;

        for (int j = 0; j < kQuakeCinematicImgSize; ++j)
        {
            const ColorRGBA32 color = cinematic_palette[source[frac >> 16]];
            dest[j] = color;
            frac += fracstep;
        }
    }

    h += 45; // FIXME HACK - Image scaling is probably broken.
             // Cinematics are not filling up the buffer as they should...

    // Update the cinematic GPU texture from our CPU buffer
    g_Renderer->UploadTexture(static_cast<const TextureImageImpl *>(cin_tex));

    // Draw a fullscreen quadrilateral with the cinematic texture applied to it
    g_Renderer->SBatch(SpriteBatch::kDrawPics)->PushQuadTextured(
        float(x), float(y), float(w), float(h), 
        static_cast<const TextureImageImpl *>(cin_tex),
        Renderer::kColorWhite);
}

///////////////////////////////////////////////////////////////////////////////

static void CinematicSetPalette(const qbyte * palette)
{
    TextureStore::SetCinematicPaletteFromRaw(palette);
}

///////////////////////////////////////////////////////////////////////////////

static void BeginFrame(float /*camera_separation*/)
{
    FASTASSERT(!g_Renderer->FrameStarted());
    g_Renderer->BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

static void EndFrame()
{
    FASTASSERT(g_Renderer->FrameStarted());
    g_Renderer->EndFrame();
}

///////////////////////////////////////////////////////////////////////////////

static void RenderFrame(refdef_t * view_def)
{
    FASTASSERT(view_def != nullptr);
    FASTASSERT(g_Renderer->FrameStarted());

    // A world map should have been loaded already by BeginRegistration().
    if (g_Renderer->MdlStore()->WorldModel() == nullptr && !(view_def->rdflags & RDF_NOWORLDMODEL))
    {
        GameInterface::Errorf("RenderFrame: Null world model!");
    }

    g_Renderer->RenderView(*view_def);
}

} // D3D11
} // MrQ2

///////////////////////////////////////////////////////////////////////////////

extern "C" REFLIB_DLL_EXPORT refexport_t GetRefAPI(refimport_t ri)
{
    MrQ2::GameInterface::Initialize(ri, "D3D11");

    refexport_t re;
    re.api_version         = REF_API_VERSION;
    re.vidref              = VIDREF_D3D11;
    re.Init                = MrQ2::D3D11::InitRefresh;
    re.Shutdown            = MrQ2::D3D11::ShutdownRefresh;
    re.BeginRegistration   = MrQ2::D3D11::BeginRegistration;
    re.RegisterModel       = MrQ2::D3D11::RegisterModel;
    re.RegisterSkin        = MrQ2::D3D11::RegisterSkin;
    re.RegisterPic         = MrQ2::D3D11::RegisterPic;
    re.SetSky              = MrQ2::D3D11::SetSky;
    re.EndRegistration     = MrQ2::D3D11::EndRegistration;
    re.RenderFrame         = MrQ2::D3D11::RenderFrame;
    re.DrawGetPicSize      = MrQ2::D3D11::DrawGetPicSize;
    re.DrawPic             = MrQ2::D3D11::DrawPic;
    re.DrawStretchPic      = MrQ2::D3D11::DrawStretchPic;
    re.DrawChar            = MrQ2::D3D11::DrawChar;
    re.DrawTileClear       = MrQ2::D3D11::DrawTileClear;
    re.DrawFill            = MrQ2::D3D11::DrawFill;
    re.DrawFadeScreen      = MrQ2::D3D11::DrawFadeScreen;
    re.DrawStretchRaw      = MrQ2::D3D11::DrawStretchRaw;
    re.CinematicSetPalette = MrQ2::D3D11::CinematicSetPalette;
    re.BeginFrame          = MrQ2::D3D11::BeginFrame;
    re.EndFrame            = MrQ2::D3D11::EndFrame;
    re.AppActivate         = MrQ2::D3D11::AppActivate;
    return re;
}

///////////////////////////////////////////////////////////////////////////////
