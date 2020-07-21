//
// DLLInterfaceD3D12.cpp
//  Exposes GetRefAPI as the DLL entry point for Quake and the function pointers
//  required by refexport_t. Sets up the D3D12 renderer DLL.
//

#include "../common/DLLInterface.hpp"

// Dx12 libraries
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "d3dcompiler")

///////////////////////////////////////////////////////////////////////////////
// GetRefAPI()
///////////////////////////////////////////////////////////////////////////////

extern "C" MRQ2_RENDERLIB_DLL_EXPORT refexport_t GetRefAPI(refimport_t ri)
{
    MrQ2::GameInterface::Initialize(ri, "D3D12");

    refexport_t re;
    re.api_version         = REF_API_VERSION;
    re.vidref              = VIDREF_D3D12;
    re.Init                = &MrQ2::DLLInterface::Init;
    re.Shutdown            = &MrQ2::DLLInterface::Shutdown;
    re.BeginRegistration   = &MrQ2::DLLInterface::BeginRegistration;
    re.RegisterModel       = &MrQ2::DLLInterface::RegisterModel;
    re.RegisterSkin        = &MrQ2::DLLInterface::RegisterSkin;
    re.RegisterPic         = &MrQ2::DLLInterface::RegisterPic;
    re.SetSky              = &MrQ2::DLLInterface::SetSky;
    re.EndRegistration     = &MrQ2::DLLInterface::EndRegistration;
    re.RenderFrame         = &MrQ2::DLLInterface::RenderView;
    re.DrawGetPicSize      = &MrQ2::DLLInterface::GetPicSize;
    re.DrawPic             = &MrQ2::DLLInterface::DrawPic;
    re.DrawStretchPic      = &MrQ2::DLLInterface::DrawStretchPic;
    re.DrawChar            = &MrQ2::DLLInterface::DrawChar;
    re.DrawTileClear       = &MrQ2::DLLInterface::DrawTileClear;
    re.DrawFill            = &MrQ2::DLLInterface::DrawFill;
    re.DrawFadeScreen      = &MrQ2::DLLInterface::DrawFadeScreen;
    re.DrawStretchRaw      = &MrQ2::DLLInterface::DrawStretchRaw;
    re.CinematicSetPalette = &MrQ2::DLLInterface::CinematicSetPalette;
    re.BeginFrame          = &MrQ2::DLLInterface::BeginFrame;
    re.EndFrame            = &MrQ2::DLLInterface::EndFrame;
    re.AppActivate         = &MrQ2::DLLInterface::AppActivate;
    return re;
}

///////////////////////////////////////////////////////////////////////////////
