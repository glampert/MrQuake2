//
// RenderDocUtils.cpp
//  RenderDoc integration to trigger captures from code.
//

#include "RenderDocUtils.hpp"
#include "Common.hpp"
#include "Win32Window.hpp"

// RenderDoc function pointers and structures:
#include "external/renderdoc/renderdoc_app.h"

// NOTES:
// * renderdoc.dll and optionally dbghelp.dll have to be
//   available in the same path as the executable.
// * Higher-level code will only initialize RenderDoc
//   if the 'r_renderdoc' Cvar is set.

namespace MrQ2
{
namespace RenderDocUtils
{

///////////////////////////////////////////////////////////////////////////////

static RENDERDOC_API_1_4_1 * g_renderdoc_api = nullptr;
static HMODULE               g_renderdoc_dll = nullptr;
static const char * const    g_captures_path = "\\renderdoc\\mrquake2";

///////////////////////////////////////////////////////////////////////////////

bool Initialize()
{
    if (IsInitialized())
    {
        GameInterface::Printf("Duplicate call to RenderDocUtils::Initialize() ...");
        return false;
    }

    g_renderdoc_dll = LoadLibrary("renderdoc.dll");
    if (g_renderdoc_dll == nullptr)
    {
        GameInterface::Printf("Failed to load RenderDoc DLL - Error: %#x", GetLastError());
        Shutdown();
        return false;
    }

    auto get_api_func = (pRENDERDOC_GetAPI)GetProcAddress(g_renderdoc_dll, "RENDERDOC_GetAPI");
    if (get_api_func == nullptr)
    {
        GameInterface::Printf("Failed to find RENDERDOC_GetAPI() from RenderDoc DLL handle.");
        Shutdown();
        return false;
    }

    const int ret = get_api_func(eRENDERDOC_API_Version_1_4_1, (void **)&g_renderdoc_api);
    if (ret != 1)
    {
        GameInterface::Printf("RENDERDOC_GetAPI() failed with error %i", ret);
        Shutdown();
        return false;
    }

    int major, minor, patch;
    g_renderdoc_api->GetAPIVersion(&major, &minor, &patch);

    GameInterface::Printf("---- RenderDocUtils Initialized ----");
    GameInterface::Printf("API version: %i.%i.%i", major, minor, patch);

    // Both Quake and the Visual Studio debugger already claim the default F12 capture key.
    RENDERDOC_InputButton capture_keys[] = { eRENDERDOC_Key_F11 };
    g_renderdoc_api->SetCaptureKeys(capture_keys, static_cast<int>(ArrayLength(capture_keys)));

    // Path where captures will be saved (created if needed)
    std::string capture_dir = Win32Window::CurrentWorkingDir();
    capture_dir += g_captures_path;
    g_renderdoc_api->SetCaptureFilePathTemplate(capture_dir.c_str());

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void Shutdown()
{
    if (!IsInitialized())
    {
        return;
    }

    FreeLibrary(g_renderdoc_dll);
    g_renderdoc_dll = nullptr;
    g_renderdoc_api = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

bool IsInitialized()
{
    return (g_renderdoc_dll != nullptr);
}

///////////////////////////////////////////////////////////////////////////////

void TriggerCapture()
{
    if (!IsInitialized())
    {
        return;
    }

    g_renderdoc_api->TriggerCapture();
}

///////////////////////////////////////////////////////////////////////////////

} // RenderDocUtils
} // MrQ2
