//
// ShaderProgramD3D11.cpp
//

#include "../common/Win32Window.hpp"
#include "ShaderProgramD3D11.hpp"
#include "DeviceD3D11.hpp"
#include <d3dcompiler.h>

namespace MrQ2
{

// Path from the project root where to find shaders for this renderer
static const char D3D11ShadersPath[] = "src\\renderers\\shaders\\hlsl";

bool ShaderProgramD3D11::LoadFromFile(const DeviceD3D11 & device, const VertexInputLayoutD3D11 & input_layout, const char * filename)
{
    return LoadFromFile(device, input_layout, filename, "VS_main", "PS_main", device.debug_validation);
}

bool ShaderProgramD3D11::LoadFromFile(const DeviceD3D11 & device, const VertexInputLayoutD3D11 & input_layout,
                                      const char * filename, const char * vs_entry, const char * ps_entry, const bool debug)
{
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first

    constexpr size_t kMaxPathLen = 1024;
    char full_shader_path[kMaxPathLen] = {};
    sprintf_s(full_shader_path, "%s\\%s.fx", D3D11ShadersPath, filename);

    size_t num_converted = 0;
    wchar_t full_shader_path_wide[kMaxPathLen] = {};
    mbstowcs_s(&num_converted, full_shader_path_wide, full_shader_path, kMaxPathLen);
    MRQ2_ASSERT(num_converted != 0);

    FxLoaderInfo loader_info{};
    loader_info.vs_entry = vs_entry;
    loader_info.vs_model = "vs_5_0";
    loader_info.ps_entry = ps_entry;
    loader_info.ps_model = "ps_5_0";
    loader_info.debug    = debug;

    Blobs shader_bytecode{};
    if (!LoadFromFxFile(full_shader_path_wide, loader_info, &shader_bytecode))
    {
        return false;
    }

    // Convert the D3D11 input layout
    const char * const d3d_input_layout_type_conv[] = { nullptr, "POSITION", "TEXCOORD", "COLOR" };
    static_assert(ArrayLength(d3d_input_layout_type_conv) == VertexInputLayoutD3D11::kElementTypeCount);

    const DXGI_FORMAT d3d_input_layout_format_conv[] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };
    static_assert(ArrayLength(d3d_input_layout_format_conv) == VertexInputLayoutD3D11::kElementFormatCount);

    uint32_t num_elements = 0;
    D3D11_INPUT_ELEMENT_DESC input_layout_d3d[VertexInputLayoutD3D11::kMaxVertexElements] = {};

    for (const auto & element : input_layout.elements)
    {
        if (element.type   == VertexInputLayoutD3D11::kInvalidElementType ||
            element.format == VertexInputLayoutD3D11::kInvalidElementFormat)
        {
            continue;
        }

        input_layout_d3d[num_elements].SemanticName         = d3d_input_layout_type_conv[element.type];
        input_layout_d3d[num_elements].SemanticIndex        = 0;
        input_layout_d3d[num_elements].Format               = d3d_input_layout_format_conv[element.format];
        input_layout_d3d[num_elements].InputSlot            = 0;
        input_layout_d3d[num_elements].AlignedByteOffset    = element.offset;
        input_layout_d3d[num_elements].InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
        input_layout_d3d[num_elements].InstanceDataStepRate = 0;
        ++num_elements;
    }

    auto * device11 = device.device;

    // Create the vertex shader:
    HRESULT hr = device11->CreateVertexShader(shader_bytecode.vs_blob->GetBufferPointer(),
                                              shader_bytecode.vs_blob->GetBufferSize(), nullptr, m_vertex_shader.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create vertex shader '%s'", vs_entry);
    }

    // Create the pixel shader:
    hr = device11->CreatePixelShader(shader_bytecode.ps_blob->GetBufferPointer(),
                                     shader_bytecode.ps_blob->GetBufferSize(), nullptr, m_pixel_shader.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create pixel shader '%s'", ps_entry);
    }

    // Input Layout:
    MRQ2_ASSERT(num_elements != 0);
    hr = device11->CreateInputLayout(input_layout_d3d, num_elements, shader_bytecode.vs_blob->GetBufferPointer(),
                                     shader_bytecode.vs_blob->GetBufferSize(), m_vertex_layout.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create vertex input layout!");
    }

    m_device = &device;
    m_is_loaded = true;

    return true;
}

void ShaderProgramD3D11::Shutdown()
{
    m_device        = nullptr;
    m_vertex_shader = nullptr;
    m_pixel_shader  = nullptr;
    m_vertex_layout = nullptr;
    m_is_loaded     = false;
}

bool ShaderProgramD3D11::CompileShaderFromFile(const wchar_t * filename, const char * entry_point, const char * shader_model, const bool debug, ID3DBlob ** out_blob)
{
    UINT shader_flags = D3DCOMPILE_ENABLE_STRICTNESS;

    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows
    // the shaders to be optimized and to run exactly the way they will run in
    // the release configuration.
    if (debug)
    {
        shader_flags |= D3DCOMPILE_DEBUG;
    }

    D11ComPtr<ID3DBlob> error_blob{};
    const auto hr = D3DCompileFromFile(filename, nullptr, nullptr, entry_point, shader_model,
                                       shader_flags, 0, out_blob, error_blob.GetAddressOf());

    if (FAILED(hr))
    {
        auto * details = (error_blob ? static_cast<const char *>(error_blob->GetBufferPointer()) : "<no info>");
        GameInterface::Printf(
            "WARNING: Failed to compile shader: %s\n"
            "Shader Compiler error info: %s",
            Win32Window::ErrorToString(hr).c_str(), details);
        return false;
    }

    return true;
}

bool ShaderProgramD3D11::LoadFromFxFile(const wchar_t * filename, const FxLoaderInfo & info, Blobs * out_blobs)
{
    if (!CompileShaderFromFile(filename, info.vs_entry, info.vs_model, info.debug, out_blobs->vs_blob.GetAddressOf()))
    {
        return false;
    }
    if (out_blobs->vs_blob == nullptr)
    {
        return false;
    }

    if (!CompileShaderFromFile(filename, info.ps_entry, info.ps_model, info.debug, out_blobs->ps_blob.GetAddressOf()))
    {
        return false;
    }
    if (out_blobs->ps_blob == nullptr)
    {
        return false;
    }

    return true;
}

} // MrQ2
