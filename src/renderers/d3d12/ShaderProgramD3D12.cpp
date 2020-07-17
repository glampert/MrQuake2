//
// ShaderProgramD3D12.cpp
//

#include "../common/Win32Window.hpp"
#include "ShaderProgramD3D12.hpp"
#include "DeviceD3D12.hpp"
#include <d3dcompiler.h>

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// ShaderProgramD3D12
///////////////////////////////////////////////////////////////////////////////

// Path from the project root where to find shaders for this renderer
static const char D3D12ShadersPath[] = "src\\renderers\\d3d12\\shaders";

bool ShaderProgramD3D12::LoadFromFile(const DeviceD3D12 & device, const VertexInputLayoutD3D12 & input_layout, const char * filename)
{
    return LoadFromFile(device, input_layout, filename, "PS_main", "VS_main", device.debug_validation);
}

bool ShaderProgramD3D12::LoadFromFile(const DeviceD3D12 & device, const VertexInputLayoutD3D12 & input_layout,
                                      const char * filename, const char * vs_entry, const char * ps_entry, const bool debug)
{
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first

    constexpr size_t kMaxPathLen = 1024;
    char full_shader_path[kMaxPathLen] = {};
    sprintf_s(full_shader_path, "%s\\%s", D3D12ShadersPath, filename);

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

    if (!LoadFromFxFile(full_shader_path_wide, loader_info, &m_shader_bytecode))
    {
        return false;
    }

    m_device = &device;
    m_inputLayout = input_layout;
    return true;
}

void ShaderProgramD3D12::Shutdown()
{
    m_device          = nullptr;
    m_shader_bytecode = {};
    m_inputLayout     = {};
}

bool ShaderProgramD3D12::CompileShaderFromFile(const wchar_t * filename, const char * entry_point, const char * shader_model, const bool debug, ID3DBlob ** out_blob)
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

    D12ComPtr<ID3DBlob> error_blob{};
    const auto hr = D3DCompileFromFile(filename, nullptr, nullptr, entry_point, shader_model,
                                       shader_flags, 0, out_blob, error_blob.GetAddressOf());

    if (FAILED(hr))
    {
        auto * details = (error_blob ? static_cast<const char *>(error_blob->GetBufferPointer()) : "<no info>");
        GameInterface::Printf("WARNING: Failed to compile shader: %s.\n\nError info: %s", Win32Window::ErrorToString(hr).c_str(), details);
        return false;
    }

    return true;
}

bool ShaderProgramD3D12::LoadFromFxFile(const wchar_t * filename, const FxLoaderInfo & info, Blobs * out_blobs)
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
