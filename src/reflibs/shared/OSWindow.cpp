//
// OSWindow.cpp
//  Application window creation and management.
//

#include "OSWindow.hpp"
#include "RefShared.hpp"

#include <comdef.h>
#include <direct.h>

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// OSWindow
///////////////////////////////////////////////////////////////////////////////

void OSWindow::Init(const char * name, HINSTANCE hInst, WNDPROC wndProc,
                    const int w, const int h, const bool fs, const bool debug)
{
    this->hinst            = hInst;
    this->wndproc          = wndProc;
    this->window_name      = name;
    this->width            = w;
    this->height           = h;
    this->fullscreen       = fs;
    this->debug_validation = debug;

    Create();
}

///////////////////////////////////////////////////////////////////////////////

void OSWindow::Create()
{
    WNDCLASSEX wcex    = {0};
    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcex.lpfnWndProc   = wndproc;
    wcex.hInstance     = hinst;
    wcex.lpszClassName = window_name.c_str();
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);

    const auto result = RegisterClassEx(&wcex);
    if (!result)
    {
        GameInterface::Errorf("RegisterClassEx failed with error code %#x", unsigned(result));
    }

    unsigned exstyle, stylebits;
    if (fullscreen)
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
    r.right  = width;
    r.bottom = height;
    AdjustWindowRect(&r, stylebits, FALSE);

    int x = 0, y = 0;
    const int w = int(r.right  - r.left);
    const int h = int(r.bottom - r.top);
    GameInterface::Printf("Creating window %ix%i ...", w, h);

    if (!fullscreen)
    {
        auto vid_xpos = GameInterface::Cvar::Get("vid_xpos", "0", 0);
        auto vid_ypos = GameInterface::Cvar::Get("vid_ypos", "0", 0);
        x = vid_xpos.AsInt();
        y = vid_ypos.AsInt();
    }

    hwnd = CreateWindowEx(exstyle, window_name.c_str(), window_name.c_str(),
                          stylebits, x, y, w, h, nullptr, nullptr, hinst, nullptr);
    if (!hwnd)
    {
        GameInterface::Errorf("Couldn't create application window!");
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    InitRenderWindow();

    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    GameInterface::Video::NewWindow(width, height);
}

///////////////////////////////////////////////////////////////////////////////

void OSWindow::Destroy()
{
    if (hwnd != nullptr)
    {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
    if (hinst != nullptr)
    {
        UnregisterClass(window_name.c_str(), hinst);
        hinst = nullptr;
    }

    wndproc = nullptr;
    window_name.clear();
}

///////////////////////////////////////////////////////////////////////////////

OSWindow::~OSWindow()
{
    Destroy();
}

///////////////////////////////////////////////////////////////////////////////

std::string OSWindow::ErrorToString(HRESULT hr)
{
    _com_error err(hr);
    const char * errmsg = err.ErrorMessage();
    return errmsg;
}

///////////////////////////////////////////////////////////////////////////////

std::string OSWindow::CurrentWorkingDir()
{
    char cwd[512] = {0};
    _getcwd(cwd, sizeof(cwd));
    return cwd;
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
