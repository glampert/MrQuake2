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

// winquake.h: Win32-specific Quake header file

#ifndef WINQUAKE_H
#define WINQUAKE_H

#include "common/q_common.h"

#define NOMINMAX
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum { WINQUAKE_MAX_NUM_ARGVS = 128 };

typedef struct
{
    // Window state
    HINSTANCE hinstance;
    HWND      hwnd;    // Set on WM_CREATE, cleared on WM_DESTROY
    void *    wndproc; // Pointer to WIN_MainWndProc
    qboolean  active_app;
    qboolean  minimized;
    qboolean  alttab_disabled;

    // Program command line, unix style
    int argc;
    char * argv[WINQUAKE_MAX_NUM_ARGVS];
} winquake_t;

extern winquake_t winquake;

void WIN_Init();
void IN_MouseEvent(int mstate);
void Sys_DebugOutput(const char * message);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // WINQUAKE_H
