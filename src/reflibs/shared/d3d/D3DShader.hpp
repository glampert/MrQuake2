//
// D3DShader.hpp
//  Common shader loading API for the D3D back-ends (Dx11 & Dx12).
//
#pragma once

#include <d3dcompiler.h>

namespace MrQ2
{

// Shader loading helper.
namespace D3DShader
{
    using Microsoft::WRL::ComPtr;

    struct Info final
    {
        const char * vs_entry;
        const char * vs_model;
        const char * ps_entry;
        const char * ps_model;
        bool         debug;
    };

    struct Blobs final
    {
        ComPtr<ID3DBlob> vs_blob;
        ComPtr<ID3DBlob> ps_blob;
    };

    inline void CompileShaderFromFile(const wchar_t * filename, const char * entry_point, const char * shader_model, const bool debug, ID3DBlob ** out_blob)
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

        ComPtr<ID3DBlob> error_blob;
        HRESULT hr = D3DCompileFromFile(filename, nullptr, nullptr, entry_point, shader_model,
                                        shader_flags, 0, out_blob, error_blob.GetAddressOf());

        if (FAILED(hr))
        {
            auto * details = (error_blob ? static_cast<const char *>(error_blob->GetBufferPointer()) : "<no info>");
            GameInterface::Errorf("Failed to compile shader: %s.\n\nError info: %s", Win32Window::ErrorToString(hr).c_str(), details);
        }
    }

    inline void LoadFromFxFile(const wchar_t * filename, const Info & info, Blobs * out_blobs)
    {
        MRQ2_ASSERT(filename != nullptr && filename[0] != L'\0');
        MRQ2_ASSERT(out_blobs != nullptr);

        MRQ2_ASSERT(info.vs_entry != nullptr && info.vs_entry[0] != '\0');
        MRQ2_ASSERT(info.vs_model != nullptr && info.vs_model[0] != '\0');

        MRQ2_ASSERT(info.ps_entry != nullptr && info.ps_entry[0] != '\0');
        MRQ2_ASSERT(info.ps_model != nullptr && info.ps_model[0] != '\0');

        CompileShaderFromFile(filename, info.vs_entry, info.vs_model, info.debug, out_blobs->vs_blob.GetAddressOf());
        MRQ2_ASSERT(out_blobs->vs_blob != nullptr);

        CompileShaderFromFile(filename, info.ps_entry, info.ps_model, info.debug, out_blobs->ps_blob.GetAddressOf());
        MRQ2_ASSERT(out_blobs->ps_blob != nullptr);
    }

} // D3DShader
} // MrQ2
