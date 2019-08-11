//
// RefAPI_D3D12.cpp
//  Exposes GetRefAPI as the DLL entry point for Quake and the function pointers
//  required by refexport_t. Sets up the D3D12 refresh DLL.
//

#include "Renderer_D3D12.hpp"
#include "reflibs/shared/D3DCommon.hpp"

// Quake includes
#include "client/ref.h"
#include "client/vid.h"

// Refresh common lib
#pragma comment(lib, "RefShared.lib")

namespace MrQ2
{
namespace D3D12
{

//static MrQ2::D3DRefAPICommon<D3D12::Renderer> g_RefAPI12;
static MrQ2::D3DRefAPICommon_NullDraw<D3D12::Renderer> g_RefAPI12;

///////////////////////////////////////////////////////////////////////////////
// Game => Renderer interface
///////////////////////////////////////////////////////////////////////////////

static int InitRefresh(void * hInstance, void * wndproc, int fullscreen)
{
    #if defined(DEBUG) || defined(_DEBUG)
    constexpr bool debug_validation = true;
    #else // DEBUG
    constexpr bool debug_validation = false;
    #endif // DEBUG

    CreateRendererInstance();
    g_RefAPI12.Init(g_Renderer, "MrQuake2 (D3D12)", (HINSTANCE)hInstance, (WNDPROC)wndproc, !!fullscreen, debug_validation);

    return true;
}

///////////////////////////////////////////////////////////////////////////////

static void ShutdownRefresh()
{
    DestroyRendererInstance();
    g_RefAPI12.Shutdown();
}

///////////////////////////////////////////////////////////////////////////////

static void AppActivate(int activate)
{
    g_RefAPI12.AppActivate(!!activate);
}

///////////////////////////////////////////////////////////////////////////////

static void BeginRegistration(const char * map_name)
{
    GameInterface::Printf("**** D3D12::BeginRegistration ****");
    g_RefAPI12.BeginRegistration(map_name);
}

///////////////////////////////////////////////////////////////////////////////

static void EndRegistration()
{
    GameInterface::Printf("**** D3D12::EndRegistration ****");
    g_RefAPI12.EndRegistration();
}

///////////////////////////////////////////////////////////////////////////////

static model_s * RegisterModel(const char * name)
{
    return g_RefAPI12.RegisterModel(name);
}

///////////////////////////////////////////////////////////////////////////////

static image_s * RegisterSkin(const char * name)
{
    return g_RefAPI12.RegisterSkin(name);
}

///////////////////////////////////////////////////////////////////////////////

static image_s * RegisterPic(const char * name)
{
    return g_RefAPI12.RegisterPic(name);
}

///////////////////////////////////////////////////////////////////////////////

static void SetSky(const char * name, float rotate, vec3_t axis)
{
    g_RefAPI12.SetSky(name, rotate, axis);
}

///////////////////////////////////////////////////////////////////////////////

static void DrawGetPicSize(int * w, int * h, const char * name)
{
    g_RefAPI12.GetPicSize(w, h, name);
}

///////////////////////////////////////////////////////////////////////////////

static void DrawPic(int x, int y, const char * name)
{
    g_RefAPI12.DrawPic(x, y, name);
}

///////////////////////////////////////////////////////////////////////////////

static void DrawStretchPic(int x, int y, int w, int h, const char * name)
{
    g_RefAPI12.DrawStretchPic(x, y, w, h, name);
}

///////////////////////////////////////////////////////////////////////////////

static void DrawChar(int x, int y, int c)
{
    g_RefAPI12.DrawChar(x, y, c);
}

///////////////////////////////////////////////////////////////////////////////

static void DrawTileClear(int x, int y, int w, int h, const char * name)
{
    g_RefAPI12.DrawTileClear(x, y, w, h, name);
}

///////////////////////////////////////////////////////////////////////////////

static void DrawFill(int x, int y, int w, int h, int c)
{
    g_RefAPI12.DrawFill(x, y, w, h, c);
}

///////////////////////////////////////////////////////////////////////////////

static void DrawFadeScreen()
{
    g_RefAPI12.DrawFadeScreen();
}

///////////////////////////////////////////////////////////////////////////////

static void DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const qbyte * data)
{
    g_RefAPI12.DrawStretchRaw(x, y, w, h, cols, rows, data);
}

///////////////////////////////////////////////////////////////////////////////

static void CinematicSetPalette(const qbyte * palette)
{
    g_RefAPI12.CinematicSetPalette(palette);
}

///////////////////////////////////////////////////////////////////////////////

static void BeginFrame(float /*camera_separation*/)
{
    g_RefAPI12.BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

static void EndFrame()
{
    g_RefAPI12.EndFrame();
}

///////////////////////////////////////////////////////////////////////////////

static void RenderFrame(refdef_t * view_def)
{
    g_RefAPI12.RenderFrame(view_def);
}

} // D3D12
} // MrQ2

///////////////////////////////////////////////////////////////////////////////

extern "C" REFLIB_DLL_EXPORT refexport_t GetRefAPI(refimport_t ri)
{
    MrQ2::GameInterface::Initialize(ri, "D3D12");

    refexport_t re;
    re.api_version         = REF_API_VERSION;
    re.vidref              = VIDREF_D3D12;
    re.Init                = MrQ2::D3D12::InitRefresh;
    re.Shutdown            = MrQ2::D3D12::ShutdownRefresh;
    re.BeginRegistration   = MrQ2::D3D12::BeginRegistration;
    re.RegisterModel       = MrQ2::D3D12::RegisterModel;
    re.RegisterSkin        = MrQ2::D3D12::RegisterSkin;
    re.RegisterPic         = MrQ2::D3D12::RegisterPic;
    re.SetSky              = MrQ2::D3D12::SetSky;
    re.EndRegistration     = MrQ2::D3D12::EndRegistration;
    re.RenderFrame         = MrQ2::D3D12::RenderFrame;
    re.DrawGetPicSize      = MrQ2::D3D12::DrawGetPicSize;
    re.DrawPic             = MrQ2::D3D12::DrawPic;
    re.DrawStretchPic      = MrQ2::D3D12::DrawStretchPic;
    re.DrawChar            = MrQ2::D3D12::DrawChar;
    re.DrawTileClear       = MrQ2::D3D12::DrawTileClear;
    re.DrawFill            = MrQ2::D3D12::DrawFill;
    re.DrawFadeScreen      = MrQ2::D3D12::DrawFadeScreen;
    re.DrawStretchRaw      = MrQ2::D3D12::DrawStretchRaw;
    re.CinematicSetPalette = MrQ2::D3D12::CinematicSetPalette;
    re.BeginFrame          = MrQ2::D3D12::BeginFrame;
    re.EndFrame            = MrQ2::D3D12::EndFrame;
    re.AppActivate         = MrQ2::D3D12::AppActivate;
    return re;
}

///////////////////////////////////////////////////////////////////////////////
