//
// RefShared.cpp
//  Code shared by all refresh modules.
//

#include "RefShared.hpp"
#include "OSWindow.hpp"

#include <cstdarg>
#include <cstdio>

// Quake includes
extern "C"
{
#include "common/q_common.h"
#include "client/ref.h"
} // extern "C"

///////////////////////////////////////////////////////////////////////////////
// GameInterface
///////////////////////////////////////////////////////////////////////////////

namespace GameInterface
{

///////////////////////////////////////////////////////////////////////////////

static refimport_t g_ref_import;
static const char * g_ref_name;
enum { MAXPRINTMSG = 4096 };

///////////////////////////////////////////////////////////////////////////////

void Initialize(const refimport_s & ri, const char * ref_name)
{
    g_ref_import = ri;
    g_ref_name = ref_name;
}

///////////////////////////////////////////////////////////////////////////////

void Shutdown()
{
    g_ref_import = {};
    g_ref_name = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void Printf(const char * fmt, ...)
{
    va_list argptr;
    char msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    g_ref_import.Con_Printf(PRINT_ALL, "[%s]: %s\n", g_ref_name, msg);
}

///////////////////////////////////////////////////////////////////////////////

void Errorf(const char * fmt, ...)
{
    va_list argptr;
    char msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    g_ref_import.Con_Printf(PRINT_ALL, "[%s] FATAL ERROR: %s\n", g_ref_name, msg);

    MessageBox(NULL, msg, "Fatal Error", MB_OK);
    std::abort();
}

///////////////////////////////////////////////////////////////////////////////

int Cmd::Argc()
{
    return g_ref_import.Cmd_Argc();
}

///////////////////////////////////////////////////////////////////////////////

const char * Cmd::Argv(int i)
{
    return g_ref_import.Cmd_Argv(i);
}

///////////////////////////////////////////////////////////////////////////////

void Cmd::RegisterCommand(const char * name, void (*cmd_func)())
{
    FASTASSERT(name != nullptr);
    FASTASSERT(cmd_func != nullptr);
    g_ref_import.Cmd_AddCommand(name, cmd_func);
}

///////////////////////////////////////////////////////////////////////////////

void Cmd::RemoveCommand(const char * name)
{
    FASTASSERT(name != nullptr);
    g_ref_import.Cmd_RemoveCommand(name);
}

///////////////////////////////////////////////////////////////////////////////

void Cmd::ExecuteCommandText(const char * text)
{
    FASTASSERT(text != nullptr);
    g_ref_import.Cmd_ExecuteText(EXEC_NOW, text);
}

///////////////////////////////////////////////////////////////////////////////

void Cmd::InsertCommandText(const char * text)
{
    FASTASSERT(text != nullptr);
    g_ref_import.Cmd_ExecuteText(EXEC_INSERT, text);
}

///////////////////////////////////////////////////////////////////////////////

void Cmd::AppendCommandText(const char * text)
{
    FASTASSERT(text != nullptr);
    g_ref_import.Cmd_ExecuteText(EXEC_APPEND, text);
}

///////////////////////////////////////////////////////////////////////////////

int FS::LoadFile(const char * name, void ** out_buf)
{
    FASTASSERT(name != nullptr);
    FASTASSERT(out_buf != nullptr);
    return g_ref_import.FS_LoadFile(name, out_buf);
}

///////////////////////////////////////////////////////////////////////////////

void FS::FreeFile(void * out_buf)
{
    if (out_buf != nullptr)
    {
        g_ref_import.FS_FreeFile(out_buf);
    }
}

///////////////////////////////////////////////////////////////////////////////

const char * FS::GameDir()
{
    return g_ref_import.FS_Gamedir();
}

///////////////////////////////////////////////////////////////////////////////

CvarWrapper Cvar::Get(const char * name, const char * default_value, unsigned flags)
{
    FASTASSERT(name != nullptr);
    FASTASSERT(default_value != nullptr);
    return CvarWrapper{ g_ref_import.Cvar_Get(name, default_value, flags) };
}

///////////////////////////////////////////////////////////////////////////////

CvarWrapper Cvar::Set(const char * name, const char * value)
{
    FASTASSERT(name != nullptr);
    FASTASSERT(value != nullptr);
    return CvarWrapper{ g_ref_import.Cvar_Set(name, value) };
}

///////////////////////////////////////////////////////////////////////////////

void Cvar::SetValue(const char * name, float value)
{
    FASTASSERT(name != nullptr);
    g_ref_import.Cvar_SetValue(name, value);
}

///////////////////////////////////////////////////////////////////////////////

void Cvar::SetValue(const char * name, int value)
{
    FASTASSERT(name != nullptr);
    g_ref_import.Cvar_SetValue(name, static_cast<float>(value));
}

///////////////////////////////////////////////////////////////////////////////

void Video::MenuInit()
{
    g_ref_import.Vid_MenuInit();
}

///////////////////////////////////////////////////////////////////////////////

void Video::NewWindow(int width, int height)
{
    g_ref_import.Vid_NewWindow(width, height);
}

///////////////////////////////////////////////////////////////////////////////

bool Video::GetModeInfo(int & out_width, int & out_height, int mode_index)
{
    return !!g_ref_import.Vid_GetModeInfo(&out_width, &out_height, mode_index);
}

} // GameInterface

///////////////////////////////////////////////////////////////////////////////
// CvarWrapper
///////////////////////////////////////////////////////////////////////////////

int CvarWrapper::AsInt() const
{
    FASTASSERT(IsNotNull());
    return static_cast<int>(m_wrapped_var->value);
}

///////////////////////////////////////////////////////////////////////////////

float CvarWrapper::AsFloat() const
{
    FASTASSERT(IsNotNull());
    return m_wrapped_var->value;
}

///////////////////////////////////////////////////////////////////////////////

const char * CvarWrapper::AsStr() const
{
    FASTASSERT(IsNotNull());
    return m_wrapped_var->string;
}

///////////////////////////////////////////////////////////////////////////////

void CvarWrapper::SetInt(int value)
{
    FASTASSERT(IsNotNull());

    char val_str[64];
    std::snprintf(val_str, sizeof(val_str), "%i", value);
    GameInterface::g_ref_import.Cvar_Set(m_wrapped_var->name, val_str);
}

///////////////////////////////////////////////////////////////////////////////

void CvarWrapper::SetFloat(float value)
{
    FASTASSERT(IsNotNull());

    char val_str[64];
    std::snprintf(val_str, sizeof(val_str), "%f", value);
    GameInterface::g_ref_import.Cvar_Set(m_wrapped_var->name, val_str);
}

///////////////////////////////////////////////////////////////////////////////

void CvarWrapper::SetStr(const char * value)
{
    FASTASSERT(IsNotNull());
    GameInterface::g_ref_import.Cvar_Set(m_wrapped_var->name, value);
}

///////////////////////////////////////////////////////////////////////////////

unsigned CvarWrapper::Flags() const
{
    FASTASSERT(IsNotNull());
    return m_wrapped_var->flags;
}

///////////////////////////////////////////////////////////////////////////////

bool CvarWrapper::IsModified() const
{
    FASTASSERT(IsNotNull());
    return !!m_wrapped_var->modified;
}

///////////////////////////////////////////////////////////////////////////////

const char * CvarWrapper::Name() const
{
    FASTASSERT(IsNotNull());
    return m_wrapped_var->name;
}

///////////////////////////////////////////////////////////////////////////////
