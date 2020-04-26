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
// Renderer
///////////////////////////////////////////////////////////////////////////////

const DirectX::XMFLOAT4A Renderer::kClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
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
    sm_state->m_window.Init("MrQuake2 (D3D11)", hinst, wndproc, width, height, fullscreen, debug_validation);

    // 2D sprite/UI batch setup
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawChar)].Init(6 * 6000); // 6 verts per quad (expand to 2 triangles each)
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
    sm_state->m_depth_test_states.Init(Device(),
        true,  D3D11_COMPARISON_LESS,   D3D11_DEPTH_WRITE_MASK_ALL,   // When ON
        false, D3D11_COMPARISON_ALWAYS, D3D11_DEPTH_WRITE_MASK_ALL);  // When OFF

    sm_state->m_depth_write_states.Init(Device(),
        true,  D3D11_COMPARISON_LESS,   D3D11_DEPTH_WRITE_MASK_ALL,   // When ON
        true,  D3D11_COMPARISON_ALWAYS, D3D11_DEPTH_WRITE_MASK_ZERO); // When OFF
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::LoadShaders()
{
    GameInterface::Printf("CWD......: %s", Win32Window::CurrentWorkingDir().c_str());
    GameInterface::Printf("GameDir..: %s", GameInterface::FS::GameDir());

    // UI/2D sprites:
    {
        const D3D11_INPUT_ELEMENT_DESC layout[] = { // DrawVertex2D
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex2D, xy_uv), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex2D, rgba),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        const int num_elements = ARRAYSIZE(layout);
        sm_state->m_shader_ui_sprites.LoadFromFxFile(Device(), REFD3D11_SHADER_PATH_WIDE L"UISprites2D.fx",
                                                     "VS_main", "PS_main", { layout, num_elements }, DebugValidation());

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
        sm_state->m_shader_geometry.LoadFromFxFile(Device(), REFD3D11_SHADER_PATH_WIDE L"GeometryCommon.fx",
                                                   "VS_main", "PS_main", { layout, num_elements }, DebugValidation());

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

    auto hr = SwapChain()->Present(0, 0);
    if (FAILED(hr))
    {
        GameInterface::Errorf("SwapChain Present failed: %s", RenderWindow::ErrorToString(hr).c_str());
    }

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
        cbuffer_data_ui.screen_dimensions.x = static_cast<float>(Width());
        cbuffer_data_ui.screen_dimensions.y = static_cast<float>(Height());
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

///////////////////////////////////////////////////////////////////////////////

} // D3D11
} // MrQ2
