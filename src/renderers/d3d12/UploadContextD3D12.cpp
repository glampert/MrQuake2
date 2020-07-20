//
// UploadContextD3D12.cpp
//

#include "UploadContextD3D12.hpp"
#include "TextureD3D12.hpp"
#include "DeviceD3D12.hpp"

namespace MrQ2
{

void UploadContextD3D12::Init(const DeviceD3D12 & device)
{
    MRQ2_ASSERT(m_device == nullptr);
    m_device = &device;

    D12CHECK(device.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

    m_fence_event = CreateEventEx(nullptr, "UploadContextFence", FALSE, EVENT_ALL_ACCESS);
    if (m_fence_event == nullptr)
    {
        GameInterface::Errorf("Failed to create UploadContext fence event.");
    }

    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.NodeMask = 1;

    D12CHECK(device.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_command_queue)));
    D12CHECK(device.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_command_allocator)));
    D12CHECK(device.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_command_allocator.Get(), nullptr, IID_PPV_ARGS(&m_command_list)));
    D12CHECK(m_command_list->Close());

    D12SetDebugName(m_command_queue, L"UploadContextCmdQueue");
    D12SetDebugName(m_command_list,  L"UploadContextCmdList");
}

void UploadContextD3D12::Shutdown()
{
    if (m_fence_event != nullptr)
    {
        CloseHandle(m_fence_event);
        m_fence_event = nullptr;
    }

    m_fence             = nullptr;
    m_command_queue     = nullptr;
    m_command_allocator = nullptr;
    m_command_list      = nullptr;
    m_device            = nullptr;
}

void UploadContextD3D12::UploadTextureImmediate(const TextureUploadD3D12& upload_info)
{
    MRQ2_ASSERT(m_device != nullptr);

    const uint32_t bytes_per_pixel   = 4; // All our textures are RGBA8 for now
    const uint32_t width             = upload_info.width;
    const uint32_t height            = upload_info.height;
    const uint32_t upload_pitch      = D12Align(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, width * bytes_per_pixel);
    const uint32_t upload_size       = height * upload_pitch;

    D3D12_RESOURCE_DESC res_desc     = {};
    res_desc.Dimension               = D3D12_RESOURCE_DIMENSION_BUFFER;
    res_desc.Alignment               = 0;
    res_desc.Width                   = upload_size;
    res_desc.Height                  = 1;
    res_desc.DepthOrArraySize        = 1;
    res_desc.MipLevels               = 1;
    res_desc.Format                  = DXGI_FORMAT_UNKNOWN;
    res_desc.SampleDesc.Count        = 1;
    res_desc.SampleDesc.Quality      = 0;
    res_desc.Layout                  = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    res_desc.Flags                   = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type                  = D3D12_HEAP_TYPE_UPLOAD;
    heap_props.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;

    D12ComPtr<ID3D12Resource> upload_buffer;
    D12CHECK(m_device->device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_buffer)));
    D12SetDebugName(upload_buffer, L"TextureUploadBuffer");

    void * mapped_ptr = nullptr;
    const D3D12_RANGE map_range = { 0, upload_size };
    D12CHECK(upload_buffer->Map(0, &map_range, &mapped_ptr));
    {
        auto * dest = reinterpret_cast<uint8_t *>(mapped_ptr);
        auto * src  = reinterpret_cast<const uint8_t *>(upload_info.pixels);

        for (size_t y = 0; y < height; ++y)
        {
            std::memcpy(dest + y * upload_pitch, src + y * width * bytes_per_pixel, width * bytes_per_pixel);
        }
    }
    upload_buffer->Unmap(0, &map_range);

    D3D12_TEXTURE_COPY_LOCATION src_location        = {};
    src_location.pResource                          = upload_buffer.Get();
    src_location.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src_location.PlacedFootprint.Footprint.Format   = DXGI_FORMAT_R8G8B8A8_UNORM;
    src_location.PlacedFootprint.Footprint.Width    = width;
    src_location.PlacedFootprint.Footprint.Height   = height;
    src_location.PlacedFootprint.Footprint.Depth    = 1;
    src_location.PlacedFootprint.Footprint.RowPitch = upload_pitch;

    D3D12_TEXTURE_COPY_LOCATION dst_location        = {};
    dst_location.pResource                          = upload_info.texture->m_resource.Get();
    dst_location.Type                               = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_location.SubresourceIndex                   = 0;

    D12CHECK(m_command_list->Reset(m_command_allocator.Get(), nullptr));
    m_command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

    // Regular textures (non-scrap) are never updated/changed, so we can transition them to shader resource.
    if (!upload_info.is_scrap)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = upload_info.texture->m_resource.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        m_command_list->ResourceBarrier(1, &barrier);
    }

    D12CHECK(m_command_list->Close());

    ID3D12CommandList * cmd_lists[] = { m_command_list.Get() };
    m_command_queue->ExecuteCommandLists(1, cmd_lists);

    D12CHECK(m_command_queue->Signal(m_fence.Get(), m_next_fence_value));
    D12CHECK(m_fence->SetEventOnCompletion(m_next_fence_value, m_fence_event));

    if (WaitForSingleObjectEx(m_fence_event, INFINITE, FALSE) != WAIT_OBJECT_0)
    {
        GameInterface::Errorf("WaitForSingleObjectEx failed! Error: %u", ::GetLastError());
    }

    ++m_next_fence_value;
}

} // namespace MrQ2
