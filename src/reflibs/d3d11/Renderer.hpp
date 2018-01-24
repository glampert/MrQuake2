//
// Renderer.hpp
//  D3D11 renderer interface for Quake2.
//
#pragma once

#include "RenderWindow.hpp"

#include <array>
#include <tuple>

#include <DirectXMath.h>
#include <DirectXColors.h>

namespace D3D11
{

using InputLayoutDesc = std::tuple<const D3D11_INPUT_ELEMENT_DESC *, int>;

struct Vertex2D
{
    DirectX::XMFLOAT4A xy_uv;
    DirectX::XMFLOAT4A rgba;
};

/*
===============================================================================

    D3D11 ShaderProgram

===============================================================================
*/
class ShaderProgram final
{
public:

    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader>  ps;
    ComPtr<ID3D11InputLayout>  vertex_layout;

    void LoadFromFxFile(const wchar_t * filename, const char * vs_entry,
                        const char * ps_entry, const InputLayoutDesc & layout);

    void CreateVertexLayout(const D3D11_INPUT_ELEMENT_DESC * desc,
                            int num_elements, ID3DBlob & vs_blob);

    ShaderProgram() = default;

    // Disallow copy.
    ShaderProgram(const ShaderProgram &) = delete;
    ShaderProgram & operator=(const ShaderProgram &) = delete;
};

/*
===============================================================================

    D3D11 SpriteBatch

===============================================================================
*/
class SpriteBatch final
{
private:

    int num_verts    = 0;
    int used_verts   = 0;
    int buffer_index = 0;

    std::array<ComPtr<ID3D11Buffer>,     2> vertex_buffers;
    std::array<D3D11_MAPPED_SUBRESOURCE, 2> mapping_info;

public:

    SpriteBatch() = default;
    void Init(int max_verts);

    void BeginFrame();
    void EndFrame(const ShaderProgram & program);

    Vertex2D * Increment(int count);
    void PushTriVerts(const Vertex2D tri[3]);
    void PushQuadVerts(const Vertex2D quad[4]);
    void PushQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  const DirectX::XMFLOAT4A & color);

    // Disallow copy.
    SpriteBatch(const SpriteBatch &) = delete;
    SpriteBatch & operator=(const SpriteBatch &) = delete;
};

/*
===============================================================================

    D3D11 Renderer

===============================================================================
*/
class Renderer final
{
private:

    RenderWindow window;
    SpriteBatch sprite_batch;
    bool frame_started = false;

    // Shader programs
    ShaderProgram shader_ui_sprites;

public:

    // Color used to wipe the screen at the start of a frame
    static const DirectX::XMVECTORF32 kClearColor;

    // Convenience getters
    SpriteBatch            * SpriteBatch()           { return &sprite_batch;                    }
    ID3D11Device           * Device()          const { return window.device.Get();              }
    ID3D11DeviceContext    * DeviceContext()   const { return window.device_context.Get();      }
    IDXGISwapChain         * SwapChain()       const { return window.swap_chain.Get();          }
    ID3D11Texture2D        * FramebufferTex()  const { return window.framebuffer_texture.Get(); }
    ID3D11RenderTargetView * FramebufferRTV()  const { return window.framebuffer_rtv.Get();     }
    bool                     DebugValidation() const { return window.debug_validation;          }
    bool                     FrameStarted()    const { return frame_started;                    }

    Renderer();
    ~Renderer();

    void Init(const char * window_name, HINSTANCE hinst, WNDPROC wndproc,
              int width, int height, bool fullscreen, bool debug_validation,
              int sprite_batch_size);

    void BeginFrame();
    void EndFrame();

    void CompileShaderFromFile(const wchar_t * filename, const char * entry_point,
                               const char * shader_model, ID3DBlob ** out_blob) const;

private:

    void LoadShaders();
};

// Global Renderer instance
extern Renderer * renderer;
void CreateRendererInstance();
void DestroyRendererInstance();

} // D3D11
