//
// ShaderProgramD3D12.hpp
//
#pragma once

#include "UtilsD3D12.hpp"

namespace MrQ2
{

class DeviceD3D12;

struct RootSignatureD3D12 final
{
    enum RootParameterIndex : uint32_t
    {
        kRootParamIndexCBuffer,
        kRootParamIndexTexture
    };
};

struct VertexInputLayoutD3D12 final
{
    enum ElementType : uint8_t
    {
        kVertexPosition,
		kVertexTexCoords,
        kVertexColor,

        kElementTypeCount
    };

    enum ElementFormat : uint8_t
    {
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

class ShaderProgramD3D12 final
{
public:

    // Defaults to VS_main/PS_main and debug if the Device has debug validation on.
    bool LoadFromFile(const DeviceD3D12 & device,
                      const VertexInputLayoutD3D12 & input_layout,
                      const char * filename);

    bool LoadFromFile(const DeviceD3D12 & device,
                      const VertexInputLayoutD3D12 & input_layout,
                      const char * filename,
                      const char * vs_entry, const char * ps_entry,
                      const bool debug);

    void Shutdown();

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
        D12ComPtr<ID3DBlob> vs_blob;
        D12ComPtr<ID3DBlob> ps_blob;
    };

    static bool CompileShaderFromFile(const wchar_t * filename, const char * entry_point, const char * shader_model, const bool debug, ID3DBlob ** out_blob);
    static bool LoadFromFxFile(const wchar_t * filename, const FxLoaderInfo & info, Blobs * out_blobs);

    const DeviceD3D12 *    m_device{ nullptr };
    Blobs                  m_shader_bytecode{};
    VertexInputLayoutD3D12 m_inputLayout{};
};

} // MrQ2
