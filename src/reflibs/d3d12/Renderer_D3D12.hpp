//
// Renderer_D3D12.hpp
//  D3D12 renderer interface for Quake2.
//
#pragma once

#include "RenderWindow_D3D12.hpp"
#include "reflibs/shared/Pool.hpp"
#include "reflibs/shared/Memory.hpp"
#include "reflibs/shared/ModelStore.hpp"
#include "reflibs/shared/TextureStore.hpp"
#include "reflibs/shared/ViewDraw.hpp"
#include "reflibs/shared/RenderDocUtils.hpp"

#include <array>
#include <tuple>
#include <DirectXMath.h>

#ifndef NDEBUG
    #define REFD3D12_WITH_DEBUG_FRAME_EVENTS 1
#else // NDEBUG
    #define REFD3D12_WITH_DEBUG_FRAME_EVENTS 0
#endif // NDEBUG

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
    {
    }

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
    {
    }

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

/*
===============================================================================

    D3D12 SpriteBatch

===============================================================================
*/
class SpriteBatch final
{
public:

    enum BatchId
    {
        kDrawChar, // Only used to draw console chars
        kDrawPics, // Used by DrawPic, DrawStretchPic, etc

        // Number of items in the enum - not a valid id.
        kCount,
    };

    SpriteBatch() = default;
    void Init(int max_verts);

    void BeginFrame();
    void EndFrame();

    DrawVertex2D * Increment(const int count) { return nullptr; } // TODO

    void PushTriVerts(const DrawVertex2D tri[3]);
    void PushQuadVerts(const DrawVertex2D quad[4]);

    void PushQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  const DirectX::XMFLOAT4A & color);

    void PushQuadTextured(float x, float y, float w, float h,
                          const TextureImage * tex, const DirectX::XMFLOAT4A & color);

    void PushQuadTexturedUVs(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                             const TextureImage * tex, const DirectX::XMFLOAT4A & color);

    // Disallow copy.
    SpriteBatch(const SpriteBatch &) = delete;
    SpriteBatch & operator=(const SpriteBatch &) = delete;

private:

    struct DeferredTexQuad
    {
        int quad_start_vtx;
        const TextureImageImpl * tex;
    };
    std::vector<DeferredTexQuad> m_deferred_textured_quads;
};

using SpriteBatchSet = std::array<SpriteBatch, SpriteBatch::kCount>;

/*
===============================================================================

    D3D12 Renderer

===============================================================================
*/
class Renderer final
{
public:

    static const DirectX::XMFLOAT4A kClearColor; // Color used to wipe the screen at the start of a frame
    static const DirectX::XMFLOAT4A kColorWhite; // Alpha=1
    static const DirectX::XMFLOAT4A kColorBlack; // Alpha=1
    static const DirectX::XMFLOAT4A kFloat4Zero; // All zeros
    static const DirectX::XMFLOAT4A kFloat4One;  // All ones

    // Convenience getters
    static SpriteBatch       * SBatch(SpriteBatchIdx id) { return &sm_state->m_sprite_batches[size_t(id)]; }
    static TextureStoreImpl  * TexStore()                { return &sm_state->m_tex_store;                  }
    static ModelStoreImpl    * MdlStore()                { return &sm_state->m_mdl_store;                  }
    static ViewDrawStateImpl * ViewState()               { return &sm_state->m_view_draw_state;            }
    static bool                DebugValidation()         { return sm_state->m_window.debug_validation;     }
    static bool                FrameStarted()            { return sm_state->m_frame_started;               }
    static int                 Width()                   { return sm_state->m_window.width;                }
    static int                 Height()                  { return sm_state->m_window.height;               }

    static void Init(HINSTANCE hinst, WNDPROC wndproc, int width, int height, bool fullscreen, bool debug_validation);
    static void Shutdown();

    static void RenderView(const refdef_s & view_def);
    static void BeginFrame();
    static void EndFrame();
    static void Flush2D();

    static void EnableDepthTest();
    static void DisableDepthTest();

    static void EnableDepthWrites();
    static void DisableDepthWrites();

    static void EnableAlphaBlending();
    static void DisableAlphaBlending();

    static void UploadTexture(const TextureImage * tex);

    //
    // Debug frame annotations/makers
    //
    #if REFD3D12_WITH_DEBUG_FRAME_EVENTS
    static void InitDebugEvents();
    static void PushEventF(const wchar_t * format, ...);
    static void PushEvent(const wchar_t * event_name)   { } // TODO
    static void PopEvent()                              { } // TODO
    #else // REFD3D12_WITH_DEBUG_FRAME_EVENTS
    static void InitDebugEvents()                {}
    static void PushEventF(const wchar_t *, ...) {}
    static void PushEvent(const wchar_t *)       {}
    static void PopEvent()                       {}
    #endif // REFD3D12_WITH_DEBUG_FRAME_EVENTS

private:

    struct ConstantBufferDataUIVS
    {
        DirectX::XMFLOAT4A screen_dimensions;
    };

    struct ConstantBufferDataSGeomVS
    {
        DirectX::XMMATRIX mvp_matrix;
    };

    struct ConstantBufferDataSGeomPS
    {
        DirectX::XMFLOAT4A texture_color_scaling; // Multiplied with texture color
        DirectX::XMFLOAT4A vertex_color_scaling;  // Multiplied with vertex color
    };

    static void RenderViewUpdateCBuffers(const ViewDrawStateImpl::FrameData& frame_data);

private:

    struct State
    {
        // Renderer main data:
        RenderWindow      m_window;
        SpriteBatchSet    m_sprite_batches;
        TextureStoreImpl  m_tex_store;
        ModelStoreImpl    m_mdl_store{ m_tex_store };
        ViewDrawStateImpl m_view_draw_state;
        bool              m_frame_started  = false;
        bool              m_window_resized = true;

        // Cached Cvars:
        CvarWrapper       m_disable_texturing;
        CvarWrapper       m_blend_debug_color;
    };
    static State * sm_state;
};

} // D3D12
} // MrQ2
