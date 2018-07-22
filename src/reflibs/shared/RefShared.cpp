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
#include "common/q_common.h"
#include "client/ref.h"

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

void VectorsFromAngles(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
    float angle;

    angle = angles[YAW] * (M_PI * 2.0f / 360.0f);
    const float sy = std::sin(angle);
    const float cy = std::cos(angle);

    angle = angles[PITCH] * (M_PI * 2.0f / 360.0f);
    const float sp = std::sin(angle);
    const float cp = std::cos(angle);

    angle = angles[ROLL] * (M_PI * 2.0f / 360.0f);
    const float sr = std::sin(angle);
    const float cr = std::cos(angle);

    forward[0] = cp * cy;
    forward[1] = cp * sy;
    forward[2] = -sp;

    right[0] = (-1.0f * sr * sp * cy + -1.0f * cr * -sy);
    right[1] = (-1.0f * sr * sp * sy + -1.0f * cr * cy);
    right[2] = (-1.0f * sr * cp);

    up[0] = (cr * sp * cy + -sr * -sy);
    up[1] = (cr * sp * sy + -sr * cy);
    up[2] = (cr * cp);
}

///////////////////////////////////////////////////////////////////////////////

void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal)
{
    const float inv_denom = 1.0f / Vec3Dot(normal, normal);
    const float d = Vec3Dot(normal, p) * inv_denom;

    vec3_t n;
    n[0] = normal[0] * inv_denom;
    n[1] = normal[1] * inv_denom;
    n[2] = normal[2] * inv_denom;

    dst[0] = p[0] - d * n[0];
    dst[1] = p[1] - d * n[1];
    dst[2] = p[2] - d * n[2];
}

///////////////////////////////////////////////////////////////////////////////

void PerpendicularVector(vec3_t dst, const vec3_t src)
{
    // Assumes "src" is normalized

    int pos, i;
    float minelem = 1.0f;
    vec3_t tempvec;

    // find the smallest magnitude axially aligned vector
    for (pos = 0, i = 0; i < 3; ++i)
    {
        if (std::fabs(src[i]) < minelem)
        {
            pos = i;
            minelem = std::fabs(src[i]);
        }
    }

    tempvec[0] = tempvec[1] = tempvec[2] = 0.0f;
    tempvec[pos] = 1.0f;

    // project the point onto the plane defined by src
    ProjectPointOnPlane(dst, tempvec, src);

    // normalize the result
    Vec3Normalize(dst);
}

///////////////////////////////////////////////////////////////////////////////

void ConcatRotations(float in1[3][3], float in2[3][3], float out[3][3])
{
    out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
                in1[0][2] * in2[2][0];
    out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
                in1[0][2] * in2[2][1];
    out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
                in1[0][2] * in2[2][2];
    out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
                in1[1][2] * in2[2][0];
    out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
                in1[1][2] * in2[2][1];
    out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
                in1[1][2] * in2[2][2];
    out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
                in1[2][2] * in2[2][0];
    out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
                in1[2][2] * in2[2][1];
    out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
                in1[2][2] * in2[2][2];
}

///////////////////////////////////////////////////////////////////////////////

void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, const float degrees)
{
    float m[3][3];
    float im[3][3];
    float zrot[3][3];
    float tmpmat[3][3];
    float rot[3][3];
    vec3_t vr, vup, vf;

    vf[0] = dir[0];
    vf[1] = dir[1];
    vf[2] = dir[2];

    PerpendicularVector(vr, dir);
    Vec3Cross(vr, vf, vup);

    m[0][0] = vr[0];
    m[1][0] = vr[1];
    m[2][0] = vr[2];

    m[0][1] = vup[0];
    m[1][1] = vup[1];
    m[2][1] = vup[2];

    m[0][2] = vf[0];
    m[1][2] = vf[1];
    m[2][2] = vf[2];

    std::memcpy(im, m, sizeof(im));

    im[0][1] = m[1][0];
    im[0][2] = m[2][0];
    im[1][0] = m[0][1];
    im[1][2] = m[2][1];
    im[2][0] = m[0][2];
    im[2][1] = m[1][2];

    std::memset(zrot, 0, sizeof(zrot));
    zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0f;

    zrot[0][0] =  std::cos(DegToRad(degrees));
    zrot[0][1] =  std::sin(DegToRad(degrees));
    zrot[1][0] = -std::sin(DegToRad(degrees));
    zrot[1][1] =  std::cos(DegToRad(degrees));

    ConcatRotations(m, zrot, tmpmat);
    ConcatRotations(tmpmat, im, rot);

    for (int i = 0; i < 3; ++i)
    {
        dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
    }
}

///////////////////////////////////////////////////////////////////////////////

int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const cplane_s * p)
{
    // Returns 1, 2, or 1 + 2

    float dist1, dist2;

    // fast axial cases
    if (p->type < 3)
    {
        if (p->dist <= emins[p->type])
        {
            return 1;
        }
        if (p->dist >= emaxs[p->type])
        {
            return 2;
        }
        return 3;
    }

    // general case
    switch (p->signbits)
    {
    case 0:
        dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
        dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
        break;
    case 1:
        dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
        dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
        break;
    case 2:
        dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
        dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
        break;
    case 3:
        dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
        dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
        break;
    case 4:
        dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
        dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
        break;
    case 5:
        dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
        dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
        break;
    case 6:
        dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
        dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
        break;
    case 7:
        dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
        dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
        break;
    default:
        dist1 = dist2 = 0.0f; // shut up compiler
        FASTASSERT(false);
        break;
    } // switch

    int sides = 0;
    if (dist1 >= p->dist)
    {
        sides = 1;
    }
    if (dist2 < p->dist)
    {
        sides |= 2;
    }

    FASTASSERT(sides != 0);
    return sides;
}

///////////////////////////////////////////////////////////////////////////////

inline void Vec3MergeXY(const vec3_t V1, const vec3_t V2, vec3_t out)
{
    out[0] = V1[0];
    out[1] = V2[0];
    out[2] = V1[1];
    out[3] = V2[1];
}

///////////////////////////////////////////////////////////////////////////////

inline void Vec3MergeZW(const vec3_t V1, const vec3_t V2, vec3_t out)
{
    out[0] = V1[2];
    out[1] = V2[2];
    out[2] = V1[3];
    out[3] = V2[3];
}

///////////////////////////////////////////////////////////////////////////////
// RenderMatrix
///////////////////////////////////////////////////////////////////////////////

static_assert(sizeof(RenderMatrix) == sizeof(float) * 16, "Unexpected padding in RenderMatrix struct?");

///////////////////////////////////////////////////////////////////////////////

RenderMatrix RenderMatrix::Multiply(const RenderMatrix & M1, const RenderMatrix & M2)
{
    RenderMatrix result;
    // Cache the invariants in registers
    float x = M1.m[0][0];
    float y = M1.m[0][1];
    float z = M1.m[0][2];
    float w = M1.m[0][3];
    // Perform the operation on the first row
    result.m[0][0] = (M2.m[0][0] * x) + (M2.m[1][0] * y) + (M2.m[2][0] * z) + (M2.m[3][0] * w);
    result.m[0][1] = (M2.m[0][1] * x) + (M2.m[1][1] * y) + (M2.m[2][1] * z) + (M2.m[3][1] * w);
    result.m[0][2] = (M2.m[0][2] * x) + (M2.m[1][2] * y) + (M2.m[2][2] * z) + (M2.m[3][2] * w);
    result.m[0][3] = (M2.m[0][3] * x) + (M2.m[1][3] * y) + (M2.m[2][3] * z) + (M2.m[3][3] * w);
    // Repeat for all the other rows
    x = M1.m[1][0];
    y = M1.m[1][1];
    z = M1.m[1][2];
    w = M1.m[1][3];
    result.m[1][0] = (M2.m[0][0] * x) + (M2.m[1][0] * y) + (M2.m[2][0] * z) + (M2.m[3][0] * w);
    result.m[1][1] = (M2.m[0][1] * x) + (M2.m[1][1] * y) + (M2.m[2][1] * z) + (M2.m[3][1] * w);
    result.m[1][2] = (M2.m[0][2] * x) + (M2.m[1][2] * y) + (M2.m[2][2] * z) + (M2.m[3][2] * w);
    result.m[1][3] = (M2.m[0][3] * x) + (M2.m[1][3] * y) + (M2.m[2][3] * z) + (M2.m[3][3] * w);
    x = M1.m[2][0];
    y = M1.m[2][1];
    z = M1.m[2][2];
    w = M1.m[2][3];
    result.m[2][0] = (M2.m[0][0] * x) + (M2.m[1][0] * y) + (M2.m[2][0] * z) + (M2.m[3][0] * w);
    result.m[2][1] = (M2.m[0][1] * x) + (M2.m[1][1] * y) + (M2.m[2][1] * z) + (M2.m[3][1] * w);
    result.m[2][2] = (M2.m[0][2] * x) + (M2.m[1][2] * y) + (M2.m[2][2] * z) + (M2.m[3][2] * w);
    result.m[2][3] = (M2.m[0][3] * x) + (M2.m[1][3] * y) + (M2.m[2][3] * z) + (M2.m[3][3] * w);
    x = M1.m[3][0];
    y = M1.m[3][1];
    z = M1.m[3][2];
    w = M1.m[3][3];
    result.m[3][0] = (M2.m[0][0] * x) + (M2.m[1][0] * y) + (M2.m[2][0] * z) + (M2.m[3][0] * w);
    result.m[3][1] = (M2.m[0][1] * x) + (M2.m[1][1] * y) + (M2.m[2][1] * z) + (M2.m[3][1] * w);
    result.m[3][2] = (M2.m[0][2] * x) + (M2.m[1][2] * y) + (M2.m[2][2] * z) + (M2.m[3][2] * w);
    result.m[3][3] = (M2.m[0][3] * x) + (M2.m[1][3] * y) + (M2.m[2][3] * z) + (M2.m[3][3] * w);
    return result;
}

///////////////////////////////////////////////////////////////////////////////

RenderMatrix RenderMatrix::Transpose(const RenderMatrix & M)
{
    RenderMatrix P, MT;
    Vec3MergeXY(M.rows[0], M.rows[2], P.rows[0]);
    Vec3MergeXY(M.rows[1], M.rows[3], P.rows[1]);
    Vec3MergeZW(M.rows[0], M.rows[2], P.rows[2]);
    Vec3MergeZW(M.rows[1], M.rows[3], P.rows[3]);
    Vec3MergeXY(P.rows[0], P.rows[1], MT.rows[0]);
    Vec3MergeZW(P.rows[0], P.rows[1], MT.rows[1]);
    Vec3MergeXY(P.rows[2], P.rows[3], MT.rows[2]);
    Vec3MergeZW(P.rows[2], P.rows[3], MT.rows[3]);
    return MT;
}

///////////////////////////////////////////////////////////////////////////////

RenderMatrix RenderMatrix::LookToLH(const vec3_t eye_position, const vec3_t eye_direction, const vec3_t up_direction)
{
    vec3_t R2;
    Vec3Copy(eye_direction, R2);
    Vec3Normalize(R2);

    vec3_t R0;
    Vec3Cross(up_direction, R2, R0);
    Vec3Normalize(R0);

    vec3_t R1;
    Vec3Cross(R2, R0, R1);

    vec3_t neg_eye_position;
    Vec3Copy(eye_position, neg_eye_position);
    Vec3Negate(neg_eye_position);

    const float D0 = Vec3Dot(R0, neg_eye_position);
    const float D1 = Vec3Dot(R1, neg_eye_position);
    const float D2 = Vec3Dot(R2, neg_eye_position);

    RenderMatrix M;
    Vec3Copy(R0, M.rows[0]); M.rows[0][3] = D0;
    Vec3Copy(R1, M.rows[1]); M.rows[1][3] = D1;
    Vec3Copy(R2, M.rows[2]); M.rows[2][3] = D2;

    M.rows[3][0] = 0.0f;
    M.rows[3][1] = 0.0f;
    M.rows[3][2] = 0.0f;
    M.rows[3][3] = 1.0f;

    M = Transpose(M);
    return M;
}

///////////////////////////////////////////////////////////////////////////////

RenderMatrix RenderMatrix::LookAtRH(const vec3_t eye_position, const vec3_t focus_position, const vec3_t up_direction)
{
    vec3_t neg_eye_direction;
    Vec3Sub(eye_position, focus_position, neg_eye_direction);
    return LookToLH(eye_position, neg_eye_direction, up_direction);
}

///////////////////////////////////////////////////////////////////////////////

RenderMatrix RenderMatrix::PerspectiveFovRH(const float fov_angle_y, const float aspect_ratio, const float near_z, const float far_z)
{
    const float sin_fov = std::sin(0.5f * fov_angle_y);
    const float cos_fov = std::cos(0.5f * fov_angle_y);
    const float height  = cos_fov / sin_fov;
    const float width   = height  / aspect_ratio;
    const float range   = far_z   / (near_z - far_z);

    RenderMatrix M;
    M.m[0][0] = width;
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;
    M.m[1][0] = 0.0f;
    M.m[1][1] = height;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;
    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = range;
    M.m[2][3] = -1.0f;
    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = range * near_z;
    M.m[3][3] = 0.0f;
    return M;
}

///////////////////////////////////////////////////////////////////////////////

RenderMatrix RenderMatrix::Translation(const float offset_x, const float offset_y, const float offset_z)
{
    RenderMatrix M;
    M.m[0][0] = 1.0f;
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;
    M.m[1][0] = 0.0f;
    M.m[1][1] = 1.0f;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;
    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = 1.0f;
    M.m[2][3] = 0.0f;
    M.m[3][0] = offset_x;
    M.m[3][1] = offset_y;
    M.m[3][2] = offset_z;
    M.m[3][3] = 1.0f;
    return M;
}

///////////////////////////////////////////////////////////////////////////////

RenderMatrix RenderMatrix::RotationX(const float angle_radians)
{
    const float sin_angle = std::sin(angle_radians);
    const float cos_angle = std::cos(angle_radians);

    RenderMatrix M;
    M.m[0][0] = 1.0f;
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;
    M.m[1][0] = 0.0f;
    M.m[1][1] = cos_angle;
    M.m[1][2] = sin_angle;
    M.m[1][3] = 0.0f;
    M.m[2][0] = 0.0f;
    M.m[2][1] = -sin_angle;
    M.m[2][2] = cos_angle;
    M.m[2][3] = 0.0f;
    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = 0.0f;
    M.m[3][3] = 1.0f;
    return M;
}

///////////////////////////////////////////////////////////////////////////////

RenderMatrix RenderMatrix::RotationY(const float angle_radians)
{
    const float sin_angle = std::sin(angle_radians);
    const float cos_angle = std::cos(angle_radians);

    RenderMatrix M;
    M.m[0][0] = cos_angle;
    M.m[0][1] = 0.0f;
    M.m[0][2] = -sin_angle;
    M.m[0][3] = 0.0f;
    M.m[1][0] = 0.0f;
    M.m[1][1] = 1.0f;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;
    M.m[2][0] = sin_angle;
    M.m[2][1] = 0.0f;
    M.m[2][2] = cos_angle;
    M.m[2][3] = 0.0f;
    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = 0.0f;
    M.m[3][3] = 1.0f;
    return M;
}

///////////////////////////////////////////////////////////////////////////////

RenderMatrix RenderMatrix::RotationZ(const float angle_radians)
{
    const float sin_angle = std::sin(angle_radians);
    const float cos_angle = std::cos(angle_radians);

    RenderMatrix M;
    M.m[0][0] = cos_angle;
    M.m[0][1] = sin_angle;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;
    M.m[1][0] = -sin_angle;
    M.m[1][1] = cos_angle;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;
    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = 1.0f;
    M.m[2][3] = 0.0f;
    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = 0.0f;
    M.m[3][3] = 1.0f;
    return M;
}

///////////////////////////////////////////////////////////////////////////////

RenderMatrix RenderMatrix::RotationAxis(const float angle_radians, const float x, const float y, const float z)
{
    const float s = std::sinf(angle_radians);
    const float c = std::cosf(angle_radians);

    const float xy = x * y;
    const float yz = y * z;
    const float zx = z * x;
    const float one_minus_cos = 1.0f - c;

    const vec4_t r0 = { (((x * x) * one_minus_cos) + c), ((xy * one_minus_cos) + (z * s)), ((zx * one_minus_cos) - (y * s)), 0.0f };
    const vec4_t r1 = { ((xy * one_minus_cos) - (z * s)), (((y * y) * one_minus_cos) + c), ((yz * one_minus_cos) + (x * s)), 0.0f };
    const vec4_t r2 = { ((zx * one_minus_cos) + (y * s)), ((yz * one_minus_cos) - (x * s)), (((z * z) * one_minus_cos) + c), 0.0f };
    const vec4_t r3 = { 0.0f, 0.0f, 0.0f, 1.0f };

    RenderMatrix M;
    Vec4Copy(r0, M.rows[0]);
    Vec4Copy(r1, M.rows[1]);
    Vec4Copy(r2, M.rows[2]);
    Vec4Copy(r3, M.rows[3]);
    return M;
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
