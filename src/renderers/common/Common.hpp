//
// Common.hpp
//  Code shared by all renderer back-ends.
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
#define MRQ2_RENDERLIB_DLL_EXPORT __declspec(dllexport)

// For Errorf() which always aborts.
#define MRQ2_RENDERLIB_NORETURN __declspec(noreturn)

// Prevents an otherwise inlineable function from being inlined.
#define MRQ2_RENDERLIB_NOINLINE __declspec(noinline)

// Debug-only assert that triggers an immediate debug break.
// Also generates less code (no function calls emitted).
#ifndef NDEBUG
    #define MRQ2_ASSERT(expr)         (void)((expr) || (__debugbreak(), 0))
    #define MRQ2_ASSERT_ALIGN16(ptr)  MRQ2_ASSERT((std::uintptr_t(ptr) % 16) == 0)
#else // NDEBUG
    #define MRQ2_ASSERT(expr)         // nothing
    #define MRQ2_ASSERT_ALIGN16(ptr)  // nothing
#endif // NDEBUG

#define MRQ2_MAKE_WIDE_STR_IMPL(s) L ## s
#define MRQ2_MAKE_WIDE_STR(s)      MRQ2_MAKE_WIDE_STR_IMPL(s)

#define MRQ2_CAT_TOKEN_IMPL(a, b)  a ## b
#define MRQ2_CAT_TOKEN(a, b)       MRQ2_CAT_TOKEN_IMPL(a, b)

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
using Bool32      = std::uint32_t;

struct Vec2u16
{
    std::uint16_t x;
    std::uint16_t y;
};

template<typename T, unsigned Size>
constexpr unsigned ArrayLength(const T (&)[Size])
{
    return Size;
}

constexpr float DegToRad(const float degrees)
{
    return degrees * (3.14159265358979323846f / 180.0f);
}

inline void * AlignPtr(const void * value, const std::size_t alignment)
{
    return reinterpret_cast<void *>((reinterpret_cast<std::uintptr_t>(value) + (alignment - 1)) & ~(alignment - 1));
}

template<int N>
inline void VecSplatN(float (&vec)[N], const float val)
{
    for (int i = 0; i < N; ++i)
    {
        vec[i] = val;
    }
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

inline void Vec3MAdd(const vec3_t veca, const float scale, const vec3_t vecb, vec3_t vecc)
{
    vecc[0] = veca[0] + (scale * vecb[0]);
    vecc[1] = veca[1] + (scale * vecb[1]);
    vecc[2] = veca[2] + (scale * vecb[2]);
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

/*
===============================================================================

    RenderMatrix (row-major 4x4, float4 vectors, 16 aligned)

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

    enum IdentityInitializer { kIdentity };

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

    Frustum

===============================================================================
*/
struct alignas(16) Frustum final
{
    // view * projection:
    RenderMatrix clipMatrix;
    RenderMatrix projection;

    // Frustum planes:
    enum { A, B, C, D };
    float p[6][4];

    // Sets everything to zero / identity.
    Frustum();

    // Compute a fresh projection matrix for the frustum.
    void SetProjection(float fovyRadians, int width, int height, float zn, float zf);

    // Compute frustum planes from camera matrix. Also sets clipMatrix by multiplying view and projection.
    void SetClipPlanes(const RenderMatrix & view);

    // Test if point inside frustum.
    bool TestPoint(float x, float y, float z) const;

    // Bounding sphere inside frustum or partially intersecting.
    bool TestSphere(float x, float y, float z, float radius) const;

    // Cube inside frustum or partially intersecting.
    bool TestCube(float x, float y, float z, float size) const;

    // Axis-aligned bounding box. True if box is partly intersecting or fully contained in the frustum.
    bool TestAabb(const vec3_t mins, const vec3_t maxs) const;
};

/*
===============================================================================

    Frustum inline methods:

===============================================================================
*/

inline bool Frustum::TestPoint(const float x, const float y, const float z) const
{
    for (int i = 0; i < 6; ++i)
    {
        if ((p[i][A] * x + p[i][B] * y + p[i][C] * z + p[i][D]) <= 0.0f)
        {
            return false;
        }
    }
    return true;
}

inline bool Frustum::TestSphere(const float x, const float y, const float z, const float radius) const
{
    for (int i = 0; i < 6; ++i)
    {
        if ((p[i][A] * x + p[i][B] * y + p[i][C] * z + p[i][D]) <= -radius)
        {
            return false;
        }
    }
    return true;
}

inline bool Frustum::TestCube(const float x, const float y, const float z, const float size) const
{
    for (int i = 0; i < 6; ++i)
    {
        if ((p[i][A] * (x - size) + p[i][B] * (y - size) + p[i][C] * (z - size) + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * (x + size) + p[i][B] * (y - size) + p[i][C] * (z - size) + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * (x - size) + p[i][B] * (y + size) + p[i][C] * (z - size) + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * (x + size) + p[i][B] * (y + size) + p[i][C] * (z - size) + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * (x - size) + p[i][B] * (y - size) + p[i][C] * (z + size) + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * (x + size) + p[i][B] * (y - size) + p[i][C] * (z + size) + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * (x - size) + p[i][B] * (y + size) + p[i][C] * (z + size) + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * (x + size) + p[i][B] * (y + size) + p[i][C] * (z + size) + p[i][D]) > 0.0f) { continue; }
        return false;
    }
    return true;
}

inline bool Frustum::TestAabb(const vec3_t mins, const vec3_t maxs) const
{
    for (int i = 0; i < 6; ++i)
    {
        if ((p[i][A] * mins[0] + p[i][B] * mins[1] + p[i][C] * mins[2] + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * maxs[0] + p[i][B] * mins[1] + p[i][C] * mins[2] + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * mins[0] + p[i][B] * maxs[1] + p[i][C] * mins[2] + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * maxs[0] + p[i][B] * maxs[1] + p[i][C] * mins[2] + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * mins[0] + p[i][B] * mins[1] + p[i][C] * maxs[2] + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * maxs[0] + p[i][B] * mins[1] + p[i][C] * maxs[2] + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * mins[0] + p[i][B] * maxs[1] + p[i][C] * maxs[2] + p[i][D]) > 0.0f) { continue; }
        if ((p[i][A] * maxs[0] + p[i][B] * maxs[1] + p[i][C] * maxs[2] + p[i][D]) > 0.0f) { continue; }
        return false;
    }
    return true;
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
        MRQ2_ASSERT(path != nullptr);
        m_length = static_cast<std::uint32_t>(std::strlen(path));

        MRQ2_ASSERT(m_length < kNameMaxLen);
        strcpy_s(m_string, path);

        m_hash = FnvHash32(reinterpret_cast<const std::uint8_t *>(m_string), m_length);
    }

    PathName(const std::uint32_t hash, const char * const path, const std::uint32_t len)
        : m_hash{ hash }
        , m_length{ len }
    {
        MRQ2_ASSERT(hash != 0);
        MRQ2_ASSERT(path != nullptr);
        MRQ2_ASSERT(len < kNameMaxLen);
        strcpy_s(m_string, path);
    }

    static std::uint32_t CalcHash(const char * const path)
    {
        MRQ2_ASSERT(path != nullptr);
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
MRQ2_RENDERLIB_NORETURN void Errorf(const char * fmt, ...);

int GetTimeMilliseconds();

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

/*
===============================================================================

    Global configuration parameters

===============================================================================
*/
namespace Config
{
    // Video
    extern CvarWrapper vid_xpos;
    extern CvarWrapper vid_ypos;
    extern CvarWrapper vid_mode;
    extern CvarWrapper vid_width;
    extern CvarWrapper vid_height;

    // Renderer misc
    extern CvarWrapper r_renderdoc;
    extern CvarWrapper r_debug;
    extern CvarWrapper r_debug_frame_events;
    extern CvarWrapper r_draw_fps_counter;
    extern CvarWrapper r_draw_cull_stats;
    extern CvarWrapper r_surf_use_debug_color;
    extern CvarWrapper r_blend_debug_color;
    extern CvarWrapper r_max_anisotropy;
    extern CvarWrapper r_no_mipmaps;
    extern CvarWrapper r_debug_mipmaps;
    extern CvarWrapper r_tex_filtering;
    extern CvarWrapper r_disable_texturing;
    extern CvarWrapper r_force_mip_level;
    extern CvarWrapper r_world_ambient;
    extern CvarWrapper r_sky_use_pal_textures;
    extern CvarWrapper r_sky_force_full_draw;
    extern CvarWrapper r_lightmap_format;
    extern CvarWrapper r_lightmap_intensity;
    extern CvarWrapper r_debug_lightmaps;
    extern CvarWrapper r_no_draw;

    // ViewRenderer configs
    extern CvarWrapper r_use_vertex_index_buffers;
    extern CvarWrapper r_force_null_entity_models;
    extern CvarWrapper r_lerp_entity_models;
    extern CvarWrapper r_skip_draw_alpha_surfs;
    extern CvarWrapper r_skip_draw_texture_chains;
    extern CvarWrapper r_skip_draw_world;
    extern CvarWrapper r_skip_draw_sky;
    extern CvarWrapper r_skip_draw_entities;
    extern CvarWrapper r_skip_brush_mods;
    extern CvarWrapper r_intensity;
    extern CvarWrapper r_water_hack;
    extern CvarWrapper r_draw_model_bounds;
    extern CvarWrapper r_draw_world_bounds;

    // Cache all the CVars above.
    void Initialize();

} // Config

} // MrQ2
