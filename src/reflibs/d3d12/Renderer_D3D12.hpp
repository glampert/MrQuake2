//
// Renderer_D3D12.hpp
//  D3D12 renderer interface for Quake2.
//
#pragma once

#include "Impl_D3D12.hpp"

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

    D3D12 Renderer

===============================================================================
*/
class Renderer final
{
public:

    static const DirectX::XMFLOAT4A kClearColor; // Color used to wipe the screen at the start of a frame
    static const DirectX::XMFLOAT4A kFloat4Zero; // All zeros
    static const DirectX::XMFLOAT4A kFloat4One;  // All ones

    // Convenience getters
    static SpriteBatch       * SBatch(SpriteBatchIdx id) { return &sm_state->m_sprite_batches[size_t(id)];               }
    static TextureStoreImpl  * TexStore()                { return &sm_state->m_tex_store;                                }
    static ModelStoreImpl    * MdlStore()                { return &sm_state->m_mdl_store;                                }
    static ViewDrawStateImpl * ViewState()               { return &sm_state->m_view_draw_state;                          }
    static ID3D12Device5     * Device()                  { return sm_state->m_window.device_helper.device.Get();         }
    static IDXGISwapChain4   * SwapChain()               { return sm_state->m_window.swap_chain_helper.swap_chain.Get(); }
    static bool                DebugValidation()         { return sm_state->m_window.debug_validation;                   }
    static bool                FrameStarted()            { return sm_state->m_frame_started;                             }
    static int                 Width()                   { return sm_state->m_window.width;                              }
    static int                 Height()                  { return sm_state->m_window.height;                             }

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
    static void PushEvent(const wchar_t * event_name) { } // TODO
    static void PopEvent()                            { } // TODO
    #else // REFD3D12_WITH_DEBUG_FRAME_EVENTS
    static void InitDebugEvents()                {}
    static void PushEventF(const wchar_t *, ...) {}
    static void PushEvent(const wchar_t *)       {}
    static void PopEvent()                       {}
    #endif // REFD3D12_WITH_DEBUG_FRAME_EVENTS

private:

    using SpriteBatchSet = std::array<SpriteBatch, SpriteBatch::kCount>;

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

    static void LoadShaders();
    static void CreatePipelineStates();
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

        // Shader programs / render states:
        ShaderProgram     m_shader_ui_sprites;
        ShaderProgram     m_shader_geometry;

        // Cached Cvars:
        CvarWrapper       m_disable_texturing;
        CvarWrapper       m_blend_debug_color;
    };
    static State * sm_state;
};

} // D3D12
} // MrQ2
