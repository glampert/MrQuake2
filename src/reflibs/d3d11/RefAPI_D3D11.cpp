//
// RefAPI_D3D11.cpp
//  Exposes GetRefAPI as the DLL entry point for Quake and the function pointers
//  required by refexport_t. Sets up the D3D11 refresh DLL.
//

#include "Renderer.hpp"

// Quake includes
extern "C"
{
#include "client/ref.h"
#include "client/vid.h"
} // extern "C"

// Refresh common lib
#pragma comment(lib, "RefShared.lib")

///////////////////////////////////////////////////////////////////////////////
// Game => Renderer interface
///////////////////////////////////////////////////////////////////////////////

namespace D3D11
{

static int InitRefresh(void * hInstance, void * wndproc, int fullscreen)
{
    auto vid_mode   = GameInterface::Cvar::Get("vid_mode", "6", CvarWrapper::kFlagArchive);
    auto vid_width  = GameInterface::Cvar::Get("vid_width", "1024", CvarWrapper::kFlagArchive);
    auto vid_height = GameInterface::Cvar::Get("vid_height", "768", CvarWrapper::kFlagArchive);
    auto sb_size    = GameInterface::Cvar::Get("r_spritebatch_size", "40000", CvarWrapper::kFlagArchive);

    int w, h;
    if (!GameInterface::Video::GetModeInfo(w, h, vid_mode.AsInt()))
    {
        // An invalid vid_mode (i.e.: -1) uses the explicit size
        w = vid_width.AsInt();
        h = vid_height.AsInt();
    }

    #if defined(DEBUG) || defined(_DEBUG)
    const bool debug_validation = true;
    #else // DEBUG
    const bool debug_validation = false;
    #endif // DEBUG

    CreateRendererInstance();
    g_Renderer->Init("MrQuake2 (D3D11)", (HINSTANCE)hInstance, (WNDPROC)wndproc,
                     w, h, !!fullscreen, debug_validation, sb_size.AsInt());

    MemTagsPrintAll();
    return true;
}

///////////////////////////////////////////////////////////////////////////////

static void ShutdownRefresh()
{
    DestroyRendererInstance();
    GameInterface::Shutdown();
}

///////////////////////////////////////////////////////////////////////////////

static void BeginRegistration(const char * map_name)
{
    GameInterface::Printf("*** D3D11::BeginRegistration ***");

    g_Renderer->TexStore()->BeginRegistration();

    MemTagsPrintAll();
}

///////////////////////////////////////////////////////////////////////////////

static void EndRegistration()
{
    GameInterface::Printf("*** D3D11::EndRegistration ***");

    g_Renderer->TexStore()->EndRegistration();

    MemTagsPrintAll();
}

///////////////////////////////////////////////////////////////////////////////

static model_s * RegisterModel(const char * name)
{
    // TODO
    return nullptr;
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
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void RenderFrame(refdef_t * fd)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void DrawGetPicSize(int * w, int * h, const char * name)
{
    // This can be called outside Begin/End frame.
    const TextureImage * tex = g_Renderer->TexStore()->FindOrLoad(name, TextureType::kPic);
    if (tex == nullptr)
    {
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

    //TODO
    //DrawTexImage(x, y, tex);
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

    //TODO
    //DrawScaledTexImage(x, y, w, h, tex);
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
    const auto fill_color = DirectX::XMFLOAT4A(DirectX::Colors::White);

    g_Renderer->SBatch()->PushQuad(
        x,
        y,
        kGlyphSize,
        kGlyphSize,
        fcol,
        frow,
        fcol + kGlyphUVScale,
        frow + kGlyphUVScale,
        fill_color);
}

///////////////////////////////////////////////////////////////////////////////

static void DrawTileClear(int x, int y, int w, int h, const char * name)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void DrawFill(int x, int y, int w, int h, int c)
{
    FASTASSERT(g_Renderer->FrameStarted());
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void DrawFadeScreen(void)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const qbyte * data)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void CinematicSetPalette(const qbyte * palette)
{
    // TODO
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

static void AppActivate(int activate)
{
    // TODO
}

} // D3D11

///////////////////////////////////////////////////////////////////////////////

extern "C" REFLIB_DLL_EXPORT refexport_t GetRefAPI(refimport_t ri)
{
    GameInterface::Initialize(ri, "D3D11");

    refexport_t re;
    re.api_version         = REF_API_VERSION;
    re.vidref              = VIDREF_D3D11;
    re.Init                = D3D11::InitRefresh;
    re.Shutdown            = D3D11::ShutdownRefresh;
    re.BeginRegistration   = D3D11::BeginRegistration;
    re.RegisterModel       = D3D11::RegisterModel;
    re.RegisterSkin        = D3D11::RegisterSkin;
    re.RegisterPic         = D3D11::RegisterPic;
    re.SetSky              = D3D11::SetSky;
    re.EndRegistration     = D3D11::EndRegistration;
    re.RenderFrame         = D3D11::RenderFrame;
    re.DrawGetPicSize      = D3D11::DrawGetPicSize;
    re.DrawPic             = D3D11::DrawPic;
    re.DrawStretchPic      = D3D11::DrawStretchPic;
    re.DrawChar            = D3D11::DrawChar;
    re.DrawTileClear       = D3D11::DrawTileClear;
    re.DrawFill            = D3D11::DrawFill;
    re.DrawFadeScreen      = D3D11::DrawFadeScreen;
    re.DrawStretchRaw      = D3D11::DrawStretchRaw;
    re.CinematicSetPalette = D3D11::CinematicSetPalette;
    re.BeginFrame          = D3D11::BeginFrame;
    re.EndFrame            = D3D11::EndFrame;
    re.AppActivate         = D3D11::AppActivate;
    return re;
}

///////////////////////////////////////////////////////////////////////////////
