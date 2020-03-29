//
// Impl_D3D12.hpp
//  D3D12 renderer back-end implementations for the Quake2 render objects.
//
#pragma once

#include "Helpers_D3D12.hpp"
#include "reflibs/shared/Pool.hpp"
#include "reflibs/shared/Memory.hpp"
#include "reflibs/shared/ModelStore.hpp"
#include "reflibs/shared/ViewDraw.hpp"

namespace MrQ2
{
namespace D3D12
{

/*
===============================================================================

    D3D12 TextureImageImpl

===============================================================================
*/
class TextureImageImpl final
    : public TextureImage
{
public:
    using TextureImage::TextureImage;

    void InitD3DSpecific();
    void InitFromScrap(const TextureImageImpl * scrap_tex);
};

/*
===============================================================================

    D3D12 TextureStoreImpl

===============================================================================
*/
class TextureStoreImpl final
    : public TextureStore
{
public:

    TextureStoreImpl()
        : m_teximages_pool{ MemTag::kRenderer }
    { }

    ~TextureStoreImpl()
    {
        DestroyAllLoadedTextures();
    }

    void Init();

    void UploadScrapIfNeeded();
    const TextureImageImpl * ScrapImpl() const { return static_cast<const TextureImageImpl *>(tex_scrap); }

protected:

    /*virtual*/ TextureImage * CreateScrap(int size, const ColorRGBA32 * pix) override;
    /*virtual*/ TextureImage * CreateTexture(const ColorRGBA32 * pix, std::uint32_t regn, TextureType tt, bool use_scrap,
                                             int w, int h, Vec2u16 scrap0, Vec2u16 scrap1, const char * name) override;
    /*virtual*/ void DestroyTexture(TextureImage * tex) override;

private:

    Pool<TextureImageImpl, kTexturePoolSize> m_teximages_pool;
    bool m_scrap_dirty = false;
};

/*
===============================================================================

    D3D12 ModelInstanceImpl

===============================================================================
*/
class ModelInstanceImpl final
    : public ModelInstance
{
public:
    using ModelInstance::ModelInstance;
    // Nothing back-end specific for the Render Models for now.
};

/*
===============================================================================

    D3D12 ModelStoreImpl

===============================================================================
*/
class ModelStoreImpl final
    : public ModelStore
{
public:

    explicit ModelStoreImpl(TextureStore & tex_store)
        : ModelStore{ tex_store }
        , m_models_pool{ MemTag::kRenderer }
    { }

    ~ModelStoreImpl();
    void Init();

protected:

    /*virtual*/ ModelInstance * GetInlineModel(int model_index) override { return m_inline_models[model_index]; }
    /*virtual*/ ModelInstance * CreateModel(const char * name, ModelType mt, std::uint32_t regn) override;
    /*virtual*/ void DestroyModel(ModelInstance * mdl) override;

private:

    Pool<ModelInstanceImpl, kModelPoolSize> m_models_pool;
    std::vector<ModelInstanceImpl *>        m_inline_models;
};

/*
===============================================================================

    D3D12 ViewDrawStateImpl

===============================================================================
*/
class ViewDrawStateImpl final
    : public ViewDrawState
{
public:

    ViewDrawStateImpl() = default;
    virtual ~ViewDrawStateImpl() { DeleteObject(m_draw_cmds, MemTag::kRenderer); }

    void Init(int max_verts);
    void SetViewProjMatrix(const DirectX::XMMATRIX & mtx) { m_viewproj_mtx = mtx; }
    void BeginRenderPass();
    void EndRenderPass();

    // Disallow copy.
    ViewDrawStateImpl(const ViewDrawStateImpl &) = delete;
    ViewDrawStateImpl & operator=(const ViewDrawStateImpl &) = delete;

protected:

    /*virtual*/ MiniImBatch BeginBatch(const BeginBatchArgs & args) override final;
    /*virtual*/ void EndBatch(MiniImBatch & batch) override final;

private:

    struct DrawCmd
    {
        DirectX::XMMATRIX    model_mtx;
        const TextureImage * texture;
        unsigned             first_vert;
        unsigned             num_verts;
        PrimitiveTopology    topology;
        bool                 depth_hack;
    };

    using DrawCmdList = FixedSizeArray<DrawCmd, 2048>;

    DrawCmd m_current_draw_cmd = {};
    DrawCmdList * m_draw_cmds  = nullptr;

    DirectX::XMMATRIX m_viewproj_mtx = {};
    bool              m_batch_open   = false;
};

} // D3D12
} // MrQ2
