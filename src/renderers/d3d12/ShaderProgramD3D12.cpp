//
// ShaderProgramD3D12.cpp
//

#include "../common/Win32Window.hpp"
#include "../common/Array.hpp"
#include "ShaderProgramD3D12.hpp"
#include "DeviceD3D12.hpp"
#include <d3dcompiler.h>

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// RootSignatureD3D12
///////////////////////////////////////////////////////////////////////////////

RootSignatureD3D12 RootSignatureD3D12::sm_global;

void RootSignatureD3D12::Init(const DeviceD3D12 & device, const D3D12_ROOT_SIGNATURE_DESC & root_sig_desc)
{
    D12ComPtr<ID3DBlob> blob{};
    if (FAILED(D3D12SerializeRootSignature(&root_sig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr)))
    {
        GameInterface::Errorf("Failed to serialize D3D12 RootSignature!");
    }

    MRQ2_ASSERT(blob != nullptr);
    MRQ2_ASSERT(root_sig == nullptr);

    if (FAILED(device.device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_sig))))
    {
        GameInterface::Errorf("Failed to create D3D12 RootSignature!");
    }
}

void RootSignatureD3D12::Shutdown()
{
    root_sig = nullptr;
}

void RootSignatureD3D12::CreateGlobalRootSignature(const DeviceD3D12 & device)
{
    FixedSizeArray<D3D12_ROOT_PARAMETER, kRootParameterCount> params{};

    // Constant buffers b0,bN-1
    int cbuffer_slot = 0;
    for (; cbuffer_slot < kCBufferCount - 1; ++cbuffer_slot)
    {
        D3D12_ROOT_PARAMETER param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        param.Descriptor.ShaderRegister = cbuffer_slot;
        param.Descriptor.RegisterSpace  = 0;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params.push_back(param);
    }

    // Last constant buffer is actually a set of Root constants so we can efficiently updated them per draw
    {
        D3D12_ROOT_PARAMETER param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param.Constants.ShaderRegister = cbuffer_slot;
        param.Constants.RegisterSpace  = 0;
		param.Constants.Num32BitValues = kMaxInlineRootConstants;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params.push_back(param);
    }

    // Sampler/texture s0
    {
        D3D12_DESCRIPTOR_RANGE descriptor_range = {};
        descriptor_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        descriptor_range.NumDescriptors = 1;
        descriptor_range.BaseShaderRegister = 0;
        descriptor_range.RegisterSpace = 0;
        descriptor_range.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.DescriptorTable.NumDescriptorRanges = 1;
        param.DescriptorTable.pDescriptorRanges = &descriptor_range;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        params.push_back(param);
    }

    // Sampler descs
    D3D12_STATIC_SAMPLER_DESC static_samplers[1] = {};
    static_samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    static_samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    static_samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    static_samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    static_samplers[0].MipLODBias = 0.0f;
    static_samplers[0].MaxAnisotropy = 0;
    static_samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    static_samplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    static_samplers[0].MinLOD = 0.0f;
    static_samplers[0].MaxLOD = 0.0f;
    static_samplers[0].ShaderRegister = 0;
    static_samplers[0].RegisterSpace = 0;
    static_samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC root_sig_desc = {};
    root_sig_desc.NumParameters     = params.size();
    root_sig_desc.pParameters       = params.data();
    root_sig_desc.NumStaticSamplers = ArrayLength(static_samplers);
    root_sig_desc.pStaticSamplers   = static_samplers;
    root_sig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                          D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                          D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                          D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    sm_global.Init(device, root_sig_desc);
}

///////////////////////////////////////////////////////////////////////////////
// ShaderProgramD3D12
///////////////////////////////////////////////////////////////////////////////

// Path from the project root where to find shaders for this renderer
static const char D3D12ShadersPath[] = "src\\renderers\\shaders\\hlsl";

bool ShaderProgramD3D12::LoadFromFile(const DeviceD3D12 & device, const VertexInputLayoutD3D12 & input_layout, const char * filename)
{
    return LoadFromFile(device, input_layout, filename, "VS_main", "PS_main", device.debug_validation);
}

bool ShaderProgramD3D12::LoadFromFile(const DeviceD3D12 & device, const VertexInputLayoutD3D12 & input_layout,
                                      const char * filename, const char * vs_entry, const char * ps_entry, const bool debug)
{
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first

    constexpr size_t kMaxPathLen = 1024;
    char full_shader_path[kMaxPathLen] = {};
    sprintf_s(full_shader_path, "%s\\%s.fx", D3D12ShadersPath, filename);

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

    // Convert and cache the D3D12 input layout
    const char * const d3d_input_layout_type_conv[] = { nullptr, "POSITION", "TEXCOORD", "COLOR" };
    static_assert(ArrayLength(d3d_input_layout_type_conv) == VertexInputLayoutD3D12::kElementTypeCount);

    const DXGI_FORMAT d3d_input_layout_format_conv[] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };
    static_assert(ArrayLength(d3d_input_layout_format_conv) == VertexInputLayoutD3D12::kElementFormatCount);

    uint32_t e = 0;
    for (const auto & element : input_layout.elements)
    {
        if (element.type   == VertexInputLayoutD3D12::kInvalidElementType ||
            element.format == VertexInputLayoutD3D12::kInvalidElementFormat)
        {
            continue;
        }

        m_input_layout_d3d[e].SemanticName         = d3d_input_layout_type_conv[element.type];
        m_input_layout_d3d[e].SemanticIndex        = 0;
        m_input_layout_d3d[e].Format               = d3d_input_layout_format_conv[element.format];
        m_input_layout_d3d[e].InputSlot            = 0;
        m_input_layout_d3d[e].AlignedByteOffset    = element.offset;
        m_input_layout_d3d[e].InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        m_input_layout_d3d[e].InstanceDataStepRate = 0;
        ++e;
    }

    m_device = &device;
    m_input_layout_count = e;
    m_is_loaded = true;

    return true;
}

void ShaderProgramD3D12::Shutdown()
{
    m_device          = nullptr;
    m_shader_bytecode = {};
    m_is_loaded       = false;
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
        GameInterface::Printf(
            "WARNING: Failed to compile shader: %s\n"
            "Shader Compiler error info: %s",
            Win32Window::ErrorToString(hr).c_str(), details);
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
