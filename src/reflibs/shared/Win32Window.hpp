//
// Win32Window.hpp
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

    Win32Window

===============================================================================
*/
class Win32Window
{
public:

    Win32Window() = default;
    virtual ~Win32Window();

    void Init(const char * name, HINSTANCE hInst, WNDPROC wndProc,
              const int w, const int h, const bool fullscreen, const bool debug);

    int  Width()           const { return m_width;            }
    int  Height()          const { return m_height;           }
    bool FullScreen()      const { return m_fullscreen;       }
    bool DebugValidation() const { return m_debug_validation; }

    // Disallow copy.
    Win32Window(const Win32Window &) = delete;
    Win32Window & operator=(const Win32Window &) = delete;

    // Static helpers
    static std::string ErrorToString(HRESULT hr);
    static std::string CurrentWorkingDir();

protected:

    HINSTANCE   m_hInst            = nullptr;
    WNDPROC     m_wndproc          = nullptr;
    HWND        m_hWnd             = nullptr;
    std::string m_window_name      = {};
    int         m_width            = 0;
    int         m_height           = 0;
    bool        m_fullscreen       = false;
    bool        m_debug_validation = false; // Enable D3D-level debug validation?

    // Implemented by the RenderWindow.
    virtual void InitRenderWindow() { }

private:

    void Create();
    void Destroy();
};

} // MrQ2
