//
// RefShared.hpp
//  Code shared by all refresh modules.
//
#pragma once

/*
===============================================================================

    Helper macros & definitions

===============================================================================
*/

// For marking GetRefAPI in each DLL.
#define REFLIB_DLL_EXPORT __declspec(dllexport)

// For Errorf() which always aborts.
#define REFLIB_NORETURN __declspec(noreturn)

// Debug-only assert that triggers an immediate debug break.
// Also generates less code (no function calls emitted).
#ifndef NDEBUG
    #define FASTASSERT(expr) (void)((expr) || (__debugbreak(), 0))
#else // NDEBUG
    #define FASTASSERT(expr) /* nothing */
#endif // NDEBUG

/*
===============================================================================

    Quake forward declarations

===============================================================================
*/

struct refimport_s;
struct cvar_s;

/*
===============================================================================

    CvarWrapper

===============================================================================
*/
class CvarWrapper final
{
public:
    // These mirror the flags in q_shared.h
    enum {
        kFlagArchive    = 1,
        kFlagUserInfo   = 2,
        kFlagServerInfo = 4,
        kFlagNoSet      = 8,
        kFlagLatch      = 16,
    };

    CvarWrapper(cvar_s * v = nullptr)
        : m_wrapped_var{v}
    {
    }
    CvarWrapper & operator = (cvar_s * v)
    {
        m_wrapped_var = v;
        return *this;
    }

    // Access Cvar value with cast
    int AsInt() const;
    float AsFloat() const;
    const char * AsStr() const;

    // Set Cvar value
    void SetInt(int value);
    void SetFloat(float value);
    void SetStr(const char * value);

    // Cvar flags
    unsigned Flags() const;
    bool IsModified() const;

    // Metadata
    const char * Name() const;
    bool IsNotNull() const { return m_wrapped_var != nullptr; }

private:
    cvar_s * m_wrapped_var;
};

/*
===============================================================================

    GameInterface

===============================================================================
*/
namespace GameInterface
{

void Initialize(const refimport_s & ri, const char * ref_name);
void Shutdown();

void Printf(const char * fmt, ...);
REFLIB_NORETURN void Errorf(const char * fmt, ...);

namespace Cmd
{
    int Argc();
    const char * Argv(int i);
    void RegisterCommand(const char * name, void (*cmd_func)());
    void RemoveCommand(const char * name);
    void ExecuteCommandText(const char * text);
    void InsertCommandText(const char * text);
    void AppendCommandText(const char * text);
} // Cmd

namespace FS
{
    int LoadFile(const char * name, void ** out_buf);
    void FreeFile(void * out_buf);
    const char * GameDir();
} // FS

namespace Cvar
{
    CvarWrapper Get(const char * name, const char * default_value, unsigned flags);
    CvarWrapper Set(const char * name, const char * value);
    void SetValue(const char * name, float value);
    void SetValue(const char * name, int value);
} // Cvar

namespace Video
{
    void MenuInit();
    void NewWindow(int width, int height);
    bool GetModeInfo(int & out_width, int & out_height, int mode_index);
} // Video

} // GameInterface
