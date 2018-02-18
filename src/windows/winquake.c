/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "winquake.h"
#include "client/client.h"
#include "client/cdaudio.h"
#include "client/sound.h"

//=============================================================================

winquake_t winquake = {0};
cvar_t * win_noalttab = NULL;

extern bool com_is_quitting;
extern unsigned sys_msg_time;
extern cvar_t * dedicated;
extern cvar_t * vid_fullscreen;
extern cvar_t * vid_height;
extern cvar_t * vid_width;

// Load assets from MrQuake2/data/baseq2/
#define BASEPATH_OVERRIDE "data"

//=============================================================================

/*
==================
WIN_Init
==================
*/
void WIN_Init()
{
    win_noalttab = Cvar_Get("win_noalttab", "0", CVAR_ARCHIVE);
}

/*
==================
scantokey[]
==================
*/
static const qbyte scantokey[128] = {
    0, 27, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', K_BACKSPACE, 9,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', 13, K_CTRL, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', K_SHIFT, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', K_SHIFT, '*',
    K_ALT, ' ', 0, K_F1, K_F2, K_F3, K_F4, K_F5,
    K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE, 0, K_HOME,
    K_UPARROW, K_PGUP, K_KP_MINUS, K_LEFTARROW, K_KP_5,
    K_RIGHTARROW, K_KP_PLUS, K_END, K_DOWNARROW, K_PGDN,
    K_INS, K_DEL, 0, 0, 0, K_F11, K_F12, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/*
==================
WIN_MapKey

Map from windows to quake keynums
==================
*/
static int WIN_MapKey(int key)
{
    int modified = (key >> 16) & 255;
    qboolean is_extended = false;

    if (modified > 127)
    {
        return 0;
    }
    if (key & (1 << 24))
    {
        is_extended = true;
    }

    int result = scantokey[modified];

    if (!is_extended)
    {
        switch (result)
        {
        case K_HOME:
            return K_KP_HOME;
        case K_UPARROW:
            return K_KP_UPARROW;
        case K_PGUP:
            return K_KP_PGUP;
        case K_LEFTARROW:
            return K_KP_LEFTARROW;
        case K_RIGHTARROW:
            return K_KP_RIGHTARROW;
        case K_END:
            return K_KP_END;
        case K_DOWNARROW:
            return K_KP_DOWNARROW;
        case K_PGDN:
            return K_KP_PGDN;
        case K_INS:
            return K_KP_INS;
        case K_DEL:
            return K_KP_DEL;
        default:
            return result;
        } // switch
    }
    else
    {
        switch (result)
        {
        case 0x0D:
            return K_KP_ENTER;
        case 0x2F:
            return K_KP_SLASH;
        case 0xAF:
            return K_KP_PLUS;
        } // switch
        return result;
    }
}

/*
==================
WIN_DisableAltTab
==================
*/
static void WIN_DisableAltTab(void)
{
    if (winquake.alttab_disabled)
    {
        return;
    }

    RegisterHotKey(NULL, 0, MOD_ALT, VK_TAB);
    RegisterHotKey(NULL, 1, MOD_ALT, VK_RETURN);
    winquake.alttab_disabled = true;
}

/*
==================
WIN_EnableAltTab
==================
*/
static void WIN_EnableAltTab(void)
{
    if (!winquake.alttab_disabled)
    {
        return;
    }

    UnregisterHotKey(NULL, 0);
    UnregisterHotKey(NULL, 1);
    winquake.alttab_disabled = false;
}

/*
==================
WIN_AppActivate
==================
*/
static void WIN_AppActivate(int active, int minimized)
{
    Sys_DebugOutput(va("WIN_AppActivate: active=%d, minimized=%d\n", active, minimized));

    winquake.minimized = minimized;
    Key_ClearStates();

    // we don't want to act like we're active if we're minimized
    if (active && !minimized)
    {
        winquake.active_app = true;
    }
    else
    {
        winquake.active_app = false;
    }

    // minimize/restore mouse-capture on demand
    if (!winquake.active_app)
    {
        IN_Activate(false);
        CDAudio_Activate(false);
        S_Activate(false);

        if (win_noalttab->value)
        {
            WIN_EnableAltTab();
        }
    }
    else
    {
        IN_Activate(true);
        CDAudio_Activate(true);
        S_Activate(true);

        if (win_noalttab->value)
        {
            WIN_DisableAltTab();
        }
    }
}

/*
==================
WIN_MainWndProc

Returns 0 if handled message, 1 if not
==================
*/
static LRESULT CALLBACK WIN_MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_HOTKEY: {
        // event consumed
        return 0;
    }
    case WM_CREATE: {
        winquake.hwnd = hWnd;
        break;
    }
    case WM_DESTROY: {
        // closing the window will quit the game!
        if (winquake.hwnd && winquake.hinstance)
        {
            Sys_DebugOutput("WM_DESTROY received, shutting down...\n");
            winquake.active_app = false;
            winquake.hinstance = NULL;
            winquake.hwnd = NULL;
            if (!com_is_quitting)
            {
                Com_Quit();
            }
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if ((int)HIWORD(wParam) > 0)
        {
            Key_Event(K_MWHEELUP, true, sys_msg_time);
            Key_Event(K_MWHEELUP, false, sys_msg_time);
        }
        else
        {
            Key_Event(K_MWHEELDOWN, true, sys_msg_time);
            Key_Event(K_MWHEELDOWN, false, sys_msg_time);
        }
        break;
    }
    case WM_PAINT: {
        // force entire screen to update next frame
        SCR_DirtyScreen();
        break;
    }
    case WM_ACTIVATE: {
        int active    = (int)LOWORD(wParam);
        int minimized = (int)HIWORD(wParam);
        WIN_AppActivate(active != WA_INACTIVE, !!minimized);
        if (re.AppActivate)
        {
            re.AppActivate(!(active == WA_INACTIVE));
        }
        break;
    }
    case WM_MOVE: {
        if (!vid_fullscreen->value)
        {
            int xPos = (int)LOWORD(lParam); // horizontal position
            int yPos = (int)HIWORD(lParam); // vertical position
            RECT r;
            r.left   = 0;
            r.top    = 0;
            r.right  = 1;
            r.bottom = 1;
            DWORD style = GetWindowLong(hWnd, GWL_STYLE);
            AdjustWindowRect(&r, style, FALSE);
            cvar_t * xpos = Cvar_Set("vid_xpos", va("%d", xPos + (int)r.left));
            cvar_t * ypos = Cvar_Set("vid_ypos", va("%d", yPos + (int)r.top));
            xpos->modified = false;
            ypos->modified = false;
            if (winquake.active_app)
            {
                IN_Activate(true);
            }
        }
        break;
    }
    // this is complicated because Win32 seems to pack multiple mouse events into
    // one update sometimes, so we always check all states and look for events
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEMOVE: {
        int temp = 0;
        if (wParam & MK_LBUTTON)
            temp |= 1;
        if (wParam & MK_RBUTTON)
            temp |= 2;
        if (wParam & MK_MBUTTON)
            temp |= 4;
        IN_MouseEvent(temp);
        break;
    }
    case WM_SYSCOMMAND: {
        if (wParam == SC_SCREENSAVE)
        {
            // don't allow screen saver to run
            return 0;
        }
        break;
    }
    case WM_SYSKEYDOWN: {
        if (wParam == 13)
        {
            if (vid_fullscreen)
            {
                Cvar_SetValue("vid_fullscreen", !vid_fullscreen->value);
            }
            return 0;
        }
    }
    // fall through
    case WM_KEYDOWN: {
        Key_Event(WIN_MapKey(lParam), true, sys_msg_time);
        break;
    }
    case WM_SYSKEYUP:
    case WM_KEYUP: {
        Key_Event(WIN_MapKey(lParam), false, sys_msg_time);
        break;
    }
    default: {
        // pass all unhandled messages to DefWindowProc
        break;
    }
    } // switch

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/*
==================
WIN_ParseCommandLine

Fills winquake.argc and argv[]
==================
*/
static void WIN_ParseCommandLine(LPSTR lpCmdLine)
{
    winquake.argc = 1;
    winquake.argv[0] = "MrQuake2.exe";

    while (*lpCmdLine && (winquake.argc < WINQUAKE_MAX_NUM_ARGVS))
    {
        while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
        {
            lpCmdLine++;
        }

        if (*lpCmdLine)
        {
            winquake.argv[winquake.argc] = lpCmdLine;
            winquake.argc++;

            while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
            {
                lpCmdLine++;
            }

            if (*lpCmdLine)
            {
                *lpCmdLine = 0;
                lpCmdLine++;
            }
        }
    }
}

/*
==================
Quake2 WinMain
==================
*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int Q_UNUSED_ARG(nCmdShow))
{
    MSG msg;
    int time, oldtime, newtime;

    // previous instances do not exist in Win32
    if (hPrevInstance)
    {
        MessageBox(NULL, "Prev instance was not null!", "Fatal Error", MB_OK);
        return 0;
    }

    #pragma warning(suppress: 4054) // 'type cast': from function pointer to data pointer 'void *'
    winquake.wndproc   = (void *)WIN_MainWndProc;
    winquake.hinstance = hInstance;

    WIN_ParseCommandLine(lpCmdLine);

    // Set a custom base path for development so we load assets from a normal directory instead of a pak.
    // This must be set before common init (FS init, actually).
#ifdef BASEPATH_OVERRIDE
    FS_SetDefaultBasePath(BASEPATH_OVERRIDE);
#endif // BASEPATH_OVERRIDE

    Qcommon_Init(winquake.argc, winquake.argv);
    oldtime = Sys_Milliseconds();

    // main window message loop
    for (;;)
    {
        // if at a full screen console or minimized, don't update at full speed
        if (winquake.minimized || (dedicated && dedicated->value))
        {
            Sleep(1);
        }

        while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            if (!GetMessage(&msg, NULL, 0, 0))
            {
                Com_Quit();
            }

            sys_msg_time = msg.time;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        do
        {
            newtime = Sys_Milliseconds();
            time = newtime - oldtime;
        } while (time < 1);

        Qcommon_Frame(time);
        oldtime = newtime;
    }
}
