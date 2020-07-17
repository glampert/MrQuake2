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

    // Disallow copy.
    Win32Window(const Win32Window &) = delete;
    Win32Window & operator=(const Win32Window &) = delete;

    void Init(const char * name, HINSTANCE hInst, WNDPROC wndProc, const int width, const int height, const bool fullscreen);
    void Shutdown();

    int  Width()        const { return m_width; }
    int  Height()       const { return m_height; }
    bool IsFullScreen() const { return m_fullscreen; }
    HWND WindowHandle() const { return m_hWnd; }

    // Static helpers
    static std::string ErrorToString(HRESULT hr);
    static std::string CurrentWorkingDir();

private:

    HINSTANCE   m_hInst{ nullptr };
    WNDPROC     m_wndproc{ nullptr };
    HWND        m_hWnd{ nullptr };
    int         m_width{ 0 };
    int         m_height{ 0 };
    bool        m_fullscreen{ false };
    std::string m_window_name;
};

} // MrQ2
