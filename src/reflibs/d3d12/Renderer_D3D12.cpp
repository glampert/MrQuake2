//
// Renderer_D3D12.cpp
//  D3D12 renderer interface for Quake2.
//

#include "Renderer_D3D12.hpp"

// Debug markers need these.
#include <cstdarg>
#include <cstddef>
#include <cwchar>

// Path from the project root where to find shaders for this renderer (wchar_t)
#define REFD3D12_SHADER_PATH_WIDE L"src\\reflibs\\d3d12\\shaders\\"

namespace MrQ2
{
namespace D3D12
{

///////////////////////////////////////////////////////////////////////////////
// TextureImageImpl
///////////////////////////////////////////////////////////////////////////////

void TextureImageImpl::InitD3DSpecific()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void TextureImageImpl::InitFromScrap(const TextureImageImpl * const scrap_tex)
{
    FASTASSERT(scrap_tex != nullptr);

    // TODO
}

///////////////////////////////////////////////////////////////////////////////
// TextureStoreImpl
///////////////////////////////////////////////////////////////////////////////

void TextureStoreImpl::Init()
{
    // Load the default resident textures now
    TouchResidentTextures();
}

///////////////////////////////////////////////////////////////////////////////

void TextureStoreImpl::UploadScrapIfNeeded()
{
    if (m_scrap_dirty)
    {
        g_Renderer->UploadTexture(ScrapImpl());
        m_scrap_dirty = false;
    }
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStoreImpl::CreateScrap(int size, const ColorRGBA32 * pix)
{
    TextureImageImpl * scrap_impl = m_teximages_pool.Allocate();
    Construct(scrap_impl, pix, RegistrationNum(), TextureType::kPic, /*use_scrap =*/true,
              size, size, Vec2u16{0,0}, Vec2u16{std::uint16_t(size), std::uint16_t(size)}, "pics/scrap.pcx");

    scrap_impl->InitD3DSpecific();
    return scrap_impl;
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStoreImpl::CreateTexture(const ColorRGBA32 * pix, std::uint32_t regn, TextureType tt, bool use_scrap,
                                               int w, int h, Vec2u16 scrap0, Vec2u16 scrap1, const char * name)
{
    TextureImageImpl * impl = m_teximages_pool.Allocate();
    Construct(impl, pix, regn, tt, use_scrap, w, h, scrap0, scrap1, name);

    if (use_scrap)
    {
        impl->InitFromScrap(ScrapImpl());
        m_scrap_dirty = true; // Upload the D3D texture on next opportunity
    }
    else
    {
        impl->InitD3DSpecific();
    }

    return impl;
}

///////////////////////////////////////////////////////////////////////////////

void TextureStoreImpl::DestroyTexture(TextureImage * tex)
{
    auto * impl = static_cast<TextureImageImpl *>(tex);
    Destroy(impl);
    m_teximages_pool.Deallocate(impl);
}

///////////////////////////////////////////////////////////////////////////////
// ModelStoreImpl
///////////////////////////////////////////////////////////////////////////////

ModelStoreImpl::~ModelStoreImpl()
{
    for (ModelInstance * mdl : m_inline_models)
    {
        DestroyModel(mdl);
    }

    m_inline_models.clear();
    DestroyAllLoadedModels();
}

///////////////////////////////////////////////////////////////////////////////

void ModelStoreImpl::Init()
{
    CommonInitInlineModelsPool(m_inline_models,
        [this]() -> ModelInstanceImpl * {
            return m_models_pool.Allocate(); // First page in the pool will contain the inlines.
        });
}

///////////////////////////////////////////////////////////////////////////////

ModelInstance * ModelStoreImpl::CreateModel(const char * name, ModelType mt, std::uint32_t regn)
{
    ModelInstanceImpl * impl = m_models_pool.Allocate();
    Construct(impl, name, mt, regn, /* inline_mdl = */false);
    return impl;
}

///////////////////////////////////////////////////////////////////////////////

void ModelStoreImpl::DestroyModel(ModelInstance * mdl)
{
    auto * impl = static_cast<ModelInstanceImpl *>(mdl);
    Destroy(impl);
    m_models_pool.Deallocate(impl);
}

///////////////////////////////////////////////////////////////////////////////
// SpriteBatch
///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::Init(const int max_verts)
{
    (void) max_verts;
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::BeginFrame()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::EndFrame()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushTriVerts(const DrawVertex2D tri[3])
{
    DrawVertex2D * verts = Increment(3);
    std::memcpy(verts, tri, sizeof(DrawVertex2D) * 3);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadVerts(const DrawVertex2D quad[4])
{
    DrawVertex2D * tri = Increment(6);       // Expand quad into 2 triangles
    const int indexes[6] = { 0,1,2, 2,3,0 }; // CW winding
    for (int i = 0; i < 6; ++i)
    {
        tri[i] = quad[indexes[i]];
    }
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuad(const float x, const float y, const float w, const float h,
                           const float u0, const float v0, const float u1, const float v1,
                           const DirectX::XMFLOAT4A & color)
{
    DrawVertex2D quad[4];
    quad[0].xy_uv[0] = x;
    quad[0].xy_uv[1] = y;
    quad[0].xy_uv[2] = u0;
    quad[0].xy_uv[3] = v0;
    quad[0].rgba[0]  = color.x;
    quad[0].rgba[1]  = color.y;
    quad[0].rgba[2]  = color.z;
    quad[0].rgba[3]  = color.w;
    quad[1].xy_uv[0] = x + w;
    quad[1].xy_uv[1] = y;
    quad[1].xy_uv[2] = u1;
    quad[1].xy_uv[3] = v0;
    quad[1].rgba[0]  = color.x;
    quad[1].rgba[1]  = color.y;
    quad[1].rgba[2]  = color.z;
    quad[1].rgba[3]  = color.w;
    quad[2].xy_uv[0] = x + w;
    quad[2].xy_uv[1] = y + h;
    quad[2].xy_uv[2] = u1;
    quad[2].xy_uv[3] = v1;
    quad[2].rgba[0]  = color.x;
    quad[2].rgba[1]  = color.y;
    quad[2].rgba[2]  = color.z;
    quad[2].rgba[3]  = color.w;
    quad[3].xy_uv[0] = x;
    quad[3].xy_uv[1] = y + h;
    quad[3].xy_uv[2] = u0;
    quad[3].xy_uv[3] = v1;
    quad[3].rgba[0]  = color.x;
    quad[3].rgba[1]  = color.y;
    quad[3].rgba[2]  = color.z;
    quad[3].rgba[3]  = color.w;
    PushQuadVerts(quad);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadTextured(const float x, const float y,
                                   const float w, const float h,
                                   const TextureImage * const tex,
                                   const DirectX::XMFLOAT4A & color)
{
    FASTASSERT(tex != nullptr);
    const int quad_start_vtx = 0;//TODO m_buffers.CurrentPosition();
    PushQuad(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color);
    m_deferred_textured_quads.push_back({ quad_start_vtx, static_cast<const TextureImageImpl *>(tex) });
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadTexturedUVs(const float x, const float y,
                                      const float w, const float h,
                                      const float u0, const float v0,
                                      const float u1, const float v1,
                                      const TextureImage * const tex,
                                      const DirectX::XMFLOAT4A & color)
{
    FASTASSERT(tex != nullptr);
    const int quad_start_vtx = 0;//TODO m_buffers.CurrentPosition();
    PushQuad(x, y, w, h, u0, v0, u1, v1, color);
    m_deferred_textured_quads.push_back({ quad_start_vtx, static_cast<const TextureImageImpl *>(tex) });
}

///////////////////////////////////////////////////////////////////////////////
// ViewDrawStateImpl
///////////////////////////////////////////////////////////////////////////////

void ViewDrawStateImpl::Init(const int max_verts)
{
    m_draw_cmds = new(MemTag::kRenderer) DrawCmdList{};

    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawStateImpl::BeginRenderPass()
{
    FASTASSERT(m_batch_open == false);
    FASTASSERT(m_draw_cmds->empty());

    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawStateImpl::EndRenderPass()
{
    FASTASSERT(m_batch_open == false);

    // TODO
}

///////////////////////////////////////////////////////////////////////////////

MiniImBatch ViewDrawStateImpl::BeginBatch(const BeginBatchArgs & args)
{
    FASTASSERT(m_batch_open == false);
    FASTASSERT_ALIGN16(args.model_matrix.floats);

    m_current_draw_cmd.model_mtx  = DirectX::XMMATRIX{ args.model_matrix.floats };
    m_current_draw_cmd.texture    = args.optional_tex ? args.optional_tex : g_Renderer->TexStore()->tex_white2x2;
    m_current_draw_cmd.topology   = args.topology;
    m_current_draw_cmd.depth_hack = args.depth_hack;
    m_current_draw_cmd.first_vert = 0;
    m_current_draw_cmd.num_verts  = 0;

    m_batch_open = true;

    // TODO
    //return MiniImBatch{ m_buffers.CurrentVertexPtr(), m_buffers.NumVertsRemaining(), args.topology };
    return MiniImBatch{ nullptr, 0, args.topology };
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawStateImpl::EndBatch(MiniImBatch & batch)
{
    FASTASSERT(batch.IsValid());
    FASTASSERT(m_batch_open == true);
    FASTASSERT(m_current_draw_cmd.topology == batch.Topology());

    m_current_draw_cmd.first_vert = 0;//TODO m_buffers.CurrentPosition();
    m_current_draw_cmd.num_verts  = batch.UsedVerts();

//    m_buffers.Increment(batch.UsedVerts());

    m_draw_cmds->push_back(m_current_draw_cmd);
    m_current_draw_cmd = {};

    batch.Clear();
    m_batch_open = false;
}

///////////////////////////////////////////////////////////////////////////////
// Renderer
///////////////////////////////////////////////////////////////////////////////

const DirectX::XMFLOAT4A Renderer::kClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
const DirectX::XMFLOAT4A Renderer::kColorWhite{ 1.0f, 1.0f, 1.0f, 1.0f };
const DirectX::XMFLOAT4A Renderer::kColorBlack{ 0.0f, 0.0f, 0.0f, 1.0f };
const DirectX::XMFLOAT4A Renderer::kFloat4Zero{ 0.0f, 0.0f, 0.0f, 0.0f };
const DirectX::XMFLOAT4A Renderer::kFloat4One { 1.0f, 1.0f, 1.0f, 1.0f };

///////////////////////////////////////////////////////////////////////////////

Renderer::Renderer() 
    : m_tex_store{}
    , m_mdl_store{ m_tex_store }
{
    GameInterface::Printf("D3D12 Renderer instance created.");
}

///////////////////////////////////////////////////////////////////////////////

Renderer::~Renderer()
{
    GameInterface::Printf("D3D12 Renderer shutting down.");
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::Init(const char * const window_name, HINSTANCE hinst, WNDPROC wndproc,
                    const int width, const int height, const bool fullscreen, const bool debug_validation)
{
    GameInterface::Printf("D3D12 Renderer initializing.");

    m_disable_texturing = GameInterface::Cvar::Get("r_disable_texturing", "0", 0);
    m_blend_debug_color = GameInterface::Cvar::Get("r_blend_debug_color", "0", 0);

    // RenderWindow setup
    m_window.window_name      = window_name;
    m_window.class_name       = window_name;
    m_window.hinst            = hinst;
    m_window.wndproc          = wndproc;
    m_window.width            = width;
    m_window.height           = height;
    m_window.fullscreen       = fullscreen;
    m_window.debug_validation = debug_validation;
    m_window.Init();

    // 2D sprite/UI batch setup
    m_sprite_batches[SpriteBatch::kDrawChar].Init(6 * 5000); // 6 verts per quad (expand to 2 triangles each)
    m_sprite_batches[SpriteBatch::kDrawPics].Init(6 * 128);

    // Initialize the stores/caches
    m_tex_store.Init();
    m_mdl_store.Init();

    // World geometry rendering helper
    constexpr int kViewDrawBatchSize = 25000; // size in vertices
    m_view_draw_state.Init(kViewDrawBatchSize);

    // So we can annotate our RenderDoc captures
    InitDebugEvents();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderView(const refdef_s & view_def)
{
    PushEvent(L"Renderer::RenderView");

    ViewDrawStateImpl::FrameData frame_data{ m_tex_store, *m_mdl_store.WorldModel(), view_def };

    // Enter 3D mode (depth test ON)
    EnableDepthTest();

    // Set up camera/view (fills frame_data)
    m_view_draw_state.RenderViewSetup(frame_data);

    // Update the constant buffers for this frame
    RenderViewUpdateCBuffers(frame_data);

    // Set the camera/world-view:
    FASTASSERT_ALIGN16(frame_data.view_proj_matrix.floats);
    const auto vp_mtx = DirectX::XMMATRIX{ frame_data.view_proj_matrix.floats };
    m_view_draw_state.SetViewProjMatrix(vp_mtx);

    //
    // Render solid geometries (world and entities)
    //

    m_view_draw_state.BeginRenderPass();

    PushEvent(L"RenderWorldModel");
    m_view_draw_state.RenderWorldModel(frame_data);
    PopEvent(); // "RenderWorldModel"

    PushEvent(L"RenderSkyBox");
    m_view_draw_state.RenderSkyBox(frame_data);
    PopEvent(); // "RenderSkyBox"

    PushEvent(L"RenderSolidEntities");
    m_view_draw_state.RenderSolidEntities(frame_data);
    PopEvent(); // "RenderSolidEntities"

    m_view_draw_state.EndRenderPass();

    //
    // Transparencies/alpha pass
    //

    // Color Blend ON
    EnableAlphaBlending();

    PushEvent(L"RenderTranslucentSurfaces");
    m_view_draw_state.BeginRenderPass();
    m_view_draw_state.RenderTranslucentSurfaces(frame_data);
    m_view_draw_state.EndRenderPass();
    PopEvent(); // "RenderTranslucentSurfaces"

    PushEvent(L"RenderTranslucentEntities");
    DisableDepthWrites(); // Disable z writes in case they stack up
    m_view_draw_state.BeginRenderPass();
    m_view_draw_state.RenderTranslucentEntities(frame_data);
    m_view_draw_state.EndRenderPass();
    EnableDepthWrites();
    PopEvent(); // "RenderTranslucentEntities"

    // Color Blend OFF
    DisableAlphaBlending();

    // Back to 2D rendering mode (depth test OFF)
    DisableDepthTest();

    PopEvent(); // "Renderer::RenderView"
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderViewUpdateCBuffers(const ViewDrawStateImpl::FrameData& frame_data)
{
    FASTASSERT_ALIGN16(frame_data.view_proj_matrix.floats);

    // TODO

    ConstantBufferDataSGeomVS cbuffer_data_geometry_vs;
    cbuffer_data_geometry_vs.mvp_matrix = DirectX::XMMATRIX{ frame_data.view_proj_matrix.floats };
//    DeviceContext()->UpdateSubresource(m_cbuffer_geometry_vs.Get(), 0, nullptr, &cbuffer_data_geometry_vs, 0, 0);

    ConstantBufferDataSGeomPS cbuffer_data_geometry_ps;
    if (m_disable_texturing.IsSet()) // Use only debug vertex color
    {
        cbuffer_data_geometry_ps.texture_color_scaling = kFloat4Zero;
        cbuffer_data_geometry_ps.vertex_color_scaling  = kFloat4One;
    }
    else if (m_blend_debug_color.IsSet()) // Blend debug vertex color with texture
    {
        cbuffer_data_geometry_ps.texture_color_scaling = kFloat4One;
        cbuffer_data_geometry_ps.vertex_color_scaling  = kFloat4One;
    }
    else // Normal rendering
    {
        cbuffer_data_geometry_ps.texture_color_scaling = kFloat4One;
        cbuffer_data_geometry_ps.vertex_color_scaling  = kFloat4Zero;
    }
//    DeviceContext()->UpdateSubresource(m_cbuffer_geometry_ps.Get(), 0, nullptr, &cbuffer_data_geometry_ps, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EnableDepthTest()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::DisableDepthTest()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EnableDepthWrites()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::DisableDepthWrites()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EnableAlphaBlending()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::DisableAlphaBlending()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::BeginFrame()
{
    PushEvent(L"Renderer::BeginFrame");
    m_frame_started = true;

    PushEvent(L"Renderer::ClearRenderTargets");
    {
        // TODO
    }
    PopEvent(); // "ClearRenderTargets"

    m_sprite_batches[SpriteBatch::kDrawChar].BeginFrame();
    m_sprite_batches[SpriteBatch::kDrawPics].BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EndFrame()
{
    Flush2D();

    // TODO
//    m_window.swap_chain->Present(0, 0);

    m_frame_started  = false;
    m_window_resized = false;

    PopEvent(); // "Renderer::BeginFrame" 
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::Flush2D()
{
    PushEvent(L"Renderer::Flush2D");

    // TODO

    PopEvent(); // "Renderer::Flush2D"
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::UploadTexture(const TextureImage * const tex)
{
    FASTASSERT(tex != nullptr);
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

#if REFD3D12_WITH_DEBUG_FRAME_EVENTS
void Renderer::InitDebugEvents()
{
    auto r_debug_frame_events = GameInterface::Cvar::Get("r_debug_frame_events", "0", CvarWrapper::kFlagArchive);
    if (r_debug_frame_events.IsSet())
    {
        // TODO
    }
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::PushEventF(const wchar_t * format, ...)
{
    // TODO
    (void)format;
}
#endif // REFD3D12_WITH_DEBUG_FRAME_EVENTS

///////////////////////////////////////////////////////////////////////////////
// Global Renderer instance
///////////////////////////////////////////////////////////////////////////////

Renderer * g_Renderer = nullptr;

///////////////////////////////////////////////////////////////////////////////

void CreateRendererInstance()
{
    FASTASSERT(g_Renderer == nullptr);
    g_Renderer = new(MemTag::kRenderer) Renderer{};
}

///////////////////////////////////////////////////////////////////////////////

void DestroyRendererInstance()
{
    DeleteObject(g_Renderer, MemTag::kRenderer);
    g_Renderer = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

} // D3D12
} // MrQ2
