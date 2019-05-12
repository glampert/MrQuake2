//
// ViewDraw.hpp
//  Common view/3D frame rendering helpers.
//
#pragma once

// RefShared includes
#include "FixedSizeArray.hpp"
#include "TextureStore.hpp"
#include "ModelStore.hpp"
#include "MiniImBatch.hpp"
#include "SkyBox.hpp"

// Quake includes
#include "client/ref.h"
#include "common/q_files.h"

namespace MrQ2
{

/*
===============================================================================

    ViewDrawState

===============================================================================
*/
class ViewDrawState
{
public:

    // Max per RenderView/frame.
    static constexpr int kMaxTranslucentEntities = 128;

    struct FrameData final
    {
        FrameData(TextureStore & texstore, ModelInstance & world, const refdef_t & view)
            : tex_store{ texstore }
            , world_model{ world }
            , view_def{ view }
        { }

        // Frame matrices for the back-end
        RenderMatrix view_matrix;
        RenderMatrix proj_matrix;
        RenderMatrix view_proj_matrix;

        // Inputs
        TextureStore  & tex_store;
        ModelInstance & world_model;
        const refdef_t  view_def; // Local copy

        // Scene viewer/camera
        vec3_t camera_origin;
        vec3_t camera_lookat;
        vec3_t forward_vec;
        vec3_t right_vec;
        vec3_t up_vec;

        // View frustum for the frame, so we can cull bounding boxes out of view
        cplane_t frustum[4];

        // Batched from RenderSolidEntities for the translucencies pass.
        FixedSizeArray<const entity_t *, kMaxTranslucentEntities> translucent_entities;
    };

    ViewDrawState();
    virtual ~ViewDrawState() = default;

    // Level-load registration:
    void BeginRegistration();
    void EndRegistration();

    // Frame/view rending:
    void RenderViewSetup(FrameData & frame_data);
    void RenderWorldModel(FrameData & frame_data);
    void RenderSkyBox(FrameData & frame_data);
    void RenderSolidEntities(FrameData & frame_data);
    void RenderTranslucentSurfaces(FrameData & frame_data);
    void RenderTranslucentEntities(FrameData & frame_data);

    // Assignable ref
    SkyBox & Sky() { return m_skybox; }

protected:

    struct BeginBatchArgs
    {
        RenderMatrix         model_matrix;
        const TextureImage * optional_tex;
        PrimitiveTopology    topology;
    };

    // Implemented by the renderer back-end:
    virtual MiniImBatch BeginBatch(const BeginBatchArgs & args) = 0;
    virtual void EndBatch(MiniImBatch & batch) = 0;

private:

    // Frame setup:
    void SetUpViewClusters(const FrameData & frame_data);
    void SetUpFrustum(FrameData & frame_data) const;

    // World rendering:
    void RecursiveWorldNode(const FrameData & frame_data, const ModelInstance & world_mdl, const ModelNode * node);
    void MarkLeaves(ModelInstance & world_mdl);
    void DrawTextureChains(FrameData & frame_data);
    void DrawAnimatedWaterPolys(const ModelSurface & surf, float frame_time, const vec4_t color);

    // Entity rendering:
    void DrawBrushModel(const FrameData & frame_data, const entity_t & entity);
    void DrawSpriteModel(const FrameData & frame_data, const entity_t & entity);
    void DrawAliasMD2Model(const FrameData & frame_data, const entity_t & entity);
    void DrawBeamModel(const FrameData & frame_data, const entity_t & entity);
    void DrawNullModel(const FrameData & frame_data, const entity_t & entity);

    // Lighting/shading:
    void CalcPointLightColor(const FrameData & frame_data, const entity_t & entity,
                             vec4_t out_shade_light_color) const;

    // Defined in DrawAliasMD2.cpp
    void DrawAliasMD2FrameLerp(const entity_t & entity, const dmdl_t * const alias_header, const float backlerp,
                               const vec3_t shade_light, const RenderMatrix & model_matrix,
                               const TextureImage * const model_skin);

private:

    // Current frame number/count
    int m_frame_count = 0;

    // Bumped when going to a new PVS
    int m_vis_frame_count = 0;

    // View clusters:
    // BeginRegistration() has to reset them to -1 for a new map.
    int m_view_cluster      = -1;
    int m_view_cluster2     = -1;
    int m_old_view_cluster  = -1;
    int m_old_view_cluster2 = -1;

    // Cached Cvars:
    CvarWrapper m_force_null_entity_models;
    CvarWrapper m_lerp_entity_models;
    CvarWrapper m_skip_draw_alpha_surfs;
    CvarWrapper m_skip_draw_texture_chains;
    CvarWrapper m_skip_draw_world;
    CvarWrapper m_skip_draw_sky;
    CvarWrapper m_skip_draw_entities;
    CvarWrapper m_intensity;

    // Chain of world surfaces that draw with transparency (water/glass).
    const ModelSurface * m_alpha_world_surfaces = nullptr;

    // SkyBox rendering helper
    SkyBox m_skybox;
};

} // MrQ2
