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

    HINSTANCE   hinst            = nullptr;
    WNDPROC     wndproc          = nullptr;
    HWND        hwnd             = nullptr;
    std::string window_name      = {};
    int         width            = 0;
    int         height           = 0;
    bool        fullscreen       = false;
    bool        debug_validation = false; // Enable D3D-level debug validation?

    OSWindow() = default;
    virtual ~OSWindow();

    void Init(const char * name, HINSTANCE hInst, WNDPROC wndProc,
              const int w, const int h, const bool fs, const bool debug);

    // Disallow copy.
    OSWindow(const OSWindow &) = delete;
    OSWindow & operator=(const OSWindow &) = delete;

    // Static helpers
    static std::string ErrorToString(HRESULT hr);
    static std::string CurrentWorkingDir();

protected:

    // Implemented by the RenderWindow.
    virtual void InitRenderWindow() { }

private:

    void Create();
    void Destroy();
};

} // MrQ2
