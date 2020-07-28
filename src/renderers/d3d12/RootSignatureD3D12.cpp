//
// RootSignatureD3D12.cpp
//

#include "../common/Array.hpp"
#include "RootSignatureD3D12.hpp"
#include "DeviceD3D12.hpp"

namespace MrQ2
{

RootSignatureD3D12 RootSignatureD3D12::sm_global;

void RootSignatureD3D12::Init(const DeviceD3D12 & device, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC & root_sig_desc)
{
    D12ComPtr<ID3DBlob> blob{};
    D12ComPtr<ID3DBlob> error_blob{};

    auto hr = D3D12SerializeVersionedRootSignature(&root_sig_desc, &blob, &error_blob);
    if (FAILED(hr))
    {
        auto * details = (error_blob ? static_cast<const char *>(error_blob->GetBufferPointer()) : "<no info>");
        GameInterface::Errorf("Failed to serialize D3D12 RootSignature: %s", details);
    }

    MRQ2_ASSERT(blob != nullptr);
    MRQ2_ASSERT(root_sig == nullptr);

    hr = device.device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_sig));
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create D3D12 RootSignature!");
    }
}

void RootSignatureD3D12::CreateGlobalRootSignature(const DeviceD3D12 & device)
{
    FixedSizeArray<D3D12_ROOT_PARAMETER1, kRootParameterCount> params{};

    // Constant buffers b0,bN-1
    int cbuffer_slot = 0;
    for (; cbuffer_slot < kCBufferCount - 1; ++cbuffer_slot)
    {
        D3D12_ROOT_PARAMETER1 param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        param.Descriptor.ShaderRegister = cbuffer_slot;
        param.Descriptor.RegisterSpace  = 0;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params.push_back(param);
    }

    // Last constant buffer is actually a set of Root constants so we can efficiently updated them per draw
    {
        D3D12_ROOT_PARAMETER1 param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param.Constants.ShaderRegister = cbuffer_slot;
        param.Constants.RegisterSpace  = 0;
		param.Constants.Num32BitValues = kMaxInlineRootConstants;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params.push_back(param);
    }

    // Texture t0
    {
        D3D12_DESCRIPTOR_RANGE1 descriptor_ranges[1] = {};
        descriptor_ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        descriptor_ranges[0].NumDescriptors = kTextureCount;

        D3D12_ROOT_PARAMETER1 param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.DescriptorTable.NumDescriptorRanges = ArrayLength(descriptor_ranges);
        param.DescriptorTable.pDescriptorRanges   = descriptor_ranges;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        params.push_back(param);
    }

    // Sampler s0
    {
        D3D12_DESCRIPTOR_RANGE1 descriptor_ranges[1] = {};
        descriptor_ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        descriptor_ranges[0].NumDescriptors = kSamplerCount;

        D3D12_ROOT_PARAMETER1 param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.DescriptorTable.NumDescriptorRanges = ArrayLength(descriptor_ranges);
        param.DescriptorTable.pDescriptorRanges   = descriptor_ranges;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        params.push_back(param);
    }

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc = {};
    root_sig_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    root_sig_desc.Desc_1_1.NumParameters = params.size();
    root_sig_desc.Desc_1_1.pParameters   = params.data();
    root_sig_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                   D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                   D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                   D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    sm_global.Init(device, root_sig_desc);
}

} // MrQ2
