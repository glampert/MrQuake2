//
// RefShared.hpp
//  Code shared by all refresh modules.
//
#pragma once

#include <cstdint>
#include <cstring>
#include <intrin.h>

/*
===============================================================================

    Helper macros & common definitions

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

// ============================================================================

std::uint64_t FnvHash64(const std::uint8_t * bytes, std::size_t len);
std::uint32_t FnvHash32(const std::uint8_t * bytes, std::size_t len);

using Color8      = std::uint8_t;
using ColorRGBA32 = std::uint32_t;

struct Vec2u16
{
    std::uint16_t x;
    std::uint16_t y;
};

template<typename T, std::size_t Size>
constexpr std::size_t ArrayLength(const T (&)[Size])
{
    return Size;
}

// ============================================================================

class PathName final
{
public:

    static constexpr unsigned kNameMaxLen = 64; // MAX_QPATH

    PathName(const char * const path)
    {
        FASTASSERT(path != nullptr);
        m_length = static_cast<std::uint32_t>(std::strlen(path));

        FASTASSERT(m_length < kNameMaxLen);
        strcpy_s(m_string, path);

        m_hash = FnvHash32(reinterpret_cast<const std::uint8_t *>(m_string), m_length);
    }

    PathName(const std::uint32_t hash, const char * const path, const std::uint32_t len)
        : m_hash{ hash }
        , m_length{ len }
    {
        FASTASSERT(hash != 0);
        FASTASSERT(path != nullptr);
        FASTASSERT(len < kNameMaxLen);
        strcpy_s(m_string, path);
    }

    static std::uint32_t CalcHash(const char * const path)
    {
        FASTASSERT(path != nullptr);
        return FnvHash32(reinterpret_cast<const std::uint8_t *>(path), std::strlen(path));
    }

    const char * CStrNoExt(char * buffer) const
    {
        if (const char * lastDot = std::strrchr(m_string, '.'))
        {
            const auto num = std::size_t(lastDot - m_string);
            std::memcpy(buffer, m_string, num);
            buffer[num] = '\0';
            return buffer;
        }
        else
        {
            return m_string;
        }
    }

    std::uint32_t Hash()   const { return m_hash;   }
    std::uint32_t Length() const { return m_length; }
    const char *  CStr()   const { return m_string; }

private:

    std::uint32_t m_hash;       // Hash of the following string, for faster lookup.
    std::uint32_t m_length;     // Cached length of string not including terminator.
    char m_string[kNameMaxLen]; // File name with game path including extension.
};

/*
===============================================================================

    Quake forward declarations

===============================================================================
*/

struct cvar_s;
struct model_s;
struct image_s;
struct refimport_s;

/*
===============================================================================

    CvarWrapper

===============================================================================
*/
class CvarWrapper final
{
public:

    // These mirror the flags in q_shared.h
    enum VarFlags 
    {
        kFlagArchive    = 1,
        kFlagUserInfo   = 2,
        kFlagServerInfo = 4,
        kFlagNoSet      = 8,
        kFlagLatch      = 16,
    };

    CvarWrapper(cvar_s * v = nullptr)
        : m_wrapped_var{ v }
    {
    }
    CvarWrapper & operator=(cvar_s * v)
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

namespace FS
{
    int LoadFile(const char * name, void ** out_buf);
    void FreeFile(void * out_buf);
    void CreatePath(const char * path);
    const char * GameDir();

    struct ScopedFile final
    {
        void * data_ptr;
        const int length;

        explicit ScopedFile(const char * const name)
            : data_ptr{ nullptr }, length{ FS::LoadFile(name, &data_ptr) }
        { }

        ~ScopedFile() { FS::FreeFile(data_ptr); }

        bool IsLoaded() const { return (data_ptr != nullptr && length > 0); }
    };
} // FS

} // GameInterface
