//
// RootSignatureD3D12.hpp
//
#pragma once

#include "UtilsD3D12.hpp"

namespace MrQ2
{

class DeviceD3D12;

struct RootSignatureD3D12 final
{
    enum Constants : uint32_t
    {
        // Buffers
        kRootParamIndexCBuffer0, // PerFrameShaderConstants
        kRootParamIndexCBuffer1, // PerViewShaderConstants
        kRootParamIndexCBuffer2, // PerDrawShaderConstants

        // Textures
        kRootParamIndexTexture0,

        // Internal counts
        kCBufferCount = 3,
        kTextureCount = 1,
        kRootParameterCount = kCBufferCount + kTextureCount,

        // In 32bit values
        kMaxInlineRootConstants = 16
    };

    D12ComPtr<ID3D12RootSignature> root_sig{};

    void Init(const DeviceD3D12 & device, const D3D12_ROOT_SIGNATURE_DESC & root_sig_desc);
    void Shutdown() { root_sig = nullptr; }

    static RootSignatureD3D12 sm_global;
    static void CreateGlobalRootSignature(const DeviceD3D12 & device);
};

} // MrQ2
