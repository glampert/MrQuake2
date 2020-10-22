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

    hr = device.Device()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_sig));
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create D3D12 RootSignature!");
    }
}

void RootSignatureD3D12::CreateGlobalRootSignature(const DeviceD3D12 & device)
{
    FixedSizeArray<D3D12_ROOT_PARAMETER1, kRootParameterCount> params{};

    // Constant buffers b0,bN-1
    {
        int cbuffer_slot = 0;
        for (; cbuffer_slot < kCBufferCount - 1; ++cbuffer_slot)
        {
            D3D12_ROOT_PARAMETER1 param = {};
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            param.Descriptor.ShaderRegister = cbuffer_slot + kRootParamIndexCBuffer0;
            param.Descriptor.RegisterSpace = 0;
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            params.push_back(param);
        }

        // Last constant buffer is actually a set of Root constants so we can efficiently updated them per draw
        {
            D3D12_ROOT_PARAMETER1 param = {};
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            param.Constants.ShaderRegister = cbuffer_slot + kRootParamIndexCBuffer0;
            param.Constants.RegisterSpace  = 0;
            param.Constants.Num32BitValues = kMaxInlineRootConstants;
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            params.push_back(param);
        }
    }

    // Texture t0,t1
    D3D12_DESCRIPTOR_RANGE1 texture_descriptor_ranges[kTextureCount] = {};
    for (int tex_slot = 0; tex_slot < kTextureCount; ++tex_slot)
    {
        texture_descriptor_ranges[tex_slot].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        texture_descriptor_ranges[tex_slot].NumDescriptors = 1;
        texture_descriptor_ranges[tex_slot].BaseShaderRegister = tex_slot + kRootParamIndexTexture0;
        texture_descriptor_ranges[tex_slot].RegisterSpace = 0;

        D3D12_ROOT_PARAMETER1 param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.DescriptorTable.NumDescriptorRanges = 1;
        param.DescriptorTable.pDescriptorRanges = &texture_descriptor_ranges[tex_slot];
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        params.push_back(param);
    }

    // Sampler s0,s1
    D3D12_DESCRIPTOR_RANGE1 sampler_descriptor_ranges[kSamplerCount] = {};
    for (int sampler_slot = 0; sampler_slot < kSamplerCount; ++sampler_slot)
    {
        sampler_descriptor_ranges[sampler_slot].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        sampler_descriptor_ranges[sampler_slot].NumDescriptors = 1;
        sampler_descriptor_ranges[sampler_slot].BaseShaderRegister = sampler_slot + kRootParamIndexTexture0;
        sampler_descriptor_ranges[sampler_slot].RegisterSpace = 0;

        D3D12_ROOT_PARAMETER1 param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.DescriptorTable.NumDescriptorRanges = 1;
        param.DescriptorTable.pDescriptorRanges = &sampler_descriptor_ranges[sampler_slot];
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        params.push_back(param);
    }

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc = {};
    root_sig_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    root_sig_desc.Desc_1_1.NumParameters = params.size();
    root_sig_desc.Desc_1_1.pParameters = params.data();
    root_sig_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                   D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                   D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                   D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    sm_global.Init(device, root_sig_desc);
    D12SetDebugName(sm_global.root_sig, L"GlobalRootSignature");
}

} // MrQ2
