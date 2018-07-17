//
// ViewDraw.cpp
//  Common view/3D frame rendering helpers.
//

#include "ViewDraw.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// Local ViewDrawState helpers:
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

static RenderMatrix MakeEntityModelMatrix(const entity_t & entity, const bool flipUpV = true)
{
    const auto t  = RenderMatrix::Translation(entity.origin[0], entity.origin[1], entity.origin[2]);
    const auto rx = RenderMatrix::RotationX(DegToRad(-entity.angles[ROLL]));
    const auto ry = RenderMatrix::RotationY(DegToRad(entity.angles[PITCH] * (flipUpV ? -1.0f : 1.0f)));
    const auto rz = RenderMatrix::RotationZ(DegToRad(entity.angles[YAW]));
    return rx * ry * rz * t;
}

///////////////////////////////////////////////////////////////////////////////
// ViewDrawState
///////////////////////////////////////////////////////////////////////////////

ViewDrawState::ViewDrawState()
    : m_force_null_entity_models{ GameInterface::Cvar::Get("r_force_null_entity_models", "0", 0) }
    , m_lerp_entity_models{ GameInterface::Cvar::Get("r_lerp_entity_models", "1", 0) }
{
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
    const float near_z = 4.0f;    // From ref_gl
    const float far_z  = 4096.0f; // From ref_gl

    // Set projection and view matrices for the frame
    const vec3_t up_direction = { -frame_data.up_vec[0], -frame_data.up_vec[1], -frame_data.up_vec[2] };
    frame_data.view_matrix = RenderMatrix::LookAtRH(frame_data.camera_origin, frame_data.camera_lookat, up_direction);
    frame_data.proj_matrix = RenderMatrix::PerspectiveFovRH(fov_y, aspect_ratio, near_z, far_z);
    frame_data.view_proj_matrix = RenderMatrix::Multiply(frame_data.view_matrix, frame_data.proj_matrix);

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

    BeginBatchArgs args;
    args.model_matrix = RenderMatrix{ RenderMatrix::Identity };
    args.topology     = PrimitiveTopology::TriangleList;

    // Draw with sorting by texture:
    for (TextureImage * tex : tex_store)
    {
        FASTASSERT(tex->width > 0 && tex->height > 0);
        FASTASSERT(tex->type != TextureType::kCount);

        if (tex->texture_chain == nullptr)
        {
            continue;
        }

        args.optional_tex = tex;

        MiniImBatch batch = BeginBatch(args);
        {
            for (const ModelSurface * surf = tex->texture_chain; surf; surf = surf->texture_chain)
            {
                // Need at least one triangle.
                if (surf->polys == nullptr || surf->polys->num_verts < 3)
                {
                    continue;
                }
                batch.PushModelSurface(*surf);
            }
        }
        EndBatch(batch);

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

void ViewDrawState::RenderSolidEntities(FrameData & frame_data)
{
    const int num_entities = frame_data.view_def.num_entities;
    const entity_t * const entities_list = frame_data.view_def.entities;
    const bool force_null_entity_models = m_force_null_entity_models.IsSet();

    for (int e = 0; e < num_entities; ++e)
    {
        const entity_t & entity = entities_list[e];

        if (entity.flags & RF_TRANSLUCENT)
        {
            frame_data.translucent_entities.push_back(&entity);
            continue; // Drawn on the next pass
        }

        // ONLY DRAWS AS TRANSPARENCY
        FASTASSERT(!(entity.flags & RF_BEAM));
//        if (entity.flags & RF_BEAM)
//        {
//            DrawBeamModel(frame_data, entity);
//            continue;
//        }

        // entity_t::model is an opaque pointer outside the Refresh module, so we need the cast.
        const auto * model = reinterpret_cast<const ModelInstance *>(entity.model);
        if (model == nullptr || force_null_entity_models)
        {
            DrawNullModel(frame_data, entity);
            continue;
        }

        switch (model->type)
        {
        case ModelType::kBrush    : { DrawBrushModel(frame_data, entity);    break; }
        case ModelType::kSprite   : { DrawSpriteModel(frame_data, entity);   break; }
        case ModelType::kAliasMD2 : { DrawAliasMD2Model(frame_data, entity); break; }
        default : GameInterface::Errorf("RenderSolidEntities: Bad model type for '%s'!", model->name.CStr());
        } // switch
    }
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::DrawBrushModel(const FrameData & frame_data, const entity_t & entity)
{
    const auto * model = reinterpret_cast<const ModelInstance *>(entity.model);
    FASTASSERT(model != nullptr);

    if (model->data.num_model_surfaces == 0)
    {
        return;
    }

    vec3_t mins, maxs;
    bool rotated;

    if (entity.angles[0] || entity.angles[1] || entity.angles[2])
    {
        rotated = true;
        for (int i = 0; i < 3; ++i)
        {
            mins[i] = entity.origin[i] - model->data.radius;
            maxs[i] = entity.origin[i] + model->data.radius;
        }
    }
    else
    {
        rotated = false;
        Vec3Add(entity.origin, model->data.mins, mins);
        Vec3Add(entity.origin, model->data.maxs, maxs);
    }

    if (ShouldCullBBox(frame_data.frustum, mins, maxs))
    {
        return;
    }

    vec3_t model_origin;
    Vec3Sub(frame_data.view_def.vieworg, entity.origin, model_origin);

    if (rotated)
    {
        vec3_t temp;
        vec3_t forward, right, up;

        Vec3Copy(model_origin, temp);
        MrQ2::VectorsFromAngles(entity.angles, forward, right, up);

        model_origin[0] =  Vec3Dot(temp, forward);
        model_origin[1] = -Vec3Dot(temp, right);
        model_origin[2] =  Vec3Dot(temp, up);
    }

    const auto mdl_mtx = MakeEntityModelMatrix(entity, /* flipUpV = */false);

    /* TODO
    // Calculate dynamic lighting for bmodel
    if (!gl_flashblend->value)
    {
        lt = r_newrefdef.dlights;
        for (k = 0; k < r_newrefdef.num_dlights; k++, lt++)
        {
            R_MarkLights(lt, 1 << k, currentmodel->nodes + currentmodel->firstnode);
        }
    }
    */

    /* TODO
    // Handle transparency pass
    if (entity.flags & RF_TRANSLUCENT)
    {
        qglEnable(GL_BLEND);
        qglColor4f(1, 1, 1, 0.25);
        GL_TexEnv(GL_MODULATE);
    }
    */

    const ModelSurface * surf = &model->data.surfaces[model->data.first_model_surface];
    const int num_surfaces = model->data.num_model_surfaces;

    for (int i = 0; i < num_surfaces; ++i, ++surf)
    {
        // Find which side of the node we are on
        const cplane_t plane = *surf->plane;
        const float dot = Vec3Dot(model_origin, plane.normal) - plane.dist;

        // Draw the polygon
        if (((surf->flags & kSurf_PlaneBack) && (dot < -kBackFaceEpsilon)) ||
           (!(surf->flags & kSurf_PlaneBack) && (dot >  kBackFaceEpsilon)))
        {
            /*
            if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
            {
                // Add to the translucent chain
                surf->texturechain = r_alpha_surfaces;
                r_alpha_surfaces = surf;
            }
            else if (qglMTexCoord2fSGIS && !(psurf->flags & SURF_DRAWTURB))
            {
                GL_RenderLightmappedPoly(psurf);
            }
            else
            {
                GL_EnableMultitexture(false);
                R_RenderBrushPoly(psurf);
                GL_EnableMultitexture(true);
            }
            */

            //TODO handle water polys (SURF_DRAWTURB) as in
            //R_RenderBrushPoly

            //TODO probably becomes an assert once water polygons are handled???
            if (surf->polys == nullptr) continue;

            BeginBatchArgs args;
            args.model_matrix = mdl_mtx;
            args.optional_tex = TextureAnimation(surf->texinfo);
            args.topology     = PrimitiveTopology::TriangleList;

            MiniImBatch batch = BeginBatch(args);
            {
                batch.PushModelSurface(*surf);
            }
            EndBatch(batch);
        }
    }

    /* TODO
    if (!(entity.flags & RF_TRANSLUCENT))
    {
        if (!qglMTexCoord2fSGIS)
            R_BlendLightmaps();
    }
    else
    {
        qglDisable(GL_BLEND);
        qglColor4f(1, 1, 1, 1);
        GL_TexEnv(GL_REPLACE);
    }
    */
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::DrawSpriteModel(const FrameData & frame_data, const entity_t & entity)
{
    //TODO
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::DrawAliasMD2Model(const FrameData & frame_data, const entity_t & entity)
{
    // TODO - weapon depth hack
    const vec3_t shade_light = { 1.0f, 1.0f, 1.0f }; // TODO - temp

    const float  backlerp = (m_lerp_entity_models.IsSet() ? entity.backlerp : 0.0f);
    const auto   mdl_mtx  = MakeEntityModelMatrix(entity, /* flipUpV = */false);
    const auto * model    = reinterpret_cast<const ModelInstance *>(entity.model);

    // Select skin texture:
    const TextureImage * skin = nullptr;
    if (entity.skin != nullptr)
    {
        // Custom player skin (opaque outside the renderer)
        skin = reinterpret_cast<const TextureImage *>(entity.skin);
    }
    else
    {
        if (entity.skinnum >= MAX_MD2SKINS)
        {
            skin = model->data.skins[0];
        }
        else
        {
            skin = model->data.skins[entity.skinnum];
            if (skin == nullptr)
            {
                skin = model->data.skins[0];
            }
        }
    }
    if (skin == nullptr)
    {
        skin = frame_data.tex_store.tex_white2x2; // fallback...
    }

    // Draw interpolated frame:
    DrawAliasMD2FrameLerp(entity, model->hunk.ViewBaseAs<dmdl_t>(), backlerp, shade_light, mdl_mtx, skin);
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::DrawBeamModel(const FrameData & frame_data, const entity_t & entity)
{
    constexpr unsigned kNumBeamSegs = 6;

    vec3_t perp_vec;
    vec3_t old_origin, origin;
    vec3_t direction, normalized_direction;

    Vec3Copy(entity.oldorigin, old_origin);
    Vec3Copy(entity.origin, origin);

    normalized_direction[0] = direction[0] = old_origin[0] - origin[0];
    normalized_direction[1] = direction[1] = old_origin[1] - origin[1];
    normalized_direction[2] = direction[2] = old_origin[2] - origin[2];

    if (Vec3Normalize(normalized_direction) == 0.0f)
    {
        return;
    }

    MrQ2::PerpendicularVector(perp_vec, normalized_direction);
    Vec3Scale(perp_vec, entity.frame / 2, perp_vec);

    const ColorRGBA32 color = TextureStore::ColorForIndex(entity.skinnum & 0xFF);
    const std::uint8_t bR = (color & 0xFF);
    const std::uint8_t bG = (color >> 8)  & 0xFF;
    const std::uint8_t bB = (color >> 16) & 0xFF;

    const float fR = bR * (1.0f / 255.0f);
    const float fG = bG * (1.0f / 255.0f);
    const float fB = bB * (1.0f / 255.0f);
    const float fA = entity.alpha;

    DrawVertex3D start_points[kNumBeamSegs] = {};
    DrawVertex3D end_points[kNumBeamSegs] = {};

    for (unsigned i = 0; i < kNumBeamSegs; ++i)
    {
        MrQ2::RotatePointAroundVector(start_points[i].position, normalized_direction,
                                      perp_vec, (360.0f / kNumBeamSegs) * float(i));

        Vec3Add(start_points[i].position, origin, start_points[i].position);
        Vec3Add(start_points[i].position, direction, end_points[i].position);

        start_points[i].rgba[0] = end_points[i].rgba[0] = fR;
        start_points[i].rgba[1] = end_points[i].rgba[1] = fG;
        start_points[i].rgba[2] = end_points[i].rgba[2] = fB;
        start_points[i].rgba[3] = end_points[i].rgba[3] = fA;
    }

    BeginBatchArgs args;
    args.model_matrix = RenderMatrix{ RenderMatrix::Identity };
    args.optional_tex = nullptr; // No texture
    args.topology     = PrimitiveTopology::TriangleStrip;

    //TODO - missing states!
//    qglDisable(GL_TEXTURE_2D);
//    qglEnable(GL_BLEND);
//    qglDepthMask(GL_FALSE);

    MiniImBatch batch = BeginBatch(args);
    {
        for (unsigned i = 0; i < kNumBeamSegs; ++i)
        {
            batch.PushVertex(start_points[i]);
            batch.PushVertex(end_points[i]);
            batch.PushVertex(start_points[(i + 1) % kNumBeamSegs]);
            batch.PushVertex(end_points[(i + 1) % kNumBeamSegs]);
        }
    }
    EndBatch(batch);
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::DrawNullModel(const FrameData & frame_data, const entity_t & entity)
{
    vec4_t color;
    if (entity.flags & RF_FULLBRIGHT)
    {
        color[0] = 1.0f;
        color[1] = 1.0f;
        color[2] = 1.0f;
        color[3] = 1.0f;
    }
    else
    {
        CalcPointLightColor(frame_data, entity, color);
    }

    const vec2_t uvs[3] = {
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
    };

    BeginBatchArgs args;
    args.model_matrix = MakeEntityModelMatrix(entity);
    args.optional_tex = frame_data.tex_store.tex_debug;
    args.topology     = PrimitiveTopology::TriangleFan;

    // Draw a small octahedron as a placeholder for the entity model:
    MiniImBatch batch = BeginBatch(args);
    {
        // Bottom halve
        batch.SetTriangleFanFirstVertex({ {0.0f, 0.0f, -16.0f}, {0.0f, 0.0f},
                                          {color[0], color[1], color[2], color[3]} });
        for (int i = 0, j = 0; i <= 4; ++i)
        {
            batch.PushVertex({ {16.0f * std::cosf(i * M_PI / 2.0f), 16.0f * std::sinf(i * M_PI / 2.0f), 0.0f},
                               {uvs[j][0], uvs[j][1]}, {color[0], color[1], color[2], color[3]} });
            if (++j > 2) j = 1;
        }

        // Top halve
        batch.SetTriangleFanFirstVertex({ {0.0f, 0.0f, 16.0f}, {0.0f, 0.0f},
                                          {color[0], color[1], color[2], color[3]} });
        for (int i = 4, j = 0; i >= 0; --i)
        {
            batch.PushVertex({ {16.0f * std::cosf(i * M_PI / 2.0f), 16.0f * std::sinf(i * M_PI / 2.0f), 0.0f},
                               {uvs[j][0], uvs[j][1]}, {color[0], color[1], color[2], color[3]} });
            if (++j > 2) j = 1;
        }
    }
    EndBatch(batch);
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawState::CalcPointLightColor(const FrameData & frame_data, const entity_t & entity,
                                        vec4_t out_shade_light_color) const
{
    //TODO
    out_shade_light_color[0] = out_shade_light_color[1] =
    out_shade_light_color[2] = out_shade_light_color[3] = 1.0f;
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
