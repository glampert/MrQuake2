//
// RefAPI_D3D11.cpp
//  Exposes GetRefAPI as the DLL entry point for Quake and the function pointers
//  required by refexport_t. Sets up the D3D11 refresh DLL.
//

#include "Renderer.hpp"

// Refresh common lib
#ifdef _MSC_VER
    #pragma comment(lib, "RefShared.lib")
#endif // _MSC_VER

extern "C"
{

// Quake includes
#include "client/ref.h"
#include "client/vid.h"

///////////////////////////////////////////////////////////////////////////////

static int D3D11Init(void * hInstance, void * wndproc, int fullscreen)
{
    auto vid_mode   = GameInterface::Cvar::Get("vid_mode", "6", CvarWrapper::kFlagArchive);
    auto vid_width  = GameInterface::Cvar::Get("vid_width", "1024", CvarWrapper::kFlagArchive);
    auto vid_height = GameInterface::Cvar::Get("vid_height", "768", CvarWrapper::kFlagArchive);
    auto sb_size    = GameInterface::Cvar::Get("d3d11_spritebatch_size", "80000", CvarWrapper::kFlagArchive);

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

    D3D11::CreateRendererInstance();
    D3D11::renderer->Init("MrQuake2 (D3D11)", (HINSTANCE)hInstance, (WNDPROC)wndproc,
                          w, h, !!fullscreen, debug_validation, sb_size.AsInt());
    return true;
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11Shutdown()
{
    D3D11::DestroyRendererInstance();
    GameInterface::Shutdown();
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11BeginRegistration(const char * map_name)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static model_s * D3D11RegisterModel(const char * name)
{
    // TODO
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

static image_s * D3D11RegisterSkin(const char * name)
{
    // TODO
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

static image_s * D3D11RegisterPic(const char * name)
{
    // TODO
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11SetSky(const char * name, float rotate, vec3_t axis)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11EndRegistration()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11RenderFrame(refdef_t * fd)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11DrawGetPicSize(int * w, int * h, const char * name)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11DrawPic(int x, int y, const char * name)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11DrawStretchPic(int x, int y, int w, int h, const char * name)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11DrawChar(int x, int y, int c)
{
    FASTASSERT(D3D11::renderer->FrameStarted());

    // Draws one 8*8 graphics character with 0 being transparent.
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

    D3D11::renderer->SpriteBatch()->PushQuad(
        x,
        y,
        x + kGlyphSize,
        y + kGlyphSize,
        fcol,
        frow,
        fcol + kGlyphUVScale,
        frow + kGlyphUVScale,
        fill_color);
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11DrawTileClear(int x, int y, int w, int h, const char * name)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11DrawFill(int x, int y, int w, int h, int c)
{
    FASTASSERT(D3D11::renderer->FrameStarted());
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11DrawFadeScreen(void)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const qbyte * data)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11CinematicSetPalette(const qbyte * palette)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11BeginFrame(float /*camera_separation*/)
{
    FASTASSERT(!D3D11::renderer->FrameStarted());
    D3D11::renderer->BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11EndFrame()
{
    FASTASSERT(D3D11::renderer->FrameStarted());
    D3D11::renderer->EndFrame();
}

///////////////////////////////////////////////////////////////////////////////

static void D3D11AppActivate(int activate)
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

REFLIB_DLL_EXPORT refexport_t GetRefAPI(refimport_t ri)
{
    GameInterface::Initialize(ri, "D3D11");

    refexport_t re;
    re.api_version         = REF_API_VERSION;
    re.vidref              = VIDREF_D3D11;
    re.Init                = D3D11Init;
    re.Shutdown            = D3D11Shutdown;
    re.BeginRegistration   = D3D11BeginRegistration;
    re.RegisterModel       = D3D11RegisterModel;
    re.RegisterSkin        = D3D11RegisterSkin;
    re.RegisterPic         = D3D11RegisterPic;
    re.SetSky              = D3D11SetSky;
    re.EndRegistration     = D3D11EndRegistration;
    re.RenderFrame         = D3D11RenderFrame;
    re.DrawGetPicSize      = D3D11DrawGetPicSize;
    re.DrawPic             = D3D11DrawPic;
    re.DrawStretchPic      = D3D11DrawStretchPic;
    re.DrawChar            = D3D11DrawChar;
    re.DrawTileClear       = D3D11DrawTileClear;
    re.DrawFill            = D3D11DrawFill;
    re.DrawFadeScreen      = D3D11DrawFadeScreen;
    re.DrawStretchRaw      = D3D11DrawStretchRaw;
    re.CinematicSetPalette = D3D11CinematicSetPalette;
    re.BeginFrame          = D3D11BeginFrame;
    re.EndFrame            = D3D11EndFrame;
    re.AppActivate         = D3D11AppActivate;
    return re;
}

///////////////////////////////////////////////////////////////////////////////

} // extern "C"
