//
// ViewRenderer.hpp
//  Common view/3D frame rendering helpers.
//
#pragma once

#include "Array.hpp"
#include "RenderInterface.hpp"
#include "TextureStore.hpp"
#include "ModelStore.hpp"
#include "SkyBox.hpp"

// Quake includes
#include "client/ref.h"
#include "common/q_files.h"

namespace MrQ2
{

using ViewConstBuffers = ArrayBase<const ConstantBuffer *>;

/*
===============================================================================

    ViewRenderer

===============================================================================
*/
class ViewRenderer final
{
public:

    // Max per RenderView
    static constexpr uint32_t kMaxTranslucentEntities = 128;

    struct FrameData
    {
        FrameData(TextureStore & texstore, ModelInstance & world, const refdef_t & view, GraphicsContext & cx, const ViewConstBuffers & cbs)
            : context{ cx }
            , cbuffers{ cbs }
            , tex_store{ texstore }
            , world_model{ world }
            , view_def{ view }
        { }

        GraphicsContext & context;
        const ViewConstBuffers & cbuffers;

        // Frame matrices for the back-end
        RenderMatrix view_matrix{};
        RenderMatrix proj_matrix{};
        RenderMatrix view_proj_matrix{};

        // Inputs
        TextureStore  & tex_store;
        ModelInstance & world_model;
        const refdef_t  view_def; // Local copy

        // Scene viewer/camera
        vec3_t camera_origin{};
        vec3_t camera_lookat{};
        vec3_t forward_vec{};
        vec3_t right_vec{};
        vec3_t up_vec{};

        // View frustum for the frame, so we can cull bounding boxes out of view
        Frustum frustum;

        // Batched from RenderSolidEntities for the translucencies pass.
        FixedSizeArray<const entity_t *, kMaxTranslucentEntities> translucent_entities;

        // Debug counters
        int alias_models_culled{ 0 };
        int brush_models_culled{ 0 };
        int world_nodes_culled{ 0 };
    };

    ViewRenderer() = default;

    // Not copyable.
    ViewRenderer(const ViewRenderer &) = delete;
    ViewRenderer & operator=(const ViewRenderer &) = delete;

    void Init(const RenderDevice & device, const TextureStore & tex_store);
    void Shutdown();

    // Level-load registration:
    void BeginRegistration();
    void EndRegistration();

    // Frame/view rendering:
    void RenderViewSetup(FrameData & frame_data);
    void DoRenderView(FrameData & frame_data);

    // Assignable ref
    SkyBox & Sky() { return m_skybox; }

private:

    struct BeginBatchArgs
    {
        RenderMatrix         model_matrix;
        const TextureImage * diffuse_tex;  // optional
        const TextureImage * lightmap_tex; // optional
        PrimitiveTopology    topology;
        bool                 depth_hack;
    };

    MiniImBatch BeginBatch(const BeginBatchArgs & args);
    void EndBatch(MiniImBatch & batch);

    enum RenderPass : int
    {
        kPass_SolidGeometry = 0,
        kPass_TranslucentSurfaces,
        kPass_TranslucentEntities,
        kPass_DLights,

        kRenderPassCount,
        kPass_Invalid = kRenderPassCount
    };

    const PipelineState & PipelineStateForPass(const int pass_index) const;
    void BatchImmediateModeDrawCmds();
    void FlushImmediateModeDrawCmds(FrameData & frame_data);

    // Frame setup & rendering:
    void SetUpViewClusters(const FrameData & frame_data);
    void RenderWorldModel(FrameData & frame_data);
    void RenderSkyBox(FrameData & frame_data);
    void RenderSolidEntities(FrameData & frame_data);
    void RenderTranslucentSurfaces(FrameData & frame_data);
    void RenderTranslucentEntities(FrameData & frame_data);
    void RenderParticles(const FrameData & frame_data);
    void RenderDLights(const FrameData & frame_data);
    void MarkDLights(const dlight_t * light, const int bit, ModelInstance & world_mdl, const ModelNode * node) const;
    void PushDLights(FrameData & frame_data) const;
    void SetLightLevel(const FrameData & frame_data) const;

    // World rendering:
    void RecursiveWorldNode(FrameData & frame_data, const ModelInstance & world_mdl, const ModelNode * node);
    void MarkLeaves(ModelInstance & world_mdl);
    const TextureImage * GetSurfaceLightmap(const refdef_t & view_def, const ModelSurface & surf) const;
    void DrawTextureChains(FrameData & frame_data);
    void DrawAnimatedWaterPolys(const refdef_t & view_def, const ModelSurface & surf, float frame_time, const vec4_t color);

    // Entity rendering:
    void DrawBrushModel(FrameData & frame_data, const entity_t & entity);
    void DrawSpriteModel(const FrameData & frame_data, const entity_t & entity);
    void DrawAliasMD2Model(FrameData & frame_data, const entity_t & entity);
    void DrawBeamModel(const FrameData & frame_data, const entity_t & entity);
    void DrawNullModel(const FrameData & frame_data, const entity_t & entity);

    // Lighting/shading:
    bool ShouldCullAliasMD2Model(const Frustum & frustum, const entity_t & entity, vec3_t bbox[8]) const;
    void ShadeAliasMD2Model(const FrameData & frame_data, const entity_t & entity, vec4_t out_shade_light_color, vec3_t out_light_spot) const;
    void CalcPointLightColor(const FrameData & frame_data, const vec3_t point, vec4_t out_shade_light_color, vec3_t out_light_spot) const;

    // Defined in DrawAliasMD2.cpp
    void DrawAliasMD2FrameLerp(const entity_t & entity, const dmdl_t * const alias_header, const float backlerp,
                               const vec3_t shade_light, const RenderMatrix & model_matrix, const TextureImage * const model_skin);
    void DrawAliasMD2Shadow(const entity_t & entity, const dmdl_t * const alias_header,
                            const RenderMatrix & model_matrix, const vec3_t light_spot);

private:

    // Current frame number/count
    int m_frame_count{ 0 };

    // Bumped when going to a new PVS
    int m_vis_frame_count{ 0 };

    // View clusters:
    // BeginRegistration() has to reset them to -1 for a new map.
    int m_view_cluster{ -1 };
    int m_view_cluster2{ -1 };
    int m_old_view_cluster{ -1 };
    int m_old_view_cluster2{ -1 };

    // Chain of world surfaces that draw with transparency (water/glass).
    const ModelSurface * m_alpha_world_surfaces{ nullptr };

    // SkyBox rendering helper
    SkyBox m_skybox;

    //
    // Low-level render back-end state / immediate mode rendering emulation.
    //

    struct PerDrawShaderConstants
    {
        RenderMatrix model_matrix;
    };

    struct DrawCmd
    {
        PerDrawShaderConstants consts;
        const TextureImage *   diffuse_tex;
        const TextureImage *   lightmap_tex;
        uint32_t               first_vert;
        uint32_t               vertex_count;
        PrimitiveTopology      topology;
        bool                   depth_hack;
    };

    using DrawCmdList = FixedSizeArray<DrawCmd, 4096>;
    using VBuffers    = VertexBuffers<DrawVertex3D>;

    PipelineState        m_pipeline_solid_geometry;
    PipelineState        m_pipeline_translucent_world_geometry;
    PipelineState        m_pipeline_translucent_entities;
    PipelineState        m_pipeline_dlights;
    ShaderProgram        m_render3d_shader;
    ConstantBuffer       m_per_draw_shader_consts;
    const TextureImage * m_tex_white2x2{ nullptr };
    bool                 m_batch_open{ false };
    VBuffers             m_vertex_buffers{};
    RenderPass           m_current_pass{ kPass_Invalid };
    DrawCmd              m_current_draw_cmd{};
    DrawCmdList          m_draw_cmds[kRenderPassCount]{};
};

} // MrQ2
