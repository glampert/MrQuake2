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

#include "client/client.h"
#include "winquake.h"

//=============================================================================

viddef_t viddef = {0}; // global video state
refexport_t re  = {0}; // renderer dll interface

cvar_t * vid_ref        = NULL;
cvar_t * vid_height     = NULL;
cvar_t * vid_width      = NULL;
cvar_t * vid_fullscreen = NULL;

static HMODULE reflib_dll = NULL;

typedef struct
{
    const char * description;
    int width, height;
    int mode;
} vidmode_t;

static const vidmode_t vid_modes[] = {
  { "Mode 0: 320x240",   320,  240,  0 },
  { "Mode 1: 400x300",   400,  300,  1 },
  { "Mode 2: 512x384",   512,  384,  2 },
  { "Mode 3: 640x480",   640,  480,  3 },
  { "Mode 4: 800x600",   800,  600,  4 },
  { "Mode 5: 960x720",   960,  720,  5 },
  { "Mode 6: 1024x768",  1024, 768,  6 },
  { "Mode 7: 1152x864",  1152, 864,  7 },
  { "Mode 8: 1280x960",  1280, 960,  8 },
  { "Mode 9: 1600x1200", 1600, 1200, 9 }
};

enum { VID_NUM_MODES = sizeof(vid_modes) / sizeof(vid_modes[0]) };
enum { MAXPRINTMSG = 4096 };

//=============================================================================

/*
==================
VID_Printf
==================
*/
void VID_Printf(print_level_t print_level, const char * fmt, ...)
{
    va_list argptr;
    char msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    if (print_level == PRINT_ALL)
    {
        Com_Printf("%s", msg);
    }
    else
    {
        Com_DPrintf("%s", msg);
    }
}

/*
==================
VID_Error
==================
*/
void VID_Error(error_level_t err_level, const char * fmt, ...)
{
    va_list argptr;
    char msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    Com_Error(err_level, "%s", msg);
}

/*
==================
VID_NewWindow
==================
*/
void VID_NewWindow(int width, int height)
{
    viddef.width  = width;
    viddef.height = height;
}

/*
==================
VID_GetModeInfo
==================
*/
int VID_GetModeInfo(int * width, int * height, int mode)
{
    if (mode < 0 || mode >= VID_NUM_MODES)
    {
        return false;
    }

    *width  = vid_modes[mode].width;
    *height = vid_modes[mode].height;
    return true;
}

/*
==================
VID_UnloadRefreshDLL
==================
*/
void VID_UnloadRefreshDLL()
{
    if (reflib_dll)
    {
        Com_DPrintf("Unloading Reflib...\n");
        if (!FreeLibrary(reflib_dll))
        {
            Com_Error(ERR_FATAL, "Reflib FreeLibrary failed!\n");
        }
        reflib_dll = NULL;
    }
    memset(&re, 0, sizeof(re));
}

/*
==================
VID_LoadRefreshDLL
==================
*/
void VID_LoadRefreshDLL(const char * dll_name)
{
    refimport_t ri;
    GetRefAPI_t GetRefAPI;

    VID_Shutdown();
    Com_Printf("---- Loading Refresh DLL %s ----\n", dll_name);

    reflib_dll = LoadLibrary(dll_name);
    if (!reflib_dll)
    {
        Com_Error(ERR_FATAL, "LoadLibrary('%s') failed!\n", dll_name);
    }

    GetRefAPI = (GetRefAPI_t)GetProcAddress(reflib_dll, "GetRefAPI");
    if (!GetRefAPI)
    {
        Com_Error(ERR_FATAL, "GetProcAddress failed on %s\n", dll_name);
    }

    ri.Sys_Error         = VID_Error;
    ri.Con_Printf        = VID_Printf;
    ri.Cmd_AddCommand    = Cmd_AddCommand;
    ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
    ri.Cmd_ExecuteText   = Cbuf_ExecuteText;
    ri.Cmd_Argc          = Cmd_Argc;
    ri.Cmd_Argv          = Cmd_Argv;
    ri.FS_LoadFile       = FS_LoadFile;
    ri.FS_FreeFile       = FS_FreeFile;
    ri.FS_Gamedir        = FS_Gamedir;
    ri.Cvar_Get          = Cvar_Get;
    ri.Cvar_Set          = Cvar_Set;
    ri.Cvar_SetValue     = Cvar_SetValue;
    ri.Vid_MenuInit      = VID_MenuInit;
    ri.Vid_NewWindow     = VID_NewWindow;
    ri.Vid_GetModeInfo   = VID_GetModeInfo;

    re = GetRefAPI(ri);

    if (re.api_version != REF_API_VERSION)
    {
        VID_UnloadRefreshDLL();
        Com_Error(ERR_FATAL, "Refresh %s has incompatible API version!\n", dll_name);
    }

    if (!re.Init(winquake.hinstance, winquake.wndproc, (int)vid_fullscreen->value))
    {
        VID_UnloadRefreshDLL();
        Com_Error(ERR_FATAL, "Couldn't start refresh %s!\n", dll_name);
    }

    vidref_val = re.vidref;
    Com_Printf( "------------------------------------\n");
}

/*
==================
VID_Init
==================
*/
void VID_Init(void)
{
    vid_ref = Cvar_Get("vid_ref", "D3D11", CVAR_ARCHIVE);
    vid_width = Cvar_Get("vid_width", "1024", CVAR_ARCHIVE);
    vid_height = Cvar_Get("vid_height", "768", CVAR_ARCHIVE);
    vid_fullscreen = Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);

    viddef.width  = (int)vid_width->value;
    viddef.height = (int)vid_height->value;

    char dll_name[256];
    Com_sprintf(dll_name, sizeof(dll_name), "Ref%s.dll", vid_ref->string);
    VID_LoadRefreshDLL(dll_name);
}

/*
==================
VID_Shutdown
==================
*/
void VID_Shutdown(void)
{
    if (re.Shutdown)
    {
        re.Shutdown();
    }
    VID_UnloadRefreshDLL();
}

/*
==================
VID_CheckChanges
==================
*/
void VID_CheckChanges(void)
{
    // TODO
}

/*
==================
VID_MenuInit
==================
*/
void VID_MenuInit(void)
{
    // TODO
}

/*
==================
VID_MenuDraw
==================
*/
void VID_MenuDraw(void)
{
    // TODO
}

/*
==================
VID_MenuKey
==================
*/
const char * VID_MenuKey(int k)
{
    // TODO
    (void)k;
    return NULL;
}
