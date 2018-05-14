//
// ViewDraw.hpp
//  Common view/3D frame rendering helpers.
//
#pragma once

// RefShared includes
#include "RefShared.hpp"
#include "ModelStore.hpp"
#include "TextureStore.hpp"
#include "FixedSizeArray.hpp"

// TODO remove D3D dependency - RefShared should be standalone!
#include <DirectXMath.h>

// Quake includes
#include "client/ref.h"
#include "common/q_files.h"

// If set to non-zero, deconstruct PrimitiveTopology::TriangleFan primitives in the 
// MiniImBatch into PrimitiveTopology::TriangleList primitives, to support back-end APIs
// that are not capable of drawing triangle fans natively.
#define REFLIB_EMULATED_TRIANGLE_FANS 1

namespace MrQ2
{

/*
===============================================================================

    DrawVertex3D / DrawVertex2D / PrimitiveTopology

===============================================================================
*/

struct DrawVertex3D
{
    vec3_t position;
    vec2_t uv;
    vec4_t rgba;
};

struct DrawVertex2D
{
    vec4_t xy_uv;
    vec4_t rgba;
};

enum class PrimitiveTopology : std::uint8_t
{
    TriangleList,
    TriangleStrip,
    TriangleFan,
};

// Max per RenderView/frame.
constexpr int kMaxTranslucentEntities = 128;

/*
===============================================================================

    MiniImBatch

===============================================================================
*/
class MiniImBatch final
{
public:

    MiniImBatch(DrawVertex3D * const verts_ptr, const int num_verts, const PrimitiveTopology topology)
        : m_verts_ptr{ verts_ptr }
        , m_num_verts{ num_verts }
        , m_used_verts{ 0 }
        , m_topology{ topology }
        #if REFLIB_EMULATED_TRIANGLE_FANS
        , m_tri_fan_vert_count{ 0 }
        , m_tri_fan_first_vert{}
        , m_tri_fan_last_vert{}
        #endif // REFLIB_EMULATED_TRIANGLE_FANS
    { }

    void Clear()
    {
        m_verts_ptr  = nullptr;
        m_num_verts  = 0;
        m_used_verts = 0;
    }

    DrawVertex3D * Increment(const int num_verts)
    {
        auto * first_vert = (m_verts_ptr + m_used_verts);
        m_used_verts += num_verts;
        if (m_used_verts > m_num_verts)
        {
            OverflowError();
        }
        return first_vert;
    }

    void SetTriangleFanFirstVertex(const DrawVertex3D & vert)
    {
        #if REFLIB_EMULATED_TRIANGLE_FANS
        {
            m_tri_fan_vert_count = 1;
            m_tri_fan_first_vert = vert;
        }
        #else // REFLIB_EMULATED_TRIANGLE_FANS
        {
            PushVertex(vert);
        }
        #endif // REFLIB_EMULATED_TRIANGLE_FANS
    }

    void PushVertex(const DrawVertex3D & vert);
    void PushModelSurface(const ModelSurface & surf);

    int NumVerts()  const { return m_num_verts; }
    int UsedVerts() const { return m_used_verts; }
    bool IsValid()  const { return m_verts_ptr != nullptr; }
    PrimitiveTopology Topology() const { return m_topology; }

private:

    REFLIB_NORETURN void OverflowError() const;

    DrawVertex3D *    m_verts_ptr;
    int               m_num_verts;
    int               m_used_verts;
    PrimitiveTopology m_topology;

    #if REFLIB_EMULATED_TRIANGLE_FANS
    std::uint8_t      m_tri_fan_vert_count;
    DrawVertex3D      m_tri_fan_first_vert;
    DrawVertex3D      m_tri_fan_last_vert;
    #endif // REFLIB_EMULATED_TRIANGLE_FANS
};

/*
===============================================================================

    ViewDrawState

===============================================================================
*/
class ViewDrawState
{
public:

    struct FrameData final
    {
        FrameData(TextureStore & ts, ModelInstance & world, const refdef_t & view)
            : tex_store{ ts }
            , world_model{ world }
            , view_def{ view }
        { }

        // Inputs
        TextureStore & tex_store;
        ModelInstance & world_model;
        const refdef_t view_def; // Local copy

        // Scene viewer/camera
        vec3_t camera_origin;
        vec3_t camera_lookat;
        vec3_t forward_vec;
        vec3_t right_vec;
        vec3_t up_vec;

        // View frustum for the frame, so we can cull bounding boxes out of view
        cplane_t frustum[4];

        // Frame matrices for the back-end
        // TODO: Replace D3D mtx with something not windows-specific
        DirectX::XMMATRIX view_matrix;
        DirectX::XMMATRIX proj_matrix;
        DirectX::XMMATRIX view_proj_matrix;

        // Batched from RenderSolidEntities for the translucencies pass.
        FixedSizeArray<const entity_t *, kMaxTranslucentEntities> translucent_entities;
    };

    ViewDrawState();
    virtual ~ViewDrawState() = default;

    // Level-load registration:
    void BeginRegistration();

    // Frame/view rending:
    void RenderViewSetup(FrameData & frame_data);
    void RenderWorldModel(FrameData & frame_data);
    void RenderSolidEntities(FrameData & frame_data);

protected:

    struct BeginBatchArgs
    {
        DirectX::XMMATRIX    model_matrix; // TODO kill dependency on D3D!
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
    void RecursiveWorldNode(const FrameData & frame_data, const ModelInstance & world_mdl, const ModelNode * node) const;
    void MarkLeaves(ModelInstance & world_mdl);
    void DrawTextureChains(FrameData & frame_data);

    // Entity rendering:
    void DrawBrushModel(const FrameData & frame_data, const entity_t & entity);
    void DrawSpriteModel(const FrameData & frame_data, const entity_t & entity);
    void DrawAliasMD2Model(const FrameData & frame_data, const entity_t & entity);
    void DrawBeamModel(const FrameData & frame_data, const entity_t & entity);
    void DrawNullModel(const FrameData & frame_data, const entity_t & entity);

    // Lighting/shading:
    void CalcPointLightColor(const FrameData & frame_data, const entity_t & entity, vec4_t outShadeLightColor) const;

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
};

} // MrQ2
