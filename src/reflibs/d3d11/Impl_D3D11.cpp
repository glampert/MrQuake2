//
// Impl_D3D11.cpp
//  D3D11 renderer back-end implementations for the Quake2 render objects.
//

#include "Impl_D3D11.hpp"
#include "Renderer_D3D11.hpp"

namespace MrQ2
{
namespace D3D11
{

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
    FASTASSERT(fmt == DXGI_FORMAT_R8G8B8A8_UNORM); // only format supported at the moment
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

static inline D3D_PRIMITIVE_TOPOLOGY PrimitiveTopologyToD3D(const PrimitiveTopology topology)
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

} // D3D11
} // MrQ2
