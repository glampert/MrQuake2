//
// ViewRenderer.cpp
//  Common view/3D frame rendering helpers.
//

#include "ViewRenderer.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// Local ViewRenderer helpers:
///////////////////////////////////////////////////////////////////////////////

// Returns the proper texture for a given time and base texture.
static const TextureImage * TextureAnimation(const ModelTexInfo * tex, const int frame_num)
{
    MRQ2_ASSERT(tex != nullptr);
    const TextureImage * out = nullptr;

    // End of animation / not animated
    if (tex->next == nullptr)
    {
        out = tex->teximage;
    }
    else // Find next image frame
    {
        int c = frame_num % tex->num_frames;
        while (c)
        {
            tex = tex->next;
            --c;
        }

        out = tex->teximage;
    }

    return out;
}

///////////////////////////////////////////////////////////////////////////////

// Returns true if the bounding box is completely outside the frustum
// and should be culled. False if visible and allowed to draw.
static bool ShouldCullBBox(const cplane_t frustum[4], const vec3_t mins, const vec3_t maxs)
{
    // FIXME - culling doesn't seem to working correctly right now...
    /*for (int i = 0; i < 4; ++i)
    {
        if (MrQ2::BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
        {
            return true;
        }
    }*/
    return false;
}

///////////////////////////////////////////////////////////////////////////////

static const ModelLeaf * FindLeafNodeForPoint(const vec3_t p, const ModelInstance & model)
{
    MRQ2_ASSERT(model.data.nodes != nullptr);
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
// ViewRenderer
///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::Init(const RenderDevice & device, const TextureStore & tex_store)
{
    m_tex_white2x2 = tex_store.tex_white2x2;

    m_use_vertex_index_buffers = GameInterface::Cvar::Get("r_use_vertex_index_buffers", "1", CvarWrapper::kFlagArchive);
    m_force_null_entity_models = GameInterface::Cvar::Get("r_force_null_entity_models", "0", 0);
    m_lerp_entity_models       = GameInterface::Cvar::Get("r_lerp_entity_models", "1", 0);
    m_skip_draw_alpha_surfs    = GameInterface::Cvar::Get("r_skip_draw_alpha_surfs", "0", 0);
    m_skip_draw_texture_chains = GameInterface::Cvar::Get("r_skip_draw_texture_chains", "0", 0);
    m_skip_draw_world          = GameInterface::Cvar::Get("r_skip_draw_world", "0", 0);
    m_skip_draw_sky            = GameInterface::Cvar::Get("r_skip_draw_sky", "0", 0);
    m_skip_draw_entities       = GameInterface::Cvar::Get("r_skip_draw_entities", "0", 0);
    m_skip_brush_mods          = GameInterface::Cvar::Get("r_skip_brush_mods", "0", 0);
    m_intensity                = GameInterface::Cvar::Get("r_intensity", "2", CvarWrapper::kFlagArchive);

    constexpr uint32_t kViewDrawBatchSize = 25000; // max vertices * num buffers
    m_vertex_buffers.Init(device, kViewDrawBatchSize);

    m_per_draw_shader_consts.Init(device, sizeof(PerDrawShaderConstants), ConstantBuffer::kOptimizeForSingleDraw);

    // Shaders
    const VertexInputLayout vertex_input_layout = {
        // DrawVertex3D
        {
            { VertexInputLayout::kVertexPosition,  VertexInputLayout::kFormatFloat3, offsetof(DrawVertex3D, position) },
            { VertexInputLayout::kVertexTexCoords, VertexInputLayout::kFormatFloat2, offsetof(DrawVertex3D, uv)       },
            { VertexInputLayout::kVertexColor,     VertexInputLayout::kFormatFloat4, offsetof(DrawVertex3D, rgba)     },
        }
    };
    if (!m_render3d_shader.LoadFromFile(device, vertex_input_layout, "Draw3D"))
    {
        GameInterface::Errorf("Failed to load Draw3D shader!");
    }

    // Opaque/solid geometry
    m_pipeline_solid_geometry.Init(device);
    m_pipeline_solid_geometry.SetPrimitiveTopology(PrimitiveTopology::kTriangleList);
    m_pipeline_solid_geometry.SetShaderProgram(m_render3d_shader);
    m_pipeline_solid_geometry.SetAlphaBlendingEnabled(false);
    m_pipeline_solid_geometry.SetDepthTestEnabled(true);
    m_pipeline_solid_geometry.SetDepthWritesEnabled(true);
    m_pipeline_solid_geometry.SetCullEnabled(true);
    m_pipeline_solid_geometry.Finalize();

    // World translucencies (windows/glass)
    m_pipeline_translucent_world_geometry.Init(device);
    m_pipeline_translucent_world_geometry.SetPrimitiveTopology(PrimitiveTopology::kTriangleList);
    m_pipeline_translucent_world_geometry.SetShaderProgram(m_render3d_shader);
    m_pipeline_translucent_world_geometry.SetAlphaBlendingEnabled(true);
    m_pipeline_translucent_world_geometry.SetDepthTestEnabled(true);
    m_pipeline_translucent_world_geometry.SetDepthWritesEnabled(true);
    m_pipeline_translucent_world_geometry.SetCullEnabled(true);
    m_pipeline_translucent_world_geometry.Finalize();

    // Translucent entities (disable z writes in case they stack up)
    m_pipeline_translucent_entities.Init(device);
    m_pipeline_translucent_entities.SetPrimitiveTopology(PrimitiveTopology::kTriangleList);
    m_pipeline_translucent_entities.SetShaderProgram(m_render3d_shader);
    m_pipeline_translucent_entities.SetAlphaBlendingEnabled(true);
    m_pipeline_translucent_entities.SetDepthTestEnabled(true);
    m_pipeline_translucent_entities.SetDepthWritesEnabled(false);
    m_pipeline_translucent_entities.SetCullEnabled(true);
    m_pipeline_translucent_entities.Finalize();

    // Dynamic lights: Use additive blending
    m_pipeline_dlights.Init(device);
    m_pipeline_dlights.SetPrimitiveTopology(PrimitiveTopology::kTriangleList);
    m_pipeline_dlights.SetShaderProgram(m_render3d_shader);
    m_pipeline_dlights.SetAlphaBlendingEnabled(true);
    m_pipeline_dlights.SetAdditiveBlending(true);
    m_pipeline_dlights.SetDepthTestEnabled(true);
    m_pipeline_dlights.SetDepthWritesEnabled(false);
    m_pipeline_dlights.SetCullEnabled(true);
    m_pipeline_dlights.Finalize();
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::Shutdown()
{
    m_skybox = {};
    m_alpha_world_surfaces = nullptr;
    m_tex_white2x2 = nullptr;

    for (int pass = 0; pass < kRenderPassCount; ++pass)
    {
        m_draw_cmds[pass].clear();
    }

    m_pipeline_solid_geometry.Shutdown();
    m_pipeline_translucent_world_geometry.Shutdown();
    m_pipeline_translucent_entities.Shutdown();
    m_pipeline_dlights.Shutdown();
    m_render3d_shader.Shutdown();
    m_per_draw_shader_consts.Shutdown();
    m_vertex_buffers.Shutdown();
}

///////////////////////////////////////////////////////////////////////////////

MiniImBatch ViewRenderer::BeginBatch(const BeginBatchArgs & args)
{
    MRQ2_ASSERT(m_batch_open == false);
    MRQ2_ASSERT_ALIGN16(args.model_matrix.floats);

    m_current_draw_cmd.consts.model_matrix = args.model_matrix;
    m_current_draw_cmd.texture      = (args.optional_tex != nullptr) ? args.optional_tex : m_tex_white2x2;
    m_current_draw_cmd.topology     = args.topology;
    m_current_draw_cmd.depth_hack   = args.depth_hack;
    m_current_draw_cmd.first_vert   = 0;
    m_current_draw_cmd.vertex_count = 0;

    m_batch_open = true;

    return MiniImBatch{ m_vertex_buffers.CurrentVertexPtr(), m_vertex_buffers.NumVertsRemaining(), args.topology };
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::EndBatch(MiniImBatch & batch)
{
    MRQ2_ASSERT(batch.IsValid());
    MRQ2_ASSERT(m_batch_open == true);
    MRQ2_ASSERT(m_current_draw_cmd.topology == batch.Topology());

    m_current_draw_cmd.first_vert   = m_vertex_buffers.CurrentPosition();
    m_current_draw_cmd.vertex_count = batch.UsedVerts();

    m_vertex_buffers.Increment(batch.UsedVerts());

    MRQ2_ASSERT(m_current_pass < kRenderPassCount);
    m_draw_cmds[m_current_pass].push_back(m_current_draw_cmd);
    m_current_draw_cmd = {};

    batch.Clear();
    m_batch_open = false;
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::BeginRegistration()
{
    // New map loaded, clear the view clusters.
    m_view_cluster      = -1;
    m_view_cluster2     = -1;
    m_old_view_cluster  = -1;
    m_old_view_cluster2 = -1;
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::EndRegistration()
{
    // Currently not required.
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::SetUpViewClusters(const FrameData & frame_data)
{
    if ((frame_data.view_def.rdflags & RDF_NOWORLDMODEL) || m_skip_draw_world.IsSet())
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

void ViewRenderer::SetUpFrustum(FrameData & frame_data) const
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

const PipelineState & ViewRenderer::PipelineStateForPass(const int pass) const
{
    switch (pass)
    {
    case kPass_SolidGeometry:
        return m_pipeline_solid_geometry;
    case kPass_TranslucentSurfaces:
        return m_pipeline_translucent_world_geometry;
    case kPass_TranslucentEntities:
        return m_pipeline_translucent_entities;
    case kPass_DLights:
        return m_pipeline_dlights;
    default:
        GameInterface::Errorf("Invalid pass index!");
    } // switch
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::BatchImmediateModeDrawCmds()
{
    MRQ2_ASSERT(m_batch_open == false);
    MRQ2_ASSERT(m_current_pass == kPass_Invalid);

    for (int pass = 0; pass < kRenderPassCount; ++pass)
    {
        MRQ2_ASSERT(m_draw_cmds[pass].empty());
    }

    m_vertex_buffers.BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::FlushImmediateModeDrawCmds(FrameData & frame_data)
{
    MRQ2_ASSERT(m_batch_open == false);

    auto PushRenderPassMarker = [](GraphicsContext & context, const int pass)
    {
        switch (pass)
        {
        case kPass_SolidGeometry       : MRQ2_PUSH_GPU_MARKER(context, "SolidGeometry");       break;
        case kPass_TranslucentSurfaces : MRQ2_PUSH_GPU_MARKER(context, "TranslucentSurfaces"); break;
        case kPass_TranslucentEntities : MRQ2_PUSH_GPU_MARKER(context, "TranslucentEntities"); break;
        case kPass_DLights             : MRQ2_PUSH_GPU_MARKER(context, "DLights");             break;
        default : GameInterface::Errorf("Invalid pass index!");
        } // switch
    };

    auto & context  = frame_data.context;
    auto & cbuffers = frame_data.cbuffers;

    const auto draw_buf = m_vertex_buffers.EndFrame();
    const VertexBuffer & vertex_buffer = *draw_buf.buffer_ptr;

    for (int pass = 0; pass < kRenderPassCount; ++pass)
    {
        if (m_draw_cmds[pass].empty())
        {
            continue;
        }

        PushRenderPassMarker(context, pass);

        context.SetPipelineState(PipelineStateForPass(pass));
        context.SetVertexBuffer(vertex_buffer);

        uint32_t cbuffer_slot = 0;
        for (; cbuffer_slot < cbuffers.size(); ++cbuffer_slot)
        {
            context.SetConstantBuffer(*cbuffers[cbuffer_slot], cbuffer_slot);
        }

        for (const DrawCmd & cmd : m_draw_cmds[pass])
        {
            // Depth hack to prevent weapons from poking into geometry.
            if (cmd.depth_hack)
            {
                constexpr float depth_min = 0.0f;
                constexpr float depth_max = 1.0f;
                context.SetDepthRange(depth_min, depth_min + 0.3f * (depth_max - depth_min));
            }

            context.SetAndUpdateConstantBufferForDraw(m_per_draw_shader_consts, cbuffer_slot, cmd.consts);

            context.SetPrimitiveTopology(cmd.topology);
            context.SetTexture(cmd.texture->BackendTexture(), 0);

            context.Draw(cmd.first_vert, cmd.vertex_count);

            // Restore to default if we did a depth-hacked draw.
            context.RestoreDepthRange();
        }

        MRQ2_POP_GPU_MARKER(context);
        m_draw_cmds[pass].clear();
    }

    m_current_pass = kPass_Invalid;
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::DoRenderView(FrameData & frame_data)
{
    BatchImmediateModeDrawCmds();

    //
    // Opaque/solid geometry pass
    //
    m_current_pass = kPass_SolidGeometry;
    RenderWorldModel(frame_data);
    RenderSkyBox(frame_data);
    RenderSolidEntities(frame_data);

    //
    // Transparencies/alpha passes
    //
    m_current_pass = kPass_TranslucentSurfaces; // Color Blend ON for static world geometry
    RenderTranslucentSurfaces(frame_data);

    m_current_pass = kPass_TranslucentEntities; // Disable Z writes in case entities stack up
    RenderTranslucentEntities(frame_data);

    m_current_pass = kPass_TranslucentEntities; // Also with Z writes disabled
    RenderParticles(frame_data);

    m_current_pass = kPass_DLights; // Simulated light sources use additive blending
    RenderDLights(frame_data);

    FlushImmediateModeDrawCmds(frame_data);
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::RenderViewSetup(FrameData & frame_data)
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
    const float near_z = 0.5f;    // was 4.0 in ref_gl, which causes some clipping in the gun model
    const float far_z  = 4096.0f; // original value from ref_gl

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
void ViewRenderer::RecursiveWorldNode(const FrameData & frame_data,
                                       const ModelInstance & world_mdl,
                                       const ModelNode * const node)
{
    MRQ2_ASSERT(node != nullptr);

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
            m_skybox.AddSkySurface(*surf, frame_data.view_def.vieworg);
        }
        else if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66 | SURF_WARP))
        {
            // Add to the translucent draw chain.
            surf->texture_chain = m_alpha_world_surfaces;
            m_alpha_world_surfaces = surf;
        }
        else // Opaque texture chain
        {
            const TextureImage * image = TextureAnimation(surf->texinfo, static_cast<int>(frame_data.view_def.time * 2.0f));
            MRQ2_ASSERT(image != nullptr);

            surf->texture_chain = image->DrawChainPtr();
            image->SetDrawChainPtr(surf);
        }
    }

    // Finally recurse down the back side:
    RecursiveWorldNode(frame_data, world_mdl, node->children[!side]);
}

///////////////////////////////////////////////////////////////////////////////

// Mark the leaves and nodes that are in the PVS for the current cluster.
void ViewRenderer::MarkLeaves(ModelInstance & world_mdl)
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
        MRQ2_ASSERT(unsigned(c) < (ArrayLength(combined_vis_pvs) / sizeof(std::uint32_t)));

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

void ViewRenderer::DrawTextureChains(FrameData & frame_data)
{
    const bool do_draw   = !m_skip_draw_texture_chains.IsSet();
    const bool use_vb_ib = m_use_vertex_index_buffers.IsSet();

    auto & context  = frame_data.context;
    auto & cbuffers = frame_data.cbuffers;

    BeginBatchArgs batch_args;
    batch_args.model_matrix = RenderMatrix{ RenderMatrix::kIdentity };
    batch_args.topology     = PrimitiveTopology::kTriangleList;
    batch_args.depth_hack   = false;

    if (use_vb_ib && kUseVertexAndIndexBuffers)
    {
        MRQ2_PUSH_GPU_MARKER(context, "DrawTextureChains");

        context.SetPipelineState(m_pipeline_solid_geometry);
        context.SetVertexBuffer(frame_data.world_model.vb);
        context.SetIndexBuffer(frame_data.world_model.ib);
        context.SetPrimitiveTopology(batch_args.topology);

        uint32_t cbuffer_slot = 0;
        for (; cbuffer_slot < cbuffers.size(); ++cbuffer_slot)
        {
            context.SetConstantBuffer(*cbuffers[cbuffer_slot], cbuffer_slot);
        }

        PerDrawShaderConstants consts;
        consts.model_matrix = batch_args.model_matrix;
        context.SetAndUpdateConstantBufferForDraw(m_per_draw_shader_consts, cbuffer_slot, consts);
    }

    // Draw with sorting by texture:
    for (const TextureImage * tex : frame_data.tex_store)
    {
        MRQ2_ASSERT(tex->Width() > 0 && tex->Height() > 0);
        MRQ2_ASSERT(tex->Type() != TextureType::kCount);

        if (tex->DrawChainPtr() == nullptr)
        {
            continue;
        }

        if (do_draw)
        {
            if (use_vb_ib && kUseVertexAndIndexBuffers) // Use the prebaked vertex and index buffers
            {
                for (const ModelSurface * surf = tex->DrawChainPtr(); surf; surf = surf->texture_chain)
                {
                    if (const ModelPoly * poly = surf->polys)
                    {
                        if (poly->num_verts >= 3) // Need at least one triangle.
                        {
                            const auto range = poly->index_buffer;
                            MRQ2_ASSERT(range.first_index >= 0 && range.index_count > 0 && range.base_vertex >= 0);

                            context.SetTexture(tex->BackendTexture(), 0);
                            context.DrawIndexed(range.first_index, range.index_count, range.base_vertex);
                        }
                    }
                }
            }
            else // Immediate mode emulation
            {
                batch_args.optional_tex = tex;

                MiniImBatch batch = BeginBatch(batch_args);
                {
                    for (const ModelSurface * surf = tex->DrawChainPtr(); surf; surf = surf->texture_chain)
                    {
                        if (const ModelPoly * poly = surf->polys)
                        {
                            if (poly->num_verts >= 3) // Need at least one triangle.
                            {
                                batch.PushModelSurface(*surf);
                            }
                        }
                    }
                }
                EndBatch(batch);
            }
        }

        // All world geometry using this texture has been drawn, clear for the next frame.
        tex->SetDrawChainPtr(nullptr);
    }

    if (use_vb_ib && kUseVertexAndIndexBuffers)
    {
        MRQ2_POP_GPU_MARKER(context);
    }
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::RenderTranslucentSurfaces(FrameData & frame_data)
{
    if (m_skip_draw_alpha_surfs.IsSet())
    {
        return;
    }

    // The textures are prescaled up for a better
    // lighting range, so scale it back down.
    const float inv_intensity = 1.0f / m_intensity.AsFloat();

    // Draw water surfaces and windows.
    // The BSP tree is walked front to back, so unwinding the chain
    // of alpha surfaces will draw back to front, giving proper ordering.

    for (const ModelSurface * surf = m_alpha_world_surfaces; surf; surf = surf->texture_chain)
    {
        // Need at least one triangle.
        if (surf->polys == nullptr || surf->polys->num_verts < 3)
        {
            continue;
        }

        vec4_t color_alpha;
        if (surf->texinfo->flags & SURF_TRANS33)
        {
            color_alpha[0] = inv_intensity;
            color_alpha[1] = inv_intensity;
            color_alpha[2] = inv_intensity;
            color_alpha[3] = 0.33f;
        }
        else if (surf->texinfo->flags & SURF_TRANS66)
        {
            color_alpha[0] = inv_intensity;
            color_alpha[1] = inv_intensity;
            color_alpha[2] = inv_intensity;
            color_alpha[3] = 0.66f;
        }
        else // Solid color
        {
            color_alpha[0] = inv_intensity;
            color_alpha[1] = inv_intensity;
            color_alpha[2] = inv_intensity;
            color_alpha[3] = 1.0f;
        }

        if (surf->flags & kSurf_DrawTurb) // Draw with vertex animation/displacement
        {
            DrawAnimatedWaterPolys(*surf, frame_data.view_def.time, color_alpha);
        }
        else // Static translucent surface (glass, completely still fluid)
        {
            BeginBatchArgs args;
            args.model_matrix = RenderMatrix{ RenderMatrix::kIdentity };
            args.optional_tex = surf->texinfo->teximage;
            args.topology     = PrimitiveTopology::kTriangleList;
            args.depth_hack   = false;

            MiniImBatch batch = BeginBatch(args);
            {
                batch.PushModelSurface(*surf, &color_alpha);
            }
            EndBatch(batch);
        }
    }

    m_alpha_world_surfaces = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::RenderTranslucentEntities(FrameData & frame_data)
{
    if (m_skip_draw_entities.IsSet())
    {
        return;
    }

    const bool force_null_entity_models = m_force_null_entity_models.IsSet();

    for (const entity_t * entity : frame_data.translucent_entities)
    {
        if (!(entity->flags & RF_TRANSLUCENT))
        {
            continue; // Already done in the solid pass
        }

        if (entity->flags & RF_BEAM) // Special case beam model
        {
            DrawBeamModel(frame_data, *entity);
            continue;
        }

        // entity_t::model is an opaque pointer outside the Refresh module, so we need the cast.
        const auto * model = reinterpret_cast<const ModelInstance *>(entity->model);
        if (model == nullptr || force_null_entity_models)
        {
            DrawNullModel(frame_data, *entity);
            continue;
        }

        switch (model->type)
        {
        case ModelType::kBrush    : { DrawBrushModel(frame_data, *entity);    break; }
        case ModelType::kSprite   : { DrawSpriteModel(frame_data, *entity);   break; }
        case ModelType::kAliasMD2 : { DrawAliasMD2Model(frame_data, *entity); break; }
        default : GameInterface::Errorf("RenderTranslucentEntities: Bad model type for '%s'!", model->name.CStr());
        } // switch
    }
}

///////////////////////////////////////////////////////////////////////////////

// Classic blocky Quake2 particles using a single triangle and
// a special 8x8 texture with a dot-like pattern in its top-left corner.
void ViewRenderer::RenderParticles(const FrameData & frame_data)
{
    const int num_particles = frame_data.view_def.num_particles;
    if (num_particles <= 0)
    {
        return;
    }

    vec3_t up, right;
    Vec3Scale(frame_data.up_vec,    1.5f, up);
    Vec3Scale(frame_data.right_vec, 1.5f, right);

    BeginBatchArgs args;
    args.model_matrix = RenderMatrix{ RenderMatrix::kIdentity };
    args.optional_tex = frame_data.tex_store.tex_particle;
    args.topology     = PrimitiveTopology::kTriangleList;
    args.depth_hack   = false;

    MiniImBatch batch = BeginBatch(args);

    const particle_t * const particles = frame_data.view_def.particles;
    for (int i = 0; i < num_particles; ++i)
    {
        const particle_t * p = &particles[i];

        // hack a scale up to keep particles from disappearing
        float scale = (p->origin[0] - frame_data.camera_origin[0]) * frame_data.forward_vec[0] +
                      (p->origin[1] - frame_data.camera_origin[1]) * frame_data.forward_vec[1] +
                      (p->origin[2] - frame_data.camera_origin[2]) * frame_data.forward_vec[2];

        if (scale < 20.0f)
            scale = 1.0f;
        else
            scale = 1.0f + scale * 0.004f;

        const ColorRGBA32 color = TextureStore::ColorForIndex(p->color & 0xFF);
        const std::uint8_t bR = (color & 0xFF);
        const std::uint8_t bG = (color >> 8) & 0xFF;
        const std::uint8_t bB = (color >> 16) & 0xFF;

        const float fR = bR * (1.0f / 255.0f);
        const float fG = bG * (1.0f / 255.0f);
        const float fB = bB * (1.0f / 255.0f);
        const float fA = p->alpha;

        DrawVertex3D v;
        v.rgba[0] = fR;
        v.rgba[1] = fG;
        v.rgba[2] = fB;
        v.rgba[3] = fA;

        v.position[0] = p->origin[0];
        v.position[1] = p->origin[1];
        v.position[2] = p->origin[2];
        v.uv[0] = 0.0625f;
        v.uv[1] = 0.0625f;
        batch.PushVertex(v);

        v.position[0] = p->origin[0] + up[0] * scale;
        v.position[1] = p->origin[1] + up[1] * scale;
        v.position[2] = p->origin[2] + up[2] * scale;
        v.uv[0] = 1.0625f;
        v.uv[1] = 0.0625f;
        batch.PushVertex(v);

        v.position[0] = p->origin[0] + right[0] * scale;
        v.position[1] = p->origin[1] + right[1] * scale;
        v.position[2] = p->origin[2] + right[2] * scale;
        v.uv[0] = 0.0625f;
        v.uv[1] = 1.0625f;
        batch.PushVertex(v);
    }

    EndBatch(batch);
}

///////////////////////////////////////////////////////////////////////////////

// A Quake2 Dynamic Light (DLight) is a point light simulated with a circular billboarded sprite that follows the light source.
// This is used to simulate gunshot flares for example. The sprite is rendered with additive blending (qglBlendFunc(GL_ONE, GL_ONE)).
void ViewRenderer::RenderDLights(const FrameData & frame_data)
{
    const int num_dlights = frame_data.view_def.num_dlights;
    if (num_dlights <= 0)
    {
        return;
    }

    BeginBatchArgs args;
    args.model_matrix = RenderMatrix{ RenderMatrix::kIdentity };
    args.optional_tex = nullptr;
    args.topology     = PrimitiveTopology::kTriangleFan;
    args.depth_hack   = false;

    const dlight_t * const dlights = frame_data.view_def.dlights;
    for (int l = 0; l < num_dlights; ++l)
    {
        const dlight_t * const light = &dlights[l];

        MiniImBatch batch = BeginBatch(args);
        {
            DrawVertex3D vert = {};

            vert.rgba[0] = light->color[0] * 0.2f;
            vert.rgba[1] = light->color[1] * 0.2f;
            vert.rgba[2] = light->color[2] * 0.2f;
            vert.rgba[3] = 1.0f;

            const float radius = light->intensity * 0.35f;
            for (int v = 0; v < 3; ++v)
            {
                vert.position[v] = light->origin[v] - frame_data.forward_vec[v] * radius;
            }

            batch.SetTriangleFanFirstVertex(vert);

            vert.rgba[0] = 0.0f;
            vert.rgba[1] = 0.0f;
            vert.rgba[2] = 0.0f;
            vert.rgba[3] = 1.0f;

            for (int i = 16; i >= 0; --i)
            {
                const float a = i / 16.0f * M_PI * 2.0f;
                for (int j = 0; j < 3; ++j)
                {
                    vert.position[j] = light->origin[j] + frame_data.right_vec[j] * std::cosf(a) * radius + frame_data.up_vec[j] * std::sinf(a) * radius;
                }

                batch.PushVertex(vert);
            }
        }
        EndBatch(batch);
    }
}

///////////////////////////////////////////////////////////////////////////////

constexpr float kTurbScale = (256.0f / (2.0f * M_PI));

#ifdef _MSC_VER
    // "conversion from 'double' to 'const float' requires a narrowing conversion"
    // warpsin values from Quake 2 are missing the .f suffix on float values.
    #pragma warning(push)
    #pragma warning(disable: 4838)
#endif // _MSC_VER

static const float s_turb_sin[] = {
#include "client/warpsin.h"
};

#ifdef _MSC_VER
    #pragma warning(pop)
#endif // _MSC_VER

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::DrawAnimatedWaterPolys(const ModelSurface & surf, const float frame_time, const vec4_t color)
{
    float scroll;
    if (surf.texinfo->flags & SURF_FLOWING)
    {
        scroll = -float(kSubdivideSize) * ((frame_time * 0.5f) - int(frame_time * 0.5f));
    }
    else
    {
        scroll = 0.0f;
    }

    BeginBatchArgs args;
    args.model_matrix = RenderMatrix{ RenderMatrix::kIdentity };
    args.optional_tex = surf.texinfo->teximage;
    args.topology     = PrimitiveTopology::kTriangleFan;
    args.depth_hack   = false;

    // HACK: There's some noticeable z-fighting happening with lava and water touching walls when you go underwater.
    // Adding a small offset to the positions resolves it. No idea why this didn't happen with the original
    // OpenGL renderer, maybe the lower precision floating-point math was actually hiding the flickering?
    static auto r_water_hack = GameInterface::Cvar::Get("r_water_hack", "0.5", CvarWrapper::kFlagArchive);
    const float water_position_offset_hack = r_water_hack.AsFloat();

    for (const ModelPoly * poly = surf.polys; poly; poly = poly->next)
    {
        MiniImBatch batch = BeginBatch(args);
        {
            const int num_verts = poly->num_verts;
            for (int v = 0; v < num_verts; ++v)
            {
                const float os = poly->vertexes[v].texture_s;
                const float ot = poly->vertexes[v].texture_t;

                float s = os + s_turb_sin[int((ot * 0.125f + frame_time) * kTurbScale) & 255];
                s += scroll;
                s *= (1.0f / kSubdivideSize);

                float t = ot + s_turb_sin[int((os * 0.125f + frame_time) * kTurbScale) & 255];
                t *= (1.0f / kSubdivideSize);

                DrawVertex3D vert;

                vert.position[0] = poly->vertexes[v].position[0];
                vert.position[1] = poly->vertexes[v].position[1];
                vert.position[2] = poly->vertexes[v].position[2];

                vert.uv[0] = s;
                vert.uv[1] = t;

                vert.rgba[0] = color[0];
                vert.rgba[1] = color[1];
                vert.rgba[2] = color[2];
                vert.rgba[3] = color[3];

                // X
                if (vert.position[0] > 0.0f) vert.position[0] += water_position_offset_hack;
                if (vert.position[0] < 0.0f) vert.position[0] -= water_position_offset_hack;
                // Y
                if (vert.position[1] > 0.0f) vert.position[1] += water_position_offset_hack;
                if (vert.position[1] < 0.0f) vert.position[1] -= water_position_offset_hack;

                if (v == 0)
                {
                    batch.SetTriangleFanFirstVertex(vert);
                }
                else
                {
                    batch.PushVertex(vert);
                }
            }
        }
        EndBatch(batch);
    }
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::RenderWorldModel(FrameData & frame_data)
{
    m_alpha_world_surfaces = nullptr;
    m_skybox.Clear(); // RecursiveWorldNode adds to the sky bounds

    if ((frame_data.view_def.rdflags & RDF_NOWORLDMODEL) || m_skip_draw_world.IsSet())
    {
        return;
    }

    MarkLeaves(frame_data.world_model);
    RecursiveWorldNode(frame_data, frame_data.world_model, frame_data.world_model.data.nodes);
    DrawTextureChains(frame_data);
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::RenderSkyBox(FrameData & frame_data)
{
    // Skybox bounds rendering if visible:
    if (m_skybox.IsAnyPlaneVisible() && !m_skip_draw_sky.IsSet())
    {
        const auto sky_t = RenderMatrix::Translation(frame_data.view_def.vieworg[0],
                                                     frame_data.view_def.vieworg[1],
                                                     frame_data.view_def.vieworg[2]);

        const float sky_rotate = DegToRad(frame_data.view_def.time * m_skybox.RotateDegrees());
        const auto  sky_rxyz   = RenderMatrix::RotationAxis(sky_rotate, m_skybox.AxisX(), m_skybox.AxisY(), m_skybox.AxisZ());
        const auto  sky_mtx    = sky_rxyz * sky_t;

        for (int i = 0; i < SkyBox::kNumSides; ++i)
        {
            BeginBatchArgs args;
            DrawVertex3D sky_verts[6];
            const TextureImage * sky_tex = nullptr;

            if (m_skybox.BuildSkyPlane(i, sky_verts, &sky_tex))
            {
                args.model_matrix = sky_mtx;
                args.optional_tex = sky_tex;
                args.topology     = PrimitiveTopology::kTriangleList;
                args.depth_hack   = false;

                MiniImBatch batch = BeginBatch(args);
                for (unsigned v = 0; v < ArrayLength(sky_verts); ++v)
                {
                    batch.PushVertex(sky_verts[v]);
                }
                EndBatch(batch);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::RenderSolidEntities(FrameData & frame_data)
{
    if (m_skip_draw_entities.IsSet())
    {
        return;
    }

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

        // Draws with the translucent entities.
        MRQ2_ASSERT(!(entity.flags & RF_BEAM));

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

// Draw an inline brush model either using immediate mode emulation or vertex/index buffers.
// This renders things like doors, windows and moving platforms.
void ViewRenderer::DrawBrushModel(const FrameData & frame_data, const entity_t & entity)
{
    if (m_skip_brush_mods.IsSet())
    {
        return;
    }

    const auto * model = reinterpret_cast<const ModelInstance *>(entity.model);
    MRQ2_ASSERT(model != nullptr);

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

    const bool use_vb_ib = m_use_vertex_index_buffers.IsSet();
    auto & context  = frame_data.context;
    auto & cbuffers = frame_data.cbuffers;

    // IndexBuffer rendering
    if (use_vb_ib && kUseVertexAndIndexBuffers)
    {
        MRQ2_PUSH_GPU_MARKER(context, "DrawBrushModel");

        const PipelineState * pipeline;
        if (entity.flags & RF_TRANSLUCENT)
        {
            pipeline = &m_pipeline_translucent_entities;
        }
        else
        {
            pipeline = &m_pipeline_solid_geometry;
        }

        context.SetPipelineState(*pipeline);
        context.SetVertexBuffer(frame_data.world_model.vb);
        context.SetIndexBuffer(frame_data.world_model.ib);
        context.SetPrimitiveTopology(PrimitiveTopology::kTriangleList);

        uint32_t cbuffer_slot = 0;
        for (; cbuffer_slot < cbuffers.size(); ++cbuffer_slot)
        {
            context.SetConstantBuffer(*cbuffers[cbuffer_slot], cbuffer_slot);
        }

        PerDrawShaderConstants consts;
        consts.model_matrix = mdl_mtx;
        context.SetAndUpdateConstantBufferForDraw(m_per_draw_shader_consts, cbuffer_slot, consts);
    }

    ModelSurface * surf = &model->data.surfaces[model->data.first_model_surface];
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
            if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66 | SURF_WARP))
            {
                // Add to the translucent draw chain.
                surf->texture_chain = m_alpha_world_surfaces;
                m_alpha_world_surfaces = surf;
            }
            else 
            {
                // TODO handle water polys (SURF_DRAWTURB) as done in R_RenderBrushPoly
                /*
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

                if (const ModelPoly * poly = surf->polys)
                {
                    // IndexBuffer rendering
                    if (use_vb_ib && kUseVertexAndIndexBuffers && (poly->index_buffer.index_count > 0))
                    {
                        const auto range = poly->index_buffer;
                        MRQ2_ASSERT(range.first_index >= 0 && range.base_vertex >= 0);

                        auto * tex = TextureAnimation(surf->texinfo, entity.frame);
                        context.SetTexture(tex->BackendTexture(), 0);
                        context.DrawIndexed(range.first_index, range.index_count, range.base_vertex);
                    }
                    else // Immediate mode emulation
                    {
                        BeginBatchArgs args;
                        args.model_matrix = mdl_mtx;
                        args.optional_tex = TextureAnimation(surf->texinfo, entity.frame);
                        args.topology     = PrimitiveTopology::kTriangleList;
                        args.depth_hack   = false;

                        MiniImBatch batch = BeginBatch(args);
                        {
                            batch.PushModelSurface(*surf);
                        }
                        EndBatch(batch);
                    }
                }
            }
        }
    }

    if (use_vb_ib && kUseVertexAndIndexBuffers)
    {
        MRQ2_POP_GPU_MARKER(context);
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

void ViewRenderer::DrawSpriteModel(const FrameData & frame_data, const entity_t & entity)
{
    const auto * model = reinterpret_cast<const ModelInstance *>(entity.model);
    const auto * p_sprite = model->hunk.ViewBaseAs<dsprite_t>();

    const int frame_num = entity.frame % p_sprite->numframes;
    const dsprframe_t* const frame = &p_sprite->frames[frame_num];
    MRQ2_ASSERT(frame_num < kMaxMD2Skins);

    const float * const up = frame_data.up_vec;
    const float * const right = frame_data.right_vec;

    float alpha = 1.0f;
    if (entity.flags & RF_TRANSLUCENT)
    {
        alpha = entity.alpha;
    }

    // Camera facing billboarded quad:
    DrawVertex3D quad[4];
    const int indexes[6] = { 0,1,2, 2,3,0 };

    quad[0].uv[0] = 0.0f;
    quad[0].uv[1] = 1.0f;
    MrQ2::VecSplatN(quad[0].rgba, 1.0f);
    quad[0].rgba[3] = alpha;
    MrQ2::Vec3MAdd(entity.origin, -frame->origin_y, up, quad[0].position);
    MrQ2::Vec3MAdd(quad[0].position, -frame->origin_x, right, quad[0].position);

    quad[1].uv[0] = 0.0f;
    quad[1].uv[1] = 0.0f;
    MrQ2::VecSplatN(quad[1].rgba, 1.0f);
    quad[1].rgba[3] = alpha;
    MrQ2::Vec3MAdd(entity.origin, frame->height - frame->origin_y, up, quad[1].position);
    MrQ2::Vec3MAdd(quad[1].position, -frame->origin_x, right, quad[1].position);

    quad[2].uv[0] = 1.0f;
    quad[2].uv[1] = 0.0f;
    MrQ2::VecSplatN(quad[2].rgba, 1.0f);
    quad[2].rgba[3] = alpha;
    MrQ2::Vec3MAdd(entity.origin, frame->height - frame->origin_y, up, quad[2].position);
    MrQ2::Vec3MAdd(quad[2].position, frame->width - frame->origin_x, right, quad[2].position);

    quad[3].uv[0] = 1.0f;
    quad[3].uv[1] = 1.0f;
    MrQ2::VecSplatN(quad[3].rgba, 1.0f);
    quad[3].rgba[3] = alpha;
    MrQ2::Vec3MAdd(entity.origin, -frame->origin_y, up, quad[3].position);
    MrQ2::Vec3MAdd(quad[3].position, frame->width - frame->origin_x, right, quad[3].position);

    BeginBatchArgs args;
    args.model_matrix = RenderMatrix{ RenderMatrix::kIdentity };
    args.optional_tex = model->data.skins[frame_num];
    args.topology     = PrimitiveTopology::kTriangleList;
    args.depth_hack   = false;

    MiniImBatch batch = BeginBatch(args);
    {
        DrawVertex3D * tri = batch.Increment(6);
        for (int i = 0; i < 6; ++i)
        {
            tri[i] = quad[indexes[i]];
        }
    }
    EndBatch(batch);
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::DrawAliasMD2Model(const FrameData & frame_data, const entity_t & entity)
{
    vec4_t shade_light = { 1.0f, 1.0f, 1.0f, 1.0f };
    ShadeAliasMD2Model(frame_data, entity, shade_light);

    const float  backlerp = (m_lerp_entity_models.IsSet() ? entity.backlerp : 0.0f);
    const auto   mdl_mtx  = MakeEntityModelMatrix(entity, /* flipUpV = */false);
    const auto * model    = reinterpret_cast<const ModelInstance *>(entity.model);

    // Select skin texture:
    const TextureImage * skin = nullptr;
    if (entity.skin != nullptr)
    {
        // Custom player skin (opaque pointer outside the renderer)
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

// Draw a translucent cylinder. Z writes should be OFF.
void ViewRenderer::DrawBeamModel(const FrameData & frame_data, const entity_t & entity)
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
    args.model_matrix = RenderMatrix{ RenderMatrix::kIdentity };
    args.optional_tex = nullptr; // No texture
    args.topology     = PrimitiveTopology::kTriangleStrip;
    args.depth_hack   = false;

    // Draw together with the translucent entities so we can assume Z writes are off and blending is enabled.
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

void ViewRenderer::DrawNullModel(const FrameData & frame_data, const entity_t & entity)
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
    args.optional_tex = frame_data.tex_store.tex_debug; // Use the debug checker pattern texture.
    args.topology     = PrimitiveTopology::kTriangleFan;
    args.depth_hack   = false;

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

void ViewRenderer::ShadeAliasMD2Model(const FrameData & frame_data, const entity_t & entity, vec4_t out_shade_light_color) const
{
    //
    // Hacks for the original Quake2 ref_gl follow
    //

    // PMM - rewrote, reordered to handle new shells & mixing
    if (entity.flags & (RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
    {
        // PMM - special case for godmode
        if ((entity.flags & RF_SHELL_RED) && (entity.flags & RF_SHELL_BLUE) && (entity.flags & RF_SHELL_GREEN))
        {
            for (int i = 0; i < 3; ++i)
                out_shade_light_color[i] = 1.0f;
        }
        else if (entity.flags & (RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
        {
            Vec3Zero(out_shade_light_color);

            if (entity.flags & RF_SHELL_RED)
            {
                out_shade_light_color[0] = 1.0f;
                if (entity.flags & (RF_SHELL_BLUE | RF_SHELL_DOUBLE))
                {
                    out_shade_light_color[2] = 1.0f;
                }
            }
            else if (entity.flags & RF_SHELL_BLUE)
            {
                if (entity.flags & RF_SHELL_DOUBLE)
                {
                    out_shade_light_color[1] = 1.0f;
                    out_shade_light_color[2] = 1.0f;
                }
                else
                {
                    out_shade_light_color[2] = 1.0f;
                }
            }
            else if (entity.flags & RF_SHELL_DOUBLE)
            {
                out_shade_light_color[0] = 0.9f;
                out_shade_light_color[1] = 0.7f;
            }
        }
        else if (entity.flags & (RF_SHELL_HALF_DAM | RF_SHELL_GREEN))
        {
            Vec3Zero(out_shade_light_color);

            // PMM - new colors
            if (entity.flags & RF_SHELL_HALF_DAM)
            {
                out_shade_light_color[0] = 0.56f;
                out_shade_light_color[1] = 0.59f;
                out_shade_light_color[2] = 0.45f;
            }
            if (entity.flags & RF_SHELL_GREEN)
            {
                out_shade_light_color[1] = 1.0f;
            }
        }
    }
    else if (entity.flags & RF_FULLBRIGHT)
    {
        for (int i = 0; i < 3; ++i)
            out_shade_light_color[i] = 1.0f;
    }
    else
    {
        CalcPointLightColor(frame_data, entity, out_shade_light_color);
    }

    if (entity.flags & RF_MINLIGHT)
    {
        int i;
        for (i = 0; i < 3; ++i)
        {
            if (out_shade_light_color[i] > 0.1f)
                break;
        }
        if (i == 3)
        {
            out_shade_light_color[0] = 0.1f;
            out_shade_light_color[1] = 0.1f;
            out_shade_light_color[2] = 0.1f;
        }
    }

    if (entity.flags & RF_GLOW)
    {
        // bonus items will pulse with time
        float min;
        const float scale = 0.1f * std::sinf(frame_data.view_def.time * 7.0f);
        for (int i = 0; i < 3; ++i)
        {
            min = out_shade_light_color[i] * 0.8f;
            out_shade_light_color[i] += scale;
            if (out_shade_light_color[i] < min)
                out_shade_light_color[i] = min;
        }
    }

    // PGM - IR goggles color override
    if ((frame_data.view_def.rdflags & RDF_IRGOGGLES) && (entity.flags & RF_IR_VISIBLE))
    {
        out_shade_light_color[0] = 1.0f;
        out_shade_light_color[1] = 0.0f;
        out_shade_light_color[2] = 0.0f;
    }
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::CalcPointLightColor(const FrameData & frame_data, const entity_t & entity, vec4_t out_shade_light_color) const
{
    // TODO - compute lighting (R_LightPoint - sample lightmap color at point)
    out_shade_light_color[0] = out_shade_light_color[1] =
    out_shade_light_color[2] = out_shade_light_color[3] = 1.0f;
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
