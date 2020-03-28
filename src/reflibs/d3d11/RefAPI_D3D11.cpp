//
// RefAPI_D3D11.cpp
//  Exposes GetRefAPI as the DLL entry point for Quake and the function pointers
//  required by refexport_t. Sets up the D3D11 refresh DLL.
//

#include "Renderer_D3D11.hpp"
#include "reflibs/shared/d3d/D3DCommon.hpp"

// Quake includes
#include "client/ref.h"
#include "client/vid.h"

// Refresh common lib
#pragma comment(lib, "RefShared.lib")

// Dx11
#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "d3dcompiler")

///////////////////////////////////////////////////////////////////////////////

extern "C" REFLIB_DLL_EXPORT refexport_t GetRefAPI(refimport_t ri)
{
    MrQ2::GameInterface::Initialize(ri, "D3D11");

    using RefAPI11 = MrQ2::D3DCommon<MrQ2::D3D11::Renderer>;

    refexport_t re;
    re.api_version         = REF_API_VERSION;
    re.vidref              = VIDREF_D3D11;
    re.Init                = &RefAPI11::Init;
    re.Shutdown            = &RefAPI11::Shutdown;
    re.BeginRegistration   = &RefAPI11::BeginRegistration;
    re.RegisterModel       = &RefAPI11::RegisterModel;
    re.RegisterSkin        = &RefAPI11::RegisterSkin;
    re.RegisterPic         = &RefAPI11::RegisterPic;
    re.SetSky              = &RefAPI11::SetSky;
    re.EndRegistration     = &RefAPI11::EndRegistration;
    re.RenderFrame         = &RefAPI11::RenderFrame;
    re.DrawGetPicSize      = &RefAPI11::DrawGetPicSize;
    re.DrawPic             = &RefAPI11::DrawPic;
    re.DrawStretchPic      = &RefAPI11::DrawStretchPic;
    re.DrawChar            = &RefAPI11::DrawChar;
    re.DrawTileClear       = &RefAPI11::DrawTileClear;
    re.DrawFill            = &RefAPI11::DrawFill;
    re.DrawFadeScreen      = &RefAPI11::DrawFadeScreen;
    re.DrawStretchRaw      = &RefAPI11::DrawStretchRaw;
    re.CinematicSetPalette = &RefAPI11::CinematicSetPalette;
    re.BeginFrame          = &RefAPI11::BeginFrame;
    re.EndFrame            = &RefAPI11::EndFrame;
    re.AppActivate         = &RefAPI11::AppActivate;
    return re;
}

///////////////////////////////////////////////////////////////////////////////
