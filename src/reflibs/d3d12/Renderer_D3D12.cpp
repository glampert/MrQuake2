//
// Renderer_D3D12.cpp
//  D3D12 renderer interface for Quake2.
//

#include "Renderer_D3D12.hpp"
#include <dxgidebug.h>

// Debug markers need these.
#include <cstdarg>
#include <cstddef>
#include <cwchar>

// Path from the project root where to find shaders for this renderer (wchar_t)
#define REFD3D12_SHADER_PATH_WIDE L"src\\reflibs\\d3d11\\shaders\\"
//*** TODO - share these with the dx11 renderer??? put the in the RefShared proj i guess so? ***

namespace MrQ2
{
namespace D3D12
{

///////////////////////////////////////////////////////////////////////////////
// Renderer
///////////////////////////////////////////////////////////////////////////////

const DirectX::XMFLOAT4A Renderer::kClearColor{ 0.0f, 0.5f, 0.0f, 1.0f };
const DirectX::XMFLOAT4A Renderer::kFloat4Zero{ 0.0f, 0.0f, 0.0f, 0.0f };
const DirectX::XMFLOAT4A Renderer::kFloat4One { 1.0f, 1.0f, 1.0f, 1.0f };

Renderer::State * Renderer::sm_state = nullptr;

///////////////////////////////////////////////////////////////////////////////

void Renderer::Init(HINSTANCE hinst, WNDPROC wndproc, const int width, const int height, const bool fullscreen, const bool debug_validation)
{
    if (sm_state != nullptr)
    {
        GameInterface::Errorf("D3D12 Renderer is already initialized!");
    }

    GameInterface::Printf("D3D12 Renderer initializing.");

    sm_state = new(MemTag::kRenderer) State{};

    sm_state->m_disable_texturing = GameInterface::Cvar::Get("r_disable_texturing", "0", 0);
    sm_state->m_blend_debug_color = GameInterface::Cvar::Get("r_blend_debug_color", "0", 0);

    // RenderWindow setup
    sm_state->m_window.Init("MrQuake2 (D3D12)", hinst, wndproc, width, height, fullscreen, debug_validation);

    // 2D sprite/UI batch setup
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawChar)].Init(Device(), 6 * 5000); // 6 verts per quad (expand to 2 triangles each)
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawPics)].Init(Device(), 6 * 128);

    // Initialize the stores/caches
    sm_state->m_tex_store.Init();
    sm_state->m_mdl_store.Init();

    LoadShaders();

    // World geometry rendering helper
    constexpr int kViewDrawBatchSize = 25000; // size in vertices
    sm_state->m_view_draw_state.Init(kViewDrawBatchSize);

    // So we can annotate our RenderDoc captures
    InitDebugEvents();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::Shutdown()
{
    GameInterface::Printf("D3D12 Renderer shutting down.");

    const bool debug_check_live_objects = DebugValidation();

    DeleteObject(sm_state, MemTag::kRenderer);
    sm_state = nullptr;

    // At this point there should be no D3D objects left.
    if (debug_check_live_objects)
    {
        ComPtr<IDXGIDebug1> debug_interface;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug_interface))))
        {
            debug_interface->ReportLiveObjects(DXGI_DEBUG_ALL, (DXGI_DEBUG_RLO_FLAGS)(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::LoadShaders()
{
    GameInterface::Printf("CWD......: %s", OSWindow::CurrentWorkingDir().c_str());
    GameInterface::Printf("GameDir..: %s", GameInterface::FS::GameDir());

    {
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
        heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_desc.NumDescriptors = 1;
        heap_desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        if (FAILED(Device()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&sm_state->m_srv_descriptor_heap))))
        {
            GameInterface::Errorf("Failed to create SRV descriptor heap!");
        }
    }

    // UI/2D sprites:
    {
        sm_state->m_shader_ui_sprites.LoadFromFxFile(REFD3D12_SHADER_PATH_WIDE L"UISprites2D.fx", "VS_main", "PS_main", DebugValidation());

        static const D3D12_INPUT_ELEMENT_DESC s_layout[] = { // DrawVertex2D
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex2D, xy_uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex2D, rgba),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };
        //const int num_elements = ARRAYSIZE(s_layout);

        //TODO PSO

        /*
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
        */
    }

    // Common 3D geometry:
    {
        sm_state->m_shader_geometry.LoadFromFxFile(REFD3D12_SHADER_PATH_WIDE L"GeometryCommon.fx", "VS_main", "PS_main", DebugValidation());

        static const D3D12_INPUT_ELEMENT_DESC s_layout[] = { // DrawVertex3D
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(DrawVertex3D, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(DrawVertex3D, uv),       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex3D, rgba),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };
        //const int num_elements = ARRAYSIZE(s_layout);

        //TODO PSO

        /*
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
        */
    }

    GameInterface::Printf("Shaders loaded successfully.");
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::CreatePipelineStates()
{
    /*
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {}; // TODO

    if (FAILED(Renderer::Device()->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso))))
    {
        GameInterface::Errorf("Failed to create graphics pipeline state!");
    }
	*/
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

void Renderer::RenderViewUpdateCBuffers(const ViewDrawStateImpl::FrameData& frame_data)
{
    FASTASSERT_ALIGN16(frame_data.view_proj_matrix.floats);

    // TODO

    ConstantBufferDataSGeomVS cbuffer_data_geometry_vs;
    cbuffer_data_geometry_vs.mvp_matrix = DirectX::XMMATRIX{ frame_data.view_proj_matrix.floats };
//    DeviceContext()->UpdateSubresource(sm_state->m_cbuffer_geometry_vs.Get(), 0, nullptr, &cbuffer_data_geometry_vs, 0, 0);

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
//    DeviceContext()->UpdateSubresource(sm_state->m_cbuffer_geometry_ps.Get(), 0, nullptr, &cbuffer_data_geometry_ps, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EnableDepthTest()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::DisableDepthTest()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EnableDepthWrites()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::DisableDepthWrites()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EnableAlphaBlending()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::DisableAlphaBlending()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::BeginFrame()
{
    PushEvent(L"Renderer::BeginFrame");
    sm_state->m_frame_started = true;

    const auto frame_index = sm_state->m_window.swap_chain_helper.frame_index;
    const auto back_buffer_index = sm_state->m_window.swap_chain_helper.swap_chain->GetCurrentBackBufferIndex();

    ID3D12CommandAllocator * cmd_allocator = sm_state->m_window.swap_chain_helper.command_allocators[frame_index].Get();
    ID3D12GraphicsCommandList * gfx_cmd_list = sm_state->m_window.swap_chain_helper.gfx_command_list.Get();

    auto back_buffer_rtv = sm_state->m_window.render_targets.render_target_descriptors[back_buffer_index];
    ID3D12Resource * back_buffer_resource = sm_state->m_window.render_targets.render_rarget_resources[back_buffer_index].Get();

    // Set back buffer to render target
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = back_buffer_resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    gfx_cmd_list->Reset(cmd_allocator, nullptr);
    gfx_cmd_list->ResourceBarrier(1, &barrier);
    gfx_cmd_list->ClearRenderTargetView(back_buffer_rtv, reinterpret_cast<const float *>(&kClearColor), 0, nullptr);
    gfx_cmd_list->OMSetRenderTargets(1, &back_buffer_rtv, false, nullptr);
    gfx_cmd_list->SetDescriptorHeaps(1, sm_state->m_srv_descriptor_heap.GetAddressOf());

    //sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawChar)].BeginFrame();
    //sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawPics)].BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EndFrame()
{
    Flush2D();

    ID3D12GraphicsCommandList * gfx_cmd_list = sm_state->m_window.swap_chain_helper.gfx_command_list.Get();
    ID3D12CommandList * cmd_lists[] = { gfx_cmd_list };

    const auto back_buffer_index = sm_state->m_window.swap_chain_helper.swap_chain->GetCurrentBackBufferIndex();
    ID3D12Resource * back_buffer_resource = sm_state->m_window.render_targets.render_rarget_resources[back_buffer_index].Get();

    // Set back buffer to present
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = back_buffer_resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    gfx_cmd_list->ResourceBarrier(1, &barrier);
    gfx_cmd_list->Close();

    CmdQueue()->ExecuteCommandLists(1, cmd_lists);

    auto hr = SwapChain()->Present(0, 0); // Present(0, 0): without vsync; Present(1, 0): with vsync
    if (FAILED(hr))
    {
        GameInterface::Errorf("SwapChain Present failed: %s", RenderWindow::ErrorToString(hr).c_str());
    }

    sm_state->m_frame_started  = false;
    sm_state->m_window_resized = false;

    PopEvent(); // "Renderer::BeginFrame"

    sm_state->m_window.swap_chain_helper.MoveToNextFrame();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::Flush2D()
{
    PushEvent(L"Renderer::Flush2D");

    // TODO

    PopEvent(); // "Renderer::Flush2D"
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::UploadTexture(const TextureImage * const tex)
{
    FASTASSERT(tex != nullptr);
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

#if REFD3D12_WITH_DEBUG_FRAME_EVENTS
void Renderer::InitDebugEvents()
{
    auto r_debug_frame_events = GameInterface::Cvar::Get("r_debug_frame_events", "0", CvarWrapper::kFlagArchive);
    if (r_debug_frame_events.IsSet())
    {
        // TODO
    }
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::PushEventF(const wchar_t * format, ...)
{
    // TODO
    (void)format;
}
#endif // REFD3D12_WITH_DEBUG_FRAME_EVENTS

///////////////////////////////////////////////////////////////////////////////

} // D3D12
} // MrQ2
