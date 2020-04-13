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

    sm_state->m_upload_ctx.Init(Device());
    sm_state->m_srv_descriptor_heap.Init(Device(), TextureStore::kTexturePoolSize);

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

static ID3D10Blob * g_pVertexShaderBlob = NULL;
static ID3D10Blob * g_pPixelShaderBlob = NULL;

void Renderer::CreateDeviceObjects_Temp()
{
	// Create the root signature
	{
		D3D12_DESCRIPTOR_RANGE descRange = {};
		descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descRange.NumDescriptors = 1;
		descRange.BaseShaderRegister = 0;
		descRange.RegisterSpace = 0;
		descRange.OffsetInDescriptorsFromTableStart = 0;

		D3D12_ROOT_PARAMETER param[2] = {};

		param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		param[0].Constants.ShaderRegister = 0;
		param[0].Constants.RegisterSpace = 0;
		param[0].Constants.Num32BitValues = 16;
		param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param[1].DescriptorTable.NumDescriptorRanges = 1;
		param[1].DescriptorTable.pDescriptorRanges = &descRange;
		param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;// D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.MipLODBias = 0.f;
		staticSampler.MaxAnisotropy = 0;
		staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		staticSampler.MinLOD = 0.f;
		staticSampler.MaxLOD = 0.f;
		staticSampler.ShaderRegister = 0;
		staticSampler.RegisterSpace = 0;
		staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = ARRAYSIZE(param);
		desc.pParameters = param;
		desc.NumStaticSamplers = 1;
		desc.pStaticSamplers = &staticSampler;
		desc.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		ID3DBlob* blob = NULL;
		if (D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, NULL) != S_OK)
			return;

		Device()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&sm_state->m_root_signature));
		blob->Release();
	}

	// By using D3DCompile() from <d3dcompiler.h> / d3dcompiler.lib, we introduce a dependency to a given version of d3dcompiler_XX.dll (see D3DCOMPILER_DLL_A)
	// If you would like to use this DX12 sample code but remove this dependency you can:
	//  1) compile once, save the compiled shader blobs into a file or source code and pass them to CreateVertexShader()/CreatePixelShader() [preferred solution]
	//  2) use code to detect any version of the DLL and grab a pointer to D3DCompile from the DLL.
	// See https://github.com/ocornut/imgui/pull/638 for sources and details.

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	memset(&psoDesc, 0, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.NodeMask = 1;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.pRootSignature = sm_state->m_root_signature.Get();
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	// Create the vertex shader
	{
		static const char* vertexShader =
			"cbuffer vertexBuffer : register(b0) \
            {\
              float4x4 ProjectionMatrix; \
            };\
            struct VS_INPUT\
            {\
              float4 xy_uv : POSITION;\
              float4 rgba : COLOR;\
            };\
            \
            struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR;\
              float2 uv  : TEXCOORD;\
            };\
            \
            PS_INPUT main(VS_INPUT input)\
            {\
              PS_INPUT output;\
              output.pos = mul( ProjectionMatrix, float4(input.xy_uv.xy, 0.f, 1.f));\
              output.col = input.rgba;\
              output.uv  = input.xy_uv.zw;\
              return output;\
            }";

		D3DCompile(vertexShader, strlen(vertexShader), NULL, NULL, NULL, "main", "vs_5_0", 0, 0, &g_pVertexShaderBlob, NULL);
		if (g_pVertexShaderBlob == NULL) // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
			return;
		psoDesc.VS = { g_pVertexShaderBlob->GetBufferPointer(), g_pVertexShaderBlob->GetBufferSize() };

		// Create the input layout
		static const D3D12_INPUT_ELEMENT_DESC s_layout[] = { // DrawVertex2D
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex2D, xy_uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(DrawVertex2D, rgba),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		psoDesc.InputLayout = { s_layout, ARRAYSIZE(s_layout) };
	}

	// Create the pixel shader
	{
		static const char* pixelShader =
			"struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR;\
              float2 uv  : TEXCOORD;\
            };\
            SamplerState sampler0 : register(s0);\
            Texture2D texture0 : register(t0);\
            \
            float4 main(PS_INPUT input) : SV_Target\
            {\
              float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
				/*float4 out_col = float4(input.uv, 0.0f, 1.0f);*/ \
				/*out_col.a = 1.0f;*/ \
              return out_col; \
            }";

		D3DCompile(pixelShader, strlen(pixelShader), NULL, NULL, NULL, "main", "ps_5_0", 0, 0, &g_pPixelShaderBlob, NULL);
		if (g_pPixelShaderBlob == NULL)  // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
			return;
		psoDesc.PS = { g_pPixelShaderBlob->GetBufferPointer(), g_pPixelShaderBlob->GetBufferSize() };
	}

	// Create the blending setup
	{
		D3D12_BLEND_DESC& desc = psoDesc.BlendState;
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
		D3D12_RASTERIZER_DESC& desc = psoDesc.RasterizerState;
		desc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.CullMode = D3D12_CULL_MODE_NONE;
		desc.FrontCounterClockwise = FALSE;
		desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		desc.DepthClipEnable = true;
		desc.MultisampleEnable = FALSE;
		desc.AntialiasedLineEnable = FALSE;
		desc.ForcedSampleCount = 0;
		desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	}

	// Create depth-stencil State
	{
		D3D12_DEPTH_STENCIL_DESC& desc = psoDesc.DepthStencilState;
		desc.DepthEnable = false;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.StencilEnable = false;
		desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.BackFace = desc.FrontFace;
	}

	if (Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sm_state->m_pipeline_state_draw2d)) != S_OK)
		return;

//	CreateFontsTexture_Temp();
}

void Renderer::LoadShaders()
{
    GameInterface::Printf("CWD......: %s", OSWindow::CurrentWorkingDir().c_str());
    GameInterface::Printf("GameDir..: %s", GameInterface::FS::GameDir());

    CreateDeviceObjects_Temp();

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
	/*
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
	*/
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

    const auto frame_index = sm_state->m_window.frame_index;
    const auto back_buffer_index = sm_state->m_window.swap_chain->GetCurrentBackBufferIndex();

    ID3D12CommandAllocator * cmd_allocator = sm_state->m_window.command_allocators[frame_index].Get();
    ID3D12GraphicsCommandList * gfx_cmd_list = sm_state->m_window.gfx_command_list.Get();

    auto back_buffer_rtv = sm_state->m_window.render_target_descriptors[back_buffer_index];
    ID3D12Resource * back_buffer_resource = sm_state->m_window.render_rarget_resources[back_buffer_index].Get();

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
    gfx_cmd_list->OMSetRenderTargets(1, &back_buffer_rtv, false, nullptr);
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

    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawChar)].BeginFrame();
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawPics)].BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EndFrame()
{
    ID3D12GraphicsCommandList * gfx_cmd_list = sm_state->m_window.gfx_command_list.Get();
    ID3D12CommandList * cmd_lists[] = { gfx_cmd_list };

	//TEMP
	{
		struct VERTEX_CONSTANT_BUFFER
		{
			float   mvp[4][4];
		};

		VERTEX_CONSTANT_BUFFER vertex_constant_buffer;
		{
			float L = 0;
			float R = 0 + Width();
			float T = 0;
			float B = 0 + Height();
			float mvp[4][4] =
			{
				{ 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
				{ 0.0f,         2.0f / (T - B),     0.0f,       0.0f },
				{ 0.0f,         0.0f,           0.5f,       0.0f },
				{ (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
			};
			memcpy(&vertex_constant_buffer.mvp, mvp, sizeof(mvp));
		}

		gfx_cmd_list->SetGraphicsRootSignature(sm_state->m_root_signature.Get());

		// Slot[0] constants
		gfx_cmd_list->SetGraphicsRoot32BitConstants(0, 16, &vertex_constant_buffer, 0);

		// Slot[1] texture/sampler
//		gfx_cmd_list->SetGraphicsRootDescriptorTable(1, ((TextureImageImpl*)TexStore()->tex_conback)->srv_gpu_descriptor_handle);

		RECT r;
		r.left = 0;
		r.top = 0;
		r.right = Width();
		r.bottom = Height();
		gfx_cmd_list->RSSetScissorRects(1, &r);

		const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
		gfx_cmd_list->OMSetBlendFactor(blend_factor);
	}
	//TEMP

    // Misc 2D geometry
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawPics)].EndFrame(gfx_cmd_list, sm_state->m_pipeline_state_draw2d.Get());

    // 2D text
    sm_state->m_sprite_batches[size_t(SpriteBatchIdx::kDrawChar)].EndFrame(gfx_cmd_list, sm_state->m_pipeline_state_draw2d.Get(), static_cast<const TextureImageImpl *>(sm_state->m_tex_store.tex_conchars));

    const auto back_buffer_index = sm_state->m_window.swap_chain->GetCurrentBackBufferIndex();
    ID3D12Resource * back_buffer_resource = sm_state->m_window.render_rarget_resources[back_buffer_index].Get();

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

    sm_state->m_window.MoveToNextFrame();
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
