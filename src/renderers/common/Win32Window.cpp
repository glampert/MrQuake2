//
// Win32Window.cpp
//  Application window creation and management.
//

#include "Win32Window.hpp"
#include "Common.hpp"

#include <comdef.h>
#include <direct.h>

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// Win32Window
///////////////////////////////////////////////////////////////////////////////

void Win32Window::Init(const char * name, HINSTANCE hInst, WNDPROC wndProc, const int width, const int height, const bool fullscreen)
{
    MRQ2_ASSERT(hInst   != nullptr);
    MRQ2_ASSERT(wndProc != nullptr);

    char title[256] = {};
    sprintf_s(title, "%s %ix%i", name, width, height);

    m_hInst            = hInst;
    m_wndproc          = wndProc;
    m_width            = width;
    m_height           = height;
    m_fullscreen       = fullscreen;
    m_window_name      = title;

    WNDCLASSEX wcex    = {};
    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcex.lpfnWndProc   = m_wndproc;
    wcex.hInstance     = m_hInst;
    wcex.lpszClassName = m_window_name.c_str();
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);

    const auto result = RegisterClassEx(&wcex);
    if (!result)
    {
        GameInterface::Errorf("RegisterClassEx failed with error code: %#x", unsigned(GetLastError()));
    }

    unsigned exstyle, stylebits;
    if (m_fullscreen)
    {
        exstyle   = WS_EX_TOPMOST;
        stylebits = WS_POPUP | WS_VISIBLE;
    }
    else
    {
        exstyle   = WS_EX_APPWINDOW;
        stylebits = WS_OVERLAPPEDWINDOW | WS_BORDER | WS_CAPTION | WS_VISIBLE;
    }

    RECT r;
    r.left   = 0;
    r.top    = 0;
    r.right  = m_width;
    r.bottom = m_height;
    AdjustWindowRect(&r, stylebits, FALSE);

    int x = 0, y = 0;
    const int w = int(r.right  - r.left);
    const int h = int(r.bottom - r.top);
    GameInterface::Printf("Creating window %ix%i ...", w, h);

    if (!m_fullscreen)
    {
        auto vid_xpos = GameInterface::Cvar::Get("vid_xpos", "0", 0);
        auto vid_ypos = GameInterface::Cvar::Get("vid_ypos", "0", 0);
        x = vid_xpos.AsInt();
        y = vid_ypos.AsInt();
    }

    m_hWnd = CreateWindowEx(exstyle, m_window_name.c_str(), m_window_name.c_str(),
                            stylebits, x, y, w, h, nullptr, nullptr, m_hInst, nullptr);
    if (!m_hWnd)
    {
        GameInterface::Errorf("Couldn't create application window!");
    }

    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);

    SetForegroundWindow(m_hWnd);
    SetFocus(m_hWnd);

    GameInterface::Video::NewWindow(m_width, m_height);
}

///////////////////////////////////////////////////////////////////////////////

void Win32Window::Shutdown()
{
    if (m_hWnd != nullptr)
    {
        if (!DestroyWindow(m_hWnd))
        {
            GameInterface::Printf("WARNING: DestroyWindow failed with error code: %#x", unsigned(GetLastError()));
        }
        m_hWnd = nullptr;
    }

    if (m_hInst != nullptr)
    {
        if (!UnregisterClass(m_window_name.c_str(), m_hInst))
        {
            GameInterface::Printf("WARNING: UnregisterClass failed with error code: %#x", unsigned(GetLastError()));
        }
        m_hInst = nullptr;
    }

    m_wndproc = nullptr;
    m_window_name.clear();
}

///////////////////////////////////////////////////////////////////////////////

std::string Win32Window::ErrorToString(HRESULT hr)
{
    _com_error err(hr);
    const char * errmsg = err.ErrorMessage();
    return errmsg;
}

///////////////////////////////////////////////////////////////////////////////

std::string Win32Window::CurrentWorkingDir()
{
    char cwd[512] = {};
    _getcwd(cwd, sizeof(cwd));
    return cwd;
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
