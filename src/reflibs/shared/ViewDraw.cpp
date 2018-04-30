//
// ViewDraw.cpp
//  Common view/3D frame rendering helpers.
//

#include "ViewDraw.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// Local helpers:
///////////////////////////////////////////////////////////////////////////////

// Returns the proper texture for a given time and base texture.
static TextureImage * TextureAnimation(const ModelTexInfo * const tex)
{
    FASTASSERT(tex != nullptr);
    const TextureImage * out = nullptr;

    // End of animation / not animated
    if (tex->next == nullptr)
    {
        out = tex->teximage;
    }
    else // Find next image frame
    {
        /* TODO
        int c = currententity->frame % tex->num_frames;
        while (c)
        {
            tex = tex->next;
            --c;
        }
        */
        out = tex->teximage;
    }

    // Ugly, but we need to set the texture_chain ptr for the world model rendering..
    return const_cast<TextureImage *>(out);
}

///////////////////////////////////////////////////////////////////////////////

// Returns true if the bounding box is completely outside the frustum
// and should be culled. False if visible and allowed to draw.
static bool ShouldCullBBox(const cplane_t frustum[4], const vec3_t mins, const vec3_t maxs)
{
    //TEMP - disabled for testing
    /*
    for (int i = 0; i < 4; ++i)
    {
        if (MrQ2::BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
        {
            return true;
        }
    }
    //*/
    return false;
}

///////////////////////////////////////////////////////////////////////////////

static const ModelLeaf * FindLeafNodeForPoint(const vec3_t p, const ModelInstance & model)
{
    FASTASSERT(model.data.nodes != nullptr);
    const ModelNode * node = model.data.nodes;

    for (;;)
    {
        if (node->contents != -1)
        {
            return reinterpret_cast<const ModelLeaf *>(node);
        }

        const cplane_t * const plane = node->plane;
        const float d = Vec3Dot(p, plane->normal) - plane->dist;

        if (d > 0.0f)
        {
            node = node->children[0];
        }
        else
        {
            node = node->children[1];
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

static const std::uint8_t * DecompressModelVis(std::uint8_t * const out_pvs,
                                               const std::uint8_t * in_pvs,
                                               const ModelInstance & model)
{
    int row = (model.data.vis->numclusters + 7) >> 3;
    std::uint8_t * out = out_pvs;

    if (in_pvs == nullptr)
    {
        // No vis info, so make all visible:
        while (row)
        {
            *out++ = 0xFF;
            row--;
        }
        return out_pvs;
    }

    do
    {
        if (*in_pvs)
        {
            *out++ = *in_pvs++;
            continue;
        }

        int c = in_pvs[1];
        in_pvs += 2;
        while (c)
        {
            *out++ = 0;
            c--;
        }
    } while (out - out_pvs < row);

    return out_pvs;
}

///////////////////////////////////////////////////////////////////////////////

static const std::uint8_t * GetClusterPVS(std::uint8_t * const out_pvs, const int cluster, const ModelInstance & model)
{
    if (cluster == -1 || model.data.vis == nullptr)
    {
        std::memset(out_pvs, 0xFF, (MAX_MAP_LEAFS / 8)); // All visible.
        return out_pvs;
    }

    auto * vid_data = reinterpret_cast<const std::uint8_t *>(model.data.vis) + model.data.vis->bitofs[cluster][DVIS_PVS];
    return DecompressModelVis(out_pvs, vid_data, model);
}

///////////////////////////////////////////////////////////////////////////////

// Sign bits are used for fast box-on-plane-side tests.
static std::uint8_t SignBitsForPlane(const cplane_t & plane)
{
    std::uint8_t bits = 0;
    for (int i = 0; i < 3; ++i)
    {
        // If the value is negative, set a bit for it.
        if (plane.normal[i] < 0.0f)
        {
            bits |= (1 << i);
        }
    }
    return bits;
}

///////////////////////////////////////////////////////////////////////////////
// ViewDrawState
///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::SetUpViewClusters(const FrameData & frame_data)
{
    if (frame_data.view_def.rdflags & RDF_NOWORLDMODEL)
    {
        return;
    }

    const ModelLeaf * leaf = FindLeafNodeForPoint(frame_data.view_def.vieworg, frame_data.world_model);

    m_old_view_cluster  = m_view_cluster;
    m_old_view_cluster2 = m_view_cluster2;
    m_view_cluster = m_view_cluster2 = leaf->cluster;

    // Check above and below so crossing solid water doesn't draw wrong:
    vec3_t temp;
    if (!leaf->contents)
    {
        // Look down a bit:
        Vec3Copy(frame_data.view_def.vieworg, temp);
        temp[2] -= 16.0f;
    }
    else
    {
        // Look up a bit:
        Vec3Copy(frame_data.view_def.vieworg, temp);
        temp[2] += 16.0f;
    }

    leaf = FindLeafNodeForPoint(temp, frame_data.world_model);

    if (!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != m_view_cluster2))
    {
        m_view_cluster2 = leaf->cluster;
    }
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::SetUpFrustum(FrameData & frame_data) const
{
    // Rotate VPN right by FOV_X/2 degrees
    MrQ2::RotatePointAroundVector(frame_data.frustum[0].normal,
                                  frame_data.up_vec,
                                  frame_data.forward_vec,
                                  -(90.0f - frame_data.view_def.fov_x / 2.0f));

    // Rotate VPN left by FOV_X/2 degrees
    MrQ2::RotatePointAroundVector(frame_data.frustum[1].normal,
                                  frame_data.up_vec,
                                  frame_data.forward_vec,
                                  (90.0f - frame_data.view_def.fov_x / 2.0f));

    // Rotate VPN up by FOV_X/2 degrees
    MrQ2::RotatePointAroundVector(frame_data.frustum[2].normal,
                                  frame_data.right_vec,
                                  frame_data.forward_vec,
                                  (90.0f - frame_data.view_def.fov_y / 2.0f));

    // Rotate VPN down by FOV_X/2 degrees
    MrQ2::RotatePointAroundVector(frame_data.frustum[3].normal,
                                  frame_data.right_vec,
                                  frame_data.forward_vec,
                                  -(90.0f - frame_data.view_def.fov_y / 2.0f));

    for (unsigned i = 0; i < ArrayLength(frame_data.frustum); ++i)
    {
        frame_data.frustum[i].type = PLANE_ANYZ;
        frame_data.frustum[i].dist = Vec3Dot(frame_data.view_def.vieworg, frame_data.frustum[i].normal);
        frame_data.frustum[i].signbits = SignBitsForPlane(frame_data.frustum[i]);
    }
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::RenderViewSetup(FrameData & frame_data)
{
    ++m_frame_count;

    // Find current view clusters
    SetUpViewClusters(frame_data);

    // Copy eye position
    for (int i = 0; i < 3; ++i)
    {
        frame_data.camera_origin[i] = frame_data.view_def.vieworg[i];
    }

    // Camera view vectors
    MrQ2::VectorsFromAngles(frame_data.view_def.viewangles, frame_data.forward_vec, frame_data.right_vec, frame_data.up_vec);
    Vec3Add(frame_data.camera_origin, frame_data.forward_vec, frame_data.camera_lookat);

    // Other camera/lens parameters
    const float aspect_ratio = float(frame_data.view_def.width) / float(frame_data.view_def.height);
    const float fov_y  = frame_data.view_def.fov_y;
    const float z_near = 4.0f;    // From ref_gl
    const float z_far  = 4096.0f; // From ref_gl

    // Set projection and view matrices for the frame
    const DirectX::XMFLOAT3A eye_position   { frame_data.camera_origin };
    const DirectX::XMFLOAT3A focus_position { frame_data.camera_lookat };
    const DirectX::XMFLOAT3A up_direction   { -frame_data.up_vec[0], -frame_data.up_vec[1], -frame_data.up_vec[2] };

    frame_data.view_matrix = DirectX::XMMatrixLookAtRH(
                        DirectX::XMLoadFloat3A(&eye_position),
                        DirectX::XMLoadFloat3A(&focus_position),
                        DirectX::XMLoadFloat3A(&up_direction));

    frame_data.proj_matrix = DirectX::XMMatrixPerspectiveFovRH(
                        fov_y, aspect_ratio, z_near, z_far);

    frame_data.view_proj_matrix = DirectX::XMMatrixMultiply(
                        frame_data.view_matrix,
                        frame_data.proj_matrix);

    // Update the frustum planes
    SetUpFrustum(frame_data);
}

///////////////////////////////////////////////////////////////////////////////

// This function will recursively mark all surfaces that should
// be drawn and add them to the appropriate draw chain, so the
// next call to DrawTextureChains() will actually render what
// was marked for draw in here.
void ViewDrawState::RecursiveWorldNode(const FrameData & frame_data,
                                       const ModelInstance & world_mdl,
                                       const ModelNode * const node) const
{
    FASTASSERT(node != nullptr);

    if (node->contents == CONTENTS_SOLID)
    {
        return;
    }
    if (node->vis_frame != m_vis_frame_count)
    {
        return;
    }
    if (ShouldCullBBox(frame_data.frustum, node->minmaxs, node->minmaxs + 3))
    {
        return;
    }

    const refdef_t & view_def = frame_data.view_def;

    // If a leaf node, it can draw if visible.
    if (node->contents != -1)
    {
        auto * leaf = reinterpret_cast<const ModelLeaf *>(node);

        // Check for door connected areas:
        if (view_def.areabits)
        {
            if (!(view_def.areabits[leaf->area >> 3] & (1 << (leaf->area & 7))))
            {
                return; // Not visible.
            }
        }

        ModelSurface ** mark = leaf->first_mark_surface;
        int num_surfs = leaf->num_mark_surfaces;
        if (num_surfs)
        {
            do
            {
                (*mark)->vis_frame = m_frame_count;
                ++mark;
            } while (--num_surfs);
        }

        return;
    }

    //
    // Node is just a decision point, so go down the appropriate sides:
    //

    float dot;
    const cplane_t * const plane = node->plane;

    // Find which side of the node we are on:
    switch (plane->type)
    {
    case PLANE_X:
        dot = view_def.vieworg[0] - plane->dist;
        break;
    case PLANE_Y:
        dot = view_def.vieworg[1] - plane->dist;
        break;
    case PLANE_Z:
        dot = view_def.vieworg[2] - plane->dist;
        break;
    default:
        dot = Vec3Dot(view_def.vieworg, plane->normal) - plane->dist;
        break;
    } // switch (plane->type)

    int side, sidebit;
    if (dot >= 0.0f)
    {
        side    = 0;
        sidebit = 0;
    }
    else
    {
        side    = 1;
        sidebit = kSurf_PlaneBack;
    }

    // Recurse down the children, front side first:
    RecursiveWorldNode(frame_data, world_mdl, node->children[side]);

    //
    // Add stuff to the draw lists:
    //
    ModelSurface * surf = world_mdl.data.surfaces + node->first_surface;
    for (int i = 0; i < node->num_surfaces; ++i, ++surf)
    {
        if (surf->vis_frame != m_frame_count)
        {
            continue;
        }
        if ((surf->flags & kSurf_PlaneBack) != sidebit)
        {
            continue; // wrong side
        }

        if (surf->texinfo->flags & SURF_SKY)
        {
            // Just adds to visible sky bounds.
            // TODO
        }
        else if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
        {
            // Add to the translucent draw chain.
            // TODO
        }
        else // Opaque texture chain
        {
            TextureImage * image = TextureAnimation(surf->texinfo);
            FASTASSERT(image != nullptr);

            surf->texture_chain  = image->texture_chain;
            image->texture_chain = surf;
        }
    }

    // Finally recurse down the back side:
    RecursiveWorldNode(frame_data, world_mdl, node->children[!side]);
}

///////////////////////////////////////////////////////////////////////////////

// Mark the leaves and nodes that are in the PVS for the current cluster.
void ViewDrawState::MarkLeaves(ModelInstance & world_mdl)
{
    if (m_old_view_cluster  == m_view_cluster  &&
        m_old_view_cluster2 == m_view_cluster2 &&
        m_view_cluster != -1)
    {
        return;
    }

    ++m_vis_frame_count;

    m_old_view_cluster  = m_view_cluster;
    m_old_view_cluster2 = m_view_cluster2;

    if (m_view_cluster == -1 || world_mdl.data.vis == nullptr)
    {
        // Mark everything as visible:
        for (int i = 0; i < world_mdl.data.num_leafs; ++i)
        {
            world_mdl.data.leafs[i].vis_frame = m_vis_frame_count;
        }
        for (int i = 0; i < world_mdl.data.num_nodes; ++i)
        {
            world_mdl.data.nodes[i].vis_frame = m_vis_frame_count;
        }
        return;
    }

    alignas(16) std::uint8_t temp_vis_pvs[MAX_MAP_LEAFS / 8] = {0};
    alignas(16) std::uint8_t combined_vis_pvs[MAX_MAP_LEAFS / 8] = {0};

    const std::uint8_t * vis_pvs = GetClusterPVS(temp_vis_pvs, m_view_cluster, world_mdl);

    // May have to combine two clusters because of solid water boundaries:
    if (m_view_cluster2 != m_view_cluster)
    {
        std::memcpy(combined_vis_pvs, vis_pvs, (world_mdl.data.num_leafs + 7) / 8);
        vis_pvs = GetClusterPVS(temp_vis_pvs, m_view_cluster2, world_mdl);

        const int c = (world_mdl.data.num_leafs + 31) / 32;
        FASTASSERT(unsigned(c) < (ArrayLength(combined_vis_pvs) / sizeof(std::uint32_t)));

        for (int i = 0; i < c; ++i)
        {
            reinterpret_cast<std::uint32_t *>(combined_vis_pvs)[i] |= reinterpret_cast<const std::uint32_t *>(vis_pvs)[i];
        }
        vis_pvs = combined_vis_pvs;
    }

    ModelLeaf * leaf = world_mdl.data.leafs;
    for (int i = 0; i < world_mdl.data.num_leafs; ++i, ++leaf)
    {
        const int cluster = leaf->cluster;
        if (cluster == -1)
        {
            continue;
        }

        if (vis_pvs[cluster >> 3] & (1 << (cluster & 7)))
        {
            auto * node = reinterpret_cast<ModelNode *>(leaf);
            do
            {
                if (node->vis_frame == m_vis_frame_count)
                {
                    break;
                }

                node->vis_frame = m_vis_frame_count;
                node = node->parent;
            } while (node);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::DrawTextureChains(FrameData & frame_data)
{
    TextureStore & tex_store = frame_data.tex_store;

    // Draw with sorting by texture:
    for (TextureImage * tex : tex_store)
    {
        FASTASSERT(tex->width > 0 && tex->height > 0);
        FASTASSERT(tex->type != TextureType::kCount);

        if (tex->texture_chain == nullptr)
        {
            continue;
        }

        BeginSurfacesBatch(*tex);
        for (const ModelSurface * surf = tex->texture_chain; surf; surf = surf->texture_chain)
        {
            // Need at least one triangle.
            if (surf->polys == nullptr || surf->polys->num_verts < 3)
            {
                continue;
            }
            BatchSurfacePolys(*surf);
        }
        EndSurfacesBatch();

        // All world geometry using this texture has been drawn, clear for the next frame.
        tex->texture_chain = nullptr;
    }
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::RenderWorldModel(FrameData & frame_data)
{
    if (frame_data.view_def.rdflags & RDF_NOWORLDMODEL)
    {
        return;
    }

    MarkLeaves(frame_data.world_model);
    RecursiveWorldNode(frame_data, frame_data.world_model, frame_data.world_model.data.nodes);
    DrawTextureChains(frame_data);
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::RenderEntities(FrameData & frame_data)
{
    const int num_entities = frame_data.view_def.num_entities;
    const entity_t * const entities_list = frame_data.view_def.entities;

    //
    // Draw solid entities first:
    //
    for (int e = 0; e < num_entities; ++e)
    {
        const entity_t & entity = entities_list[e];

        if (entity.flags & RF_TRANSLUCENT)
        {
            continue; // Drawn on the following pass
            //TODO copy then into a FixedVector for the second pass.
        }

        if (entity.flags & RF_BEAM)
        {
            //DrawBeamModel(entity);
            continue;
        }

        // entity_t::model is an opaque pointer outside the Refresh module, so we need the cast.
        const auto * model = reinterpret_cast<const ModelInstance *>(entity.model);

        if (model == nullptr)
        {
            //DrawNullModel(entity);
            continue;
        }

        switch (model->type)
        {
        case ModelType::kBrush :
            //DrawBrushModel(entity);
            break;

        case ModelType::kSprite :
            //DrawSpriteModel(entity);
            break;

        case ModelType::kAliasMD2 :
            //DrawAliasMD2Model(entity);
            break;

        default:
            GameInterface::Errorf("ViewDrawState::RenderEntities: Bad model type for '%s'!", model->name.CStr());
            break;
        } // switch
    }

    //
    // Now draw the translucent/transparent ones:
    //

    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::BeginRegistration()
{
    // New map loaded, clear the view clusters.
    m_view_cluster      = -1;
    m_view_cluster2     = -1;
    m_old_view_cluster  = -1;
    m_old_view_cluster2 = -1;
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
