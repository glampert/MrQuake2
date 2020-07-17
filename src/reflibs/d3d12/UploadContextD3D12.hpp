//
// UploadContextD3D12.hpp
//
#pragma once

namespace MrQ2
{

class DeviceD3D12;

class UploadContextD3D12 final
{
public:

    UploadContextD3D12() = default;

    // Disallow copy.
    UploadContextD3D12(const UploadContextD3D12 &) = delete;
    UploadContextD3D12 & operator=(const UploadContextD3D12 &) = delete;

    void Init(const DeviceD3D12 & device) {}
    void Shutdown() {}

    void UploadTextureImmediate(const TextureD3D12 & tex_to_upload) {}
    void UploadTextureAsync(const TextureD3D12 & tex_to_upload) {}

    // Blocks until all async texture uploads have completed.
    void Synchronize() {}
};

} // MrQ2
