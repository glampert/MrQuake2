//
// UploadContextD3D12.cpp
//

#include "../common/TextureStore.hpp"
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

///////////////////////////////////////////////////////////////////////////////
// d3dx12.h subresource helpers
///////////////////////////////////////////////////////////////////////////////

struct TextureCopyLocationD3D12 : public D3D12_TEXTURE_COPY_LOCATION
{
    TextureCopyLocationD3D12() : D3D12_TEXTURE_COPY_LOCATION{}
    {
    }

    TextureCopyLocationD3D12(ID3D12Resource * pRes, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT & Footprint)
    {
        pResource = pRes;
        Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        PlacedFootprint = Footprint;
    }

    TextureCopyLocationD3D12(ID3D12Resource * pRes, const uint32_t Sub)
    {
        pResource = pRes;
        Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        SubresourceIndex = Sub;
    }
};

// Row-by-row memcpy
static void MemcpySubresource(const D3D12_MEMCPY_DEST * pDest,
                              const D3D12_SUBRESOURCE_DATA * pSrc,
                              const uint64_t RowSizeInBytes,
                              const uint32_t NumRows,
                              const uint32_t NumSlices)
{
    for (uint32_t z = 0; z < NumSlices; ++z)
    {
        uint8_t * pDestSlice = reinterpret_cast<uint8_t *>(pDest->pData) + pDest->SlicePitch * z;
        const uint8_t * pSrcSlice = reinterpret_cast<const uint8_t *>(pSrc->pData) + pSrc->SlicePitch * z;
        for (uint32_t y = 0; y < NumRows; ++y)
        {
            std::memcpy(pDestSlice + pDest->RowPitch * y, pSrcSlice + pSrc->RowPitch * y, RowSizeInBytes);
        }
    }
}

// All arrays must be populated (e.g. by calling GetCopyableFootprints)
static bool UpdateSubresources(ID3D12GraphicsCommandList * pCmdList,
                               ID3D12Resource * pDestinationResource,
                               ID3D12Resource * pIntermediate,
                               const uint32_t FirstSubresource,
                               const uint32_t NumSubresources,
                               const uint64_t RequiredSize,
                               const D3D12_PLACED_SUBRESOURCE_FOOTPRINT * pLayouts,
                               const uint32_t * pNumRows,
                               const uint64_t * pRowSizesInBytes,
                               const D3D12_SUBRESOURCE_DATA * pSrcData)
{
    const auto IntermediateDesc = pIntermediate->GetDesc();
    const auto DestinationDesc  = pDestinationResource->GetDesc();

    if (IntermediateDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
        IntermediateDesc.Width < RequiredSize + pLayouts[0].Offset ||
        RequiredSize >= size_t(-1) ||
        (DestinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER &&
        (FirstSubresource != 0 || NumSubresources != 1)))
    {
        MRQ2_ASSERT(false && "Invalid arguments!");
        return false;
    }

    uint8_t * pData = nullptr;
    auto hr = pIntermediate->Map(0, nullptr, reinterpret_cast<void **>(&pData));
    if (FAILED(hr))
    {
        MRQ2_ASSERT(false && "Failed to map intermediate buffer!");
        return false;
    }

    for (uint32_t i = 0; i < NumSubresources; ++i)
    {
        MRQ2_ASSERT(pRowSizesInBytes[i] < size_t(-1) && "Invalid row size!");
        const D3D12_MEMCPY_DEST DestData = { pData + pLayouts[i].Offset, pLayouts[i].Footprint.RowPitch, size_t(pLayouts[i].Footprint.RowPitch) * size_t(pNumRows[i]) };
        MemcpySubresource(&DestData, &pSrcData[i], pRowSizesInBytes[i], pNumRows[i], pLayouts[i].Footprint.Depth);
    }
    pIntermediate->Unmap(0, nullptr);

    if (DestinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        pCmdList->CopyBufferRegion(pDestinationResource, 0, pIntermediate, pLayouts[0].Offset, pLayouts[0].Footprint.Width);
    }
    else // Texture
    {
        for (uint32_t i = 0; i < NumSubresources; ++i)
        {
            const TextureCopyLocationD3D12 Dst(pDestinationResource, i + FirstSubresource);
            const TextureCopyLocationD3D12 Src(pIntermediate, pLayouts[i]);
            pCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
        }
    }

    return true;
}

// Stack-allocating UpdateSubresources variation
template<uint32_t kMaxSubresources>
static bool UpdateSubresources(ID3D12Device * pDevice,
                               ID3D12GraphicsCommandList * pCmdList,
                               ID3D12Resource * pDestinationResource,
                               ID3D12Resource * pIntermediate,
                               uint64_t RequiredSize,
                               const uint32_t FirstSubresource,
                               const uint32_t NumSubresources,
                               const D3D12_SUBRESOURCE_DATA * pSrcData)
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layouts[kMaxSubresources] = {};
    uint32_t NumRows[kMaxSubresources] = {};
	uint64_t RowSizesInBytes[kMaxSubresources] = {};
    const uint32_t IntermediateOffset = 0;

    const auto Desc = pDestinationResource->GetDesc();
    pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources, IntermediateOffset, Layouts, NumRows, RowSizesInBytes, &RequiredSize);

    return UpdateSubresources(pCmdList, pDestinationResource, pIntermediate, FirstSubresource,
                              NumSubresources, RequiredSize, Layouts, NumRows, RowSizesInBytes, pSrcData);
}

// Returns required size of a buffer to be used for data upload
static uint64_t GetRequiredIntermediateSize(ID3D12Device * pDevice,
                                            ID3D12Resource * pDestinationResource,
                                            const uint32_t FirstSubresource,
                                            const uint32_t NumSubresources)
{
    uint64_t RequiredSize = 0;
    const auto Desc = pDestinationResource->GetDesc();
    pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources, 0, nullptr, nullptr, nullptr, &RequiredSize);
    return RequiredSize;
}

///////////////////////////////////////////////////////////////////////////////

void UploadContextD3D12::UploadTextureImmediate(const TextureUploadD3D12 & upload_info)
{
    MRQ2_ASSERT(m_device != nullptr);
    MRQ2_ASSERT(upload_info.texture->m_resource != nullptr);
    MRQ2_ASSERT(upload_info.mipmaps.mip_dimensions[0].x != 0);
    MRQ2_ASSERT(upload_info.mipmaps.mip_init_data[0] != nullptr);
    MRQ2_ASSERT(upload_info.mipmaps.num_mip_levels >= 1 && upload_info.mipmaps.num_mip_levels <= TextureImage::kMaxMipLevels);

    auto * tex_resource = upload_info.texture->m_resource.Get();
    const uint32_t num_mip_levels = upload_info.mipmaps.num_mip_levels;
    const uint64_t destination_size = GetRequiredIntermediateSize(m_device->device.Get(), tex_resource, 0, num_mip_levels);

    D3D12_RESOURCE_DESC res_desc     = {};
    res_desc.Dimension               = D3D12_RESOURCE_DIMENSION_BUFFER;
    res_desc.Alignment               = 0;
    res_desc.Width                   = destination_size;
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

    D12CHECK(m_command_list->Reset(m_command_allocator.Get(), nullptr));

    D3D12_SUBRESOURCE_DATA sub_res_data[TextureImage::kMaxMipLevels] = {};
    for (uint32_t mip = 0; mip < num_mip_levels; ++mip)
    {
        sub_res_data[mip].pData    = upload_info.mipmaps.mip_init_data[mip];
        sub_res_data[mip].RowPitch = upload_info.mipmaps.mip_dimensions[mip].x * TextureImage::kBytesPerPixel;
    }

    // Texture is a temp scrap already created and in PIXEL_SHADER_RESOURCE state.
    // Transition back to COPY_DEST, update, restore PIXEL_SHADER_RESOURCE.
    if (upload_info.is_scrap)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = tex_resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
        m_command_list->ResourceBarrier(1, &barrier);
    }

    UpdateSubresources<TextureImage::kMaxMipLevels>(m_device->device.Get(), m_command_list.Get(), tex_resource, upload_buffer.Get(), destination_size, 0, num_mip_levels, sub_res_data);

    // Transition back to PIXEL_SHADER_RESOURCE.
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = tex_resource;
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

} // MrQ2
