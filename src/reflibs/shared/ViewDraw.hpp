//
// ViewDraw.hpp
//  Common view/3D frame rendering helpers.
//
#pragma once

// RefShared includes
#include "RefShared.hpp"
#include "ModelStore.hpp"
#include "TextureStore.hpp"

// TODO remove D3D dependency - RefShared should be standalone!
#include <DirectXMath.h>

// Quake includes
#include "client/ref.h"
#include "common/q_files.h"

namespace MrQ2
{

/*
===============================================================================

    DrawVertex3D / DrawVertex2D

===============================================================================
*/

struct alignas(16) DrawVertex3D
{
    vec3_t position;
    vec3_t normal;
    vec2_t uv;
    vec4_t rgba;
};

struct alignas(16) DrawVertex2D
{
    vec4_t xy_uv;
    vec4_t rgba;
};

/*
===============================================================================

    MiniImBatch

===============================================================================
*/
class MiniImBatch final
{
public:

    MiniImBatch(DrawVertex3D * const verts_ptr, const int num_verts)
        : m_verts_ptr{ verts_ptr }
        , m_num_verts{ num_verts }
        , m_used_verts{ 0 }
    { }

    int  NumVerts()  const { return m_num_verts;  }
    int  UsedVerts() const { return m_used_verts; }
    bool IsValid()   const { return m_verts_ptr != nullptr; }

    DrawVertex3D * Increment(const int num_verts)
    {
        auto * first_vert = (m_verts_ptr + m_used_verts);
        m_used_verts += num_verts;
        if (m_used_verts > m_num_verts)
        {
            GameInterface::Errorf("MiniImBatch overflowed! used_verts=%i, num_verts=%i. "
                                  "Increase vertex batch size.", m_used_verts, m_num_verts);
        }
        return first_vert;
    }

    void Clear()
    {
        m_verts_ptr  = nullptr;
        m_num_verts  = 0;
        m_used_verts = 0;
    }

    void PushVertex(const DrawVertex3D & vert);
    void PushModelSurface(const ModelSurface & surf);

private:

    DrawVertex3D * m_verts_ptr;
    int m_num_verts;
    int m_used_verts;
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

        // Frame matrices
        // TODO: Replace D3D mtx with something not windows-specific
        DirectX::XMMATRIX view_matrix;
        DirectX::XMMATRIX proj_matrix;
        DirectX::XMMATRIX view_proj_matrix;
    };

    ViewDrawState() = default;
    virtual ~ViewDrawState() = default;

    void RenderViewSetup(FrameData & frame_data);
    void RenderWorldModel(FrameData & frame_data);
    void RenderEntities(FrameData & frame_data);

    void BeginRegistration();

protected:

    // Implemented by the renderer back-end.
    virtual MiniImBatch BeginSurfacesBatch(const TextureImage & tex) = 0;
    virtual void EndSurfacesBatch(MiniImBatch & batch) = 0;

private:

    // Frame setup:
    void SetUpViewClusters(const FrameData & frame_data);
    void SetUpFrustum(FrameData & frame_data) const;

    // World rendering:
    void RecursiveWorldNode(const FrameData & frame_data, const ModelInstance & world_mdl, const ModelNode * node) const;
    void MarkLeaves(ModelInstance & world_mdl);
    void DrawTextureChains(FrameData & frame_data);

    // Entity rendering:
    void DrawBrushModel(const entity_t & entity);
    void DrawSpriteModel(const entity_t & entity);
    void DrawAliasMD2Model(const entity_t & entity);
    void DrawBeamModel(const entity_t & entity);
    void DrawNullModel(const entity_t & entity);

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
};

} // MrQ2
