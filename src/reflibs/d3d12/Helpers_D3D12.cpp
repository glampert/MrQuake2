//
// Helpers_D3D12.cpp
//  Misc helper classes.
//

#include "Helpers_D3D12.hpp"
#include "Impl_D3D12.hpp"

namespace MrQ2
{
namespace D3D12
{

///////////////////////////////////////////////////////////////////////////////
// ShaderProgram
///////////////////////////////////////////////////////////////////////////////

void ShaderProgram::LoadFromFxFile(const wchar_t * filename, const char * vs_entry, const char * ps_entry, const bool debug)
{
    D3DShader::Info shader_info;
    shader_info.vs_entry = vs_entry;
    shader_info.vs_model = "vs_5_0";
    shader_info.ps_entry = ps_entry;
    shader_info.ps_model = "ps_5_0";
    shader_info.debug    = debug;

    D3DShader::LoadFromFxFile(filename, shader_info, &shader_bytecode);
}

///////////////////////////////////////////////////////////////////////////////

void ShaderProgram::CreateRootSignature(ID3D12Device5 * device, const D3D12_ROOT_SIGNATURE_DESC& rootsig_desc)
{
    ComPtr<ID3DBlob> blob;
    if (FAILED(D3D12SerializeRootSignature(&rootsig_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr)))
    {
        GameInterface::Errorf("Failed to serialize RootSignature!");
    }

    if (FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_signature))))
    {
        GameInterface::Errorf("Failed to create RootSignature!");
    }
}

///////////////////////////////////////////////////////////////////////////////
// Buffer
///////////////////////////////////////////////////////////////////////////////

bool Buffer::Init(ID3D12Device5 * device, const uint32_t size_in_bytes)
{
    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type                  = D3D12_HEAP_TYPE_UPLOAD; // Mappable buffer.
    heap_props.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask      = 1;
    heap_props.VisibleNodeMask       = 1;

    D3D12_RESOURCE_DESC res_desc     = {};
    res_desc.Dimension               = D3D12_RESOURCE_DIMENSION_BUFFER;
    res_desc.Alignment               = 0; // Must be zero for buffers.
    res_desc.Width                   = size_in_bytes;
    res_desc.Height                  = 1;
    res_desc.DepthOrArraySize        = 1;
    res_desc.MipLevels               = 1;
    res_desc.SampleDesc              = { 1, 0 };
    res_desc.Layout                  = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    res_desc.Format                  = DXGI_FORMAT_UNKNOWN;
    res_desc.Flags                   = D3D12_RESOURCE_FLAG_NONE;

    const D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_GENERIC_READ;

    if (FAILED(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, states, nullptr, IID_PPV_ARGS(&resource))))
    {
        GameInterface::Printf("WARNING: CreateCommittedResource failed for new buffer resource!");
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void * Buffer::Map()
{
    D3D12_RANGE range = {}; // No range specified.
    void * memory = nullptr;
    Dx12Check(resource->Map(0, &range, &memory));
    return memory;
}

///////////////////////////////////////////////////////////////////////////////

void Buffer::Unmap()
{
    D3D12_RANGE range = {}; // No range specified.
    resource->Unmap(0, &range);
}

///////////////////////////////////////////////////////////////////////////////
// UploadContext
///////////////////////////////////////////////////////////////////////////////

void UploadContext::Init(ID3D12Device5 * device)
{
    Dx12Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

    m_fence_event = CreateEventEx(nullptr, L"UploadContextFence", FALSE, EVENT_ALL_ACCESS);
    if (m_fence_event == nullptr)
    {
        GameInterface::Errorf("Failed to create fence event.");
    }

    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.NodeMask = 1;

    Dx12Check(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_cmd_queue)));
    Dx12Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmd_allocator)));
    Dx12Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmd_allocator.Get(), nullptr, IID_PPV_ARGS(&m_gfx_cmd_list)));
    Dx12Check(m_gfx_cmd_list->Close());

    Dx12SetDebugName(m_cmd_queue, L"UploadContextCmdQueue");
    Dx12SetDebugName(m_gfx_cmd_list, L"UploadContextGfxCmdList");
}

///////////////////////////////////////////////////////////////////////////////

/*
void UploadContext::UploadTextureSync(const D3D12_TEXTURE_COPY_LOCATION & src_location, const D3D12_TEXTURE_COPY_LOCATION & dst_location, const D3D12_RESOURCE_BARRIER & barrier)
{
    Dx12Check(m_gfx_cmd_list->Reset(m_cmd_allocator.Get(), nullptr));
    m_gfx_cmd_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);
    m_gfx_cmd_list->ResourceBarrier(1, &barrier);
    Dx12Check(m_gfx_cmd_list->Close());

    ID3D12CommandList * cmd_lists[] = { m_gfx_cmd_list.Get() };
    m_cmd_queue->ExecuteCommandLists(1, cmd_lists);

    Dx12Check(m_cmd_queue->Signal(m_fence.Get(), m_next_fence_value));
    Dx12Check(m_fence->SetEventOnCompletion(m_next_fence_value, m_fence_event));

    if (WaitForSingleObjectEx(m_fence_event, INFINITE, FALSE) != WAIT_OBJECT_0)
    {
        GameInterface::Errorf("WaitForSingleObject failed! Error: %u", ::GetLastError());
    }

    ++m_next_fence_value;
}
*/

///////////////////////////////////////////////////////////////////////////////

void UploadContext::UploadTextureSync(const Texture & tex_to_upload, ID3D12Device5 * device)
{
    const uint32_t bytes_per_pixel   = 4; // All our textures are RGBA8
    const uint32_t width             = tex_to_upload.width;
    const uint32_t height            = tex_to_upload.height;
    const uint32_t upload_pitch      = Dx12Align(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, width * bytes_per_pixel);
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

    ComPtr<ID3D12Resource> upload_buffer;
    Dx12Check(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_buffer)));
    Dx12SetDebugName(upload_buffer, L"TextureUploadBuffer");

    void * mapped_ptr = nullptr;
    const D3D12_RANGE map_range = { 0, upload_size };
    Dx12Check(upload_buffer->Map(0, &map_range, &mapped_ptr));
    {
        auto * dest = reinterpret_cast<uint8_t *>(mapped_ptr);
        auto * src  = reinterpret_cast<const uint8_t *>(tex_to_upload.pixels);

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
    dst_location.pResource                          = tex_to_upload.resource.Get();
    dst_location.Type                               = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_location.SubresourceIndex                   = 0;

    Dx12Check(m_gfx_cmd_list->Reset(m_cmd_allocator.Get(), nullptr));

    m_gfx_cmd_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

    // Regular textures (non-scrap) are never updated, so we can transition them to shader resource.
    if (!tex_to_upload.from_scrap)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = tex_to_upload.resource.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        m_gfx_cmd_list->ResourceBarrier(1, &barrier);
    }

    Dx12Check(m_gfx_cmd_list->Close());

    ID3D12CommandList * cmd_lists[] = { m_gfx_cmd_list.Get() };
    m_cmd_queue->ExecuteCommandLists(1, cmd_lists);

    Dx12Check(m_cmd_queue->Signal(m_fence.Get(), m_next_fence_value));
    Dx12Check(m_fence->SetEventOnCompletion(m_next_fence_value, m_fence_event));

    if (WaitForSingleObjectEx(m_fence_event, INFINITE, FALSE) != WAIT_OBJECT_0)
    {
        GameInterface::Errorf("WaitForSingleObjectEx failed! Error: %u", ::GetLastError());
    }

    ++m_next_fence_value;
}

///////////////////////////////////////////////////////////////////////////////

UploadContext::~UploadContext()
{
    if (m_fence_event != nullptr)
    {
        CloseHandle(m_fence_event);
    }
}

///////////////////////////////////////////////////////////////////////////////
// SpriteBatch
///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::Init(ID3D12Device5 * device, const int max_verts)
{
    m_buffers.Init(device, "SpriteBatch", max_verts);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::BeginFrame()
{
    m_buffers.Begin();
}

///////////////////////////////////////////////////////////////////////////////

//void SpriteBatch::EndFrame(GraphicsContext & ctx, const ShaderProgram & shader, const Texture * opt_tex_atlas)
void SpriteBatch::EndFrame(ID3D12GraphicsCommandList * gfx_cmd_list, ID3D12PipelineState * pipeline_state, const Texture * opt_tex_atlas)
{
    const auto draw_buf = m_buffers.End();

    gfx_cmd_list->IASetVertexBuffers(0, 1, &draw_buf.buffer_ptr->view);
    gfx_cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    gfx_cmd_list->SetPipelineState(pipeline_state);

    // Fast path - one texture for the whole batch:
    if (opt_tex_atlas != nullptr)
    {
        gfx_cmd_list->SetGraphicsRootDescriptorTable(1, opt_tex_atlas->srv_descriptor.gpu_handle);
        gfx_cmd_list->DrawInstanced(draw_buf.used_verts, 1, 0, 0);
    }
    else // Handle small unique textured draws:
    {
        D3D12_GPU_DESCRIPTOR_HANDLE previous_srv_handle = {};
        for (const auto & d : m_deferred_textured_quads)
        {
            if (d.tex->srv_descriptor.gpu_handle.ptr != previous_srv_handle.ptr)
            {
                gfx_cmd_list->SetGraphicsRootDescriptorTable(1, d.tex->srv_descriptor.gpu_handle);
                previous_srv_handle = d.tex->srv_descriptor.gpu_handle;
            }

            gfx_cmd_list->DrawInstanced(/*vertex_count=*/6, 1, d.quad_start_vtx, 0);
        }
    }

    // Clear cache for next frame:
    if (!m_deferred_textured_quads.empty())
    {
        m_deferred_textured_quads.clear();
    }
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushTriVerts(const DrawVertex2D tri[3])
{
    DrawVertex2D * verts = Increment(3);
    std::memcpy(verts, tri, sizeof(DrawVertex2D) * 3);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadVerts(const DrawVertex2D quad[4])
{
    DrawVertex2D * tri = Increment(6);       // Expand quad into 2 triangles
    const int indexes[6] = { 0,1,2, 2,3,0 }; // CW winding
    for (int i = 0; i < 6; ++i)
    {
        tri[i] = quad[indexes[i]];
    }
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuad(const float x, const float y, const float w, const float h,
                           const float u0, const float v0, const float u1, const float v1,
                           const DirectX::XMFLOAT4A & color)
{
    DrawVertex2D quad[4];
    quad[0].xy_uv[0] = x;
    quad[0].xy_uv[1] = y;
    quad[0].xy_uv[2] = u0;
    quad[0].xy_uv[3] = v0;
    quad[0].rgba[0]  = color.x;
    quad[0].rgba[1]  = color.y;
    quad[0].rgba[2]  = color.z;
    quad[0].rgba[3]  = color.w;
    quad[1].xy_uv[0] = x + w;
    quad[1].xy_uv[1] = y;
    quad[1].xy_uv[2] = u1;
    quad[1].xy_uv[3] = v0;
    quad[1].rgba[0]  = color.x;
    quad[1].rgba[1]  = color.y;
    quad[1].rgba[2]  = color.z;
    quad[1].rgba[3]  = color.w;
    quad[2].xy_uv[0] = x + w;
    quad[2].xy_uv[1] = y + h;
    quad[2].xy_uv[2] = u1;
    quad[2].xy_uv[3] = v1;
    quad[2].rgba[0]  = color.x;
    quad[2].rgba[1]  = color.y;
    quad[2].rgba[2]  = color.z;
    quad[2].rgba[3]  = color.w;
    quad[3].xy_uv[0] = x;
    quad[3].xy_uv[1] = y + h;
    quad[3].xy_uv[2] = u0;
    quad[3].xy_uv[3] = v1;
    quad[3].rgba[0]  = color.x;
    quad[3].rgba[1]  = color.y;
    quad[3].rgba[2]  = color.z;
    quad[3].rgba[3]  = color.w;
    PushQuadVerts(quad);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadTextured(const float x, const float y,
                                   const float w, const float h,
                                   const TextureImage * const tex,
                                   const DirectX::XMFLOAT4A & color)
{
    FASTASSERT(tex != nullptr);
    const int quad_start_vtx = m_buffers.CurrentPosition();
    PushQuad(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color);
    m_deferred_textured_quads.push_back({ quad_start_vtx, static_cast<const TextureImageImpl *>(tex) });
}

///////////////////////////////////////////////////////////////////////////////

void SpriteBatch::PushQuadTexturedUVs(const float x, const float y,
                                      const float w, const float h,
                                      const float u0, const float v0,
                                      const float u1, const float v1,
                                      const TextureImage * const tex,
                                      const DirectX::XMFLOAT4A & color)
{
    FASTASSERT(tex != nullptr);
    const int quad_start_vtx = m_buffers.CurrentPosition();
    PushQuad(x, y, w, h, u0, v0, u1, v1, color);
    m_deferred_textured_quads.push_back({ quad_start_vtx, static_cast<const TextureImageImpl *>(tex) });
}

///////////////////////////////////////////////////////////////////////////////

} // D3D12
} // MrQ2
