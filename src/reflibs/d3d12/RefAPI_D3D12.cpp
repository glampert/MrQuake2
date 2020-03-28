//
// RefAPI_D3D12.cpp
//  Exposes GetRefAPI as the DLL entry point for Quake and the function pointers
//  required by refexport_t. Sets up the D3D12 refresh DLL.
//

#include "Renderer_D3D12.hpp"
#include "reflibs/shared/d3d/D3DCommon.hpp"

// Quake includes
#include "client/ref.h"
#include "client/vid.h"

// Refresh common lib
#pragma comment(lib, "RefShared.lib")

// Dx12
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "d3dcompiler")

///////////////////////////////////////////////////////////////////////////////

extern "C" REFLIB_DLL_EXPORT refexport_t GetRefAPI(refimport_t ri)
{
    MrQ2::GameInterface::Initialize(ri, "D3D12");

    using RefAPI12 = MrQ2::D3DCommon_NullDraw<MrQ2::D3D12::Renderer>;

    refexport_t re;
    re.api_version         = REF_API_VERSION;
    re.vidref              = VIDREF_D3D12;
    re.Init                = &RefAPI12::Init;
    re.Shutdown            = &RefAPI12::Shutdown;
    re.BeginRegistration   = &RefAPI12::BeginRegistration;
    re.RegisterModel       = &RefAPI12::RegisterModel;
    re.RegisterSkin        = &RefAPI12::RegisterSkin;
    re.RegisterPic         = &RefAPI12::RegisterPic;
    re.SetSky              = &RefAPI12::SetSky;
    re.EndRegistration     = &RefAPI12::EndRegistration;
    re.RenderFrame         = &RefAPI12::RenderFrame;
    re.DrawGetPicSize      = &RefAPI12::DrawGetPicSize;
    re.DrawPic             = &RefAPI12::DrawPic;
    re.DrawStretchPic      = &RefAPI12::DrawStretchPic;
    re.DrawChar            = &RefAPI12::DrawChar;
    re.DrawTileClear       = &RefAPI12::DrawTileClear;
    re.DrawFill            = &RefAPI12::DrawFill;
    re.DrawFadeScreen      = &RefAPI12::DrawFadeScreen;
    re.DrawStretchRaw      = &RefAPI12::DrawStretchRaw;
    re.CinematicSetPalette = &RefAPI12::CinematicSetPalette;
    re.BeginFrame          = &RefAPI12::BeginFrame;
    re.EndFrame            = &RefAPI12::EndFrame;
    re.AppActivate         = &RefAPI12::AppActivate;
    return re;
}

///////////////////////////////////////////////////////////////////////////////
