//
// ViewRenderer.cpp
//  Common view/3D frame rendering helpers.
//

#include "ViewRenderer.hpp"
#include "Lightmaps.hpp"
#include "DebugDraw.hpp"

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

static RenderMatrix MakeEntityModelMatrix(const entity_t & entity, const bool flipUpV = true)
{
    const auto t  = RenderMatrix::Translation(entity.origin[0], entity.origin[1], entity.origin[2]);
    const auto rx = RenderMatrix::RotationX(DegToRad(-entity.angles[ROLL]));
    const auto ry = RenderMatrix::RotationY(DegToRad(entity.angles[PITCH] * (flipUpV ? -1.0f : 1.0f)));
    const auto rz = RenderMatrix::RotationZ(DegToRad(entity.angles[YAW]));
    return rx * ry * rz * t;
}

///////////////////////////////////////////////////////////////////////////////

constexpr int kDiffuseTextureSlot  = 0;
constexpr int kLightmapTextureSlot = 1;

///////////////////////////////////////////////////////////////////////////////
// ViewRenderer
///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::Init(const RenderDevice & device, const TextureStore & tex_store)
{
    m_tex_white2x2 = tex_store.tex_white2x2;

    constexpr uint32_t kViewDrawBatchSize = 35000; // max vertices * num buffers
    m_vertex_buffers.Init(device, kViewDrawBatchSize);

    m_per_draw_shader_consts.Init(device, sizeof(PerDrawShaderConstants), ConstantBuffer::kOptimizeForSingleDraw);

    // Shaders
    const VertexInputLayout vertex_input_layout = {
        // DrawVertex3D
        {
            { VertexInputLayout::kVertexPosition,  VertexInputLayout::kFormatFloat3, offsetof(DrawVertex3D, position)    },
            { VertexInputLayout::kVertexTexCoords, VertexInputLayout::kFormatFloat2, offsetof(DrawVertex3D, texture_uv)  },
            { VertexInputLayout::kVertexLmCoords,  VertexInputLayout::kFormatFloat2, offsetof(DrawVertex3D, lightmap_uv) },
            { VertexInputLayout::kVertexColor,     VertexInputLayout::kFormatFloat4, offsetof(DrawVertex3D, rgba)        },
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
    m_current_draw_cmd.diffuse_tex  = (args.diffuse_tex  != nullptr) ? args.diffuse_tex  : m_tex_white2x2;
    m_current_draw_cmd.lightmap_tex = (args.lightmap_tex != nullptr) ? args.lightmap_tex : m_tex_white2x2;
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

    const auto batch_size = batch.UsedVerts();
    if (batch_size > 0)
    {
        m_current_draw_cmd.first_vert = m_vertex_buffers.CurrentPosition();
        m_current_draw_cmd.vertex_count = batch_size;

        m_vertex_buffers.Increment(batch_size);

        MRQ2_ASSERT(m_current_pass < kRenderPassCount);
        m_draw_cmds[m_current_pass].push_back(m_current_draw_cmd);
    }

    batch.Clear();
    m_current_draw_cmd = {};
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
    if ((frame_data.view_def.rdflags & RDF_NOWORLDMODEL) || Config::r_skip_draw_world.IsSet())
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
            context.SetTexture(cmd.diffuse_tex->BackendTexture(),  kDiffuseTextureSlot);
            context.SetTexture(cmd.lightmap_tex->BackendTexture(), kLightmapTextureSlot);

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

    // Update dynamic lightmaps.
    LightmapManager::Update();

    SetLightLevel(frame_data);
}

///////////////////////////////////////////////////////////////////////////////

// Original Quake2 hack from ref_gl to convey the current ambient light at the camera position back to the game.
void ViewRenderer::SetLightLevel(const FrameData & frame_data) const
{
    if (frame_data.view_def.rdflags & RDF_NOWORLDMODEL)
    {
        return;
    }

    // Save off light value for server to look at (BIG HACK!)

    float  light_level = 0.0f;
    vec4_t shade_light = { 1.0f, 1.0f, 1.0f, 1.0f };
    vec3_t light_spot  = {};
    CalcPointLightColor(frame_data, frame_data.view_def.vieworg, shade_light, light_spot);

    // pick the greatest component, which should be the same
    // as the mono value returned by software
    if (shade_light[0] > shade_light[1])
    {
        if (shade_light[0] > shade_light[2])
            light_level = 150.0f * shade_light[0];
        else
            light_level = 150.0f * shade_light[2];
    }
    else
    {
        if (shade_light[1] > shade_light[2])
            light_level = 150.0f * shade_light[1];
        else
            light_level = 150.0f * shade_light[2];
    }

    Config::r_lightlevel.SetValueDirect(light_level);
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::RenderViewSetup(FrameData & frame_data)
{
    ++m_frame_count;

    PushDLights(frame_data);

    // Find current view clusters
    SetUpViewClusters(frame_data);

    // Copy eye position
    Vec3Copy(frame_data.view_def.vieworg, frame_data.camera_origin);

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
    frame_data.frustum.projection = frame_data.proj_matrix;
    frame_data.frustum.SetClipPlanes(frame_data.view_matrix);
}

///////////////////////////////////////////////////////////////////////////////

// This function will recursively mark all surfaces that should
// be drawn and add them to the appropriate draw chain, so the
// next call to DrawTextureChains() will actually render what
// was marked for draw in here.
void ViewRenderer::RecursiveWorldNode(FrameData & frame_data, const ModelInstance & world_mdl, const ModelNode * const node)
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
    if (!frame_data.frustum.TestAabb(node->minmaxs, node->minmaxs + 3))
    {
        frame_data.world_nodes_culled++;
        return;
    }

    if (node->num_surfaces > 0 && Config::r_draw_world_bounds.IsSet())
    {
        DebugDraw::AddAABB(node->minmaxs, node->minmaxs + 3, ColorRGBA32{ 0xFFFF00FF }); // pink
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

const TextureImage * ViewRenderer::GetSurfaceLightmap(const refdef_t & view_def, const ModelSurface & surf) const
{
    // Not a lightmapped surface.
    if (surf.lightmap_texture_num < 0)
    {
        return m_tex_white2x2;
    }

    // If we're using the fallback path emulating dynamic lights with sprites just return the base static ligthmap.
    if (!Config::r_dynamic_lightmaps.IsSet())
    {
        return LightmapManager::LightmapAtIndex(surf.lightmap_texture_num);
    }

    // These surface types are not lightmapped.
    constexpr int kNoLightmapSurfaceFlags = (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP);

    bool is_dynamic = false;
    int lmap = 0;

    // See if we need to update the dynamic lightmap
    for (; lmap < kMaxLightmaps && surf.styles[lmap] != 255; ++lmap)
    {
        if (view_def.lightstyles[surf.styles[lmap]].white != surf.cached_light[lmap])
        {
            if (!(surf.texinfo->flags & kNoLightmapSurfaceFlags))
            {
                is_dynamic = true;
            }
            break;
        }
    }

    // Is the surface lit by a dynamic light source?
    if (!is_dynamic)
    {
        if (surf.dlight_frame == m_frame_count)
        {
            if (!(surf.texinfo->flags & kNoLightmapSurfaceFlags))
            {
                is_dynamic = true;
            }
        }
    }

    const TextureImage * lightmap_tex;

    if (is_dynamic)
    {
        bool update_surf_cache;
        bool dynamic_lightmap;

        // Update existing surface lightmap
        if ((surf.styles[lmap] >= 32 || surf.styles[lmap] == 0) && (surf.dlight_frame != m_frame_count))
        {
            update_surf_cache = true;
            dynamic_lightmap  = false;
        }
        else // Update the dynamic lightmap
        {
            update_surf_cache = false;
            dynamic_lightmap  = true;
        }

        lightmap_tex = LightmapManager::UpdateSurfaceLightmap(&surf, surf.lightmap_texture_num, view_def.lightstyles,
                                                              view_def.dlights, view_def.num_dlights, m_frame_count,
                                                              update_surf_cache, dynamic_lightmap);
    }
    else // Static lightmap
    {
        lightmap_tex = LightmapManager::LightmapAtIndex(surf.lightmap_texture_num);
    }

    return lightmap_tex;
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::DrawTextureChains(FrameData & frame_data)
{
    const bool do_draw   = !Config::r_skip_draw_texture_chains.IsSet();
    const bool use_vb_ib = Config::r_use_vertex_index_buffers.IsSet();

    auto & context  = frame_data.context;
    auto & cbuffers = frame_data.cbuffers;

    BeginBatchArgs batch_args;
    batch_args.model_matrix = RenderMatrix{ RenderMatrix::kIdentity };
    batch_args.topology     = PrimitiveTopology::kTriangleList;
    batch_args.depth_hack   = false;

    if (use_vb_ib)
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
            if (use_vb_ib) // Use the prebaked vertex and index buffers
            {
                for (const ModelSurface * surf = tex->DrawChainPtr(); surf; surf = surf->texture_chain)
                {
                    if (const ModelPoly * poly = surf->polys)
                    {
                        if (poly->num_verts >= 3) // Need at least one triangle.
                        {
                            const auto range = poly->index_buffer;
                            MRQ2_ASSERT(range.first_index >= 0 && range.index_count > 0 && range.base_vertex >= 0);

                            auto * lightmap_tex = GetSurfaceLightmap(frame_data.view_def, *surf);

                            context.SetTexture(tex->BackendTexture(), kDiffuseTextureSlot);
                            context.SetTexture(lightmap_tex->BackendTexture(), kLightmapTextureSlot);

                            context.DrawIndexed(range.first_index, range.index_count, range.base_vertex);
                        }
                    }
                }
            }
            else // Immediate mode emulation
            {
                batch_args.diffuse_tex  = tex;
                batch_args.lightmap_tex = nullptr;

                MiniImBatch batch = BeginBatch(batch_args);

                for (const ModelSurface * surf = tex->DrawChainPtr(); surf; surf = surf->texture_chain)
                {
                    if (surf->lightmap_texture_num >= 0)
                    {
                        auto * lightmap_tex = GetSurfaceLightmap(frame_data.view_def, *surf);

                        if (lightmap_tex != batch_args.lightmap_tex)
                        {
                            batch_args.lightmap_tex = lightmap_tex;

                            // Lightmap texture has changed, close the current batch and start a new one.
                            EndBatch(batch);
                            batch = BeginBatch(batch_args);
                        }
                    }

                    if (const ModelPoly * poly = surf->polys)
                    {
                        if (poly->num_verts >= 3) // Need at least one triangle.
                        {
                            batch.PushModelSurface(*surf);
                        }
                    }
                }

                EndBatch(batch);
            }
        }

        // All world geometry using this texture has been drawn, clear for the next frame.
        tex->SetDrawChainPtr(nullptr);
    }

    if (use_vb_ib)
    {
        MRQ2_POP_GPU_MARKER(context);
    }
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::RenderTranslucentSurfaces(FrameData & frame_data)
{
    if (Config::r_skip_draw_alpha_surfs.IsSet())
    {
        return;
    }

    // The textures are prescaled up for a better
    // lighting range, so scale it back down.
    const float inv_intensity = 1.0f / Config::r_intensity.AsFloat();

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
            DrawAnimatedWaterPolys(frame_data.view_def, *surf, frame_data.view_def.time, color_alpha);
        }
        else // Static translucent surface (glass, completely still fluid)
        {
            BeginBatchArgs args;
            args.model_matrix = RenderMatrix{ RenderMatrix::kIdentity };
            args.diffuse_tex  = surf->texinfo->teximage;
            args.lightmap_tex = GetSurfaceLightmap(frame_data.view_def, *surf);
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
    if (Config::r_skip_draw_entities.IsSet())
    {
        return;
    }

    const bool force_null_entity_models = Config::r_force_null_entity_models.IsSet();

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

// Classic blocky Quake2 particles are rendered using a single triangle and
// a special 8x8 texture with a dot-like pattern in its top-left corner.
// Modern HD particles use a soft sprite and require a full quadrilateral to be rendered.
void ViewRenderer::RenderParticles(const FrameData & frame_data)
{
    const int num_particles = frame_data.view_def.num_particles;
    if (num_particles <= 0)
    {
        return;
    }

    const bool high_quality_particles = Config::r_hd_particles.IsSet();

    vec3_t up, right;
    Vec3Scale(frame_data.up_vec,    1.5f, up);
    Vec3Scale(frame_data.right_vec, 1.5f, right);

    BeginBatchArgs args;
    args.model_matrix = RenderMatrix{ RenderMatrix::kIdentity };
    args.diffuse_tex  = frame_data.tex_store.tex_particle;
    args.lightmap_tex = nullptr;
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
        {
            scale = 1.0f;
        }
        else
        {
            scale = 1.0f + scale * 0.004f;
        }

        const ColorRGBA32 color = TextureStore::ColorForIndex(p->color & 0xFF);
        const std::uint8_t bR = (color & 0xFF);
        const std::uint8_t bG = (color >> 8) & 0xFF;
        const std::uint8_t bB = (color >> 16) & 0xFF;

        const float fR = bR * (1.0f / 255.0f);
        const float fG = bG * (1.0f / 255.0f);
        const float fB = bB * (1.0f / 255.0f);
        const float fA = p->alpha;

        DrawVertex3D v = {};
        v.rgba[0] = fR;
        v.rgba[1] = fG;
        v.rgba[2] = fB;
        v.rgba[3] = fA;

        if (high_quality_particles)
        {
            // First triangle:
            v.position[0] = p->origin[0];
            v.position[1] = p->origin[1];
            v.position[2] = p->origin[2];
            v.texture_uv[0] = 0.0f;
            v.texture_uv[1] = 0.0f;
            batch.PushVertex(v);

            v.position[0] = p->origin[0] + up[0] * scale;
            v.position[1] = p->origin[1] + up[1] * scale;
            v.position[2] = p->origin[2] + up[2] * scale;
            v.texture_uv[0] = 0.0f;
            v.texture_uv[1] = 1.0f;
            batch.PushVertex(v);

            v.position[0] = p->origin[0] + ((up[0] + right[0]) * scale);
            v.position[1] = p->origin[1] + ((up[1] + right[1]) * scale);
            v.position[2] = p->origin[2] + ((up[2] + right[2]) * scale);
            v.texture_uv[0] = 1.0f;
            v.texture_uv[1] = 1.0f;
            batch.PushVertex(v);

            // Second triangle:
            v.position[0] = p->origin[0] + ((up[0] + right[0]) * scale);
            v.position[1] = p->origin[1] + ((up[1] + right[1]) * scale);
            v.position[2] = p->origin[2] + ((up[2] + right[2]) * scale);
            v.texture_uv[0] = 1.0f;
            v.texture_uv[1] = 1.0f;
            batch.PushVertex(v);

            v.position[0] = p->origin[0] + right[0] * scale;
            v.position[1] = p->origin[1] + right[1] * scale;
            v.position[2] = p->origin[2] + right[2] * scale;
            v.texture_uv[0] = 1.0f;
            v.texture_uv[1] = 0.0f;
            batch.PushVertex(v);

            v.position[0] = p->origin[0];
            v.position[1] = p->origin[1];
            v.position[2] = p->origin[2];
            v.texture_uv[0] = 0.0f;
            v.texture_uv[1] = 0.0f;
            batch.PushVertex(v);
        }
        else // The classic Quake2 dot particle is rendered with just a single triangle
        {
            v.position[0] = p->origin[0];
            v.position[1] = p->origin[1];
            v.position[2] = p->origin[2];
            v.texture_uv[0] = 0.0625f;
            v.texture_uv[1] = 0.0625f;
            batch.PushVertex(v);

            v.position[0] = p->origin[0] + up[0] * scale;
            v.position[1] = p->origin[1] + up[1] * scale;
            v.position[2] = p->origin[2] + up[2] * scale;
            v.texture_uv[0] = 1.0625f;
            v.texture_uv[1] = 0.0625f;
            batch.PushVertex(v);

            v.position[0] = p->origin[0] + right[0] * scale;
            v.position[1] = p->origin[1] + right[1] * scale;
            v.position[2] = p->origin[2] + right[2] * scale;
            v.texture_uv[0] = 0.0625f;
            v.texture_uv[1] = 1.0625f;
            batch.PushVertex(v);
        }
    }

    EndBatch(batch);
}

///////////////////////////////////////////////////////////////////////////////

// A Quake2 Dynamic Light (DLight) is a point light simulated with a circular billboarded sprite that follows the light source.
// This is used to simulate gunshot flares for example. The sprite is rendered with additive blending (qglBlendFunc(GL_ONE, GL_ONE)).
void ViewRenderer::RenderDLights(const FrameData & frame_data)
{
    if (Config::r_dynamic_lightmaps.IsSet())
    {
        return;
    }

    const int num_dlights = frame_data.view_def.num_dlights;
    if (num_dlights <= 0)
    {
        return;
    }

    BeginBatchArgs args;
    args.model_matrix = RenderMatrix{ RenderMatrix::kIdentity };
    args.diffuse_tex  = nullptr;
    args.lightmap_tex = nullptr;
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

void ViewRenderer::MarkDLights(const dlight_t * light, const int bit, ModelInstance & world_mdl, const ModelNode * node) const
{
    if (node->contents != -1)
    {
        return;
    }

    const cplane_t * split_plane = node->plane;
    const float dist = Vec3Dot(light->origin, split_plane->normal) - split_plane->dist;

    if (dist > light->intensity - kDLightCutoff)
    {
        MarkDLights(light, bit, world_mdl, node->children[0]);
        return;
    }
    if (dist < -light->intensity + kDLightCutoff)
    {
        MarkDLights(light, bit, world_mdl, node->children[1]);
        return;
    }

    // Mark the polygons
    ModelSurface * surf = world_mdl.data.surfaces + node->first_surface;
    for (int i = 0; i < node->num_surfaces; ++i, ++surf)
    {
        if (surf->dlight_frame != m_frame_count)
        {
            surf->dlight_bits  = 0;
            surf->dlight_frame = m_frame_count;
        }
        surf->dlight_bits |= bit;
    }

    MarkDLights(light, bit, world_mdl, node->children[0]);
    MarkDLights(light, bit, world_mdl, node->children[1]);
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::PushDLights(FrameData & frame_data) const
{
    if (Config::r_dynamic_lightmaps.IsSet())
    {
        const dlight_t * l = frame_data.view_def.dlights;
        const int num_dlights = frame_data.view_def.num_dlights;

        ModelInstance & world_mdl = frame_data.world_model;
        const ModelNode * nodes = world_mdl.data.nodes;

        for (int i = 0; i < num_dlights; ++i, ++l)
        {
            MarkDLights(l, 1 << i, world_mdl, nodes);
        }
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

void ViewRenderer::DrawAnimatedWaterPolys(const refdef_t & view_def, const ModelSurface & surf, const float frame_time, const vec4_t color)
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
    args.diffuse_tex  = surf.texinfo->teximage;
    args.lightmap_tex = GetSurfaceLightmap(view_def, surf);
    args.topology     = PrimitiveTopology::kTriangleFan;
    args.depth_hack   = false;

    // HACK: There's some noticeable z-fighting happening with lava and water touching walls when you go underwater.
    // Adding a small offset to the positions resolves it. No idea why this didn't happen with the original
    // OpenGL renderer, maybe the lower precision floating-point math was actually hiding the flickering?
    const float water_position_offset_hack = Config::r_water_hack.AsFloat();

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

                DrawVertex3D vert = {};

                vert.position[0] = poly->vertexes[v].position[0];
                vert.position[1] = poly->vertexes[v].position[1];
                vert.position[2] = poly->vertexes[v].position[2];

                vert.texture_uv[0] = s;
                vert.texture_uv[1] = t;

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

    if ((frame_data.view_def.rdflags & RDF_NOWORLDMODEL) || Config::r_skip_draw_world.IsSet())
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
    if (m_skybox.IsAnyPlaneVisible() && !Config::r_skip_draw_sky.IsSet())
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
            DrawVertex3D sky_verts[6] = {};
            const TextureImage * sky_tex = nullptr;

            if (m_skybox.BuildSkyPlane(i, sky_verts, &sky_tex))
            {
                args.model_matrix = sky_mtx;
                args.diffuse_tex  = sky_tex;
                args.lightmap_tex = nullptr;
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
    if (Config::r_skip_draw_entities.IsSet())
    {
        return;
    }

    const int num_entities = frame_data.view_def.num_entities;
    const entity_t * const entities_list = frame_data.view_def.entities;
    const bool force_null_entity_models = Config::r_force_null_entity_models.IsSet();

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
void ViewRenderer::DrawBrushModel(FrameData & frame_data, const entity_t & entity)
{
    if (Config::r_skip_brush_mods.IsSet())
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

    if (!frame_data.frustum.TestAabb(mins, maxs))
    {
        frame_data.brush_models_culled++;
        return;
    }

    if (Config::r_draw_model_bounds.IsSet())
    {
        DebugDraw::AddAABB(mins, maxs, ColorRGBA32{ 0xFF00FF00 }); // green
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

    // Calculate dynamic lighting for bmodel
    if (Config::r_dynamic_lightmaps.IsSet())
    {
        const int num_dlights = frame_data.view_def.num_dlights;
        const dlight_t * l = frame_data.view_def.dlights;

        for (int i = 0; i < num_dlights; ++i, ++l)
        {
            MarkDLights(l, 1 << i, frame_data.world_model, model->data.nodes + model->data.first_node);
        }
    }

    const bool use_vb_ib = Config::r_use_vertex_index_buffers.IsSet();
    auto & context  = frame_data.context;
    auto & cbuffers = frame_data.cbuffers;

    // IndexBuffer rendering
    if (use_vb_ib)
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
                if (const ModelPoly * poly = surf->polys)
                {
                    // IndexBuffer rendering
                    if (use_vb_ib && (poly->index_buffer.index_count > 0))
                    {
                        const auto range = poly->index_buffer;
                        MRQ2_ASSERT(range.first_index >= 0 && range.base_vertex >= 0);

                        auto * tex = TextureAnimation(surf->texinfo, entity.frame);
                        auto * lightmap_tex = GetSurfaceLightmap(frame_data.view_def, *surf);

                        context.SetTexture(tex->BackendTexture(), kDiffuseTextureSlot);
                        context.SetTexture(lightmap_tex->BackendTexture(), kLightmapTextureSlot);

                        context.DrawIndexed(range.first_index, range.index_count, range.base_vertex);
                    }
                    else // Immediate mode emulation
                    {
                        BeginBatchArgs args;
                        args.model_matrix = mdl_mtx;
                        args.diffuse_tex  = TextureAnimation(surf->texinfo, entity.frame);
                        args.lightmap_tex = GetSurfaceLightmap(frame_data.view_def, *surf);
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

    if (use_vb_ib)
    {
        MRQ2_POP_GPU_MARKER(context);
    }
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
    DrawVertex3D quad[4] = {};
    const int indexes[6] = { 0,1,2, 2,3,0 };

    quad[0].texture_uv[0] = 0.0f;
    quad[0].texture_uv[1] = 1.0f;
    MrQ2::VecSplatN(quad[0].rgba, 1.0f);
    quad[0].rgba[3] = alpha;
    MrQ2::Vec3MAdd(entity.origin, -frame->origin_y, up, quad[0].position);
    MrQ2::Vec3MAdd(quad[0].position, -frame->origin_x, right, quad[0].position);

    quad[1].texture_uv[0] = 0.0f;
    quad[1].texture_uv[1] = 0.0f;
    MrQ2::VecSplatN(quad[1].rgba, 1.0f);
    quad[1].rgba[3] = alpha;
    MrQ2::Vec3MAdd(entity.origin, frame->height - frame->origin_y, up, quad[1].position);
    MrQ2::Vec3MAdd(quad[1].position, -frame->origin_x, right, quad[1].position);

    quad[2].texture_uv[0] = 1.0f;
    quad[2].texture_uv[1] = 0.0f;
    MrQ2::VecSplatN(quad[2].rgba, 1.0f);
    quad[2].rgba[3] = alpha;
    MrQ2::Vec3MAdd(entity.origin, frame->height - frame->origin_y, up, quad[2].position);
    MrQ2::Vec3MAdd(quad[2].position, frame->width - frame->origin_x, right, quad[2].position);

    quad[3].texture_uv[0] = 1.0f;
    quad[3].texture_uv[1] = 1.0f;
    MrQ2::VecSplatN(quad[3].rgba, 1.0f);
    quad[3].rgba[3] = alpha;
    MrQ2::Vec3MAdd(entity.origin, -frame->origin_y, up, quad[3].position);
    MrQ2::Vec3MAdd(quad[3].position, frame->width - frame->origin_x, right, quad[3].position);

    BeginBatchArgs args;
    args.model_matrix = RenderMatrix{ RenderMatrix::kIdentity };
    args.diffuse_tex  = model->data.skins[frame_num];
    args.lightmap_tex = nullptr;
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

void ViewRenderer::DrawAliasMD2Model(FrameData & frame_data, const entity_t & entity)
{
    if (!(entity.flags & RF_WEAPONMODEL))
    {
        vec3_t bbox[8] = {};
        const bool cull = ShouldCullAliasMD2Model(frame_data.frustum, entity, bbox);
        if (cull)
        {
            frame_data.alias_models_culled++;
            return;
        }

        if (Config::r_draw_model_bounds.IsSet())
        {
            DebugDraw::AddAABB(bbox, ColorRGBA32{ 0xFF0000FF }); // red
        }
    }

    vec4_t shade_light = { 1.0f, 1.0f, 1.0f, 1.0f };
    vec3_t light_spot  = {};

    ShadeAliasMD2Model(frame_data, entity, shade_light, light_spot);

    const float  backlerp = (Config::r_lerp_entity_models.IsSet() ? entity.backlerp : 0.0f);
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
        skin = m_tex_white2x2; // fallback...
    }

    // Draw interpolated frame:
    DrawAliasMD2FrameLerp(entity, model->hunk.ViewBaseAs<dmdl_t>(), backlerp, shade_light, mdl_mtx, skin);

    // Simple projected shadow:
    const bool draw_shadows = Config::r_alias_shadows.IsSet();
    if (draw_shadows && !(entity.flags & (RF_TRANSLUCENT | RF_WEAPONMODEL)))
    {
        // Switch to projected shadows mode then back to previous render mode.
        // We want alpha blending to be enabled for the shadows.
        const auto prev_pass = m_current_pass;
        m_current_pass = kPass_TranslucentEntities;

        DrawAliasMD2Shadow(entity, model->hunk.ViewBaseAs<dmdl_t>(), mdl_mtx, light_spot);

        m_current_pass = prev_pass;
    }
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
    args.diffuse_tex  = nullptr; // No texture
    args.lightmap_tex = nullptr;
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
    vec4_t color = { 1.0f, 1.0f, 1.0f, 1.0f };
    vec3_t light_spot = {};

    if (!(entity.flags & RF_FULLBRIGHT))
    {
        CalcPointLightColor(frame_data, entity.origin, color, light_spot);
    }

    const vec2_t uvs[3] = {
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
    };

    BeginBatchArgs args;
    args.model_matrix = MakeEntityModelMatrix(entity);
    args.diffuse_tex  = frame_data.tex_store.tex_debug; // Use the debug checker pattern texture.
    args.lightmap_tex = nullptr;
    args.topology     = PrimitiveTopology::kTriangleFan;
    args.depth_hack   = false;

    // Draw a small octahedron as a placeholder for the entity model:
    MiniImBatch batch = BeginBatch(args);
    {
        // Bottom halve
        batch.SetTriangleFanFirstVertex({ {0.0f, 0.0f, -16.0f}, {0.0f, 0.0f}, {0.0f, 0.0f},
                                          {color[0], color[1], color[2], color[3]} });
        for (int i = 0, j = 0; i <= 4; ++i)
        {
            batch.PushVertex({ {16.0f * std::cosf(i * M_PI / 2.0f), 16.0f * std::sinf(i * M_PI / 2.0f), 0.0f},
                               {uvs[j][0], uvs[j][1]}, {0.0f, 0.0f}, {color[0], color[1], color[2], color[3]} });
            if (++j > 2) j = 1;
        }

        // Top halve
        batch.SetTriangleFanFirstVertex({ {0.0f, 0.0f, 16.0f}, {0.0f, 0.0f}, {0.0f, 0.0f},
                                          {color[0], color[1], color[2], color[3]} });
        for (int i = 4, j = 0; i >= 0; --i)
        {
            batch.PushVertex({ {16.0f * std::cosf(i * M_PI / 2.0f), 16.0f * std::sinf(i * M_PI / 2.0f), 0.0f},
                               {uvs[j][0], uvs[j][1]}, {0.0f, 0.0f}, {color[0], color[1], color[2], color[3]} });
            if (++j > 2) j = 1;
        }
    }
    EndBatch(batch);
}

///////////////////////////////////////////////////////////////////////////////

bool ViewRenderer::ShouldCullAliasMD2Model(const Frustum & frustum, const entity_t & entity, vec3_t bbox[8]) const
{
    const auto   * model     = reinterpret_cast<const ModelInstance *>(entity.model);
    const dmdl_t * paliashdr = model->hunk.ViewBaseAs<const dmdl_t>();

    if ((entity.frame >= paliashdr->num_frames) || (entity.frame < 0))
    {
        GameInterface::Errorf("ShouldCullAliasMD2Model %s: no such frame", model->name.CStr(), entity.frame);
    }
    if ((entity.oldframe >= paliashdr->num_frames) || (entity.oldframe < 0))
    {
        GameInterface::Errorf("ShouldCullAliasMD2Model %s: no such oldframe", model->name.CStr(), entity.oldframe);
    }

    auto * pframe    = reinterpret_cast<const daliasframe_t *>((const uint8_t *)paliashdr + paliashdr->ofs_frames + entity.frame    * paliashdr->framesize);
    auto * poldframe = reinterpret_cast<const daliasframe_t *>((const uint8_t *)paliashdr + paliashdr->ofs_frames + entity.oldframe * paliashdr->framesize);

    // Compute axially aligned mins and maxs
    vec3_t mins, maxs;
    if (pframe == poldframe)
    {
        for (int i = 0; i < 3; ++i)
        {
            mins[i] = pframe->translate[i];
            maxs[i] = mins[i] + pframe->scale[i] * 255.0f;
        }
    }
    else
    {
        vec3_t thismins, oldmins, thismaxs, oldmaxs;
        for (int i = 0; i < 3; ++i)
        {
            thismins[i] = pframe->translate[i];
            thismaxs[i] = thismins[i] + pframe->scale[i] * 255.0f;

            oldmins[i] = poldframe->translate[i];
            oldmaxs[i] = oldmins[i] + poldframe->scale[i] * 255.0f;

            if (thismins[i] < oldmins[i])
                mins[i] = thismins[i];
            else
                mins[i] = oldmins[i];

            if (thismaxs[i] > oldmaxs[i])
                maxs[i] = thismaxs[i];
            else
                maxs[i] = oldmaxs[i];
        }
    }

    // Compute a full bounding box
    for (int i = 0; i < 8; ++i)
    {
        vec3_t tmp;

        if (i & 1)
            tmp[0] = mins[0];
        else
            tmp[0] = maxs[0];

        if (i & 2)
            tmp[1] = mins[1];
        else
            tmp[1] = maxs[1];

        if (i & 4)
            tmp[2] = mins[2];
        else
            tmp[2] = maxs[2];

        Vec3Copy(tmp, bbox[i]);
    }

    // Rotate the bounding box
    vec3_t angles;
    vec3_t vectors[3];
    Vec3Copy(entity.angles, angles);
    angles[YAW] = -angles[YAW];
    VectorsFromAngles(angles, vectors[0], vectors[1], vectors[2]);

    for (int i = 0; i < 8; ++i)
    {
        vec3_t tmp;
        Vec3Copy(bbox[i], tmp);

        bbox[i][0] =  Vec3Dot(vectors[0], tmp);
        bbox[i][1] = -Vec3Dot(vectors[1], tmp);
        bbox[i][2] =  Vec3Dot(vectors[2], tmp);

        Vec3Add(entity.origin, bbox[i], bbox[i]);
    }

    bool intersectsFrustum = false;
    for (int i = 0; i < 8; ++i)
    {
        if (frustum.TestPoint(bbox[i][0], bbox[i][1], bbox[i][2]))
        {
            intersectsFrustum = true;
            break;
        }
    }

    return !intersectsFrustum;
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::ShadeAliasMD2Model(const FrameData & frame_data, const entity_t & entity, vec4_t out_shade_light_color, vec3_t out_light_spot) const
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
            for (int i = 0; i < 4; ++i)
                out_shade_light_color[i] = 1.0f;
        }
        else if (entity.flags & (RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
        {
            Vec3Zero(out_shade_light_color); // RGB
            out_shade_light_color[3] = 1.0f; // A

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
            Vec3Zero(out_shade_light_color); // RGB
            out_shade_light_color[3] = 1.0f; // A

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
        for (int i = 0; i < 4; ++i)
            out_shade_light_color[i] = 1.0f;
    }
    else
    {
        CalcPointLightColor(frame_data, entity.origin, out_shade_light_color, out_light_spot);
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
        out_shade_light_color[3] = 1.0f;
    }
}

///////////////////////////////////////////////////////////////////////////////

static int RecursiveLightPoint(const ModelInstance & world_mdl, const ModelNode * node, const lightstyle_t * lightstyles,
                               const vec3_t start, const vec3_t end, vec3_t out_point_color, vec3_t out_light_spot)
{
    MRQ2_ASSERT(node != nullptr);
    MRQ2_ASSERT(lightstyles != nullptr);

    if (node->contents != -1)
    {
        return -1; // Didn't hit anything
    }

    // Calculate mid point
    const cplane_t * const plane = node->plane;
    const float front = Vec3Dot(start, plane->normal) - plane->dist;
    const float back  = Vec3Dot(end,   plane->normal) - plane->dist;
    const int   side  = (front < 0.0f);

    if ((back < 0.0f) == side)
    {
        return RecursiveLightPoint(world_mdl, node->children[side], lightstyles, start, end, out_point_color, out_light_spot);
    }

    const float frac = front / (front - back);

    vec3_t mid;
    mid[0] = start[0] + (end[0] - start[0]) * frac;
    mid[1] = start[1] + (end[1] - start[1]) * frac;
    mid[2] = start[2] + (end[2] - start[2]) * frac;

    // Go down front side
    const int r = RecursiveLightPoint(world_mdl, node->children[side], lightstyles, start, mid, out_point_color, out_light_spot);
    if (r >= 0)
    {
        return r; // Hit something
    }
    if ((back < 0.0f) == side)
    {
        return -1; // Didn't hit anything
    }

    Vec3Copy(mid, out_light_spot);

    // Check for impact on this node
    const float lightmap_intensity = Config::r_lightmap_intensity.AsFloat();
    const ModelSurface * surf = world_mdl.data.surfaces + node->first_surface;

    for (int i = 0; i < node->num_surfaces; ++i, ++surf)
    {
        if (surf->flags & (kSurf_DrawTurb | kSurf_DrawSky))
        {
            continue; // No lightmaps
        }

        const ModelTexInfo * tex = surf->texinfo;

        const int s = static_cast<int>(Vec3Dot(mid, tex->vecs[0]) + tex->vecs[0][3]);
        const int t = static_cast<int>(Vec3Dot(mid, tex->vecs[1]) + tex->vecs[1][3]);

        if (s < surf->texture_mins[0] || t < surf->texture_mins[1])
        {
            continue;
        }

        int ds = s - surf->texture_mins[0];
        int dt = t - surf->texture_mins[1];

        if (ds > surf->extents[0] || dt > surf->extents[1])
        {
            continue;
        }

        if (surf->samples == nullptr)
        {
            return 0;
        }

        ds >>= 4;
        dt >>= 4;

        Vec3Zero(out_point_color);

        const std::uint8_t * lightmap = surf->samples;
        lightmap += 3 * (dt * ((surf->extents[0] >> 4) + 1) + ds);

        for (int lmap = 0; lmap < kMaxLightmaps && surf->styles[lmap] != 255; ++lmap)
        {
            vec3_t scale;
            for (int v = 0; v < 3; ++v)
            {
                scale[v] = lightmap_intensity * lightstyles[surf->styles[lmap]].rgb[v];
            }

            out_point_color[0] += lightmap[0] * scale[0] * (1.0f / 255.0f);
            out_point_color[1] += lightmap[1] * scale[1] * (1.0f / 255.0f);
            out_point_color[2] += lightmap[2] * scale[2] * (1.0f / 255.0f);

            lightmap += 3 * ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1);
        }

        return 1;
    }

    // Go down back side
    return RecursiveLightPoint(world_mdl, node->children[!side], lightstyles, mid, end, out_point_color, out_light_spot);
}

///////////////////////////////////////////////////////////////////////////////

void ViewRenderer::CalcPointLightColor(const FrameData & frame_data, const vec3_t point, vec4_t out_shade_light_color, vec3_t out_light_spot) const
{
    const ModelInstance & world_mdl = frame_data.world_model;

    if (world_mdl.data.light_data == nullptr) // fullbright
    {
        out_shade_light_color[0] = 1.0f;
        out_shade_light_color[1] = 1.0f;
        out_shade_light_color[2] = 1.0f;
        out_shade_light_color[3] = 1.0f;
        return;
    }

    const vec3_t end_point = { point[0], point[1], point[2] - 2048.0f };

    vec3_t out_point_color = {};
    const int r = RecursiveLightPoint(world_mdl, world_mdl.data.nodes, frame_data.view_def.lightstyles, point, end_point, out_point_color, out_light_spot);

    if (r == -1)
    {
        Vec3Zero(out_shade_light_color);
    }
    else
    {
        Vec3Copy(out_point_color, out_shade_light_color);
    }
    out_shade_light_color[3] = 1.0f;

    // Add dynamic lights:
    const dlight_t * dl = frame_data.view_def.dlights;
    const int num_dlights = frame_data.view_def.num_dlights;

    for (int lnum = 0; lnum < num_dlights; ++lnum, ++dl)
    {
        vec3_t dist = {};
        Vec3Sub(point, dl->origin, dist);

        float add = dl->intensity - Vec3Length(dist);
        add *= (1.0f / 256.0f);

        if (add > 0.0f)
        {
            Vec3MAdd(out_shade_light_color, add, dl->color, out_shade_light_color);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

} // MrQ2
