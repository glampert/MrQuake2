//
// Renderer.cpp
//  D3D11 renderer interface for Quake2.
//

#include "Renderer.hpp"

// Path from the project root where to find shaders for this renderer (wchar_t)
#define REFD3D11_SHADER_PATH_WIDE L"src\\reflibs\\d3d11\\shaders\\"

namespace D3D11
{

///////////////////////////////////////////////////////////////////////////////
// TextureImpl
///////////////////////////////////////////////////////////////////////////////

void TextureImpl::InitD3DSpecific()
{
    ID3D11Device* const device = g_Renderer->Device();
    const unsigned numQualityLevels = g_Renderer->TexStore()->MultisampleQualityLevel(DXGI_FORMAT_R8G8B8A8_UNORM);

    D3D11_TEXTURE2D_DESC tex2dDesc  = {};
    tex2dDesc.Usage                 = D3D11_USAGE_DEFAULT;
    tex2dDesc.BindFlags             = D3D11_BIND_SHADER_RESOURCE;
    tex2dDesc.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex2dDesc.Width                 = width;
    tex2dDesc.Height                = height;
    tex2dDesc.MipLevels             = 1;
    tex2dDesc.ArraySize             = 1;
    tex2dDesc.SampleDesc.Count      = 1;
    tex2dDesc.SampleDesc.Quality    = numQualityLevels - 1;

    D3D11_SAMPLER_DESC samplerDesc  = {};
    samplerDesc.Filter              = FilterForTextureType(type);
    samplerDesc.AddressU            = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV            = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW            = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc      = D3D11_COMPARISON_ALWAYS;
    samplerDesc.MaxAnisotropy       = 1;
    samplerDesc.MipLODBias          = 0.0f;
    samplerDesc.MinLOD              = 0.0f;
    samplerDesc.MaxLOD              = D3D11_FLOAT32_MAX;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem                = pixels;
    initData.SysMemPitch            = width * 4; // RGBA-8888

    if (FAILED(device->CreateTexture2D(&tex2dDesc, &initData, tex_resource.GetAddressOf())))
    {
        GameInterface::Errorf("CreateTexture2D failed!");
    }
    if (FAILED(device->CreateShaderResourceView(tex_resource.Get(), nullptr, srv.GetAddressOf())))
    {
        GameInterface::Errorf("CreateShaderResourceView failed!");
    }
    if (FAILED(device->CreateSamplerState(&samplerDesc, sampler.GetAddressOf())))
    {
        GameInterface::Errorf("CreateSamplerState failed!");
    }
}

///////////////////////////////////////////////////////////////////////////////

void TextureImpl::InitScrap(const TextureImpl * const scrap_tex)
{
    FASTASSERT(scrap_tex != nullptr);

    // Share the scrap texture resource(s)
    tex_resource = scrap_tex->tex_resource;
    sampler      = scrap_tex->sampler;
    srv          = scrap_tex->srv;
}

///////////////////////////////////////////////////////////////////////////////

D3D11_FILTER TextureImpl::FilterForTextureType(const TextureType tt)
{
    switch (tt)
    {
    // TODO: Maybe have a Cvar to select between different filter modes?
    // Should also generate mipmaps for the non-UI textures!!!
    case TextureType::kSkin   : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureType::kSprite : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureType::kWall   : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureType::kSky    : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureType::kPic    : return D3D11_FILTER_MIN_MAG_MIP_POINT;
    default : GameInterface::Errorf("Invalid TextureType enum!");
    } // switch
}

///////////////////////////////////////////////////////////////////////////////
// TextureStoreImpl
///////////////////////////////////////////////////////////////////////////////

void TextureStoreImpl::Init()
{
    ID3D11Device* const device = g_Renderer->Device();
    device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,
                              1, &m_multisample_quality_levels_rgba);

    // Load the default resident textures now
    TouchResidentTextures();
}

///////////////////////////////////////////////////////////////////////////////

unsigned TextureStoreImpl::MultisampleQualityLevel(const DXGI_FORMAT fmt) const
{
    FASTASSERT(fmt == DXGI_FORMAT_R8G8B8A8_UNORM); // only format support at the moment
    (void)fmt;

    return m_multisample_quality_levels_rgba;
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStoreImpl::CreateScrap(int size, const ColorRGBA32 * pix)
{
    TextureImpl * scrap_impl = m_teximages_pool.Allocate();
    Construct(scrap_impl, pix, RegistrationNum(), TextureType::kPic, /*use_scrap =*/true,
              size, size, Vec2u16{0,0}, Vec2u16{std::uint16_t(size), std::uint16_t(size)}, "pics/scrap.pcx");

    scrap_impl->InitD3DSpecific();
    return scrap_impl;
}

///////////////////////////////////////////////////////////////////////////////

TextureImage * TextureStoreImpl::CreateTexture(const ColorRGBA32 * pix, std::uint32_t regn, TextureType tt, bool use_scrap,
                                               int w, int h, Vec2u16 scrap0, Vec2u16 scrap1, const char * name)
{
    TextureImpl * impl = m_teximages_pool.Allocate();
    Construct(impl, pix, regn, tt, use_scrap, w, h, scrap0, scrap1, name);

    if (use_scrap)
    {
        impl->InitScrap(GetScrapImpl());
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
    TextureImpl * impl = static_cast<TextureImpl *>(tex);
    Destroy(impl);
    m_teximages_pool.Deallocate(impl);
}

///////////////////////////////////////////////////////////////////////////////
// ShaderProgram
///////////////////////////////////////////////////////////////////////////////

void ShaderProgram::LoadFromFxFile(const wchar_t * const filename, const char * const vs_entry,
                                   const char * const ps_entry, const InputLayoutDesc & layout)
{
    FASTASSERT(filename != nullptr && filename[0] != L'\0');
    FASTASSERT(vs_entry != nullptr && vs_entry[0] != '\0');
    FASTASSERT(ps_entry != nullptr && ps_entry[0] != '\0');

    ComPtr<ID3DBlob> vs_blob;
    g_Renderer->CompileShaderFromFile(filename, vs_entry, "vs_4_0", vs_blob.GetAddressOf());
    FASTASSERT(vs_blob != nullptr);

    ComPtr<ID3DBlob> ps_blob;
    g_Renderer->CompileShaderFromFile(filename, ps_entry, "ps_4_0", ps_blob.GetAddressOf());
    FASTASSERT(ps_blob != nullptr);

    HRESULT hr;
    ID3D11Device * const device = g_Renderer->Device();

    // Create the vertex shader:
    hr = device->CreateVertexShader(vs_blob->GetBufferPointer(),
                                    vs_blob->GetBufferSize(), nullptr, vs.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create vertex shader '%s'", vs_entry);
    }

    // Create the pixel shader:
    hr = device->CreatePixelShader(ps_blob->GetBufferPointer(),
                                   ps_blob->GetBufferSize(), nullptr, ps.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create pixel shader '%s'", ps_entry);
    }

    CreateVertexLayout(std::get<0>(layout), std::get<1>(layout), *vs_blob.Get());
}

///////////////////////////////////////////////////////////////////////////////

void ShaderProgram::CreateVertexLayout(const D3D11_INPUT_ELEMENT_DESC * const desc,
                                       const int num_elements, ID3DBlob & vs_blob)
{
    FASTASSERT(desc != nullptr && num_elements > 0);

    HRESULT hr = g_Renderer->Device()->CreateInputLayout(
            desc, num_elements, vs_blob.GetBufferPointer(),
            vs_blob.GetBufferSize(), vertex_layout.GetAddressOf());

    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create vertex layout!");
    }
}

///////////////////////////////////////////////////////////////////////////////
// SpriteBatch
///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::Init(const int max_verts)
{
    m_num_verts = max_verts;

    D3D11_BUFFER_DESC bd = {0};
    bd.Usage             = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
    bd.ByteWidth         = sizeof(Vertex2D) * max_verts;
    bd.BindFlags         = D3D11_BIND_VERTEX_BUFFER;

    for (int b = 0; b < 2; ++b)
    {
        if (FAILED(g_Renderer->Device()->CreateBuffer(&bd, nullptr, m_vertex_buffers[b].GetAddressOf())))
        {
            GameInterface::Errorf("Failed to create SpriteBatch vertex buffer %i", b);
        }

        m_mapping_info[b] = {};
    }

    MemTagsTrackAlloc(bd.ByteWidth, MemTag::kVertIndexBuffer);
    GameInterface::Printf("SpriteBatch used %s", FormatMemoryUnit(bd.ByteWidth));
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::BeginFrame()
{
    // Map the current buffer:
    if (FAILED(g_Renderer->DeviceContext()->Map(m_vertex_buffers[m_buffer_index].Get(),
               0, D3D11_MAP_WRITE_DISCARD, 0, &m_mapping_info[m_buffer_index])))
    {
        GameInterface::Errorf("Failed to map SpriteBatch vertex buffer %i", m_buffer_index);
    }
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::EndFrame(const ShaderProgram & program, const TextureImpl * const tex,
                           ID3D11BlendState * const blend_state, ID3D11Buffer * const cbuffer)
{
    ID3D11DeviceContext * const context = g_Renderer->DeviceContext();
    ID3D11Buffer * const current_buffer = m_vertex_buffers[m_buffer_index].Get();

    // Unmap current buffer:
    context->Unmap(current_buffer, 0);
    m_mapping_info[m_buffer_index] = {};

    // Constant buffer at register(b0)
    context->VSSetConstantBuffers(0, 1, &cbuffer);

    // Set blending for transparency:
    const float blend_factor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    context->OMSetBlendState(blend_state, blend_factor, 0xFFFFFFFF);

    // Fast path - one texture for the whole batch:
    if (tex != nullptr)
    {
        // Bind texture & sampler (t0, s0):
        context->PSSetShaderResources(0, 1, tex->srv.GetAddressOf());
        context->PSSetSamplers(0, 1, tex->sampler.GetAddressOf());

        // Draw with the current vertex buffer:
        g_Renderer->DrawHelper(m_used_verts, 0, program, current_buffer,
                               D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, 0, sizeof(Vertex2D));
    }
    else // Handle small unique textured draws:
    {
        const TextureImpl * previous_tex = nullptr;
        for (const auto & d : m_deferred_textured_quads)
        {
            if (previous_tex != d.tex)
            {
                context->PSSetShaderResources(0, 1, d.tex->srv.GetAddressOf());
                context->PSSetSamplers(0, 1, d.tex->sampler.GetAddressOf());
                previous_tex = d.tex;
            }

            g_Renderer->DrawHelper(/*num_verts=*/ 6, d.quad_start_vtx, program, current_buffer,
                                   D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, 0, sizeof(Vertex2D));
        }
    }

    // Restore default blend state.
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    // Move to the next buffer:
    m_buffer_index = !m_buffer_index;
    m_used_verts = 0;

    if (!m_deferred_textured_quads.empty())
    {
        m_deferred_textured_quads.clear();
    }
}

///////////////////////////////////////////////////////////////////////////////

Vertex2D * SpriteBatch::Increment(const int count)
{
    FASTASSERT(count > 0 && count <= m_num_verts);

    auto * verts = static_cast<Vertex2D *>(m_mapping_info[m_buffer_index].pData);
    FASTASSERT(verts != nullptr);

    verts += m_used_verts;
    m_used_verts += count;

    if (m_used_verts > m_num_verts)
    {
        GameInterface::Errorf("SpriteBatch overflowed! used_verts=%i, num_verts=%i. "
                              "Increase size.", m_used_verts, m_num_verts);
    }

    return verts;
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushTriVerts(const Vertex2D tri[3])
{
    Vertex2D * verts = Increment(3);
    std::memcpy(verts, tri, sizeof(Vertex2D) * 3);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadVerts(const Vertex2D quad[4])
{
    Vertex2D * tri = Increment(6);           // Expand quad into 2 triangles
    const int indexes[6] = { 0,1,2, 2,3,0 }; // CW winding
    for (int i = 0; i < 6; ++i)
    {
        tri[i] = quad[indexes[i]];
    }
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuad(const float x, const float y, const float w, const float h,
                           const float u0, const float v0, const float u1, const float v1,
                           const DirectX::XMFLOAT4A & color)
{
    Vertex2D quad[4];
    quad[0].xy_uv.x = x;
    quad[0].xy_uv.y = y;
    quad[0].xy_uv.z = u0;
    quad[0].xy_uv.w = v0;
    quad[0].rgba    = color;
    quad[1].xy_uv.x = x + w;
    quad[1].xy_uv.y = y;
    quad[1].xy_uv.z = u1;
    quad[1].xy_uv.w = v0;
    quad[1].rgba    = color;
    quad[2].xy_uv.x = x + w;
    quad[2].xy_uv.y = y + h;
    quad[2].xy_uv.z = u1;
    quad[2].xy_uv.w = v1;
    quad[2].rgba    = color;
    quad[3].xy_uv.x = x;
    quad[3].xy_uv.y = y + h;
    quad[3].xy_uv.z = u0;
    quad[3].xy_uv.w = v1;
    quad[3].rgba    = color;
    PushQuadVerts(quad);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadTextured(const float x, const float y,
                                   const float w, const float h,
                                   const TextureImpl * const tex,
                                   const DirectX::XMFLOAT4A & color)
{
    FASTASSERT(tex != nullptr);
    const int quad_start_vtx = m_used_verts;
    PushQuad(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color);
    m_deferred_textured_quads.push_back({ quad_start_vtx, tex });
}

///////////////////////////////////////////////////////////////////////////////
// Renderer
///////////////////////////////////////////////////////////////////////////////

const DirectX::XMFLOAT4A Renderer::kClearColor{ 0.2f, 0.2f, 0.2f, 1.0f };
const DirectX::XMFLOAT4A Renderer::kColorWhite{ 1.0f, 1.0f, 1.0f, 1.0f };
const DirectX::XMFLOAT4A Renderer::kColorBlack{ 0.0f, 0.0f, 0.0f, 1.0f };

///////////////////////////////////////////////////////////////////////////////

Renderer::Renderer()
{
    GameInterface::Printf("D3D11 Renderer instance created.");
}

///////////////////////////////////////////////////////////////////////////////

Renderer::~Renderer()
{
    GameInterface::Printf("D3D11 Renderer shutting down.");
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::Init(const char * const window_name, HINSTANCE hinst, WNDPROC wndproc,
                    const int width, const int height, const bool fullscreen, const bool debug_validation)
{
    GameInterface::Printf("D3D11 Renderer initializing.");

    // RenderWindow setup
    m_window.window_name      = window_name;
    m_window.class_name       = window_name;
    m_window.hinst            = hinst;
    m_window.wndproc          = wndproc;
    m_window.width            = width;
    m_window.height           = height;
    m_window.fullscreen       = fullscreen;
    m_window.debug_validation = debug_validation;
    m_window.Init();

    // 2D sprite/UI batch setup
    m_sprite_batches[SpriteBatch::kDrawChar].Init(6 * 5000);
    m_sprite_batches[SpriteBatch::kDrawPics].Init(6 * 128);

    // Texture cache
    m_tex_store.Init();

    // Load shader progs
    LoadShaders();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::LoadShaders()
{
    GameInterface::Printf("CWD......: %s", OSWindow::CurrentWorkingDir().c_str());
    GameInterface::Printf("GameDir..: %s", GameInterface::FS::GameDir());

    // UI/2D sprites:
    {
        const D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        const int num_elements = ARRAYSIZE(layout);
        m_shader_ui_sprites.LoadFromFxFile(REFD3D11_SHADER_PATH_WIDE L"UISprites2D.fx",
                                           "VS_main", "PS_main", { layout, num_elements });

        // Blend state for the screen text:
        D3D11_BLEND_DESC bs_desc                      = {};
        bs_desc.RenderTarget[0].BlendEnable           = true;
        bs_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        bs_desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        bs_desc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        bs_desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        bs_desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        bs_desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        bs_desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        if (FAILED(Device()->CreateBlendState(&bs_desc, m_blend_state_ui_sprites.GetAddressOf())))
        {
            GameInterface::Errorf("CreateBlendState failed!");
        }

        // Create the constant buffer:
        D3D11_BUFFER_DESC buf_desc = {};
        buf_desc.Usage             = D3D11_USAGE_DEFAULT;
        buf_desc.ByteWidth         = sizeof(ConstantBufferDataUI);
        buf_desc.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(Device()->CreateBuffer(&buf_desc, nullptr, m_cbuffer_ui_sprites.GetAddressOf())))
        {
            GameInterface::Errorf("Failed to create shader constant buffer!");
        }
    }

    GameInterface::Printf("Shaders loaded successfully.");
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::BeginFrame()
{
    m_frame_started = true;
    m_window.device_context->ClearRenderTargetView(m_window.framebuffer_rtv.Get(), &kClearColor.x);

    m_sprite_batches[SpriteBatch::kDrawChar].BeginFrame();
    m_sprite_batches[SpriteBatch::kDrawPics].BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EndFrame()
{
    Flush2D();

    m_window.swap_chain->Present(0, 0);

    m_frame_started  = false;
    m_window_resized = false;
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::Flush2D()
{
    FASTASSERT(m_tex_store.tex_conchars != nullptr);
    FASTASSERT(m_blend_state_ui_sprites != nullptr);
    FASTASSERT(m_cbuffer_ui_sprites != nullptr);

    if (m_window_resized)
    {
        m_cbuffer_data_ui_sprites.screen_dimensions.x = static_cast<float>(m_window.width);
        m_cbuffer_data_ui_sprites.screen_dimensions.y = static_cast<float>(m_window.height);
        DeviceContext()->UpdateSubresource(m_cbuffer_ui_sprites.Get(), 0, nullptr, &m_cbuffer_data_ui_sprites, 0, 0);
    }

    // Remaining 2D geometry:
    m_sprite_batches[SpriteBatch::kDrawPics].EndFrame(m_shader_ui_sprites, nullptr,
            m_blend_state_ui_sprites.Get(), m_cbuffer_ui_sprites.Get());

    // Flush 2D text:
    m_sprite_batches[SpriteBatch::kDrawChar].EndFrame(m_shader_ui_sprites,
            static_cast<const TextureImpl *>(m_tex_store.tex_conchars),
            m_blend_state_ui_sprites.Get(), m_cbuffer_ui_sprites.Get());
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

void Renderer::CompileShaderFromFile(const wchar_t * const filename, const char * const entry_point,
                                     const char * const shader_model, ID3DBlob ** out_blob) const
{
    UINT shader_flags = D3DCOMPILE_ENABLE_STRICTNESS;

    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows
    // the shaders to be optimized and to run exactly the way they will run in
    // the release configuration.
    if (DebugValidation())
    {
        shader_flags |= D3DCOMPILE_DEBUG;
    }

    ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3DCompileFromFile(filename, nullptr, nullptr, entry_point, shader_model,
                                    shader_flags, 0, out_blob, error_blob.GetAddressOf());

    if (FAILED(hr))
    {
        auto * details = (error_blob ? static_cast<const char *>(error_blob->GetBufferPointer()) : "<no info>");
        GameInterface::Errorf("Failed to compile shader: %s.\n\nError info: %s", OSWindow::ErrorToString(hr).c_str(), details);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Global Renderer instance
///////////////////////////////////////////////////////////////////////////////

Renderer * g_Renderer = nullptr;

///////////////////////////////////////////////////////////////////////////////

void CreateRendererInstance()
{
    FASTASSERT(g_Renderer == nullptr);
    g_Renderer = new(MemTag::kRenderer) Renderer{};
}

///////////////////////////////////////////////////////////////////////////////

void DestroyRendererInstance()
{
    DeleteObject(g_Renderer, MemTag::kRenderer);
    g_Renderer = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

} // D3D11
