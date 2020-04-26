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
#define REFD3D12_SHADER_PATH_WIDE L"src\\reflibs\\d3d12\\shaders\\"

namespace MrQ2
{
namespace D3D12
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
        GameInterface::Errorf("D3D12 Renderer is already initialized!");
    }

    GameInterface::Printf("D3D12 Renderer initializing.");

    sm_state = new(MemTag::kRenderer) State{};

    sm_state->m_disable_texturing = GameInterface::Cvar::Get("r_disable_texturing", "0", 0);
    sm_state->m_blend_debug_color = GameInterface::Cvar::Get("r_blend_debug_color", "0", 0);

    // RenderWindow setup
    sm_state->m_window.Init("MrQuake2 (D3D12)", hinst, wndproc, width, height, fullscreen, debug_validation);

    sm_state->m_upload_ctx.Init(Device());
    sm_state->m_srv_descriptor_heap.Init(Device(), TextureStore::kTexturePoolSize);

    // 2D sprite/UI batch setup
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawChar)].Init(Device(), 6 * 6000); // 6 verts per quad (expand to 2 triangles each)
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
    sm_state->m_window.FullGpuSynch();

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
    GameInterface::Printf("CWD......: %s", Win32Window::CurrentWorkingDir().c_str());
    GameInterface::Printf("GameDir..: %s", GameInterface::FS::GameDir());

    // UI/2D sprites:
    {
        ShaderProgram & sp = sm_state->m_shader_ui_sprites;
        sp.LoadFromFxFile(REFD3D12_SHADER_PATH_WIDE L"UISprites2D.fx", "VS_main", "PS_main", DebugValidation());

        D3D12_DESCRIPTOR_RANGE desc_range = {};
        desc_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        desc_range.NumDescriptors = 1;
        desc_range.BaseShaderRegister = 0;
        desc_range.RegisterSpace = 0;
        desc_range.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER param[2] = {};

        param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param[0].Constants.ShaderRegister = 0;
        param[0].Constants.RegisterSpace = 0;
        param[0].Constants.Num32BitValues = 2; // screen_dimensions (float2)
        param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param[1].DescriptorTable.NumDescriptorRanges = 1;
        param[1].DescriptorTable.pDescriptorRanges = &desc_range;
        param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC static_sampler = {};
        static_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        static_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        static_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        static_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        static_sampler.MipLODBias = 0.0f;
        static_sampler.MaxAnisotropy = 0;
        static_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        static_sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        static_sampler.MinLOD = 0.0f;
        static_sampler.MaxLOD = 0.0f;
        static_sampler.ShaderRegister = 0;
        static_sampler.RegisterSpace = 0;
        static_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rootsig_desc = {};
        rootsig_desc.NumParameters = ArrayLength(param);
        rootsig_desc.pParameters = param;
        rootsig_desc.NumStaticSamplers = 1;
        rootsig_desc.pStaticSamplers = &static_sampler;
        rootsig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                             D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                             D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                             D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        sp.CreateRootSignature(Device(), rootsig_desc);

        const D3D12_INPUT_ELEMENT_DESC input_layout[] = { // DrawVertex2D
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex2D, xy_uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex2D, rgba),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
        pso_desc.NodeMask = 1;
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.pRootSignature = sp.root_signature.Get(); // ROOT_SIG
        pso_desc.SampleMask = UINT_MAX;
        pso_desc.NumRenderTargets = 1;
        pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        pso_desc.SampleDesc.Count = 1;
        pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        pso_desc.InputLayout = { input_layout, ArrayLength(input_layout) };
        pso_desc.VS = { sp.shader_bytecode.vs_blob->GetBufferPointer(), sp.shader_bytecode.vs_blob->GetBufferSize() };
        pso_desc.PS = { sp.shader_bytecode.ps_blob->GetBufferPointer(), sp.shader_bytecode.ps_blob->GetBufferSize() };

        // Create the blending setup
        {
            D3D12_BLEND_DESC & desc = pso_desc.BlendState;
            desc.AlphaToCoverageEnable = false;
            desc.RenderTarget[0].BlendEnable = true;
            desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
            desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        // Create the rasterizer state
        {
            D3D12_RASTERIZER_DESC & desc = pso_desc.RasterizerState;
            desc.FillMode = D3D12_FILL_MODE_SOLID;
            desc.CullMode = D3D12_CULL_MODE_NONE;
            desc.FrontCounterClockwise = false;
            desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
            desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
            desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
            desc.DepthClipEnable = true;
            desc.MultisampleEnable = false;
            desc.AntialiasedLineEnable = false;
            desc.ForcedSampleCount = 0;
            desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        }

        // Create depth-stencil State
        {
            D3D12_DEPTH_STENCIL_DESC & desc = pso_desc.DepthStencilState;
            desc.DepthEnable = false;
            desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            desc.StencilEnable = false;
            desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            desc.BackFace = desc.FrontFace;
        }

        sm_state->m_pipeline_state_draw2d.CreatePso(Device(), pso_desc);
    }

    // Common 3D geometry:
    {
        ShaderProgram & sp = sm_state->m_shader_geometry;
        sp.LoadFromFxFile(REFD3D12_SHADER_PATH_WIDE L"GeometryCommon.fx", "VS_main", "PS_main", DebugValidation());

        D3D12_DESCRIPTOR_RANGE desc_range = {};
        desc_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        desc_range.NumDescriptors = 1;
        desc_range.BaseShaderRegister = 0;
        desc_range.RegisterSpace = 0;
        desc_range.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER param[2] = {};

        param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param[0].Constants.ShaderRegister = 0;
        param[0].Constants.RegisterSpace = 0;
        param[0].Constants.Num32BitValues = 16; // mvp_matrix (matrix)
        param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param[1].DescriptorTable.NumDescriptorRanges = 1;
        param[1].DescriptorTable.pDescriptorRanges = &desc_range;
        param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC static_sampler = {};
        static_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        static_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        static_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        static_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        static_sampler.MipLODBias = 0.0f;
        static_sampler.MaxAnisotropy = 0;
        static_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        static_sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        static_sampler.MinLOD = 0.0f;
        static_sampler.MaxLOD = 0.0f;
        static_sampler.ShaderRegister = 0;
        static_sampler.RegisterSpace = 0;
        static_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rootsig_desc = {};
        rootsig_desc.NumParameters = ArrayLength(param);
        rootsig_desc.pParameters = param;
        rootsig_desc.NumStaticSamplers = 1;
        rootsig_desc.pStaticSamplers = &static_sampler;
        rootsig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                             D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                             D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                             D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        sp.CreateRootSignature(Device(), rootsig_desc);

        const D3D12_INPUT_ELEMENT_DESC input_layout[] = { // DrawVertex3D
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(DrawVertex3D, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(DrawVertex3D, uv),       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex3D, rgba),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
        pso_desc.NodeMask = 1;
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.pRootSignature = sp.root_signature.Get(); // ROOT_SIG
        pso_desc.SampleMask = UINT_MAX;
        pso_desc.NumRenderTargets = 1;
        pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        pso_desc.SampleDesc.Count = 1;
        pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        pso_desc.InputLayout = { input_layout, ArrayLength(input_layout) };
        pso_desc.VS = { sp.shader_bytecode.vs_blob->GetBufferPointer(), sp.shader_bytecode.vs_blob->GetBufferSize() };
        pso_desc.PS = { sp.shader_bytecode.ps_blob->GetBufferPointer(), sp.shader_bytecode.ps_blob->GetBufferSize() };

        // Create the blending setup
        {
            D3D12_BLEND_DESC & desc = pso_desc.BlendState;
            desc.AlphaToCoverageEnable = false;
            desc.RenderTarget[0].BlendEnable = false;
            desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
            desc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
            desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        // Create the rasterizer state
        {
            D3D12_RASTERIZER_DESC & desc = pso_desc.RasterizerState;
            desc.FillMode = D3D12_FILL_MODE_SOLID;
            desc.CullMode = D3D12_CULL_MODE_BACK;
            desc.FrontCounterClockwise = false;
            desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
            desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
            desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
            desc.DepthClipEnable = true;
            desc.MultisampleEnable = false;
            desc.AntialiasedLineEnable = false;
            desc.ForcedSampleCount = 0;
            desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        }

        // Create depth-stencil State
        {
            D3D12_DEPTH_STENCIL_DESC & desc = pso_desc.DepthStencilState;
            desc.DepthEnable = true;
            desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
            desc.StencilEnable = false;
            desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            desc.BackFace = desc.FrontFace;
        }

        sm_state->m_pipeline_state_draw3d.CreatePso(Device(), pso_desc);

        // Same as above but enable alpha blending for translucencies
        {
            D3D12_BLEND_DESC & desc = pso_desc.BlendState;
            desc.AlphaToCoverageEnable = false;
            desc.RenderTarget[0].BlendEnable = true;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        }
        sm_state->m_pipeline_state_translucent.CreatePso(Device(), pso_desc);

        // Same as above but without z-writes
        {
            D3D12_DEPTH_STENCIL_DESC & desc = pso_desc.DepthStencilState;
            desc.DepthEnable = true;
            desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            desc.StencilEnable = false;
            desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            desc.BackFace = desc.FrontFace;
        }
        sm_state->m_pipeline_state_translucent_no_zwrite.CreatePso(Device(), pso_desc);

        sm_state->m_const_buffers.Init(Device(), sizeof(GeometryCommonShaderConstants));
    }

    GameInterface::Printf("Shaders loaded successfully.");
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderView(const refdef_s & view_def)
{
    PushEvent(L"Renderer::RenderView");

    ViewDrawStateImpl::FrameData frame_data{ sm_state->m_tex_store, *sm_state->m_mdl_store.WorldModel(), view_def };

    // Set up camera/view (fills frame_data)
    sm_state->m_view_draw_state.RenderViewSetup(frame_data);

    // Update the constant buffers for this frame
    RenderViewUpdateCBuffers(frame_data);

    // Set the camera/world-view:
    FASTASSERT_ALIGN16(frame_data.view_proj_matrix.floats);
    const auto vp_mtx = DirectX::XMMATRIX{ frame_data.view_proj_matrix.floats };
    sm_state->m_view_draw_state.SetViewProjMatrix(vp_mtx);

    ID3D12GraphicsCommandList * gfx_cmd_list = sm_state->m_window.gfx_command_list.Get();

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

    sm_state->m_view_draw_state.EndRenderPass(gfx_cmd_list, sm_state->m_pipeline_state_draw3d.pso.Get(), sm_state->m_shader_geometry);

    //
    // Transparencies/alpha pass
    //

    // Color Blend ON
    const float blend_factor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    gfx_cmd_list->OMSetBlendFactor(blend_factor);
    PushEvent(L"RenderTranslucentSurfaces");
    sm_state->m_view_draw_state.BeginRenderPass();
    sm_state->m_view_draw_state.RenderTranslucentSurfaces(frame_data);
    sm_state->m_view_draw_state.EndRenderPass(gfx_cmd_list, sm_state->m_pipeline_state_translucent.pso.Get(), sm_state->m_shader_geometry);
    PopEvent(); // "RenderTranslucentSurfaces"

    // Disable z writes in case they stack up
    PushEvent(L"RenderTranslucentEntities");
    sm_state->m_view_draw_state.BeginRenderPass();
    sm_state->m_view_draw_state.RenderTranslucentEntities(frame_data);
    sm_state->m_view_draw_state.EndRenderPass(gfx_cmd_list, sm_state->m_pipeline_state_translucent_no_zwrite.pso.Get(), sm_state->m_shader_geometry);
    PopEvent(); // "RenderTranslucentEntities"

    PopEvent(); // "Renderer::RenderView"
}

///////////////////////////////////////////////////////////////////////////////

//TODO: switch view shaders to use the CBuffer.
void Renderer::RenderViewUpdateCBuffers(const ViewDrawStateImpl::FrameData & frame_data)
{
    GeometryCommonShaderConstants cbuffer_data = {};

    FASTASSERT_ALIGN16(frame_data.view_proj_matrix.floats);
    cbuffer_data.vs.mvp_matrix = DirectX::XMMATRIX{ frame_data.view_proj_matrix.floats };

    if (sm_state->m_disable_texturing.IsSet()) // Use only debug vertex color
    {
        cbuffer_data.ps.texture_color_scaling = kFloat4Zero;
        cbuffer_data.ps.vertex_color_scaling  = kFloat4One;
    }
    else if (sm_state->m_blend_debug_color.IsSet()) // Blend debug vertex color with texture
    {
        cbuffer_data.ps.texture_color_scaling = kFloat4One;
        cbuffer_data.ps.vertex_color_scaling  = kFloat4One;
    }
    else // Normal rendering
    {
        cbuffer_data.ps.texture_color_scaling = kFloat4One;
        cbuffer_data.ps.vertex_color_scaling  = kFloat4Zero;
    }

    sm_state->m_const_buffers.GetCurrent().WriteStruct(cbuffer_data);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::BeginFrame()
{
    PushEvent(L"Renderer::BeginFrame");
    sm_state->m_frame_started = true;

    const auto frame_index = sm_state->m_window.frame_index;
    const auto back_buffer_index = sm_state->m_window.swap_chain->GetCurrentBackBufferIndex();

    ID3D12CommandAllocator * cmd_allocator = sm_state->m_window.command_allocators[frame_index].Get();
    ID3D12GraphicsCommandList * gfx_cmd_list = sm_state->m_window.gfx_command_list.Get();

    auto back_buffer_rtv = sm_state->m_window.render_target_descriptors[back_buffer_index];
    ID3D12Resource * back_buffer_resource = sm_state->m_window.render_target_resources[back_buffer_index].Get();

    // Set back buffer to render target
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = back_buffer_resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

    gfx_cmd_list->Reset(cmd_allocator, nullptr);
    gfx_cmd_list->ResourceBarrier(1, &barrier);
    gfx_cmd_list->ClearRenderTargetView(back_buffer_rtv, reinterpret_cast<const float *>(&kClearColor), 0, nullptr);

    const float depth_clear = 1.0f;
    const std::uint8_t stencil_clear = 0;
    gfx_cmd_list->ClearDepthStencilView(sm_state->m_window.depth_render_target_descriptor,
                                        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                                        depth_clear, stencil_clear, 0, nullptr);

    gfx_cmd_list->OMSetRenderTargets(1, &back_buffer_rtv, false, &sm_state->m_window.depth_render_target_descriptor);
    gfx_cmd_list->SetDescriptorHeaps(1, sm_state->m_srv_descriptor_heap.GetHeapAddr());

    // Setup viewport
    D3D12_VIEWPORT vp;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width    = Width();
    vp.Height   = Height();
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    gfx_cmd_list->RSSetViewports(1, &vp);

    RECT r;
    r.left   = 0;
    r.top    = 0;
    r.right  = Width();
    r.bottom = Height();
    gfx_cmd_list->RSSetScissorRects(1, &r);

    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawChar)].BeginFrame();
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawPics)].BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EndFrame()
{
    ID3D12GraphicsCommandList * gfx_cmd_list = sm_state->m_window.gfx_command_list.Get();
    ID3D12CommandList * cmd_lists[] = { gfx_cmd_list };

    // 2D begin
    {
        const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
        gfx_cmd_list->OMSetBlendFactor(blend_factor);

        gfx_cmd_list->SetGraphicsRootSignature(sm_state->m_shader_ui_sprites.root_signature.Get());

        const float screen_dimensions[2] = { // float2
            static_cast<float>(Width()),
            static_cast<float>(Height()),
        };

        // Slot[0] constants
        gfx_cmd_list->SetGraphicsRoot32BitConstants(0, ArrayLength(screen_dimensions), screen_dimensions, 0);

        // Misc 2D geometry
        sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawPics)].EndFrame(gfx_cmd_list, sm_state->m_pipeline_state_draw2d.pso.Get());

        // 2D text
        sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawChar)].EndFrame(gfx_cmd_list, sm_state->m_pipeline_state_draw2d.pso.Get(), static_cast<const TextureImageImpl *>(sm_state->m_tex_store.tex_conchars));
    }
    // 2D end

    const auto back_buffer_index = sm_state->m_window.swap_chain->GetCurrentBackBufferIndex();
    ID3D12Resource * back_buffer_resource = sm_state->m_window.render_target_resources[back_buffer_index].Get();

    // Set back buffer to present
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = back_buffer_resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;

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

    sm_state->m_const_buffers.MoveToNextFrame();
    sm_state->m_window.MoveToNextFrame();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::UploadTexture(const TextureImage * tex)
{
    FASTASSERT(tex != nullptr);
    sm_state->m_upload_ctx.UploadTextureSync(*static_cast<const TextureImageImpl *>(tex), Device());
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
