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
struct refdef_s;
struct refimport_s;

using vec2_t = float[2];
using vec3_t = float[3];
using vec4_t = float[4];

/*
===============================================================================

    Helper macros & common definitions

===============================================================================
*/

// For marking GetRefAPI in each DLL.
#define REFLIB_DLL_EXPORT __declspec(dllexport)

// For Errorf() which always aborts.
#define REFLIB_NORETURN __declspec(noreturn)

// Prevents an otherwise inlineable function from being inlined.
#define REFLIB_NOINLINE __declspec(noinline)

// Debug-only assert that triggers an immediate debug break.
// Also generates less code (no function calls emitted).
#ifndef NDEBUG
    #define FASTASSERT(expr)         (void)((expr) || (__debugbreak(), 0))
    #define FASTASSERT_ALIGN16(ptr)  FASTASSERT((std::uintptr_t(ptr) % 16) == 0)
#else // NDEBUG
    #define FASTASSERT(expr)         /* nothing */
    #define FASTASSERT_ALIGN16(ptr)  /* nothing */
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

constexpr float DegToRad(const float degrees)
{
    return degrees * (3.14159265358979323846f / 180.0f);
}

inline void Vec3Zero(vec3_t v)
{
    v[0] = v[1] = v[2] = 0.0f;
}

inline void Vec3Negate(vec3_t v)
{
    v[0] = -v[0];
    v[1] = -v[1];
    v[2] = -v[2];
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

inline void Vec4Copy(const vec4_t in, vec4_t out)
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    out[3] = in[3];
}

inline void Vec3Scale(const vec3_t in, const float scale, vec3_t out)
{
    out[0] = in[0] * scale;
    out[1] = in[1] * scale;
    out[2] = in[2] * scale;
}

inline float Vec3Normalize(vec3_t v)
{
    const float length = std::sqrt((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
    if (length != 0.0f)
    {
        const float invlength = 1.0f / length;
        v[0] *= invlength;
        v[1] *= invlength;
        v[2] *= invlength;
    }
    return length;
}

// Copied from Quake's math library since we compile as a DLL and won't link against the game EXE.
void PerpendicularVector(vec3_t dst, const vec3_t src);
void VectorsFromAngles(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, const float degrees);
int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const cplane_s * p);

/*
===============================================================================

    RenderMatrix (row-major 4x4, float32 vectors, 16 aligned)

===============================================================================
*/
struct alignas(16) RenderMatrix final
{
    union
    {
        float  floats[16];
        vec4_t rows[4];
        float  m[4][4];
    };

    enum IdentityInitializer { Identity };

    RenderMatrix() = default;         // Uninitialized matrix
    RenderMatrix(IdentityInitializer) // Identity matrix
    {
        std::memset(rows, 0, sizeof(rows));
        rows[0][0] = 1.0f;
        rows[1][1] = 1.0f;
        rows[2][2] = 1.0f;
        rows[3][3] = 1.0f;
    }

    RenderMatrix & operator*=(const RenderMatrix & M) { *this = Multiply(*this, M); return *this; }
    RenderMatrix   operator* (const RenderMatrix & M) const { return Multiply(*this, M); }

    // Concatenate/multiply
    static RenderMatrix Multiply(const RenderMatrix & M1, const RenderMatrix & M2);
    static RenderMatrix Transpose(const RenderMatrix & M);

    // Make camera/view matrices
    static RenderMatrix LookToLH(const vec3_t eye_position, const vec3_t eye_direction, const vec3_t up_direction);
    static RenderMatrix LookAtRH(const vec3_t eye_position, const vec3_t focus_position, const vec3_t up_direction);
    static RenderMatrix PerspectiveFovRH(float fov_angle_y, float aspect_ratio, float near_z, float far_z);

    // Make translation/rotation matrices
    static RenderMatrix Translation(float offset_x, float offset_y, float offset_z);
    static RenderMatrix RotationX(float angle_radians);
    static RenderMatrix RotationY(float angle_radians);
    static RenderMatrix RotationZ(float angle_radians);
    static RenderMatrix RotationAxis(float angle_radians, float x, float y, float z);
};

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

    const char *  CStr()   const { return m_string; }
    std::uint32_t Hash()   const { return m_hash;   }
    std::uint32_t Length() const { return m_length; }

private:

    char m_string[kNameMaxLen]; // File name with game path including extension - first to allow unsafe char* casts from the game code.
    std::uint32_t m_hash;       // Hash of the following string, for faster lookup.
    std::uint32_t m_length;     // Cached length of string not including terminator.
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
    bool IsSet() const { return AsInt() != 0; }
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
