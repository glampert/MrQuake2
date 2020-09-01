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
    const TextureD3D12 * texture;
    bool is_scrap;

    struct {
        uint32_t             num_mip_levels;
        const ColorRGBA32 ** mip_init_data;
        const Vec2u16 *      mip_dimensions;
    } mipmaps;
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

    void UploadTexture(const TextureUploadD3D12 & upload_info);

    // D3D12 internal
    void CreateTexture(const TextureUploadD3D12 & upload_info);
    void FlushTextureCreates();
    void UpdateCompletedUploads();

private:

    struct UploadEntry
    {
        ID3D12Resource * upload_buffer;
        ID3D12Fence *    cmd_list_executed_fence;
        uint64_t         cmd_list_executed_value;
    };

    struct CreateEntry
    {
        ID3D12Resource * upload_buffer;
    };

    D12ComPtr<ID3D12Fence>               m_fence;
    D12ComPtr<ID3D12CommandQueue>        m_command_queue;
    D12ComPtr<ID3D12CommandAllocator>    m_command_allocator;
    D12ComPtr<ID3D12GraphicsCommandList> m_command_list;
    HANDLE                               m_fence_event{ nullptr };
    uint64_t                             m_next_fence_value{ 1 };
    const DeviceD3D12 *                  m_device{ nullptr };
    uint32_t                             m_num_uploads{ 0 };
    UploadEntry                          m_uploads[8] = {};
    FixedSizeArray<CreateEntry, 512>     m_creates;
};

} // MrQ2
