//
// Helpers_D3D11.hpp
//  Misc helper classes.
//

#include "Helpers_D3D11.hpp"
#include "Renderer_D3D11.hpp"
#include "reflibs/shared/d3d/D3DShader.hpp"

namespace MrQ2
{
namespace D3D11
{

///////////////////////////////////////////////////////////////////////////////
// ShaderProgram
///////////////////////////////////////////////////////////////////////////////

void ShaderProgram::LoadFromFxFile(ID3D11Device * device,
                                   const wchar_t * filename,
                                   const char * vs_entry,
                                   const char * ps_entry,
                                   const InputLayoutDesc & layout,
                                   const bool debug)
{
    FASTASSERT(layout.desc != nullptr && layout.num_elements > 0);

    D3DShader::Info shader_info;
    shader_info.vs_entry = vs_entry;
    shader_info.vs_model = "vs_4_0";
    shader_info.ps_entry = ps_entry;
    shader_info.ps_model = "ps_4_0";
    shader_info.debug    = debug;

    D3DShader::Blobs shader_blobs;
    D3DShader::LoadFromFxFile(filename, shader_info, &shader_blobs);

    // Create the vertex shader:
    HRESULT hr = device->CreateVertexShader(shader_blobs.vs_blob->GetBufferPointer(),
                                            shader_blobs.vs_blob->GetBufferSize(), nullptr, vs.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create vertex shader '%s'", vs_entry);
    }

    // Create the pixel shader:
    hr = device->CreatePixelShader(shader_blobs.ps_blob->GetBufferPointer(),
                                   shader_blobs.ps_blob->GetBufferSize(), nullptr, ps.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create pixel shader '%s'", ps_entry);
    }

    // Input Layout:
    hr = device->CreateInputLayout(layout.desc, layout.num_elements, shader_blobs.vs_blob->GetBufferPointer(),
                                   shader_blobs.vs_blob->GetBufferSize(), vertex_layout.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create vertex layout!");
    }
}

///////////////////////////////////////////////////////////////////////////////
// DepthStates
///////////////////////////////////////////////////////////////////////////////

void DepthStates::Init(ID3D11Device * device,
                       const bool enabled_ztest,
                       const D3D11_COMPARISON_FUNC enabled_func,
                       const D3D11_DEPTH_WRITE_MASK enabled_write_mask,
                       const bool disabled_ztest,
                       const D3D11_COMPARISON_FUNC disabled_func,
                       const D3D11_DEPTH_WRITE_MASK disabled_write_mask)
{
    D3D11_DEPTH_STENCIL_DESC ds_desc = {};

    //
    // Stencil test parameters (always OFF):
    //

    ds_desc.StencilEnable                = false;
    ds_desc.StencilReadMask              = 0;
    ds_desc.StencilWriteMask             = 0;
    // Stencil operations if pixel is front-facing:
    ds_desc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
    ds_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
    ds_desc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
    ds_desc.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
    // Stencil operations if pixel is back-facing:
    ds_desc.BackFace.StencilFailOp       = D3D11_STENCIL_OP_KEEP;
    ds_desc.BackFace.StencilDepthFailOp  = D3D11_STENCIL_OP_DECR;
    ds_desc.BackFace.StencilPassOp       = D3D11_STENCIL_OP_KEEP;
    ds_desc.BackFace.StencilFunc         = D3D11_COMPARISON_ALWAYS;

    //
    // Depth test parameters:
    //

    // When ON:
    ds_desc.DepthEnable    = enabled_ztest;
    ds_desc.DepthFunc      = enabled_func;
    ds_desc.DepthWriteMask = enabled_write_mask;
    // Create depth stencil state for rendering with depth test ENABLED:
    if (FAILED(device->CreateDepthStencilState(&ds_desc, enabled_state.GetAddressOf())))
    {
        GameInterface::Errorf("CreateDepthStencilState failed!");
    }

    // When OFF:
    ds_desc.DepthEnable    = disabled_ztest;
    ds_desc.DepthFunc      = disabled_func;
    ds_desc.DepthWriteMask = disabled_write_mask;
    // Create depth stencil state for rendering with depth test DISABLED:
    if (FAILED(device->CreateDepthStencilState(&ds_desc, disabled_state.GetAddressOf())))
    {
        GameInterface::Errorf("CreateDepthStencilState failed!");
    }
}

///////////////////////////////////////////////////////////////////////////////
// SpriteBatch
///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::Init(const int max_verts)
{
    m_buffers.Init("SpriteBatch", max_verts, Renderer::Device(), Renderer::DeviceContext());
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::BeginFrame()
{
    m_buffers.Begin();
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::EndFrame(const ShaderProgram & program, const TextureImageImpl * const tex, ID3D11Buffer * const cbuffer)
{
    ID3D11DeviceContext * const context = Renderer::DeviceContext();
    const auto draw_buf = m_buffers.End();

    // Constant buffer at register(b0)
    context->VSSetConstantBuffers(0, 1, &cbuffer);

    // Set blending for transparency:
    Renderer::EnableAlphaBlending();

    // Fast path - one texture for the whole batch:
    if (tex != nullptr)
    {
        // Bind texture & sampler (t0, s0):
        context->PSSetShaderResources(0, 1, tex->srv.GetAddressOf());
        context->PSSetSamplers(0, 1, tex->sampler.GetAddressOf());

        // Draw with the current vertex buffer:
        Renderer::DrawHelper(draw_buf.used_verts, 0, program, draw_buf.buffer_ptr,
                             D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, 0, sizeof(DrawVertex2D));
    }
    else // Handle small unique textured draws:
    {
        const TextureImageImpl * previous_tex = nullptr;
        for (const auto & d : m_deferred_textured_quads)
        {
            if (previous_tex != d.tex)
            {
                context->PSSetShaderResources(0, 1, d.tex->srv.GetAddressOf());
                context->PSSetSamplers(0, 1, d.tex->sampler.GetAddressOf());
                previous_tex = d.tex;
            }

            Renderer::DrawHelper(/*num_verts=*/ 6, d.quad_start_vtx, program, draw_buf.buffer_ptr,
                                 D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, 0, sizeof(DrawVertex2D));
        }
    }

    // Restore default blend state.
    Renderer::DisableAlphaBlending();

    // Clear cache for next frame:
    if (!m_deferred_textured_quads.empty())
    {
        m_deferred_textured_quads.clear();
    }
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushTriVerts(const DrawVertex2D tri[3])
{
    DrawVertex2D * verts = Increment(3);
    std::memcpy(verts, tri, sizeof(DrawVertex2D) * 3);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadVerts(const DrawVertex2D quad[4])
{
    DrawVertex2D * tri = Increment(6);       // Expand quad into 2 triangles
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
    DrawVertex2D quad[4];
    quad[0].xy_uv[0] = x;
    quad[0].xy_uv[1] = y;
    quad[0].xy_uv[2] = u0;
    quad[0].xy_uv[3] = v0;
    quad[0].rgba[0]  = color.x;
    quad[0].rgba[1]  = color.y;
    quad[0].rgba[2]  = color.z;
    quad[0].rgba[3]  = color.w;
    quad[1].xy_uv[0] = x + w;
    quad[1].xy_uv[1] = y;
    quad[1].xy_uv[2] = u1;
    quad[1].xy_uv[3] = v0;
    quad[1].rgba[0]  = color.x;
    quad[1].rgba[1]  = color.y;
    quad[1].rgba[2]  = color.z;
    quad[1].rgba[3]  = color.w;
    quad[2].xy_uv[0] = x + w;
    quad[2].xy_uv[1] = y + h;
    quad[2].xy_uv[2] = u1;
    quad[2].xy_uv[3] = v1;
    quad[2].rgba[0]  = color.x;
    quad[2].rgba[1]  = color.y;
    quad[2].rgba[2]  = color.z;
    quad[2].rgba[3]  = color.w;
    quad[3].xy_uv[0] = x;
    quad[3].xy_uv[1] = y + h;
    quad[3].xy_uv[2] = u0;
    quad[3].xy_uv[3] = v1;
    quad[3].rgba[0]  = color.x;
    quad[3].rgba[1]  = color.y;
    quad[3].rgba[2]  = color.z;
    quad[3].rgba[3]  = color.w;
    PushQuadVerts(quad);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadTextured(const float x, const float y,
                                   const float w, const float h,
                                   const TextureImage * const tex,
                                   const DirectX::XMFLOAT4A & color)
{
    FASTASSERT(tex != nullptr);
    const int quad_start_vtx = m_buffers.CurrentPosition();
    PushQuad(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color);
    m_deferred_textured_quads.push_back({ quad_start_vtx, static_cast<const TextureImageImpl *>(tex) });
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadTexturedUVs(const float x, const float y,
                                      const float w, const float h,
                                      const float u0, const float v0,
                                      const float u1, const float v1,
                                      const TextureImage * const tex,
                                      const DirectX::XMFLOAT4A & color)
{
    FASTASSERT(tex != nullptr);
    const int quad_start_vtx = m_buffers.CurrentPosition();
    PushQuad(x, y, w, h, u0, v0, u1, v1, color);
    m_deferred_textured_quads.push_back({ quad_start_vtx, static_cast<const TextureImageImpl *>(tex) });
}

///////////////////////////////////////////////////////////////////////////////

} // D3D11
} // MrQ2
