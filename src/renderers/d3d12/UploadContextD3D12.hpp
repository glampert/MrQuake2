//
// UploadContextD3D12.hpp
//
#pragma once

#include "UtilsD3D12.hpp"

namespace MrQ2
{

class DeviceD3D12;
class TextureD3D12;

struct TextureUploadD3D12 final
{
    const TextureD3D12* texture;
    const void *        pixels;
    uint32_t            width;
    uint32_t            height;
    bool                is_scrap;
};

class UploadContextD3D12 final
{
public:

    UploadContextD3D12() = default;

    // Disallow copy.
    UploadContextD3D12(const UploadContextD3D12 &) = delete;
    UploadContextD3D12 & operator=(const UploadContextD3D12 &) = delete;

    void Init(const DeviceD3D12 & device);
    void Shutdown();

    void UploadTextureImmediate(const TextureUploadD3D12& upload_info);

    // TODO:
    // Async texture upload support?

private:

    D12ComPtr<ID3D12Fence>               m_fence;
    D12ComPtr<ID3D12CommandQueue>        m_command_queue;
    D12ComPtr<ID3D12CommandAllocator>    m_command_allocator;
    D12ComPtr<ID3D12GraphicsCommandList> m_command_list;
    HANDLE                               m_fence_event{ nullptr };
    uint64_t                             m_next_fence_value{ 1 };
    const DeviceD3D12 *                  m_device{ nullptr };
};

} // MrQ2
