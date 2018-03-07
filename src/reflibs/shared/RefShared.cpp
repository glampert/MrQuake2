//
// RefShared.cpp
//  Code shared by all refresh modules.
//

#include "RefShared.hpp"
#include "OSWindow.hpp"
#include "Memory.hpp"

#include <cstdarg>
#include <cstdio>

// Quake includes
extern "C"
{
#include "common/q_common.h"
#include "client/ref.h"
} // extern "C"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// Utility functions
///////////////////////////////////////////////////////////////////////////////

std::uint64_t FnvHash64(const std::uint8_t * const bytes, const std::size_t len)
{
    constexpr std::uint64_t FNV_offset_basis = 14695981039346656037;
    constexpr std::uint64_t FNV_prime = 1099511628211;

    std::uint64_t hash = FNV_offset_basis;
    for (std::size_t i = 0; i < len; ++i)
    {
        hash *= FNV_prime;
        hash ^= bytes[i];
    }
    return hash;
}

///////////////////////////////////////////////////////////////////////////////

std::uint32_t FnvHash32(const std::uint8_t * const bytes, const std::size_t len)
{
    constexpr std::uint32_t FNV_offset_basis = 2166136261;
    constexpr std::uint32_t FNV_prime = 16777619;

    std::uint32_t hash = FNV_offset_basis;
    for (std::size_t i = 0; i < len; ++i)
    {
        hash *= FNV_prime;
        hash ^= bytes[i];
    }
    return hash;
}

///////////////////////////////////////////////////////////////////////////////
// GameInterface
///////////////////////////////////////////////////////////////////////////////

namespace GameInterface
{

///////////////////////////////////////////////////////////////////////////////

static refimport_t g_Refimport;
static const char * g_Refname;
constexpr int MAXPRINTMSG = 4096;

///////////////////////////////////////////////////////////////////////////////

static void InstallGameMemoryHooks()
{
    // Direct game allocations through the Ref lib so they are accounted for.
    auto AllocHookFn = [](void *, std::size_t size_bytes, game_memtag_t tag)
    {
        MemTagsTrackAlloc(size_bytes, MemTag(tag));
    };
    auto FreeHookFn = [](void *, std::size_t size_bytes, game_memtag_t tag)
    {
        MemTagsTrackFree(size_bytes, MemTag(tag));
    };
    g_Refimport.Sys_SetMemoryHooks(AllocHookFn, FreeHookFn);

    MemTagsClearAll();
    Cmd::RegisterCommand("memtags", MemTagsPrintAll);
}

///////////////////////////////////////////////////////////////////////////////

static void RemoveGameMemoryHooks()
{
    g_Refimport.Sys_SetMemoryHooks(nullptr, nullptr);
    Cmd::RemoveCommand("memtags");
}

///////////////////////////////////////////////////////////////////////////////

void Initialize(const refimport_s & ri, const char * ref_name)
{
    g_Refimport = ri;
    g_Refname = ref_name;
    InstallGameMemoryHooks();
}

///////////////////////////////////////////////////////////////////////////////

void Shutdown()
{
    RemoveGameMemoryHooks();
    g_Refimport = {};
    g_Refname = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void Printf(const char * fmt, ...)
{
    va_list argptr;
    char msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    g_Refimport.Con_Printf(PRINT_ALL, "[%s]: %s\n", g_Refname, msg);
}

///////////////////////////////////////////////////////////////////////////////

void Errorf(const char * fmt, ...)
{
    va_list argptr;
    char msg[MAXPRINTMSG];

    va_start(argptr, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    g_Refimport.Con_Printf(PRINT_ALL, "[%s] FATAL ERROR: %s\n", g_Refname, msg);

    MessageBox(nullptr, msg, "Fatal Error", MB_OK);
    std::abort();
}

///////////////////////////////////////////////////////////////////////////////

int Cmd::Argc()
{
    return g_Refimport.Cmd_Argc();
}

///////////////////////////////////////////////////////////////////////////////

const char * Cmd::Argv(int i)
{
    return g_Refimport.Cmd_Argv(i);
}

///////////////////////////////////////////////////////////////////////////////

void Cmd::RegisterCommand(const char * name, void (*cmd_func)())
{
    FASTASSERT(name != nullptr);
    FASTASSERT(cmd_func != nullptr);
    g_Refimport.Cmd_AddCommand(name, cmd_func);
}

///////////////////////////////////////////////////////////////////////////////

void Cmd::RemoveCommand(const char * name)
{
    FASTASSERT(name != nullptr);
    g_Refimport.Cmd_RemoveCommand(name);
}

///////////////////////////////////////////////////////////////////////////////

void Cmd::ExecuteCommandText(const char * text)
{
    FASTASSERT(text != nullptr);
    g_Refimport.Cmd_ExecuteText(EXEC_NOW, text);
}

///////////////////////////////////////////////////////////////////////////////

void Cmd::InsertCommandText(const char * text)
{
    FASTASSERT(text != nullptr);
    g_Refimport.Cmd_ExecuteText(EXEC_INSERT, text);
}

///////////////////////////////////////////////////////////////////////////////

void Cmd::AppendCommandText(const char * text)
{
    FASTASSERT(text != nullptr);
    g_Refimport.Cmd_ExecuteText(EXEC_APPEND, text);
}

///////////////////////////////////////////////////////////////////////////////

int FS::LoadFile(const char * name, void ** out_buf)
{
    FASTASSERT(name != nullptr);
    FASTASSERT(out_buf != nullptr);
    return g_Refimport.FS_LoadFile(name, out_buf);
}

///////////////////////////////////////////////////////////////////////////////

void FS::FreeFile(void * out_buf)
{
    if (out_buf != nullptr)
    {
        g_Refimport.FS_FreeFile(out_buf);
    }
}

///////////////////////////////////////////////////////////////////////////////

void FS::CreatePath(const char * path)
{
    FASTASSERT(path != nullptr);

    char temp_path[1024];
    strcpy_s(temp_path, path);

    // Nuke any trailing file name
    if (char * lastSlash = std::strrchr(temp_path, '/'))
    {
        lastSlash[0] = '\0';
        lastSlash[1] = '\0';
    }

    // FS_CreatePath expects the string to end with a separator
    const auto len = std::strlen(temp_path);
    if (temp_path[len - 1] != '/')
    {
        temp_path[len] = '/';
        temp_path[len + 1] = '\0';
    }

    g_Refimport.FS_CreatePath(temp_path);
}

///////////////////////////////////////////////////////////////////////////////

const char * FS::GameDir()
{
    return g_Refimport.FS_Gamedir();
}

///////////////////////////////////////////////////////////////////////////////

CvarWrapper Cvar::Get(const char * name, const char * default_value, unsigned flags)
{
    FASTASSERT(name != nullptr);
    FASTASSERT(default_value != nullptr);
    return CvarWrapper{ g_Refimport.Cvar_Get(name, default_value, flags) };
}

///////////////////////////////////////////////////////////////////////////////

CvarWrapper Cvar::Set(const char * name, const char * value)
{
    FASTASSERT(name != nullptr);
    FASTASSERT(value != nullptr);
    return CvarWrapper{ g_Refimport.Cvar_Set(name, value) };
}

///////////////////////////////////////////////////////////////////////////////

void Cvar::SetValue(const char * name, float value)
{
    FASTASSERT(name != nullptr);
    g_Refimport.Cvar_SetValue(name, value);
}

///////////////////////////////////////////////////////////////////////////////

void Cvar::SetValue(const char * name, int value)
{
    FASTASSERT(name != nullptr);
    g_Refimport.Cvar_SetValue(name, static_cast<float>(value));
}

///////////////////////////////////////////////////////////////////////////////

void Video::MenuInit()
{
    g_Refimport.Vid_MenuInit();
}

///////////////////////////////////////////////////////////////////////////////

void Video::NewWindow(int width, int height)
{
    g_Refimport.Vid_NewWindow(width, height);
}

///////////////////////////////////////////////////////////////////////////////

bool Video::GetModeInfo(int & out_width, int & out_height, int mode_index)
{
    return !!g_Refimport.Vid_GetModeInfo(&out_width, &out_height, mode_index);
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
    GameInterface::g_Refimport.Cvar_Set(m_wrapped_var->name, val_str);
}

///////////////////////////////////////////////////////////////////////////////

void CvarWrapper::SetFloat(float value)
{
    FASTASSERT(IsNotNull());

    char val_str[64];
    std::snprintf(val_str, sizeof(val_str), "%f", value);
    GameInterface::g_Refimport.Cvar_Set(m_wrapped_var->name, val_str);
}

///////////////////////////////////////////////////////////////////////////////

void CvarWrapper::SetStr(const char * value)
{
    FASTASSERT(IsNotNull());
    GameInterface::g_Refimport.Cvar_Set(m_wrapped_var->name, value);
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

} // MrQ2
