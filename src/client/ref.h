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

#ifndef CL_REF_H
#define CL_REF_H

#include "common/q_common.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define REF_API_VERSION       3
#define ENTITY_FLAGS          68
#define POWERSUIT_SCALE       4.0f

#define MAX_DLIGHTS           32
#define MAX_ENTITIES          128
#define MAX_LIGHTSTYLES       256
#define MAX_PARTICLES         4096

#define SHELL_RED_COLOR       0xF2
#define SHELL_GREEN_COLOR     0xD0
#define SHELL_BLUE_COLOR      0xF3
#define SHELL_RG_COLOR        0xDC
#define SHELL_RB_COLOR        0x68
#define SHELL_BG_COLOR        0x78
#define SHELL_DOUBLE_COLOR    0xDF // 223
#define SHELL_HALF_DAM_COLOR  0x90
#define SHELL_CYAN_COLOR      0x72
#define SHELL_WHITE_COLOR     0xD7

//=============================================================================
//
// Renderer representation of the game entities/lights/particles:
//
//=============================================================================

typedef struct entity_s
{
    struct model_s * model; // opaque type outside refresh
    float angles[3];

    // most recent data:
    float origin[3]; // also used as RF_BEAM's "from"
    int frame;       // also used as RF_BEAM's diameter

    // previous data for lerping:
    float oldorigin[3]; // also used as RF_BEAM's "to"
    int oldframe;

    // misc:
    float backlerp; // 0.0 = current, 1.0 = old
    int skinnum;    // also used as RF_BEAM's palette index

    int lightstyle; // for flashing entities
    float alpha;    // ignore if RF_TRANSLUCENT isn't set

    struct image_s * skin; // NULL for inline skin
    int flags;
} entity_t;

typedef struct dlight_s
{
    vec3_t origin;
    vec3_t color;
    float intensity;
} dlight_t;

typedef struct particle_s
{
    vec3_t origin;
    int color;
    float alpha;
} particle_t;

typedef struct lightstyle_s
{
    float rgb[3]; // 0.0 - 2.0
    float white;  // highest of rgb
} lightstyle_t;

typedef struct refdef_s
{
    int x, y, width, height;    // in virtual screen coordinates
    float fov_x, fov_y;         // field of view
    float vieworg[3];           // viewer origin
    float viewangles[3];        // viewer rotation angles
    float blend[4];             // rgba 0-1 full screen blend (for R_Flash)
    float time;                 // time is used to auto animate
    int rdflags;                // RDF_NOWORLDMODEL, RDF_UNDERWATER, etc

    qbyte * areabits;            // if not NULL, only areas with set bits will be drawn
    lightstyle_t * lightstyles; // [MAX_LIGHTSTYLES]

    // Non-world geometry:
    int num_entities;
    entity_t * entities;

    int num_dlights;
    dlight_t * dlights;

    int num_particles;
    particle_t * particles;
} refdef_t;

//=============================================================================
//
// These are the functions exported by the refresh module
//
//=============================================================================

typedef struct refexport_s
{
    // if api_version is different, the dll cannot be used
    int api_version;
    vidref_type_t vidref;

    // called when the library is loaded
    int (*Init)(void * hInstance, void * wndproc, int fullscreen);

    // called before the library is unloaded
    void (*Shutdown)(void);

    // All data that will be used in a level should be
    // registered before rendering any frames to prevent disk hits,
    // but they can still be registered at a later time
    // if necessary.
    //
    // EndRegistration will free any remaining data that wasn't registered.
    // Any model_s or skin_s pointers from before the BeginRegistration
    // are no longer valid after EndRegistration.
    //
    // Skins and images need to be differentiated, because skins
    // are flood filled to eliminate mip map edge errors, and pics have
    // an implicit "pics/" prepended to the name. (a pic name that starts with a
    // slash will not use the "pics/" prefix or the ".pcx" postfix)
    void (*BeginRegistration)(const char * map_name);
    struct model_s * (*RegisterModel)(const char * name);
    struct image_s * (*RegisterSkin)(const char * name);
    struct image_s * (*RegisterPic)(const char * name);
    void (*SetSky)(const char * name, float rotate, vec3_t axis);
    void (*EndRegistration)(void);

    // renders a 3D game view.
    void (*RenderFrame)(refdef_t * fd);

    // 2D overlay drawing:
    void (*DrawGetPicSize)(int * w, int * h, const char * name); // will return 0 0 if not found
    void (*DrawPic)(int x, int y, const char * name);
    void (*DrawStretchPic)(int x, int y, int w, int h, const char * name);
    void (*DrawChar)(int x, int y, int c);
    void (*DrawTileClear)(int x, int y, int w, int h, const char * name);
    void (*DrawFill)(int x, int y, int w, int h, int c);
    void (*DrawFadeScreen)(void);

    // Draw images for cinematic rendering (which can have a different palette). Note that calls
    void (*DrawStretchRaw)(int x, int y, int w, int h, int cols, int rows, const qbyte * data);

    // video mode and refresh state management entry points:
    void (*CinematicSetPalette)(const qbyte * palette); // NULL = game palette
    void (*BeginFrame)(float camera_separation);
    void (*EndFrame)(void);

    void (*AppActivate)(int activate);

} refexport_t;

//=============================================================================
//
// These are the functions imported by the refresh module
//
//=============================================================================

typedef struct refimport_s
{
    void (*Sys_Error)(error_level_t err_level, const char * str, ...);
    void (*Con_Printf)(print_level_t print_level, const char * str, ...);

    void (*Cmd_AddCommand)(const char * name, void (*cmd)(void));
    void (*Cmd_RemoveCommand)(const char * name);
    void (*Cmd_ExecuteText)(cmd_exec_when_t exec_when, const char * text);

    int (*Cmd_Argc)(void);
    const char * (*Cmd_Argv)(int i);

    // files will be memory mapped read only
    // the returned buffer may be part of a larger pak file,
    // or a discrete file from anywhere in the quake search path
    // a -1 return means the file does not exist
    // NULL can be passed for buf to just determine existance
    int (*FS_LoadFile)(const char * name, void ** buf);
    void (*FS_FreeFile)(void * buf);

    void (*FS_CreatePath)(char * path);

    // gamedir will be the current directory that generated
    // files should be stored to, ie: "f:\quake\id1"
    char * (*FS_Gamedir)(void);

    cvar_t * (*Cvar_Get)(const char * name, const char * value, int flags);
    cvar_t * (*Cvar_Set)(const char * name, const char * value);
    void (*Cvar_SetValue)(const char * name, float value);

    void (*Vid_MenuInit)(void);
    void (*Vid_NewWindow)(int width, int height);
    int (*Vid_GetModeInfo)(int * width, int * height, int mode);

    void (*Sys_SetMemoryHooks)(void (*allocHook)(void *, size_t, game_memtag_t),
                               void (*freeHook) (void *, size_t, game_memtag_t));

} refimport_t;

// This is the only function actually exported at the linker level
typedef refexport_t (*GetRefAPI_t)(refimport_t);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CL_REF_H
