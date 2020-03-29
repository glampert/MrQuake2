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
        Renderer::UploadTexture(ScrapImpl());
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
    m_current_draw_cmd.texture    = args.optional_tex ? args.optional_tex : Renderer::TexStore()->tex_white2x2;
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

} // D3D12
} // MrQ2
