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

// vid_null.c -- null video driver to aid porting efforts
// this assumes that one of the refs is statically linked to the executable

#include "client/client.h"

viddef_t viddef; // global video state
refexport_t re;

/*
==========================================================================

NULL RENDERER

==========================================================================
*/

static int NullRef_Init(void * hInstance, void * wndproc, int fullscreen) { return 0; }
static void NullRef_Shutdown(void) {}
static void NullRef_BeginRegistration(const char * map_name) {}
static struct model_s * NullRef_RegisterModel(const char * name) { return NULL; }
static struct image_s * NullRef_RegisterSkin(const char * name) { return NULL; }
static struct image_s * NullRef_RegisterPic(const char * name) { return NULL; }
static void NullRef_SetSky(const char * name, float rotate, vec3_t axis) {}
static void NullRef_EndRegistration(void) {}
static void NullRef_RenderFrame(refdef_t * fd) {}
static void NullRef_DrawGetPicSize(int * w, int * h, const char * name) {}
static void NullRef_DrawPic(int x, int y, const char * name) {}
static void NullRef_DrawStretchPic(int x, int y, int w, int h, const char * name) {}
static void NullRef_DrawChar(int x, int y, int c) {}
static void NullRef_DrawTileClear(int x, int y, int w, int h, const char * name) {}
static void NullRef_DrawFill(int x, int y, int w, int h, int c) {}
static void NullRef_DrawFadeScreen(void) {}
static void NullRef_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const qbyte * data) {}
static void NullRef_CinematicSetPalette(const qbyte * palette) {}
static void NullRef_BeginFrame(float camera_separation) {}
static void NullRef_EndFrame(void) {}
static void NullRef_AppActivate(int activate) {}

refexport_t GetRefAPI(refimport_t rimp)
{
    (void)rimp;
    refexport_t refexp = {};
    refexp.api_version = REF_API_VERSION;
    refexp.Init = NullRef_Init;
    refexp.Shutdown = NullRef_Shutdown;
    refexp.BeginRegistration = NullRef_BeginRegistration;
    refexp.RegisterModel = NullRef_RegisterModel;
    refexp.RegisterSkin = NullRef_RegisterSkin;
    refexp.RegisterPic = NullRef_RegisterPic;
    refexp.SetSky = NullRef_SetSky;
    refexp.EndRegistration = NullRef_EndRegistration;
    refexp.RenderFrame = NullRef_RenderFrame;
    refexp.DrawGetPicSize = NullRef_DrawGetPicSize;
    refexp.DrawPic = NullRef_DrawPic;
    refexp.DrawStretchPic = NullRef_DrawStretchPic;
    refexp.DrawChar = NullRef_DrawChar;
    refexp.DrawTileClear = NullRef_DrawTileClear;
    refexp.DrawFill = NullRef_DrawFill;
    refexp.DrawFadeScreen = NullRef_DrawFadeScreen;
    refexp.DrawStretchRaw = NullRef_DrawStretchRaw;
    refexp.CinematicSetPalette = NullRef_CinematicSetPalette;
    refexp.BeginFrame = NullRef_BeginFrame;
    refexp.EndFrame = NullRef_EndFrame;
    refexp.AppActivate = NullRef_AppActivate;
    return refexp;
}

/*
==========================================================================

DIRECT LINK GLUE

==========================================================================
*/

enum
{
    MAXPRINTMSG = 4096
};

void VID_Printf(print_level_t print_level, const char * fmt, ...)
{
    va_list argptr;
    char msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    vsprintf(msg, fmt, argptr);
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

void VID_Error(error_level_t err_level, const char * fmt, ...)
{
    va_list argptr;
    char msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    vsprintf(msg, fmt, argptr);
    va_end(argptr);

    Com_Error(err_level, "%s", msg);
}

void VID_NewWindow(int width, int height)
{
    viddef.width = width;
    viddef.height = height;
}

/*
** VID_GetModeInfo
*/
typedef struct vidmode_s
{
    const char * description;
    int width, height;
    int mode;
} vidmode_t;

static const vidmode_t vid_modes[] =
{
  { "Mode 0: 320x240", 320, 240, 0 },
  { "Mode 1: 400x300", 400, 300, 1 },
  { "Mode 2: 512x384", 512, 384, 2 },
  { "Mode 3: 640x480", 640, 480, 3 },
  { "Mode 4: 800x600", 800, 600, 4 },
  { "Mode 5: 960x720", 960, 720, 5 },
  { "Mode 6: 1024x768", 1024, 768, 6 },
  { "Mode 7: 1152x864", 1152, 864, 7 },
  { "Mode 8: 1280x960", 1280, 960, 8 },
  { "Mode 9: 1600x1200", 1600, 1200, 9 }
};
enum
{
    VID_NUM_MODES = sizeof(vid_modes) / sizeof(vid_modes[0])
};

int VID_GetModeInfo(int * width, int * height, int mode)
{
    if (mode < 0 || mode >= VID_NUM_MODES)
    {
        return false;
    }

    *width = vid_modes[mode].width;
    *height = vid_modes[mode].height;

    return true;
}

void VID_Init(void)
{
    refimport_t ri = {};

    viddef.width = 320;
    viddef.height = 240;

    ri.Sys_Error = VID_Error;
    ri.Con_Printf = VID_Printf;
    ri.Cmd_AddCommand = Cmd_AddCommand;
    ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
    ri.Cmd_ExecuteText = Cbuf_ExecuteText;
    ri.Cmd_Argc = Cmd_Argc;
    ri.Cmd_Argv = Cmd_Argv;
    ri.FS_LoadFile = FS_LoadFile;
    ri.FS_FreeFile = FS_FreeFile;
    ri.FS_Gamedir = FS_Gamedir;
    ri.Cvar_Get = Cvar_Get;
    ri.Cvar_Set = Cvar_Set;
    ri.Cvar_SetValue = Cvar_SetValue;
    ri.Vid_MenuInit = VID_MenuInit;
    ri.Vid_NewWindow = VID_NewWindow;
    ri.Vid_GetModeInfo = VID_GetModeInfo;

    re = GetRefAPI(ri);

    if (re.api_version != REF_API_VERSION)
    {
        Com_Error(ERR_FATAL, "Refresh has incompatible api_version");
    }

    // call the init function
    if (re.Init(NULL, NULL, 0) == -1)
    {
        Com_Error(ERR_FATAL, "Couldn't start refresh");
    }
}

void VID_Shutdown(void)
{
    if (re.Shutdown)
    {
        re.Shutdown();
    }
}

void VID_CheckChanges(void)
{
}

void VID_MenuInit(void)
{
}

void VID_MenuDraw(void)
{
}

const char * VID_MenuKey(int k)
{
    (void)k;
    return NULL;
}
