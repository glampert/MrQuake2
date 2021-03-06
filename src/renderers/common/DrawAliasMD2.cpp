//
// DrawAliasMD2.cpp
//  Helper functions for rendering "Alias" MD2 models.
//

#include "ViewRenderer.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////

constexpr int kNumVertexNormals = 162;
constexpr int kShadeDotQuant    = 16;

#ifdef _MSC_VER
    // "conversion from 'double' to 'const float' requires a narrowing conversion"
    // The table values below from Quake 2 are missing the .f suffix on float values.
    #pragma warning(push)
    #pragma warning(disable: 4838)
#endif // _MSC_VER

// Pre-calculated vertex normals for simple models
static const float s_vertex_normals[kNumVertexNormals][3] = {
#include "client/anorms.h"
};

// Pre-calculated dot products for quantized angles
static const float s_vertex_normal_dots[kShadeDotQuant][256] = {
#include "client/anormtab.h"
};

#ifdef _MSC_VER
    #pragma warning(pop)
#endif // _MSC_VER

///////////////////////////////////////////////////////////////////////////////

struct LerpInputs final
{
    const entity_t    * entity;
    const dtrivertx_t * frame_verts;
    const dtrivertx_t * old_frame_verts;
    int                 num_verts;
    vec3_t              frontv;
    vec3_t              backv;
    vec3_t              move;
};

///////////////////////////////////////////////////////////////////////////////

static void LerpEntityVerts(const LerpInputs & in, float * lerp)
{
    const int num_verts    = in.num_verts;
    const dtrivertx_t * v  = in.frame_verts;
    const dtrivertx_t * ov = in.old_frame_verts;

    if (in.entity->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
    {
        for (int i = 0; i < num_verts; i++, v++, ov++, lerp += 3)
        {
            const float * const normal = s_vertex_normals[v->lightnormalindex];
            lerp[0] = in.move[0] + ov->v[0] * in.backv[0] + v->v[0] * in.frontv[0] + normal[0] * POWERSUIT_SCALE;
            lerp[1] = in.move[1] + ov->v[1] * in.backv[1] + v->v[1] * in.frontv[1] + normal[1] * POWERSUIT_SCALE;
            lerp[2] = in.move[2] + ov->v[2] * in.backv[2] + v->v[2] * in.frontv[2] + normal[2] * POWERSUIT_SCALE;
        }
    }
    else
    {
        for (int i = 0; i < num_verts; i++, v++, ov++, lerp += 3)
        {
            lerp[0] = in.move[0] + ov->v[0] * in.backv[0] + v->v[0] * in.frontv[0];
            lerp[1] = in.move[1] + ov->v[1] * in.backv[1] + v->v[1] * in.frontv[1];
            lerp[2] = in.move[2] + ov->v[2] * in.backv[2] + v->v[2] * in.frontv[2];
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

static inline const daliasframe_t * GetAliasFrame(const dmdl_t * const alias_header, const int frame_index)
{
    auto * data_ptr  = reinterpret_cast<const qbyte *>(alias_header) + alias_header->ofs_frames + frame_index * alias_header->framesize;
    auto * frame_ptr = reinterpret_cast<const daliasframe_t *>(data_ptr);
    return frame_ptr;
}

static inline const std::int32_t * GetAliasGLCmds(const dmdl_t * const alias_header)
{
    auto * data_ptr = reinterpret_cast<const qbyte *>(alias_header) + alias_header->ofs_glcmds;
    auto * cmds_ptr = reinterpret_cast<const std::int32_t *>(data_ptr);
    return cmds_ptr;
}

static inline const float * GetShadeDotsForEnt(const entity_t & entity)
{
    const std::size_t index = ((int)(entity.angles[YAW] * (kShadeDotQuant / 360.0f))) & (kShadeDotQuant - 1);
    MRQ2_ASSERT(index < ArrayLength(s_vertex_normal_dots));
    return s_vertex_normal_dots[index];
}

// This holds the last rendered frame of an MD2 model for a subsequent DrawAliasMD2Shadow.
static vec3_t s_lerped_positions[MAX_VERTS] = {};

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::DrawAliasMD2FrameLerp(const entity_t & entity, const dmdl_t * const alias_header, const float backlerp,
                                         const vec3_t shade_light, const RenderMatrix & model_matrix,
                                         const TextureImage * const model_skin)
{
    MRQ2_ASSERT(alias_header != nullptr);
    MRQ2_ASSERT(alias_header->num_xyz <= MAX_VERTS);

    const std::int32_t  * order = GetAliasGLCmds(alias_header);

    const daliasframe_t * frame = GetAliasFrame(alias_header, entity.frame);
    const dtrivertx_t   * verts = frame->verts;

    const daliasframe_t * old_frame = GetAliasFrame(alias_header, entity.oldframe);
    const dtrivertx_t   * old_verts = old_frame->verts;

    const float alpha        = (entity.flags & RF_TRANSLUCENT) ? entity.alpha : 1.0f;
    const float frontlerp    = 1.0f - backlerp;
    const float * shade_dots = GetShadeDotsForEnt(entity);

    vec3_t delta, move;
    vec3_t frontv, backv;
    vec3_t vectors[3];

    // move should be the delta back to the previous frame * backlerp
    Vec3Sub(entity.oldorigin, entity.origin, delta);
    VectorsFromAngles(entity.angles, vectors[0], vectors[1], vectors[2]);

    move[0] =  Vec3Dot(delta, vectors[0]); // forward
    move[1] = -Vec3Dot(delta, vectors[1]); // left
    move[2] =  Vec3Dot(delta, vectors[2]); // up

    Vec3Add(move, old_frame->translate, move);

    for (int i = 0; i < 3; ++i)
    {
        move[i] = backlerp * move[i] + frontlerp * frame->translate[i];
    }

    for (int i = 0; i < 3; ++i)
    {
        frontv[i] = frontlerp * frame->scale[i];
        backv[i]  = backlerp  * old_frame->scale[i];
    }

    // Interpolate the previous frame and the current:
    LerpInputs lerp_in;
    lerp_in.entity = &entity;
    lerp_in.frame_verts = verts;
    lerp_in.old_frame_verts = old_verts;
    lerp_in.num_verts = alias_header->num_xyz;
    Vec3Copy(frontv, lerp_in.frontv);
    Vec3Copy(backv, lerp_in.backv);
    Vec3Copy(move, lerp_in.move);
    LerpEntityVerts(lerp_in, s_lerped_positions[0]);

    BeginBatchArgs batch_args;
    batch_args.model_matrix = model_matrix;
    batch_args.diffuse_tex  = model_skin;
    batch_args.lightmap_tex = nullptr;
    batch_args.depth_hack   = (entity.flags & RF_DEPTHHACK) != 0;

    // Build the final model vertices:
    for (;;)
    {
        // Get the vertex count and primitive type
        std::int32_t count = *order++;
        if (count == 0)
        {
            break; // done
        }

        if (count < 0) // TriangleFan emulation:
        {
            count = -count;
            MRQ2_ASSERT(count > 0);

            batch_args.topology = PrimitiveTopology::kTriangleFan;
            MiniImBatch batch = BeginBatch(batch_args);

            if (entity.flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE))
            {
                for (int i = 0; i < count; ++i)
                {
                    const std::size_t index_xyz = order[2];
                    MRQ2_ASSERT(index_xyz < ArrayLength(s_lerped_positions));
                    order += 3;

                    DrawVertex3D dv;
                    Vec3Copy(s_lerped_positions[index_xyz], dv.position);
                    Vec2Zero(dv.texture_uv);
                    Vec2Zero(dv.lightmap_uv);
                    dv.rgba[0] = shade_light[0];
                    dv.rgba[1] = shade_light[1];
                    dv.rgba[2] = shade_light[2];
                    dv.rgba[3] = alpha;

                    if (i == 0)
                        batch.SetTriangleFanFirstVertex(dv);
                    else
                        batch.PushVertex(dv);
                }
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    // Texture coordinates come from the draw list
                    const float u = reinterpret_cast<const float *>(order)[0];
                    const float v = reinterpret_cast<const float *>(order)[1];

                    const std::size_t index_xyz = order[2];
                    MRQ2_ASSERT(index_xyz < ArrayLength(s_lerped_positions));
                    order += 3;

                    // Normals and vertexes come from the frame list
                    const float l = shade_dots[verts[index_xyz].lightnormalindex];

                    DrawVertex3D dv;
                    Vec3Copy(s_lerped_positions[index_xyz], dv.position);
                    dv.texture_uv[0] = u;
                    dv.texture_uv[1] = v;
                    Vec2Zero(dv.lightmap_uv);
                    dv.rgba[0] = l * shade_light[0];
                    dv.rgba[1] = l * shade_light[1];
                    dv.rgba[2] = l * shade_light[2];
                    dv.rgba[3] = alpha;

                    if (i == 0)
                        batch.SetTriangleFanFirstVertex(dv);
                    else
                        batch.PushVertex(dv);
                }
            }

            EndBatch(batch);
        }
        else // TriangleStrip fast path:
        {
            batch_args.topology = PrimitiveTopology::kTriangleStrip;
            MiniImBatch batch = BeginBatch(batch_args);

            MRQ2_ASSERT(count > 0);
            DrawVertex3D * vertex_ptr = batch.Increment(count);

            if (entity.flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE))
            {
                for (int i = 0; i < count; ++i, ++vertex_ptr)
                {
                    const std::size_t index_xyz = order[2];
                    MRQ2_ASSERT(index_xyz < ArrayLength(s_lerped_positions));
                    order += 3;

                    Vec3Copy(s_lerped_positions[index_xyz], vertex_ptr->position);
                    Vec2Zero(vertex_ptr->texture_uv);
                    Vec2Zero(vertex_ptr->lightmap_uv);
                    vertex_ptr->rgba[0] = shade_light[0];
                    vertex_ptr->rgba[1] = shade_light[1];
                    vertex_ptr->rgba[2] = shade_light[2];
                    vertex_ptr->rgba[3] = alpha;
                }
            }
            else
            {
                for (int i = 0; i < count; ++i, ++vertex_ptr)
                {
                    // Texture coordinates come from the draw list
                    const float u = reinterpret_cast<const float *>(order)[0];
                    const float v = reinterpret_cast<const float *>(order)[1];

                    const std::size_t index_xyz = order[2];
                    MRQ2_ASSERT(index_xyz < ArrayLength(s_lerped_positions));
                    order += 3;

                    // Normals and vertexes come from the frame list
                    const float l = shade_dots[verts[index_xyz].lightnormalindex];

                    Vec3Copy(s_lerped_positions[index_xyz], vertex_ptr->position);
                    vertex_ptr->texture_uv[0] = u;
                    vertex_ptr->texture_uv[1] = v;
                    Vec2Zero(vertex_ptr->lightmap_uv);
                    vertex_ptr->rgba[0] = l * shade_light[0];
                    vertex_ptr->rgba[1] = l * shade_light[1];
                    vertex_ptr->rgba[2] = l * shade_light[2];
                    vertex_ptr->rgba[3] = alpha;
                }
            }

            EndBatch(batch);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::DrawAliasMD2Shadow(const entity_t & entity, const dmdl_t * const alias_header, const RenderMatrix & model_matrix, const vec3_t light_spot)
{
    constexpr float kShadowColorOpacity = 0.5f;

    const float lheight = entity.origin[2] - light_spot[2];
    const float height  = -lheight + 1.0f;
    const float angle   = DegToRad(entity.angles[YAW]);

    vec3_t shade_vector;
    shade_vector[0] = std::cosf(-angle);
    shade_vector[1] = std::sinf(-angle);
    shade_vector[2] = 1.0f;
    Vec3Normalize(shade_vector);

    BeginBatchArgs batch_args;
    batch_args.model_matrix = model_matrix;
    batch_args.diffuse_tex  = nullptr;
    batch_args.lightmap_tex = nullptr;
    batch_args.depth_hack   = false;

    const std::int32_t * order = GetAliasGLCmds(alias_header);
    for (;;)
    {
        // get the vertex count and primitive type
        std::int32_t count = *order++;
        if (count == 0)
        {
            break; // done
        }

        if (count < 0) // TriangleFan emulation:
        {
            count = -count;
            MRQ2_ASSERT(count > 0);

            batch_args.topology = PrimitiveTopology::kTriangleFan;
            MiniImBatch batch = BeginBatch(batch_args);

            for (int i = 0; i < count; ++i)
            {
                const std::size_t index_xyz = order[2];
                MRQ2_ASSERT(index_xyz < ArrayLength(s_lerped_positions));
                order += 3;

                DrawVertex3D dv = {};
                dv.rgba[3] = kShadowColorOpacity;

                // Reuse s_lerped_positions[] from the previous DrawAliasMD2FrameLerp call.
                Vec3Copy(s_lerped_positions[index_xyz], dv.position);
                dv.position[0] -= shade_vector[0] * (dv.position[2] + lheight);
                dv.position[1] -= shade_vector[1] * (dv.position[2] + lheight);
                dv.position[2] = height;

                if (i == 0)
                    batch.SetTriangleFanFirstVertex(dv);
                else
                    batch.PushVertex(dv);
            }

            EndBatch(batch);
        }
        else // TriangleStrip fast path:
        {
            batch_args.topology = PrimitiveTopology::kTriangleStrip;
            MiniImBatch batch = BeginBatch(batch_args);

            MRQ2_ASSERT(count > 0);
            DrawVertex3D * vertex_ptr = batch.Increment(count);

            for (int i = 0; i < count; ++i)
            {
                const std::size_t index_xyz = order[2];
                MRQ2_ASSERT(index_xyz < ArrayLength(s_lerped_positions));
                order += 3;

                DrawVertex3D dv = {};
                dv.rgba[3] = kShadowColorOpacity;

                // Reuse s_lerped_positions[] from the previous DrawAliasMD2FrameLerp call.
                Vec3Copy(s_lerped_positions[index_xyz], dv.position);
                dv.position[0] -= shade_vector[0] * (dv.position[2] + lheight);
                dv.position[1] -= shade_vector[1] * (dv.position[2] + lheight);
                dv.position[2] = height;

                *vertex_ptr++ = dv;
            }

            EndBatch(batch);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
