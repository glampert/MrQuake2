//
// OSWindow.hpp
//  Application window creation and management.
//
#pragma once

#define NOIME
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <string>
#include <windows.h>
#include <wrl.h>

namespace MrQ2
{

/*
===============================================================================

    OSWindow

===============================================================================
*/
class OSWindow
{
public:

    HINSTANCE   hinst       = nullptr;
    WNDPROC     wndproc     = nullptr;
    HWND        hwnd        = nullptr;
    std::string window_name = {};
    std::string class_name  = {};
    int         width       = 0;
    int         height      = 0;
    bool        fullscreen  = false;

    OSWindow() = default;
    virtual ~OSWindow();

    void Init();
    void Shutdown();

    // Disallow copy.
    OSWindow(const OSWindow &) = delete;
    OSWindow & operator=(const OSWindow &) = delete;

    // Static helpers
    static std::string ErrorToString(HRESULT hr);
    static std::string CurrentWorkingDir();

protected:

    // Implemented by the renderer.
    virtual void InitRenderWindow() { }
};

} // MrQ2
