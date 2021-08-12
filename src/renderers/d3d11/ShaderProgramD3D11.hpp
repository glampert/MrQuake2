//
// ShaderProgramD3D11.hpp
//
#pragma once

#include "UtilsD3D11.hpp"

namespace MrQ2
{

class DeviceD3D11;
class PipelineStateD3D11;
class GraphicsContextD3D11;

struct VertexInputLayoutD3D11 final
{
    enum ElementType : uint8_t
    {
        kInvalidElementType = 0,

        kVertexPosition,
        kVertexTexCoords,
        kVertexLmCoords,
        kVertexColor,

        kElementTypeCount
    };

    enum ElementFormat : uint8_t
    {
        kInvalidElementFormat = 0,

        kFormatFloat2,
        kFormatFloat3,
        kFormatFloat4,

        kElementFormatCount
    };

    static constexpr uint32_t kMaxVertexElements = 4;

    struct VertexElement
    {
        ElementType   type;
        ElementFormat format;
        uint32_t      offset;
    } elements[kMaxVertexElements];
};

class ShaderProgramD3D11 final
{
    friend PipelineStateD3D11;
    friend GraphicsContextD3D11;

public:

    ShaderProgramD3D11() = default;

    // Disallow copy.
    ShaderProgramD3D11(const ShaderProgramD3D11 &) = delete;
    ShaderProgramD3D11 & operator=(const ShaderProgramD3D11 &) = delete;

    // Defaults to VS_main/PS_main and debug if the Device has debug validation on.
    bool LoadFromFile(const DeviceD3D11 & device,
                      const VertexInputLayoutD3D11 & input_layout,
                      const char * filename);

    bool LoadFromFile(const DeviceD3D11 & device,
                      const VertexInputLayoutD3D11 & input_layout,
                      const char * filename,
                      const char * vs_entry, const char * ps_entry,
                      const bool debug);

    void Shutdown();

    bool IsLoaded() const { return m_is_loaded; }

private:

    struct FxLoaderInfo
    {
        const char * vs_entry;
        const char * vs_model;
        const char * ps_entry;
        const char * ps_model;
        bool         debug;
    };

    struct Blobs
    {
        D11ComPtr<ID3DBlob> vs_blob;
        D11ComPtr<ID3DBlob> ps_blob;
    };

    static bool CompileShaderFromFile(const wchar_t * filename, const char * entry_point, const char * shader_model, const bool debug, ID3DBlob ** out_blob);
    static bool LoadFromFxFile(const wchar_t * filename, const FxLoaderInfo & info, Blobs * out_blobs);

    const DeviceD3D11 *           m_device{ nullptr };
    D11ComPtr<ID3D11VertexShader> m_vertex_shader;
    D11ComPtr<ID3D11PixelShader>  m_pixel_shader;
    D11ComPtr<ID3D11InputLayout>  m_vertex_layout;
    bool                          m_is_loaded{ false };
};

} // MrQ2
