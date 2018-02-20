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

#include "common/q_common.h"
#include "game/game.h"
#include "winquake.h"

#include <conio.h>
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>

#pragma comment(lib, "Winmm.lib") // timeGetTime, Joystick API, etc

//=============================================================================

int sys_curtime         = 0;
unsigned sys_msg_time   = 0;
unsigned sys_frame_time = 0;

//=============================================================================

/*
==================
Sys_DebugOutput

Prints to the Visual Studio output window
==================
*/
void Sys_DebugOutput(const char * message)
{
    OutputDebugStringA(message);
}

/*
================
Sys_Init
================
*/
void Sys_Init(void)
{
    timeBeginPeriod(1);

    OSVERSIONINFO vinfo = {0};
    vinfo.dwOSVersionInfoSize = sizeof(vinfo);

    #pragma warning(suppress: 4996) // 'GetVersionExA': was declared deprecated
    if (!GetVersionEx(&vinfo))
    {
        Sys_Error("Couldn't get OS info\n");
    }

    if (vinfo.dwMajorVersion < 4)
    {
        Sys_Error("Quake2 requires windows version 4 or greater\n");
    }

    if (vinfo.dwPlatformId == VER_PLATFORM_WIN32s)
    {
        Sys_Error("Quake2 doesn't run on Win32s\n");
    }
    else if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
    {
        Sys_DebugOutput("Found suitable Windows version...\n");
    }

    // Other Windows-specific initialization
    WIN_Init();
}

/*
================
Sys_ConsoleInput

Read input text from the dedicated console
================
*/
char * Sys_ConsoleInput(void)
{
    // TODO: unimplemented; no dedicated console
    return NULL;
}

/*
================
Sys_ConsoleOutput

Print text to the dedicated console
================
*/
void Sys_ConsoleOutput(const char * string)
{
    // TODO: no dedicated console at the moment
    Sys_DebugOutput(string);
}

/*
==================
Sys_Error

Error/abnormal program termination
==================
*/
void Sys_Error(const char * error, ...)
{
    va_list argptr;
    char text[4096] = {0};

    timeEndPeriod(1);
    CL_Shutdown();
    Qcommon_Shutdown();

    va_start(argptr, error);
    vsnprintf(text, sizeof(text), error, argptr);
    va_end(argptr);

    Sys_DebugOutput(text);
    MessageBox(NULL, text, "Fatal Error", MB_OK);
    abort();
}

/*
==================
Sys_Quit

Normal/clean program exit
==================
*/
void Sys_Quit(void)
{
    Sys_DebugOutput("Sys_Quit called...\n");

    timeEndPeriod(1);
    CL_Shutdown();
    Qcommon_Shutdown();

    exit(EXIT_SUCCESS);
}

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate(void)
{
    ShowWindow(winquake.hwnd, SW_RESTORE);
    SetForegroundWindow(winquake.hwnd);
}

/*
================
Sys_GetClipboardData
================
*/
char * Sys_GetClipboardData(void)
{
    char * data = NULL;
    char * cliptext;

    if (OpenClipboard(NULL) != 0)
    {
        HANDLE hClipboardData;
        if ((hClipboardData = GetClipboardData(CF_TEXT)) != 0)
        {
            if ((cliptext = GlobalLock(hClipboardData)) != 0)
            {
                data = (char *)Z_Malloc((int)(GlobalSize(hClipboardData) + 1));
                strcpy(data, cliptext);
                GlobalUnlock(hClipboardData);
            }
        }
        CloseClipboard();
    }
    return data;
}

/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents(void)
{
    MSG msg;

    while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
    {
        if (!GetMessage(&msg, NULL, 0, 0))
        {
            Sys_Quit();
        }
        sys_msg_time = msg.time;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // grab frame time
    sys_frame_time = timeGetTime(); // FIXME: should this be at start?
}

/*
================
Sys_Milliseconds
================
*/
int Sys_Milliseconds(void)
{
    static int baseTime;
    static qboolean initialized = false;

    if (!initialized)
    {
        baseTime = timeGetTime();
        initialized = true;
    }

    sys_curtime = timeGetTime() - baseTime;
    return sys_curtime;
}

/*
================
Sys_Mkdir
================
*/
void Sys_Mkdir(const char * path)
{
    _mkdir(path);
}

/*
================
Sys_UnloadGame
================
*/
void Sys_UnloadGame(void)
{
    // Nothing to do here, since we are
    // not dealing with dynamic link libraries.
}

/*
================
Sys_GetGameAPI
================
*/
void * Sys_GetGameAPI(void * parms)
{
    // In the original Quake2, id Software used a DLL for the
    // game code, while the Engine code was in the executable.
    // This function was where the Engine loaded the game DLL
    // and then called GetGameAPI from the DLL. In this project,
    // I will statically link with the game to keep things simple.
    return GetGameAPI((game_import_t *)parms);
}

//=============================================================================
// New memory API
//=============================================================================

static void (*g_MallocHook)(void *, size_t, game_memtag_t) = NULL;
static void (*g_MfreeHook) (void *, size_t, game_memtag_t) = NULL;

/*
================
Sys_Malloc - malloc() hook
================
*/
void * Sys_Malloc(size_t size_bytes, game_memtag_t mem_tag)
{
    void * ptr = malloc(size_bytes);

    if (g_MallocHook != NULL)
    {
        g_MallocHook(ptr, size_bytes, mem_tag);
    }

    return ptr;
}

/*
================
Sys_Mfree - free() hook
================
*/
void Sys_Mfree(void * ptr, size_t size_bytes, game_memtag_t mem_tag)
{
    if (ptr != NULL)
    {
        if (g_MfreeHook != NULL)
        {
            g_MfreeHook(ptr, size_bytes, mem_tag);
        }

        free(ptr);
    }
}

/*
================
Sys_SetMemoryHooks
================
*/
void Sys_SetMemoryHooks(void (*allocHook)(void *, size_t, game_memtag_t),
                        void (*freeHook) (void *, size_t, game_memtag_t))
{
    g_MallocHook = allocHook;
    g_MfreeHook  = freeHook;
}

//=============================================================================
// Find file API
//=============================================================================

static char findbase[MAX_OSPATH];
static char findpath[MAX_OSPATH];
static intptr_t findhandle = 0;

static qboolean CompareAttributes(unsigned found, unsigned musthave, unsigned canthave)
{
    if ((found & _A_RDONLY) && (canthave & SFF_RDONLY))
        return false;
    if ((found & _A_HIDDEN) && (canthave & SFF_HIDDEN))
        return false;
    if ((found & _A_SYSTEM) && (canthave & SFF_SYSTEM))
        return false;
    if ((found & _A_SUBDIR) && (canthave & SFF_SUBDIR))
        return false;
    if ((found & _A_ARCH) && (canthave & SFF_ARCH))
        return false;

    if ((musthave & SFF_RDONLY) && !(found & _A_RDONLY))
        return false;
    if ((musthave & SFF_HIDDEN) && !(found & _A_HIDDEN))
        return false;
    if ((musthave & SFF_SYSTEM) && !(found & _A_SYSTEM))
        return false;
    if ((musthave & SFF_SUBDIR) && !(found & _A_SUBDIR))
        return false;
    if ((musthave & SFF_ARCH) && !(found & _A_ARCH))
        return false;

    return true;
}

/*
================
Sys_FindFirst
================
*/
char * Sys_FindFirst(const char * path, unsigned musthave, unsigned canthave)
{
    struct _finddata_t findinfo;

    if (findhandle)
    {
        Sys_Error("Sys_FindFirst without close\n");
    }
    findhandle = 0;

    COM_FilePath(path, findbase);
    findhandle = _findfirst(path, &findinfo);

    if (findhandle == -1)
    {
        return NULL;
    }
    if (!CompareAttributes(findinfo.attrib, musthave, canthave))
    {
        return NULL;
    }

    Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
    return findpath;
}

/*
================
Sys_FindNext
================
*/
char * Sys_FindNext(unsigned musthave, unsigned canthave)
{
    struct _finddata_t findinfo;

    if (findhandle == -1)
    {
        return NULL;
    }
    if (_findnext(findhandle, &findinfo) == -1)
    {
        return NULL;
    }
    if (!CompareAttributes(findinfo.attrib, musthave, canthave))
    {
        return NULL;
    }

    Com_sprintf(findpath, sizeof(findpath), "%s/%s", findbase, findinfo.name);
    return findpath;
}

/*
================
Sys_FindClose
================
*/
void Sys_FindClose(void)
{
    if (findhandle != -1)
    {
        _findclose(findhandle);
    }
    findhandle = 0;
}
