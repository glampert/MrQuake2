//
// Renderer_D3D11.cpp
//  D3D11 renderer interface for Quake2.
//

#include "Renderer_D3D11.hpp"

// Debug markers need these.
#include <cstdarg>
#include <cstddef>
#include <cwchar>

// Path from the project root where to find shaders for this renderer (wchar_t)
#define REFD3D11_SHADER_PATH_WIDE L"src\\reflibs\\d3d11\\shaders\\"

namespace MrQ2
{
namespace D3D11
{

///////////////////////////////////////////////////////////////////////////////

static D3D_PRIMITIVE_TOPOLOGY PrimitiveTopologyToD3D(const PrimitiveTopology topology)
{
    switch (topology)
    {
    case PrimitiveTopology::kTriangleList  : return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case PrimitiveTopology::kTriangleStrip : return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case PrimitiveTopology::kTriangleFan   : return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; // Converted by the front-end
    default : GameInterface::Errorf("Bad PrimitiveTopology enum!");
    } // switch
}

///////////////////////////////////////////////////////////////////////////////
// TextureImageImpl
///////////////////////////////////////////////////////////////////////////////

void TextureImageImpl::InitD3DSpecific()
{
    ID3D11Device* const device = Renderer::Device();
    const unsigned numQualityLevels = Renderer::TexStore()->MultisampleQualityLevel(DXGI_FORMAT_R8G8B8A8_UNORM);

    D3D11_TEXTURE2D_DESC tex2dDesc  = {};
    tex2dDesc.Usage                 = D3D11_USAGE_DEFAULT;
    tex2dDesc.BindFlags             = D3D11_BIND_SHADER_RESOURCE;
    tex2dDesc.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex2dDesc.Width                 = width;
    tex2dDesc.Height                = height;
    tex2dDesc.MipLevels             = 1;
    tex2dDesc.ArraySize             = 1;
    tex2dDesc.SampleDesc.Count      = 1;
    tex2dDesc.SampleDesc.Quality    = numQualityLevels - 1;

    D3D11_SAMPLER_DESC samplerDesc  = {};
    samplerDesc.Filter              = FilterForTextureType(type);
    samplerDesc.AddressU            = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV            = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW            = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc      = D3D11_COMPARISON_ALWAYS;
    samplerDesc.MaxAnisotropy       = 1;
    samplerDesc.MipLODBias          = 0.0f;
    samplerDesc.MinLOD              = 0.0f;
    samplerDesc.MaxLOD              = D3D11_FLOAT32_MAX;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem                = pixels;
    initData.SysMemPitch            = width * 4; // RGBA-8888

    if (FAILED(device->CreateTexture2D(&tex2dDesc, &initData, tex_resource.GetAddressOf())))
    {
        GameInterface::Errorf("CreateTexture2D failed!");
    }
    if (FAILED(device->CreateShaderResourceView(tex_resource.Get(), nullptr, srv.GetAddressOf())))
    {
        GameInterface::Errorf("CreateShaderResourceView failed!");
    }
    if (FAILED(device->CreateSamplerState(&samplerDesc, sampler.GetAddressOf())))
    {
        GameInterface::Errorf("CreateSamplerState failed!");
    }
}

///////////////////////////////////////////////////////////////////////////////

void TextureImageImpl::InitFromScrap(const TextureImageImpl * const scrap_tex)
{
    FASTASSERT(scrap_tex != nullptr);

    // Share the scrap texture resource(s)
    tex_resource = scrap_tex->tex_resource;
    sampler      = scrap_tex->sampler;
    srv          = scrap_tex->srv;
}

///////////////////////////////////////////////////////////////////////////////

D3D11_FILTER TextureImageImpl::FilterForTextureType(const TextureType tt)
{
    switch (tt)
    {
    // TODO: Maybe have a Cvar to select between different filter modes?
    // Should also generate mipmaps for the non-UI textures!!!
    // Bi/Tri-linear filtering for cinematics? In that case, need a new type for them...
    case TextureType::kSkin   : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureType::kSprite : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureType::kWall   : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureType::kSky    : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureType::kPic    : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    default : GameInterface::Errorf("Invalid TextureType enum!");
    } // switch
}

///////////////////////////////////////////////////////////////////////////////
// TextureStoreImpl
///////////////////////////////////////////////////////////////////////////////

void TextureStoreImpl::Init()
{
    ID3D11Device* const device = Renderer::Device();
    device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,
                              1, &m_multisample_quality_levels_rgba);

    // Load the default resident textures now
    TouchResidentTextures();
}

///////////////////////////////////////////////////////////////////////////////

void TextureStoreImpl::UploadScrapIfNeeded()
{
    if (m_scrap_dirty)
    {
        Renderer::UploadTexture(ScrapImpl());
        m_scrap_dirty = false;
    }
}

///////////////////////////////////////////////////////////////////////////////

unsigned TextureStoreImpl::MultisampleQualityLevel(const DXGI_FORMAT fmt) const
{
    FASTASSERT(fmt == DXGI_FORMAT_R8G8B8A8_UNORM); // only format support at the moment
    (void)fmt;

    return m_multisample_quality_levels_rgba;
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
// ShaderProgram
///////////////////////////////////////////////////////////////////////////////

void ShaderProgram::LoadFromFxFile(const wchar_t * const filename, const char * const vs_entry,
                                   const char * const ps_entry, const InputLayoutDesc & layout)
{
    FASTASSERT(filename != nullptr && filename[0] != L'\0');
    FASTASSERT(vs_entry != nullptr && vs_entry[0] != '\0');
    FASTASSERT(ps_entry != nullptr && ps_entry[0] != '\0');

    ComPtr<ID3DBlob> vs_blob;
    Renderer::CompileShaderFromFile(filename, vs_entry, "vs_4_0", vs_blob.GetAddressOf());
    FASTASSERT(vs_blob != nullptr);

    ComPtr<ID3DBlob> ps_blob;
    Renderer::CompileShaderFromFile(filename, ps_entry, "ps_4_0", ps_blob.GetAddressOf());
    FASTASSERT(ps_blob != nullptr);

    HRESULT hr;
    ID3D11Device * const device = Renderer::Device();

    // Create the vertex shader:
    hr = device->CreateVertexShader(vs_blob->GetBufferPointer(),
                                    vs_blob->GetBufferSize(), nullptr, vs.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create vertex shader '%s'", vs_entry);
    }

    // Create the pixel shader:
    hr = device->CreatePixelShader(ps_blob->GetBufferPointer(),
                                   ps_blob->GetBufferSize(), nullptr, ps.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create pixel shader '%s'", ps_entry);
    }

    CreateVertexLayout(std::get<0>(layout), std::get<1>(layout), *vs_blob.Get());
}

///////////////////////////////////////////////////////////////////////////////

void ShaderProgram::CreateVertexLayout(const D3D11_INPUT_ELEMENT_DESC * const desc,
                                       const int num_elements, ID3DBlob & vs_blob)
{
    FASTASSERT(desc != nullptr && num_elements > 0);

    HRESULT hr = Renderer::Device()->CreateInputLayout(
            desc, num_elements, vs_blob.GetBufferPointer(),
            vs_blob.GetBufferSize(), vertex_layout.GetAddressOf());

    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create vertex layout!");
    }
}

///////////////////////////////////////////////////////////////////////////////
// DepthStateHelper
///////////////////////////////////////////////////////////////////////////////

void DepthStateHelper::Init(const bool enabled_ztest,
                            const D3D11_COMPARISON_FUNC enabled_func,
                            const D3D11_DEPTH_WRITE_MASK enabled_write_mask,
                            const bool disabled_ztest,
                            const D3D11_COMPARISON_FUNC disabled_func,
                            const D3D11_DEPTH_WRITE_MASK disabled_write_mask)
{
    D3D11_DEPTH_STENCIL_DESC ds_desc = {};

    //
    // Stencil test parameters (always OFF):
    //

    ds_desc.StencilEnable                = false;
    ds_desc.StencilReadMask              = 0;
    ds_desc.StencilWriteMask             = 0;
    // Stencil operations if pixel is front-facing:
    ds_desc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
    ds_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
    ds_desc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
    ds_desc.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
    // Stencil operations if pixel is back-facing:
    ds_desc.BackFace.StencilFailOp       = D3D11_STENCIL_OP_KEEP;
    ds_desc.BackFace.StencilDepthFailOp  = D3D11_STENCIL_OP_DECR;
    ds_desc.BackFace.StencilPassOp       = D3D11_STENCIL_OP_KEEP;
    ds_desc.BackFace.StencilFunc         = D3D11_COMPARISON_ALWAYS;

    //
    // Depth test parameters:
    //

    // When ON:
    ds_desc.DepthEnable    = enabled_ztest;
    ds_desc.DepthFunc      = enabled_func;
    ds_desc.DepthWriteMask = enabled_write_mask;
    // Create depth stencil state for rendering with depth test ENABLED:
    if (FAILED(Renderer::Device()->CreateDepthStencilState(&ds_desc, enabled_state.GetAddressOf())))
    {
        GameInterface::Errorf("CreateDepthStencilState failed!");
    }

    // When OFF:
    ds_desc.DepthEnable    = disabled_ztest;
    ds_desc.DepthFunc      = disabled_func;
    ds_desc.DepthWriteMask = disabled_write_mask;
    // Create depth stencil state for rendering with depth test DISABLED:
    if (FAILED(Renderer::Device()->CreateDepthStencilState(&ds_desc, disabled_state.GetAddressOf())))
    {
        GameInterface::Errorf("CreateDepthStencilState failed!");
    }
}

///////////////////////////////////////////////////////////////////////////////
// SpriteBatch
///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::Init(const int max_verts)
{
    m_buffers.Init("SpriteBatch", max_verts, Renderer::Device(), Renderer::DeviceContext());
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::BeginFrame()
{
    m_buffers.Begin();
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::EndFrame(const ShaderProgram & program, const TextureImageImpl * const tex, ID3D11Buffer * const cbuffer)
{
    ID3D11DeviceContext * const context = Renderer::DeviceContext();
    const auto draw_buf = m_buffers.End();

    // Constant buffer at register(b0)
    context->VSSetConstantBuffers(0, 1, &cbuffer);

    // Set blending for transparency:
    Renderer::EnableAlphaBlending();

    // Fast path - one texture for the whole batch:
    if (tex != nullptr)
    {
        // Bind texture & sampler (t0, s0):
        context->PSSetShaderResources(0, 1, tex->srv.GetAddressOf());
        context->PSSetSamplers(0, 1, tex->sampler.GetAddressOf());

        // Draw with the current vertex buffer:
        Renderer::DrawHelper(draw_buf.used_verts, 0, program, draw_buf.buffer_ptr,
                             D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, 0, sizeof(DrawVertex2D));
    }
    else // Handle small unique textured draws:
    {
        const TextureImageImpl * previous_tex = nullptr;
        for (const auto & d : m_deferred_textured_quads)
        {
            if (previous_tex != d.tex)
            {
                context->PSSetShaderResources(0, 1, d.tex->srv.GetAddressOf());
                context->PSSetSamplers(0, 1, d.tex->sampler.GetAddressOf());
                previous_tex = d.tex;
            }

            Renderer::DrawHelper(/*num_verts=*/ 6, d.quad_start_vtx, program, draw_buf.buffer_ptr,
                                 D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, 0, sizeof(DrawVertex2D));
        }
    }

    // Restore default blend state.
    Renderer::DisableAlphaBlending();

    // Clear cache for next frame:
    if (!m_deferred_textured_quads.empty())
    {
        m_deferred_textured_quads.clear();
    }
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
    const int quad_start_vtx = m_buffers.CurrentPosition();
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
    const int quad_start_vtx = m_buffers.CurrentPosition();
    PushQuad(x, y, w, h, u0, v0, u1, v1, color);
    m_deferred_textured_quads.push_back({ quad_start_vtx, static_cast<const TextureImageImpl *>(tex) });
}

///////////////////////////////////////////////////////////////////////////////
// ViewDrawStateImpl
///////////////////////////////////////////////////////////////////////////////

void ViewDrawStateImpl::Init(const int max_verts, const ShaderProgram & sp,
                             ID3D11Buffer * cbuff_vs, ID3D11Buffer * cbuff_ps)
{
    m_buffers.Init("ViewDrawStateImpl", max_verts,
                   Renderer::Device(), Renderer::DeviceContext());

    m_program    = &sp;
    m_cbuffer_vs = cbuff_vs;
    m_cbuffer_ps = cbuff_ps;

    m_draw_cmds = new(MemTag::kRenderer) DrawCmdList{};
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawStateImpl::BeginRenderPass()
{
    FASTASSERT(m_batch_open == false);
    FASTASSERT(m_draw_cmds->empty());

    m_buffers.Begin();
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawStateImpl::EndRenderPass()
{
    FASTASSERT(m_batch_open == false);

    // Flush draw:
    ID3D11DeviceContext * const context = Renderer::DeviceContext();
    const auto draw_buf = m_buffers.End();

    // Constant buffer at register(b0) (VS) and register(b1) (PS)
    context->VSSetConstantBuffers(0, 1, &m_cbuffer_vs);
    context->PSSetConstantBuffers(1, 1, &m_cbuffer_ps);

    constexpr float depth_min = 0.0f;
    constexpr float depth_max = 1.0f;
    const auto window_width   = static_cast<float>(Renderer::Width());
    const auto window_height  = static_cast<float>(Renderer::Height());

    auto SetDepthRange = [context, window_width, window_height](const float near_val, const float far_val)
    {
        D3D11_VIEWPORT vp;
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width    = window_width;
        vp.Height   = window_height;
        vp.MinDepth = near_val;
        vp.MaxDepth = far_val;
        context->RSSetViewports(1, &vp);
    };

    for (const DrawCmd & cmd : *m_draw_cmds)
    {
        bool depth_range_changed = false;

        // Depth hack to prevent weapons from poking into geometry.
        if (cmd.depth_hack)
        {
            SetDepthRange(depth_min, depth_min + 0.3f * (depth_max - depth_min));
            depth_range_changed = true;
        }

        const DirectX::XMMATRIX mvp_matrix = cmd.model_mtx * m_viewproj_mtx;
        context->UpdateSubresource(m_cbuffer_vs, 0, nullptr, &mvp_matrix, 0, 0);

        // Bind texture & sampler (t0, s0):
        const auto * tex = static_cast<const TextureImageImpl *>(cmd.texture);
        context->PSSetShaderResources(0, 1, tex->srv.GetAddressOf());
        context->PSSetSamplers(0, 1, tex->sampler.GetAddressOf());

        // Draw with the current vertex buffer:
        Renderer::DrawHelper(cmd.num_verts, cmd.first_vert, *m_program, draw_buf.buffer_ptr,
                             PrimitiveTopologyToD3D(cmd.topology), 0, sizeof(DrawVertex3D));

        // Restore to default if we did a depth hacked draw.
        if (depth_range_changed)
        {
            SetDepthRange(depth_min, depth_max);
        }
    }

    m_draw_cmds->clear();
}

///////////////////////////////////////////////////////////////////////////////

MiniImBatch ViewDrawStateImpl::BeginBatch(const BeginBatchArgs & args)
{
    FASTASSERT(m_batch_open == false);
    FASTASSERT_ALIGN16(args.model_matrix.floats);

    m_current_draw_cmd.model_mtx  = DirectX::XMMATRIX{ args.model_matrix.floats };
    m_current_draw_cmd.texture    = args.optional_tex ? args.optional_tex : Renderer::TexStore()->tex_white2x2;
    m_current_draw_cmd.topology   = args.topology;
    m_current_draw_cmd.depth_hack = args.depth_hack;
    m_current_draw_cmd.first_vert = 0;
    m_current_draw_cmd.num_verts  = 0;

    m_batch_open = true;

    return MiniImBatch{ m_buffers.CurrentVertexPtr(), m_buffers.NumVertsRemaining(), args.topology };
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawStateImpl::EndBatch(MiniImBatch & batch)
{
    FASTASSERT(batch.IsValid());
    FASTASSERT(m_batch_open == true);
    FASTASSERT(m_current_draw_cmd.topology == batch.Topology());

    m_current_draw_cmd.first_vert = m_buffers.CurrentPosition();
    m_current_draw_cmd.num_verts  = batch.UsedVerts();

    m_buffers.Increment(batch.UsedVerts());

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

Renderer::State * Renderer::sm_state = nullptr;

///////////////////////////////////////////////////////////////////////////////

void Renderer::Init(HINSTANCE hinst, WNDPROC wndproc, const int width, const int height, const bool fullscreen, const bool debug_validation)
{
    if (sm_state != nullptr)
    {
        GameInterface::Errorf("D3D11 Renderer is already initialized!");
    }

    GameInterface::Printf("D3D11 Renderer initializing.");

    sm_state = new(MemTag::kRenderer) State{};

    sm_state->m_disable_texturing = GameInterface::Cvar::Get("r_disable_texturing", "0", 0);
    sm_state->m_blend_debug_color = GameInterface::Cvar::Get("r_blend_debug_color", "0", 0);

    // RenderWindow setup
	const char * const window_name = "MrQuake2 (D3D11)";
    sm_state->m_window.window_name      = window_name;
    sm_state->m_window.class_name       = window_name;
    sm_state->m_window.hinst            = hinst;
    sm_state->m_window.wndproc          = wndproc;
    sm_state->m_window.width            = width;
    sm_state->m_window.height           = height;
    sm_state->m_window.fullscreen       = fullscreen;
    sm_state->m_window.debug_validation = debug_validation;
    sm_state->m_window.Init();

    // 2D sprite/UI batch setup
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawChar)].Init(6 * 5000); // 6 verts per quad (expand to 2 triangles each)
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawPics)].Init(6 * 128);

    // Initialize the stores/caches
    sm_state->m_tex_store.Init();
    sm_state->m_mdl_store.Init();

    // Load shader progs / render state objects
    CreateRSObjects();
    LoadShaders();

    // World geometry rendering helper
    constexpr int kViewDrawBatchSize = 25000; // size in vertices
    sm_state->m_view_draw_state.Init(kViewDrawBatchSize, sm_state->m_shader_geometry,
                                     sm_state->m_cbuffer_geometry_vs.Get(), sm_state->m_cbuffer_geometry_ps.Get());

    // So we can annotate our RenderDoc captures
    InitDebugEvents();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::Shutdown()
{
    GameInterface::Printf("D3D11 Renderer shutting down.");

    DeleteObject(sm_state, MemTag::kRenderer);
    sm_state = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::CreateRSObjects()
{
    sm_state->m_depth_test_states.Init(
        true,  D3D11_COMPARISON_LESS,   D3D11_DEPTH_WRITE_MASK_ALL,   // When ON
        false, D3D11_COMPARISON_ALWAYS, D3D11_DEPTH_WRITE_MASK_ALL);  // When OFF

    sm_state->m_depth_write_states.Init(
        true,  D3D11_COMPARISON_LESS,   D3D11_DEPTH_WRITE_MASK_ALL,   // When ON
        true,  D3D11_COMPARISON_ALWAYS, D3D11_DEPTH_WRITE_MASK_ZERO); // When OFF
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::LoadShaders()
{
    GameInterface::Printf("CWD......: %s", OSWindow::CurrentWorkingDir().c_str());
    GameInterface::Printf("GameDir..: %s", GameInterface::FS::GameDir());

    // UI/2D sprites:
    {
        const D3D11_INPUT_ELEMENT_DESC layout[] = { // DrawVertex2D
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex2D, xy_uv), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex2D, rgba),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        const int num_elements = ARRAYSIZE(layout);
        sm_state->m_shader_ui_sprites.LoadFromFxFile(REFD3D11_SHADER_PATH_WIDE L"UISprites2D.fx",
                                                     "VS_main", "PS_main", { layout, num_elements });

        // Blend state for the screen text and transparencies:
        D3D11_BLEND_DESC bs_desc                      = {};
        bs_desc.RenderTarget[0].BlendEnable           = true;
        bs_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        bs_desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        bs_desc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        bs_desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        bs_desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        bs_desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        bs_desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        if (FAILED(Device()->CreateBlendState(&bs_desc, sm_state->m_blend_state_alpha.GetAddressOf())))
        {
            GameInterface::Errorf("CreateBlendState failed!");
        }

        // Create the constant buffer:
        D3D11_BUFFER_DESC buf_desc = {};
        buf_desc.Usage             = D3D11_USAGE_DEFAULT;
        buf_desc.ByteWidth         = sizeof(ConstantBufferDataUIVS);
        buf_desc.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(Device()->CreateBuffer(&buf_desc, nullptr, sm_state->m_cbuffer_ui_sprites.GetAddressOf())))
        {
            GameInterface::Errorf("Failed to create shader constant buffer!");
        }
    }

    // Common 3D geometry:
    {
        const D3D11_INPUT_ELEMENT_DESC layout[] = { // DrawVertex3D
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(DrawVertex3D, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(DrawVertex3D, uv),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex3D, rgba),     D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        const int num_elements = ARRAYSIZE(layout);
        sm_state->m_shader_geometry.LoadFromFxFile(REFD3D11_SHADER_PATH_WIDE L"GeometryCommon.fx",
                                                   "VS_main", "PS_main", { layout, num_elements });

        // Create the constant buffers:
        D3D11_BUFFER_DESC buf_desc = {};
        buf_desc.Usage             = D3D11_USAGE_DEFAULT;
        buf_desc.ByteWidth         = sizeof(ConstantBufferDataSGeomVS);
        buf_desc.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(Device()->CreateBuffer(&buf_desc, nullptr, sm_state->m_cbuffer_geometry_vs.GetAddressOf())))
        {
            GameInterface::Errorf("Failed to create VS shader constant buffer!");
        }

        buf_desc.ByteWidth = sizeof(ConstantBufferDataSGeomPS);
        if (FAILED(Device()->CreateBuffer(&buf_desc, nullptr, sm_state->m_cbuffer_geometry_ps.GetAddressOf())))
        {
            GameInterface::Errorf("Failed to create PS shader constant buffer!");
        }
    }

    GameInterface::Printf("Shaders loaded successfully.");
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderView(const refdef_s & view_def)
{
    PushEvent(L"Renderer::RenderView");

    ViewDrawStateImpl::FrameData frame_data{ sm_state->m_tex_store, *sm_state->m_mdl_store.WorldModel(), view_def };

    // Enter 3D mode (depth test ON)
    EnableDepthTest();

    // Set up camera/view (fills frame_data)
    sm_state->m_view_draw_state.RenderViewSetup(frame_data);

    // Update the constant buffers for this frame
    RenderViewUpdateCBuffers(frame_data);

    // Set the camera/world-view:
    FASTASSERT_ALIGN16(frame_data.view_proj_matrix.floats);
    const auto vp_mtx = DirectX::XMMATRIX{ frame_data.view_proj_matrix.floats };
    sm_state->m_view_draw_state.SetViewProjMatrix(vp_mtx);

    //
    // Render solid geometries (world and entities)
    //

    sm_state->m_view_draw_state.BeginRenderPass();

    PushEvent(L"RenderWorldModel");
    sm_state->m_view_draw_state.RenderWorldModel(frame_data);
    PopEvent(); // "RenderWorldModel"

    PushEvent(L"RenderSkyBox");
    sm_state->m_view_draw_state.RenderSkyBox(frame_data);
    PopEvent(); // "RenderSkyBox"

    PushEvent(L"RenderSolidEntities");
    sm_state->m_view_draw_state.RenderSolidEntities(frame_data);
    PopEvent(); // "RenderSolidEntities"

    sm_state->m_view_draw_state.EndRenderPass();

    //
    // Transparencies/alpha pass
    //

    // Color Blend ON
    EnableAlphaBlending();

    PushEvent(L"RenderTranslucentSurfaces");
    sm_state->m_view_draw_state.BeginRenderPass();
    sm_state->m_view_draw_state.RenderTranslucentSurfaces(frame_data);
    sm_state->m_view_draw_state.EndRenderPass();
    PopEvent(); // "RenderTranslucentSurfaces"

    PushEvent(L"RenderTranslucentEntities");
    DisableDepthWrites(); // Disable z writes in case they stack up
    sm_state->m_view_draw_state.BeginRenderPass();
    sm_state->m_view_draw_state.RenderTranslucentEntities(frame_data);
    sm_state->m_view_draw_state.EndRenderPass();
    EnableDepthWrites();
    PopEvent(); // "RenderTranslucentEntities"

    // Color Blend OFF
    DisableAlphaBlending();

    // Back to 2D rendering mode (depth test OFF)
    DisableDepthTest();

    PopEvent(); // "Renderer::RenderView"
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderViewUpdateCBuffers(const ViewDrawStateImpl::FrameData & frame_data)
{
    FASTASSERT_ALIGN16(frame_data.view_proj_matrix.floats);

    ConstantBufferDataSGeomVS cbuffer_data_geometry_vs;
    cbuffer_data_geometry_vs.mvp_matrix = DirectX::XMMATRIX{ frame_data.view_proj_matrix.floats };
    DeviceContext()->UpdateSubresource(sm_state->m_cbuffer_geometry_vs.Get(), 0, nullptr, &cbuffer_data_geometry_vs, 0, 0);

    ConstantBufferDataSGeomPS cbuffer_data_geometry_ps;
    if (sm_state->m_disable_texturing.IsSet()) // Use only debug vertex color
    {
        cbuffer_data_geometry_ps.texture_color_scaling = kFloat4Zero;
        cbuffer_data_geometry_ps.vertex_color_scaling  = kFloat4One;
    }
    else if (sm_state->m_blend_debug_color.IsSet()) // Blend debug vertex color with texture
    {
        cbuffer_data_geometry_ps.texture_color_scaling = kFloat4One;
        cbuffer_data_geometry_ps.vertex_color_scaling  = kFloat4One;
    }
    else // Normal rendering
    {
        cbuffer_data_geometry_ps.texture_color_scaling = kFloat4One;
        cbuffer_data_geometry_ps.vertex_color_scaling  = kFloat4Zero;
    }
    DeviceContext()->UpdateSubresource(sm_state->m_cbuffer_geometry_ps.Get(), 0, nullptr, &cbuffer_data_geometry_ps, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EnableDepthTest()
{
    DeviceContext()->OMSetDepthStencilState(sm_state->m_depth_test_states.enabled_state.Get(), 0);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::DisableDepthTest()
{
    DeviceContext()->OMSetDepthStencilState(sm_state->m_depth_test_states.disabled_state.Get(), 0);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EnableDepthWrites()
{
    DeviceContext()->OMSetDepthStencilState(sm_state->m_depth_write_states.enabled_state.Get(), 0);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::DisableDepthWrites()
{
    DeviceContext()->OMSetDepthStencilState(sm_state->m_depth_write_states.disabled_state.Get(), 0);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EnableAlphaBlending()
{
    const float blend_factor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    DeviceContext()->OMSetBlendState(sm_state->m_blend_state_alpha.Get(), blend_factor, 0xFFFFFFFF);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::DisableAlphaBlending()
{
    DeviceContext()->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::BeginFrame()
{
    PushEvent(L"Renderer::BeginFrame");
    sm_state->m_frame_started = true;

    PushEvent(L"Renderer::ClearRenderTargets");
    {
        sm_state->m_window.device_context->ClearRenderTargetView(sm_state->m_window.framebuffer_rtv.Get(), &kClearColor.x);

        const float depth_clear = 1.0f;
        const std::uint8_t stencil_clear = 0;
        sm_state->m_window.device_context->ClearDepthStencilView(sm_state->m_window.depth_stencil_view.Get(),
                                                                 D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                                                 depth_clear, stencil_clear);
    }
    PopEvent(); // "ClearRenderTargets"

    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawChar)].BeginFrame();
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawPics)].BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EndFrame()
{
    Flush2D();

    sm_state->m_window.swap_chain->Present(0, 0);

    sm_state->m_frame_started  = false;
    sm_state->m_window_resized = false;

    PopEvent(); // "Renderer::BeginFrame" 
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::Flush2D()
{
    PushEvent(L"Renderer::Flush2D");

    FASTASSERT(sm_state->m_cbuffer_ui_sprites != nullptr);

    if (sm_state->m_window_resized)
    {
        ConstantBufferDataUIVS cbuffer_data_ui;
        cbuffer_data_ui.screen_dimensions = kFloat4Zero; // Set unused elements to zero
        cbuffer_data_ui.screen_dimensions.x = static_cast<float>(sm_state->m_window.width);
        cbuffer_data_ui.screen_dimensions.y = static_cast<float>(sm_state->m_window.height);
        DeviceContext()->UpdateSubresource(sm_state->m_cbuffer_ui_sprites.Get(), 0, nullptr, &cbuffer_data_ui, 0, 0);
    }

    // Remaining 2D geometry:
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawPics)].EndFrame(sm_state->m_shader_ui_sprites,
        nullptr, sm_state->m_cbuffer_ui_sprites.Get());

    // Flush 2D text:
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawChar)].EndFrame(sm_state->m_shader_ui_sprites,
        static_cast<const TextureImageImpl *>(sm_state->m_tex_store.tex_conchars),
        sm_state->m_cbuffer_ui_sprites.Get());

    PopEvent(); // "Renderer::Flush2D"
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::DrawHelper(const unsigned num_verts, const unsigned first_vert, const ShaderProgram & program,
                          ID3D11Buffer * const vb, const D3D11_PRIMITIVE_TOPOLOGY topology,
                          const unsigned offset, const unsigned stride)
{
    ID3D11DeviceContext * const context = DeviceContext();

    context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context->IASetPrimitiveTopology(topology);
    context->IASetInputLayout(program.vertex_layout.Get());
    context->VSSetShader(program.vs.Get(), nullptr, 0);
    context->PSSetShader(program.ps.Get(), nullptr, 0);
    context->Draw(num_verts, first_vert);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::CompileShaderFromFile(const wchar_t * const filename, const char * const entry_point,
                                     const char * const shader_model, ID3DBlob ** out_blob)
{
    UINT shader_flags = D3DCOMPILE_ENABLE_STRICTNESS;

    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows
    // the shaders to be optimized and to run exactly the way they will run in
    // the release configuration.
    if (DebugValidation())
    {
        shader_flags |= D3DCOMPILE_DEBUG;
    }

    ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3DCompileFromFile(filename, nullptr, nullptr, entry_point, shader_model,
                                    shader_flags, 0, out_blob, error_blob.GetAddressOf());

    if (FAILED(hr))
    {
        auto * details = (error_blob ? static_cast<const char *>(error_blob->GetBufferPointer()) : "<no info>");
        GameInterface::Errorf("Failed to compile shader: %s.\n\nError info: %s", OSWindow::ErrorToString(hr).c_str(), details);
    }
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::UploadTexture(const TextureImage * const tex)
{
    FASTASSERT(tex != nullptr);

    const TextureImageImpl* const impl = static_cast<const TextureImageImpl *>(tex);
    const UINT subRsrc  = 0; // no mips/slices
    const UINT rowPitch = impl->width * 4; // RGBA

    DeviceContext()->UpdateSubresource(impl->tex_resource.Get(), subRsrc, nullptr, impl->pixels, rowPitch, 0);
}

///////////////////////////////////////////////////////////////////////////////

#if REFD3D11_WITH_DEBUG_FRAME_EVENTS
void Renderer::InitDebugEvents()
{
    auto r_debug_frame_events = GameInterface::Cvar::Get("r_debug_frame_events", "0", CvarWrapper::kFlagArchive);
    if (r_debug_frame_events.IsSet())
    {
        ID3DUserDefinedAnnotation * annotations = nullptr;
        if (SUCCEEDED(DeviceContext()->QueryInterface(__uuidof(annotations), (void **)&annotations)))
        {
            sm_state->m_annotations.Attach(annotations);
            GameInterface::Printf("Successfully created ID3DUserDefinedAnnotation.");
        }
        else
        {
            GameInterface::Printf("Unable to create ID3DUserDefinedAnnotation.");
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::PushEventF(const wchar_t * format, ...)
{
    if (sm_state->m_annotations)
    {
        va_list args;
        wchar_t buffer[512];

        va_start(args, format);
        std::vswprintf(buffer, ArrayLength(buffer), format, args);
        va_end(args);

        sm_state->m_annotations->BeginEvent(buffer);
    }
}
#endif // REFD3D11_WITH_DEBUG_FRAME_EVENTS

} // D3D11
} // MrQ2
