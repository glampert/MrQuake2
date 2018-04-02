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

#ifdef _MSC_VER
// "ViewDrawState: structure was padded due to alignment specifier"
#pragma warning(disable: 4324)
#endif // _MSC_VER

namespace MrQ2
{

/*
===============================================================================

    ViewDrawState

===============================================================================
*/
class ViewDrawState final
{
public:

    struct FrameData
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

    void Setup(FrameData & frame_data);
    void RenderWorldModel(FrameData & frame_data);
    void RenderEntities(FrameData & frame_data);
    void Finish(FrameData & frame_data);
    void BeginRegistration();

private:

    const std::uint8_t * DecompressModelVis(const std::uint8_t * in, const ModelInstance & model);
    const std::uint8_t * GetClusterPVS(int cluster, const ModelInstance & model);

    void SetUpViewClusters(const FrameData & frame_data);
    void SetUpFrustum(FrameData & frame_data) const;

    void RecursiveWorldNode(const FrameData & frame_data, const ModelInstance & world_mdl, const ModelNode * node) const;
    void MarkLeaves(ModelInstance & world_mdl);
    void DrawTextureChains(FrameData & frame_data);

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

    // Buffer to decompress a cluster PVS.
    // Alignment not strictly necessary, but helps the compiler memcpy it with aligned vector ops.
    alignas(16) std::uint8_t m_dvis_pvs[MAX_MAP_LEAFS / 8] = {};
};

// ============================================================================

} // MrQ2
