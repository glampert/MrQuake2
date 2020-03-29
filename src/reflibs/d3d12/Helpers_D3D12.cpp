//
// Helpers_D3D12.cpp
//  Misc helper classes.
//

#include "Helpers_D3D12.hpp"
#include "Renderer_D3D12.hpp"

namespace MrQ2
{
namespace D3D12
{

///////////////////////////////////////////////////////////////////////////////
// ShaderProgram
///////////////////////////////////////////////////////////////////////////////

void ShaderProgram::LoadFromFxFile(const wchar_t * const filename, const char * const vs_entry, const char * const ps_entry)
{
    D3DShader::Info shader_info;
    shader_info.vs_entry = vs_entry;
    shader_info.vs_model = "vs_5_0";
    shader_info.ps_entry = ps_entry;
    shader_info.ps_model = "ps_5_0";
    shader_info.debug    = Renderer::DebugValidation();

    D3DShader::LoadFromFxFile(filename, shader_info, &shader_bytecode);
}

void ShaderProgram::CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& rootsig_desc)
{
    ComPtr<ID3DBlob> blob;
    if (FAILED(D3D12SerializeRootSignature(&rootsig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr)))
    {
        GameInterface::Errorf("Failed to serialize RootSignature!");
    }

    if (FAILED(Renderer::Device()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootsig))))
    {
        GameInterface::Errorf("Failed to create RootSignature!");
    }
}

///////////////////////////////////////////////////////////////////////////////
// SpriteBatch
///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::Init(const int max_verts)
{
    (void)max_verts;
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::BeginFrame()
{
    // TODO
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::EndFrame()
{
    // TODO
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
    const int quad_start_vtx = 0;//TODO m_buffers.CurrentPosition();
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
    const int quad_start_vtx = 0;//TODO m_buffers.CurrentPosition();
    PushQuad(x, y, w, h, u0, v0, u1, v1, color);
    m_deferred_textured_quads.push_back({ quad_start_vtx, static_cast<const TextureImageImpl *>(tex) });
}

///////////////////////////////////////////////////////////////////////////////

} // D3D12
} // MrQ2
