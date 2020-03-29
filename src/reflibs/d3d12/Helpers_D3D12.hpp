//
// Helpers_D3D12.hpp
//  Misc helper classes.
//
#pragma once

#include "RenderWindow_D3D12.hpp"
#include "reflibs/shared/MiniImBatch.hpp"
#include "reflibs/shared/TextureStore.hpp"
#include "reflibs/shared/d3d/D3DShader.hpp"
#include <DirectXMath.h>
#include <array>

namespace MrQ2
{
namespace D3D12
{

class TextureImageImpl;

/*
===============================================================================

    D3D12 ShaderProgram

===============================================================================
*/
struct ShaderProgram final
{
    ComPtr<ID3D12RootSignature> rootsig;
    D3DShader::Blobs            shader_bytecode;

    void LoadFromFxFile(const wchar_t * filename, const char * vs_entry, const char * ps_entry);
    void CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& rootsig_desc);
};

/*
===============================================================================

    D3D12 SpriteBatch

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
    void EndFrame();

    DrawVertex2D * Increment(const int count) { (void)count; return nullptr; } // TODO

    void PushTriVerts(const DrawVertex2D tri[3]);
    void PushQuadVerts(const DrawVertex2D quad[4]);

    void PushQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  const DirectX::XMFLOAT4A & color);

    void PushQuadTextured(float x, float y, float w, float h,
                          const TextureImage * tex, const DirectX::XMFLOAT4A & color);

    void PushQuadTexturedUVs(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                             const TextureImage * tex, const DirectX::XMFLOAT4A & color);

    // Disallow copy.
    SpriteBatch(const SpriteBatch &) = delete;
    SpriteBatch & operator=(const SpriteBatch &) = delete;

private:

    struct DeferredTexQuad
    {
        int quad_start_vtx;
        const TextureImageImpl * tex;
    };
    std::vector<DeferredTexQuad> m_deferred_textured_quads;
};

} // D3D12
} // MrQ2
