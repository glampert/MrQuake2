//
// Renderer.cpp
//  D3D11 renderer interface for Quake2.
//

#include "Renderer.hpp"
#include <string.h>

// Path from the project root where to find shaders for this renderer (wchar_t)
#define REFD3D11_SHADER_PATH_WIDE L"src\\reflibs\\d3d11\\shaders\\"

namespace D3D11
{

///////////////////////////////////////////////////////////////////////////////
// ShaderProgram
///////////////////////////////////////////////////////////////////////////////

void ShaderProgram::LoadFromFxFile(const wchar_t * filename, const char * vs_entry,
                                   const char * ps_entry, const InputLayoutDesc & layout)
{
    FASTASSERT(filename != nullptr && filename[0] != L'\0');
    FASTASSERT(vs_entry != nullptr && vs_entry[0] != '\0');
    FASTASSERT(ps_entry != nullptr && ps_entry[0] != '\0');

    ID3DBlob * vs_blob = nullptr;
    renderer->CompileShaderFromFile(filename, vs_entry, "vs_4_0", &vs_blob);
    FASTASSERT(vs_blob != nullptr);

    ID3DBlob * ps_blob = nullptr;
    renderer->CompileShaderFromFile(filename, ps_entry, "ps_4_0", &ps_blob);
    FASTASSERT(ps_blob != nullptr);

    HRESULT hr;
    ID3D11Device * device = renderer->Device();

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

    CreateVertexLayout(std::get<0>(layout), std::get<1>(layout), *vs_blob);

    vs_blob->Release();
    ps_blob->Release();
}

///////////////////////////////////////////////////////////////////////////////

void ShaderProgram::CreateVertexLayout(const D3D11_INPUT_ELEMENT_DESC * desc,
                                       int num_elements, ID3DBlob & vs_blob)
{
    FASTASSERT(desc != nullptr && num_elements > 0);

    HRESULT hr = renderer->Device()->CreateInputLayout(
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

void SpriteBatch::Init(int max_verts)
{
    num_verts = max_verts;

    D3D11_BUFFER_DESC bd = {0};
    bd.Usage             = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
    bd.ByteWidth         = sizeof(Vertex2D) * max_verts;
    bd.BindFlags         = D3D11_BIND_VERTEX_BUFFER;

    for (int b = 0; b < 2; ++b)
    {
        if (FAILED(renderer->Device()->CreateBuffer(&bd, nullptr, vertex_buffers[b].GetAddressOf())))
        {
            GameInterface::Errorf("Failed to create SpriteBatch vertex buffer %i", b);
        }

        mapping_info[b] = {};
    }

    GameInterface::Printf("SpriteBatch used %u KB", (bd.ByteWidth / 1024));
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::BeginFrame()
{
    // Map the current buffer:
    if (FAILED(renderer->DeviceContext()->Map(vertex_buffers[buffer_index].Get(),
               0, D3D11_MAP_WRITE_DISCARD, 0, &mapping_info[buffer_index])))
    {
        GameInterface::Errorf("Failed to map SpriteBatch vertex buffer %i", buffer_index);
    }
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::EndFrame(const ShaderProgram & program)
{
    ID3D11DeviceContext * context = renderer->DeviceContext();
    ID3D11Buffer * current_buffer = vertex_buffers[buffer_index].Get();

    // Unmap current buffer:
    context->Unmap(current_buffer, 0);
    mapping_info[buffer_index] = {};

    // Draw with the current buffer:
    const UINT offset = 0;
    const UINT stride = sizeof(Vertex2D);
    context->IASetVertexBuffers(0, 1, &current_buffer, &stride, &offset);
    context->IASetInputLayout(program.vertex_layout.Get());
//context->VSSetConstantBuffers(0, 1, &SpritesConstantBuffer); //TODO
    context->VSSetShader(program.vs.Get(), nullptr, 0);
    context->PSSetShader(program.ps.Get(), nullptr, 0);
    context->Draw(used_verts, 0);

    // Move to the next buffer:
    buffer_index = !buffer_index;
    used_verts   = 0;
}

///////////////////////////////////////////////////////////////////////////////

Vertex2D * SpriteBatch::Increment(int count)
{
    FASTASSERT(count > 0 && count <= num_verts);

    auto * verts = static_cast<Vertex2D *>(mapping_info[buffer_index].pData);
    FASTASSERT(verts != nullptr);

    verts += used_verts;
    used_verts += count;

    if (used_verts > num_verts)
    {
        GameInterface::Errorf("SpriteBatch overflowed! used_verts=%i, num_verts=%i. "
                              "Increase size.", used_verts, num_verts);
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
    const int indexes[6] = { 0,3,2, 2,1,0 }; // CW winding
    for (int i = 0; i < 6; ++i)
    {
        tri[i] = quad[indexes[i]];
    }
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuad(float x, float y, float w, float h,
                           float u0, float v0, float u1, float v1,
                           const DirectX::XMFLOAT4A & color)
{
    Vertex2D quad[4];
    quad[0].xy_uv.x = x;
    quad[0].xy_uv.y = y;
    quad[0].xy_uv.z = u0;
    quad[0].xy_uv.w = v0;
    quad[0].rgba    = color;
    quad[1].xy_uv.x = x;
    quad[1].xy_uv.y = y + h;
    quad[1].xy_uv.z = u0;
    quad[1].xy_uv.w = v1;
    quad[1].rgba    = color;
    quad[2].xy_uv.x = x + w;
    quad[2].xy_uv.y = y + h;
    quad[2].xy_uv.z = u1;
    quad[2].xy_uv.w = v1;
    quad[2].rgba    = color;
    quad[3].xy_uv.x = x + w;
    quad[3].xy_uv.y = y;
    quad[3].xy_uv.z = u1;
    quad[3].xy_uv.w = v0;
    quad[3].rgba    = color;
    PushQuadVerts(quad);
}

///////////////////////////////////////////////////////////////////////////////
// Renderer
///////////////////////////////////////////////////////////////////////////////

const DirectX::XMVECTORF32 Renderer::kClearColor = { 0.2f, 0.2f, 0.2f, 1.0f };

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

void Renderer::Init(const char * window_name, HINSTANCE hinst, WNDPROC wndproc,
                    int width, int height, bool fullscreen, bool debug_validation,
                    int sprite_batch_size)
{
    GameInterface::Printf("D3D11 Renderer initializing.");

    // RenderWindow setup
    window.window_name      = window_name;
    window.class_name       = window_name;
    window.hinst            = hinst;
    window.wndproc          = wndproc;
    window.width            = width;
    window.height           = height;
    window.fullscreen       = fullscreen;
    window.debug_validation = debug_validation;
    window.Init();

    // 2D sprite/UI batch setup
    sprite_batch.Init(sprite_batch_size);

    // Load shader progs
    LoadShaders();

    // This won't change
    DeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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
        shader_ui_sprites.LoadFromFxFile(REFD3D11_SHADER_PATH_WIDE L"UISprites2D.fx",
                                         "VS_main", "PS_main", { layout, num_elements });
    }

    GameInterface::Printf("Shaders loaded successfully.");
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::BeginFrame()
{
    frame_started = true;
    window.device_context->ClearRenderTargetView(window.framebuffer_rtv.Get(), kClearColor);

    sprite_batch.BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::EndFrame()
{
    // Flush the 2D sprites
    sprite_batch.EndFrame(shader_ui_sprites);

    window.swap_chain->Present(0, 0);
    frame_started = false;
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::CompileShaderFromFile(const wchar_t * filename, const char * entry_point,
                                     const char * shader_model, ID3DBlob ** out_blob) const
{
    UINT shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows
    // the shaders to be optimized and to run exactly the way they will run in
    // the release configuration.
    if (DebugValidation())
    {
        shaderFlags |= D3DCOMPILE_DEBUG;
    }

    ID3DBlob * error_blob = nullptr;
    HRESULT hr = D3DCompileFromFile(filename, nullptr, nullptr, entry_point, shader_model,
                                    shaderFlags, 0, out_blob, &error_blob);

    if (FAILED(hr))
    {
        auto * details = (error_blob ? static_cast<const char *>(error_blob->GetBufferPointer()) : "<no info>");
        GameInterface::Errorf("Failed to compile shader: %s.\n\nError info: %s", OSWindow::ErrorToString(hr).c_str(), details);
    }

    if (error_blob != nullptr)
    {
        error_blob->Release();
    }
}

///////////////////////////////////////////////////////////////////////////////
// Global Renderer instance
///////////////////////////////////////////////////////////////////////////////

Renderer * renderer = nullptr;

///////////////////////////////////////////////////////////////////////////////

void CreateRendererInstance()
{
    FASTASSERT(renderer == nullptr);
    renderer = new Renderer{};
}

///////////////////////////////////////////////////////////////////////////////

void DestroyRendererInstance()
{
    delete renderer;
    renderer = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

} // D3D11
