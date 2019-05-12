//
// Renderer.hpp
//  D3D11 renderer interface for Quake2.
//
#pragma once

#include "RenderWindow.hpp"
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
    #define REFD3D11_WITH_DEBUG_FRAME_EVENTS 1
#else // NDEBUG
    #define REFD3D11_WITH_DEBUG_FRAME_EVENTS 0
#endif // NDEBUG

namespace MrQ2
{
namespace D3D11
{

/*
===============================================================================

    Typedefs / Global Constants

===============================================================================
*/

// Toggle the use of multiple/single buffer(s) for the geometry batches.
constexpr int kNumViewDrawVertexBuffers    = 2;
constexpr int kNumSpriteBatchVertexBuffers = 2;

// Input element desc array + count
using InputLayoutDesc = std::tuple<const D3D11_INPUT_ELEMENT_DESC *, int>;

/*
===============================================================================

    D3D11 ShaderProgram

===============================================================================
*/
class ShaderProgram final
{
public:

    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader>  ps;
    ComPtr<ID3D11InputLayout>  vertex_layout;

    void LoadFromFxFile(const wchar_t * filename, const char * vs_entry,
                        const char * ps_entry, const InputLayoutDesc & layout);

    void CreateVertexLayout(const D3D11_INPUT_ELEMENT_DESC * desc,
                            int num_elements, ID3DBlob & vs_blob);

    ShaderProgram() = default;

    // Disallow copy.
    ShaderProgram(const ShaderProgram &) = delete;
    ShaderProgram & operator=(const ShaderProgram &) = delete;
};

/*
===============================================================================

    D3D11 DepthStateHelper

===============================================================================
*/
class DepthStateHelper final
{
public:

    ComPtr<ID3D11DepthStencilState> enabled_state;
    ComPtr<ID3D11DepthStencilState> disabled_state;

    void Init(bool enabled_ztest,
              D3D11_COMPARISON_FUNC enabled_func,
              D3D11_DEPTH_WRITE_MASK enabled_write_mask,
              bool disabled_ztest,
              D3D11_COMPARISON_FUNC disabled_func,
              D3D11_DEPTH_WRITE_MASK disabled_write_mask);
};

/*
===============================================================================

    D3D11 VertexBuffersHelper

===============================================================================
*/
template<typename VertexType, int kNumBuffers>
class VertexBuffersHelper final
{
public:

    VertexBuffersHelper() = default;

    struct DrawBuffer
    {
        ID3D11Buffer * buffer_ptr;
        int used_verts;
    };

    void Init(const char * const debug_name, const int max_verts, ID3D11Device * device, ID3D11DeviceContext * context)
    {
        FASTASSERT(device != nullptr && context != nullptr);

        m_num_verts  = max_verts;
        m_debug_name = debug_name;
        m_context    = context;

        D3D11_BUFFER_DESC bd = {};
        bd.Usage             = D3D11_USAGE_DYNAMIC;
        bd.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
        bd.ByteWidth         = sizeof(VertexType) * max_verts;
        bd.BindFlags         = D3D11_BIND_VERTEX_BUFFER;

        for (unsigned b = 0; b < m_vertex_buffers.size(); ++b)
        {
            if (FAILED(device->CreateBuffer(&bd, nullptr, m_vertex_buffers[b].GetAddressOf())))
            {
                GameInterface::Errorf("Failed to create %s vertex buffer %u", debug_name, b);
            }
            m_mapped_ptrs[b] = nullptr;
        }

        MemTagsTrackAlloc(bd.ByteWidth * kNumBuffers, MemTag::kVertIndexBuffer);
        GameInterface::Printf("%s used %s", debug_name, FormatMemoryUnit(bd.ByteWidth * kNumBuffers));
    }

    VertexType * Increment(const int count)
    {
        FASTASSERT(count > 0 && count <= m_num_verts);

        VertexType * verts = m_mapped_ptrs[m_buffer_index];
        FASTASSERT(verts != nullptr);
        FASTASSERT_ALIGN16(verts);

        verts += m_used_verts;
        m_used_verts += count;

        if (m_used_verts > m_num_verts)
        {
            GameInterface::Errorf("%s vertex buffer overflowed! used_verts=%i, num_verts=%i. "
                                  "Increase size.", m_debug_name, m_used_verts, m_num_verts);
        }

        return verts;
    }

    int BufferSize() const 
    {
        return m_num_verts;
    }

    int NumVertsRemaining() const
    {
        FASTASSERT((m_num_verts - m_used_verts) > 0);
        return m_num_verts - m_used_verts;
    }

    int CurrentPosition() const
    {
        return m_used_verts;
    }

    VertexType * CurrentVertexPtr() const
    {
        return m_mapped_ptrs[m_buffer_index] + m_used_verts;
    }

    void Begin()
    {
        FASTASSERT(m_used_verts == 0); // Missing End()?

        // Map the current buffer:
        D3D11_MAPPED_SUBRESOURCE mapping_info = {};
        if (FAILED(m_context->Map(m_vertex_buffers[m_buffer_index].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping_info)))
        {
            GameInterface::Errorf("Failed to map %s vertex buffer %i", m_debug_name, m_buffer_index);
        }

        FASTASSERT(mapping_info.pData != nullptr);
        FASTASSERT_ALIGN16(mapping_info.pData);

        m_mapped_ptrs[m_buffer_index] = static_cast<VertexType *>(mapping_info.pData);
    }

    DrawBuffer End()
    {
        FASTASSERT(m_mapped_ptrs[m_buffer_index] != nullptr); // Missing Begin()?

        ID3D11Buffer * const current_buffer = m_vertex_buffers[m_buffer_index].Get();
        const int current_position = m_used_verts;

        // Unmap current buffer so we can draw with it:
        m_context->Unmap(current_buffer, 0);
        m_mapped_ptrs[m_buffer_index] = nullptr;

        // Move to the next buffer:
        m_buffer_index = (m_buffer_index + 1) % kNumBuffers;
        m_used_verts = 0;

        return { current_buffer, current_position };
    }

private:

    int m_num_verts    = 0;
    int m_used_verts   = 0;
    int m_buffer_index = 0;

    ID3D11DeviceContext * m_context    = nullptr;
    const char *          m_debug_name = nullptr;

    std::array<ComPtr<ID3D11Buffer>, kNumBuffers> m_vertex_buffers{};
    std::array<VertexType *,         kNumBuffers> m_mapped_ptrs{};
};

/*
===============================================================================

    D3D11 TextureImageImpl

===============================================================================
*/
class TextureImageImpl final
    : public TextureImage
{
public:
    using TextureImage::TextureImage;

    ComPtr<ID3D11Texture2D>          tex_resource;
    ComPtr<ID3D11SamplerState>       sampler;
    ComPtr<ID3D11ShaderResourceView> srv;

    void InitD3DSpecific();
    void InitFromScrap(const TextureImageImpl * scrap_tex);

    static D3D11_FILTER FilterForTextureType(TextureType tt);
};

/*
===============================================================================

    D3D11 TextureStoreImpl

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

    unsigned MultisampleQualityLevel(DXGI_FORMAT fmt) const;
    const TextureImageImpl * ScrapImpl() const { return static_cast<const TextureImageImpl *>(tex_scrap); }

protected:

    /*virtual*/ TextureImage * CreateScrap(int size, const ColorRGBA32 * pix) override;
    /*virtual*/ TextureImage * CreateTexture(const ColorRGBA32 * pix, std::uint32_t regn, TextureType tt, bool use_scrap,
                                             int w, int h, Vec2u16 scrap0, Vec2u16 scrap1, const char * name) override;
    /*virtual*/ void DestroyTexture(TextureImage * tex) override;

private:

    Pool<TextureImageImpl, kTexturePoolSize> m_teximages_pool;
    unsigned m_multisample_quality_levels_rgba = 0;
    bool m_scrap_dirty = false;
};

/*
===============================================================================

    D3D11 ModelInstanceImpl

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

    D3D11 ModelStoreImpl

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

    D3D11 ViewDrawStateImpl

===============================================================================
*/
class ViewDrawStateImpl final
    : public ViewDrawState
{
public:

    ViewDrawStateImpl() = default;
    virtual ~ViewDrawStateImpl() { DeleteObject(m_draw_cmds, MemTag::kRenderer); }

    void Init(int max_verts, const ShaderProgram & sp, ID3D11Buffer * cbuff_vs, ID3D11Buffer * cbuff_ps);
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
    };

    using DrawCmdList = FixedSizeArray<DrawCmd, 2048>;

    DrawCmd m_current_draw_cmd = {};
    DrawCmdList * m_draw_cmds  = nullptr;
    VertexBuffersHelper<DrawVertex3D, kNumViewDrawVertexBuffers> m_buffers;

    // Refs are owned by the parent Renderer.
    DirectX::XMMATRIX     m_viewproj_mtx = {};
    const ShaderProgram * m_program      = nullptr;
    ID3D11Buffer        * m_cbuffer_vs   = nullptr;
    ID3D11Buffer        * m_cbuffer_ps   = nullptr;
    bool                  m_batch_open   = false;
};

/*
===============================================================================

    D3D11 SpriteBatch

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
    void EndFrame(const ShaderProgram & program, const TextureImageImpl * tex, ID3D11Buffer * cbuffer);

    DrawVertex2D * Increment(const int count) { return m_buffers.Increment(count); }

    void PushTriVerts(const DrawVertex2D tri[3]);
    void PushQuadVerts(const DrawVertex2D quad[4]);

    void PushQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  const DirectX::XMFLOAT4A & color);

    void PushQuadTextured(float x, float y, float w, float h,
                          const TextureImageImpl * tex, const DirectX::XMFLOAT4A & color);

    void PushQuadTexturedUVs(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                             const TextureImageImpl * tex, const DirectX::XMFLOAT4A & color);

    // Disallow copy.
    SpriteBatch(const SpriteBatch &) = delete;
    SpriteBatch & operator=(const SpriteBatch &) = delete;

private:

    VertexBuffersHelper<DrawVertex2D, kNumSpriteBatchVertexBuffers> m_buffers;

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

    D3D11 Renderer

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
    SpriteBatch            * SBatch(SpriteBatch::BatchId id) { return &m_sprite_batches[id];              }
    TextureStoreImpl       * TexStore()                      { return &m_tex_store;                       }
    ModelStoreImpl         * MdlStore()                      { return &m_mdl_store;                       }
    ViewDrawStateImpl      * ViewState()                     { return &m_view_draw_state;                 }
    ID3D11Device           * Device()          const         { return m_window.device.Get();              }
    ID3D11DeviceContext    * DeviceContext()   const         { return m_window.device_context.Get();      }
    IDXGISwapChain         * SwapChain()       const         { return m_window.swap_chain.Get();          }
    ID3D11Texture2D        * FramebufferTex()  const         { return m_window.framebuffer_texture.Get(); }
    ID3D11RenderTargetView * FramebufferRTV()  const         { return m_window.framebuffer_rtv.Get();     }
    bool                     DebugValidation() const         { return m_window.debug_validation;          }
    bool                     FrameStarted()    const         { return m_frame_started;                    }
    int                      Width()           const         { return m_window.width;                     }
    int                      Height()          const         { return m_window.height;                    }

    Renderer();
    ~Renderer();

    void Init(const char * window_name, HINSTANCE hinst, WNDPROC wndproc,
              int width, int height, bool fullscreen, bool debug_validation);

    void RenderView(const refdef_s & view_def);
    void BeginFrame();
    void EndFrame();
    void Flush2D();

    void EnableDepthTest();
    void DisableDepthTest();

    void EnableDepthWrites();
    void DisableDepthWrites();

    void EnableAlphaBlending();
    void DisableAlphaBlending();

    void DrawHelper(unsigned num_verts, unsigned first_vert, const ShaderProgram & program,
                    ID3D11Buffer * const vb, D3D11_PRIMITIVE_TOPOLOGY topology,
                    unsigned offset, unsigned stride);

    void CompileShaderFromFile(const wchar_t * filename, const char * entry_point,
                               const char * shader_model, ID3DBlob ** out_blob) const;

    void UploadTexture(const TextureImageImpl * tex);

    //
    // Debug frame annotations/makers
    //
    #if REFD3D11_WITH_DEBUG_FRAME_EVENTS
    void InitDebugEvents();
    void PushEventF(const wchar_t * format, ...);
    void PushEvent(const wchar_t * event_name)   { if (m_annotations) m_annotations->BeginEvent(event_name); }
    void PopEvent()                              { if (m_annotations) m_annotations->EndEvent(); }
    #else // REFD3D11_WITH_DEBUG_FRAME_EVENTS
    static void InitDebugEvents()                {}
    static void PushEventF(const wchar_t *, ...) {}
    static void PushEvent(const wchar_t *)       {}
    static void PopEvent()                       {}
    #endif // REFD3D11_WITH_DEBUG_FRAME_EVENTS

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

    void CreateRSObjects();
    void LoadShaders();
    void RenderViewUpdateCBuffers(const ViewDrawStateImpl::FrameData& frame_data);

private:

    // Renderer main data:
    RenderWindow                       m_window;
    SpriteBatchSet                     m_sprite_batches;
    TextureStoreImpl                   m_tex_store;
    ModelStoreImpl                     m_mdl_store;
    ViewDrawStateImpl                  m_view_draw_state;
    ComPtr<ID3DUserDefinedAnnotation>  m_annotations;
    bool                               m_frame_started  = false;
    bool                               m_window_resized = true;

    // Shader programs / render states:
    ShaderProgram                      m_shader_ui_sprites;
    ShaderProgram                      m_shader_geometry;
    ComPtr<ID3D11BlendState>           m_blend_state_alpha;
    DepthStateHelper                   m_depth_test_states;
    DepthStateHelper                   m_depth_write_states;
    ComPtr<ID3D11Buffer>               m_cbuffer_ui_sprites;
    ComPtr<ID3D11Buffer>               m_cbuffer_geometry_vs;
    ComPtr<ID3D11Buffer>               m_cbuffer_geometry_ps;

    // Cached Cvars:
    CvarWrapper                        m_disable_texturing;
    CvarWrapper                        m_blend_debug_color;
};

// ============================================================================

// Global Renderer instance:
extern Renderer * g_Renderer;
void CreateRendererInstance();
void DestroyRendererInstance();

// ============================================================================

} // D3D11
} // MrQ2
