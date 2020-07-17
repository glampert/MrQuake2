//
// Impl_D3D12.cpp
//  D3D12 renderer back-end implementations for the Quake2 render objects.
//

#include "Impl_D3D12.hpp"
#include "Renderer_D3D12.hpp"

namespace MrQ2
{
namespace D3D12
{

///////////////////////////////////////////////////////////////////////////////
// TextureImageImpl
///////////////////////////////////////////////////////////////////////////////

void TextureImageImpl::InitD3DSpecific()
{
    auto * device  = Renderer::Device();
    srv_descriptor = Renderer::SrvDescriptorHeap()->AllocateShaderVisibleDescriptor();

    // Texture resource:
    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type                  = D3D12_HEAP_TYPE_DEFAULT;
    heap_props.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC res_desc     = {};
    res_desc.Dimension               = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    res_desc.Alignment               = 0;
    res_desc.Width                   = width;
    res_desc.Height                  = height;
    res_desc.DepthOrArraySize        = 1;
    res_desc.MipLevels               = 1;
    res_desc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    res_desc.SampleDesc.Count        = 1;
    res_desc.SampleDesc.Quality      = 0;
    res_desc.Layout                  = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    res_desc.Flags                   = D3D12_RESOURCE_FLAG_NONE;

    Dx12Check(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource)));
    Dx12SetDebugName(resource, L"Texture2D");

    // Upload texture pixels:
    Renderer::UploadCtx()->UploadTextureSync(*this, device);

    // Create texture view:
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format                          = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels             = res_desc.MipLevels;
    srv_desc.Texture2D.MostDetailedMip       = 0;
    srv_desc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    device->CreateShaderResourceView(resource.Get(), &srv_desc, srv_descriptor.cpu_handle);
}

///////////////////////////////////////////////////////////////////////////////

void TextureImageImpl::InitFromScrap(const TextureImageImpl * const scrap_tex)
{
    MRQ2_ASSERT(scrap_tex != nullptr);
    MRQ2_ASSERT(scrap_tex->from_scrap);

    // Share the scrap texture resource(s)
    resource       = scrap_tex->resource;
    srv_descriptor = scrap_tex->srv_descriptor;
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
        Renderer::UploadCtx()->UploadTextureSync(*ScrapImpl(), Renderer::Device());
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
// ViewDrawStateImpl
///////////////////////////////////////////////////////////////////////////////

void ViewDrawStateImpl::Init(const int max_verts)
{
    m_buffers.Init(Renderer::Device(), "ViewDrawStateImpl", max_verts);
    m_draw_cmds = new(MemTag::kRenderer) DrawCmdList{};
}

///////////////////////////////////////////////////////////////////////////////

void ViewDrawStateImpl::BeginRenderPass()
{
    MRQ2_ASSERT(m_batch_open == false);
    MRQ2_ASSERT(m_draw_cmds->empty());

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

void ViewDrawStateImpl::EndRenderPass(ID3D12GraphicsCommandList * gfx_cmd_list, ID3D12PipelineState * pipeline_state, ShaderProgram& shader_prog)
{
    MRQ2_ASSERT(m_batch_open == false);

    // Flush draw:
    const auto draw_buf = m_buffers.End();

    gfx_cmd_list->IASetVertexBuffers(0, 1, &draw_buf.buffer_ptr->view);
    gfx_cmd_list->SetPipelineState(pipeline_state);
    gfx_cmd_list->SetGraphicsRootSignature(shader_prog.root_signature.Get());

    constexpr float depth_min = 0.0f;
    constexpr float depth_max = 1.0f;
    const auto window_width   = static_cast<float>(Renderer::Width());
    const auto window_height  = static_cast<float>(Renderer::Height());

    auto SetDepthRange = [gfx_cmd_list, window_width, window_height](const float near_val, const float far_val)
    {
        D3D12_VIEWPORT vp;
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = 0.0f;
        vp.Width    = window_width;
        vp.Height   = window_height;
        vp.MinDepth = near_val;
        vp.MaxDepth = far_val;
        gfx_cmd_list->RSSetViewports(1, &vp);
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

        // Slot[0] constants
        const DirectX::XMMATRIX mvp_matrix = cmd.model_mtx * m_viewproj_mtx;
        gfx_cmd_list->SetGraphicsRoot32BitConstants(0, 16, &mvp_matrix, 0);

        // Bind texture & sampler (t0, s0):
        const auto * tex = static_cast<const TextureImageImpl *>(cmd.texture);
        gfx_cmd_list->SetGraphicsRootDescriptorTable(1, tex->srv_descriptor.gpu_handle);

        gfx_cmd_list->IASetPrimitiveTopology(PrimitiveTopologyToD3D(cmd.topology));
        gfx_cmd_list->DrawInstanced(cmd.num_verts, /*num_instances=*/1, cmd.first_vert, /*first_instance=*/0);

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
    MRQ2_ASSERT(m_batch_open == false);
    MRQ2_ASSERT_ALIGN16(args.model_matrix.floats);

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
    MRQ2_ASSERT(batch.IsValid());
    MRQ2_ASSERT(m_batch_open == true);
    MRQ2_ASSERT(m_current_draw_cmd.topology == batch.Topology());

    m_current_draw_cmd.first_vert = m_buffers.CurrentPosition();
    m_current_draw_cmd.num_verts  = batch.UsedVerts();

    m_buffers.Increment(batch.UsedVerts());

    m_draw_cmds->push_back(m_current_draw_cmd);
    m_current_draw_cmd = {};

    batch.Clear();
    m_batch_open = false;
}

///////////////////////////////////////////////////////////////////////////////

} // D3D12
} // MrQ2
