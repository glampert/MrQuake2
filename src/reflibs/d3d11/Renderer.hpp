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

#include <array>
#include <tuple>
#include <DirectXMath.h>

namespace MrQ2
{
namespace D3D11
{

// ============================================================================

using InputLayoutDesc = std::tuple<const D3D11_INPUT_ELEMENT_DESC *, int>;

struct Vertex2D
{
    DirectX::XMFLOAT4A xy_uv;
    DirectX::XMFLOAT4A rgba;
};

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

    D3D11 TextureImageImpl / TextureStoreImpl

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

// ============================================================================

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

    unsigned MultisampleQualityLevel(DXGI_FORMAT fmt) const;
    const TextureImageImpl * ScrapImpl() const { return static_cast<const TextureImageImpl *>(tex_scrap); }

    // TODO Scrap texture needs to be updated when something is written to it!!!
    // Also have to handle UVs correctly when drawing a pic from the scrap atlas

protected:

    /*virtual*/ TextureImage * CreateScrap(int size, const ColorRGBA32 * pix) override;
    /*virtual*/ TextureImage * CreateTexture(const ColorRGBA32 * pix, std::uint32_t regn, TextureType tt, bool use_scrap,
                                             int w, int h, Vec2u16 scrap0, Vec2u16 scrap1, const char * name) override;
    /*virtual*/ void DestroyTexture(TextureImage * tex) override;

private:

    Pool<TextureImageImpl, kTexturePoolSize> m_teximages_pool;
    unsigned m_multisample_quality_levels_rgba = 0;
};

/*
===============================================================================

    D3D11 ModelInstanceImpl / ModelStoreImpl

===============================================================================
*/
class ModelInstanceImpl final
    : public ModelInstance
{
public:
    using ModelInstance::ModelInstance;

    // TODO

    void InitD3DSpecific();
};

// ============================================================================

class ModelStoreImpl final
    : public ModelStore
{
public:

    explicit ModelStoreImpl(TextureStore & tex_store)
        : ModelStore{ tex_store }, m_models_pool{ MemTag::kRenderer }
    {
    }

    ~ModelStoreImpl()
    {
        DestroyAllLoadedModels();
    }

    void Init();

protected:

    /*virtual*/ ModelInstance * CreateModel(const char * name, ModelType mt, std::uint32_t regn) override;
    /*virtual*/ void DestroyModel(ModelInstance * mdl) override;

private:

    Pool<ModelInstanceImpl, kModelPoolSize> m_models_pool;
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
    void EndFrame(const ShaderProgram & program, const TextureImageImpl * const tex,
                  ID3D11BlendState * const blend_state, ID3D11Buffer * const cbuffer);

    Vertex2D * Increment(int count);

    void PushTriVerts(const Vertex2D tri[3]);
    void PushQuadVerts(const Vertex2D quad[4]);

    void PushQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  const DirectX::XMFLOAT4A & color);

    void PushQuadTextured(float x, float y, float w, float h,
                          const TextureImageImpl * tex, const DirectX::XMFLOAT4A & color);

    // Disallow copy.
    SpriteBatch(const SpriteBatch &) = delete;
    SpriteBatch & operator=(const SpriteBatch &) = delete;

private:

    int m_num_verts    = 0;
    int m_used_verts   = 0;
    int m_buffer_index = 0;

    std::array<ComPtr<ID3D11Buffer>,     2> m_vertex_buffers;
    std::array<D3D11_MAPPED_SUBRESOURCE, 2> m_mapping_info;

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
    static const DirectX::XMFLOAT4A kColorWhite;
    static const DirectX::XMFLOAT4A kColorBlack;

    // Convenience getters
    SpriteBatch            * SBatch(SpriteBatch::BatchId id) { return &m_sprite_batches[id];              }
    TextureStoreImpl       * TexStore()                      { return &m_tex_store;                       }
    ModelStoreImpl         * MdlStore()                      { return &m_mdl_store;                       }
    ViewDrawState          * ViewState()                     { return &m_view_draw_state;                 }
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

    void DrawHelper(unsigned num_verts, unsigned first_vert, const ShaderProgram & program,
                    ID3D11Buffer * const vb, D3D11_PRIMITIVE_TOPOLOGY topology,
                    unsigned offset, unsigned stride);

    void CompileShaderFromFile(const wchar_t * filename, const char * entry_point,
                               const char * shader_model, ID3DBlob ** out_blob) const;

    void UploadTexture(const TextureImageImpl * tex);

private:

    struct ConstantBufferDataUI
    {
        DirectX::XMFLOAT4 screen_dimensions = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    void LoadShaders();

    // Renderer main data:
    RenderWindow     m_window;
    SpriteBatchSet   m_sprite_batches;
    TextureStoreImpl m_tex_store;
    ModelStoreImpl   m_mdl_store;
    ViewDrawState    m_view_draw_state;
    bool             m_frame_started  = false;
    bool             m_window_resized = true;

    // Shader programs / render states:
    ShaderProgram            m_shader_ui_sprites;
    ComPtr<ID3D11BlendState> m_blend_state_ui_sprites;
    ComPtr<ID3D11Buffer>     m_cbuffer_ui_sprites;
    ConstantBufferDataUI     m_cbuffer_data_ui_sprites;
};

// ============================================================================

// Global Renderer instance:
extern Renderer * g_Renderer;
void CreateRendererInstance();
void DestroyRendererInstance();

// ============================================================================

} // D3D11
} // MrQ2
