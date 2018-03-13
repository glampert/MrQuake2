//
// RefShared.hpp
//  Code shared by all refresh modules.
//
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <intrin.h>

/*
===============================================================================

    Quake forward declarations

===============================================================================
*/
struct cplane_s;
struct cvar_s;
struct dvis_s;
struct image_s;
struct model_s;
struct refimport_s;
using vec3_t = float[3];

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

namespace MrQ2
{

/*
===============================================================================

    Miscellaneous utility functions

===============================================================================
*/

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

inline void Vec3Zero(vec3_t v)
{
    v[0] = v[1] = v[2] = 0.0f;
}

inline float Vec3Dot(const vec3_t x, const vec3_t y)
{
    return (x[0] * y[0]) + (x[1] * y[1]) + (x[2] * y[2]);
}

inline float Vec3Length(const vec3_t v)
{
    float length = 0.0f;
    for (int i = 0; i < 3; ++i)
    {
        length += v[i] * v[i];
    }
    return std::sqrt(length);
}

inline void Vec3Cross(const vec3_t v1, const vec3_t v2, vec3_t cross)
{
    cross[0] = (v1[1] * v2[2]) - (v1[2] * v2[1]);
    cross[1] = (v1[2] * v2[0]) - (v1[0] * v2[2]);
    cross[2] = (v1[0] * v2[1]) - (v1[1] * v2[0]);
}

inline void Vec3Add(const vec3_t a, const vec3_t b, vec3_t out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

inline void Vec3Sub(const vec3_t a, const vec3_t b, vec3_t out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

inline void Vec3Copy(const vec3_t in, vec3_t out)
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
}

inline void Vec3Normalize(vec3_t v)
{
    const float length = std::sqrt((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
    if (length != 0.0f)
    {
        const float invlength = 1.0f / length;
        v[0] *= invlength;
        v[1] *= invlength;
        v[2] *= invlength;
    }
}

/*
===============================================================================

    PathName

===============================================================================
*/
class PathName final
{
public:

    static constexpr int kNameMaxLen = 64; // MAX_QPATH

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

} // MrQ2
