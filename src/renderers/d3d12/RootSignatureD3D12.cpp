//
// RootSignatureD3D12.cpp
//

#include "../common/Array.hpp"
#include "RootSignatureD3D12.hpp"
#include "DeviceD3D12.hpp"

namespace MrQ2
{

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

} // MrQ2
