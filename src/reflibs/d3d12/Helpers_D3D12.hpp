//
// Helpers_D3D12.hpp
//  Misc helper classes.
//
#pragma once

#include "RenderWindow_D3D12.hpp"
#include "reflibs/shared/MiniImBatch.hpp"
#include "reflibs/shared/TextureStore.hpp"
#include "reflibs/shared/d3d/D3DShader.hpp"
#include <DirectXMath.h>
#include <array>

namespace MrQ2
{
namespace D3D12
{

// TODO: TEMP - rename
class TextureImageImpl;
using Texture = TextureImageImpl;

/*
===============================================================================

    D3D12 ShaderProgram

===============================================================================
*/
struct ShaderProgram final
{
    ComPtr<ID3D12RootSignature> root_signature;
    D3DShader::Blobs            shader_bytecode;

    void LoadFromFxFile(const wchar_t * filename, const char * vs_entry, const char * ps_entry, const bool debug);
    void CreateRootSignature(ID3D12Device5 * device, const D3D12_ROOT_SIGNATURE_DESC & rootsig_desc);
};

struct PipelineState final
{
    ComPtr<ID3D12PipelineState> pso;

    void CreatePso(ID3D12Device5 * device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC & pso_desc)
    {
        Dx12Check(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso)));
    }
};

/* IDEA

// NOTE: Possibly could have a single RootSignature if all shaders have the same set of constants and texture inputs

// class with constants and pipeline state derives from ShaderProgram to define a custom shader
class UiShader : public ShaderProgram
{
	struct VertexShaderConstants
	{
		...
	} vs_constants;

	struct PixelShaderConstants
	{
		...
	} ps_constants;

	struct Resources
	{
		const Texture* texture1;
		Sampler sampler1;

		const Texture* texture2;
		Sampler sampler2;
		...
	} resources;

	// override from ShaderProgram
	void UploadConstants() override; // update the cbuffers from CPU data
	void UploadResources() override; // upload textures to the GPU
	void SetPipelineState() override;
	// etc...
};

*/

/*
===============================================================================

    D3D12 Buffer

===============================================================================
*/
struct Buffer
{
    ComPtr<ID3D12Resource> resource;

    bool Init(ID3D12Device5 * device, const uint32_t size_in_bytes);
    void * Map();
    void Unmap();
};

struct VertexBuffer final : public Buffer
{
    D3D12_VERTEX_BUFFER_VIEW view = {};

    bool Init(ID3D12Device5 * device, const uint32_t buffer_size_in_bytes, const uint32_t vertex_stride_in_bytes)
    {
        const bool buffer_ok = Buffer::Init(device, buffer_size_in_bytes);
        if (buffer_ok)
        {
            view.BufferLocation = resource->GetGPUVirtualAddress();
            view.SizeInBytes    = buffer_size_in_bytes;
            view.StrideInBytes  = vertex_stride_in_bytes;
        }
        return buffer_ok;
    }
};

struct ConstantBuffer final : Buffer
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC view = {};

    bool Init(ID3D12Device5 * device, const uint32_t buffer_size_in_bytes)
    {
        const bool buffer_ok = Buffer::Init(device, buffer_size_in_bytes);
        if (buffer_ok)
        {
            view.BufferLocation = resource->GetGPUVirtualAddress();
            view.SizeInBytes    = buffer_size_in_bytes;
        }
        return buffer_ok;
    }

    template<typename T>
    void WriteStruct(const T & cbuffer_data)
    {
        void * cbuffer_upload_mem = Map();
        std::memcpy(cbuffer_upload_mem, &cbuffer_data, sizeof(T));
        Unmap();
    }
};

class ScratchConstantBuffers final
{
    uint32_t m_current_buffer = 0;
    ConstantBuffer m_cbuffers[kNumFrameBuffers];

public:

    void Init(ID3D12Device5 * device, const uint32_t buffer_size_in_bytes)
    {
        for (ConstantBuffer & cbuf : m_cbuffers)
        {
            const bool buffer_ok = cbuf.Init(device, buffer_size_in_bytes);
            FASTASSERT(buffer_ok);
        }
    }

    ConstantBuffer & GetCurrent()
    {
        FASTASSERT(m_current_buffer < ArrayLength(m_cbuffers));
        return m_cbuffers[m_current_buffer];
    }

    void MoveToNextFrame()
    {
        m_current_buffer = (m_current_buffer + 1) % kNumFrameBuffers;
    }
};

/*
class GraphicsCmdList final
{
public:

    // Frame start/end
    void BeginFrame(const float clear_color[4], const float clear_depth, const uint8_t clear_stencil);
    void EndFrame();

    // Render states
    void SetVertexBuffer(const VertexBuffer & vb);
    void SetConstantBuffer(const ConstantBuffer & cb);
    void SetTexture(const Texture & tex);
    void SetViewport(const uint32_t x, const uint32_t y, const uint32_t w, const uint32_t h);
    void SetScissorRect(const uint32_t x, const uint32_t y, const uint32_t w, const uint32_t h);

    // this would actually be part of the PipelineState...
//    void SetShaderProgram(const ShaderProgram & shader);
//    void SetPrimitiveTopology(const PrimitiveTopology topology);
//    void EnableAlphaBlending();
//    void DisableAlphaBlending();
//    void EnableDepthTest();
//    void DisableDepthTest();
//    void EnableDepthWrites();
//    void DisableDepthWrites();

    // Draw calls
    void Draw(const uint32_t first_vertex, const uint32_t vertex_count);

    // Debug markers
    void PushMarkerf(const wchar_t * format, ...);
    void PushMarker(const wchar_t * name);
    void PopMarker();
};
*/

class UploadContext final
{
public:

    void Init(ID3D12Device5 * device);

    //void UploadTextureSync(const D3D12_TEXTURE_COPY_LOCATION & src_location, const D3D12_TEXTURE_COPY_LOCATION & dst_location, const D3D12_RESOURCE_BARRIER & barrier);

    void UploadTextureSync(const Texture & tex_to_upload, ID3D12Device5 * device);

    // TODO: UploadTextureAsync variant that enqueues the upload buffer and does not sync until later on?

    ~UploadContext();

private:

    ComPtr<ID3D12Fence>               m_fence;
    ComPtr<ID3D12CommandQueue>        m_cmd_queue;
    ComPtr<ID3D12CommandAllocator>    m_cmd_allocator;
    ComPtr<ID3D12GraphicsCommandList> m_gfx_cmd_list;
    HANDLE                            m_fence_event = nullptr;
    uint64_t                          m_next_fence_value = 1;
};

struct Descriptor final
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;
};

// Shader Resource View (SRV) descriptor heap
class DescriptorHeap final
{
public:

    void Init(ID3D12Device5 * device, const uint32_t num_srv_descriptors)
    {
        FASTASSERT(num_srv_descriptors != 0);

        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
        heap_desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_desc.NumDescriptors             = num_srv_descriptors;
        heap_desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        Dx12Check(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_srv_descriptor_heap)));
        Dx12SetDebugName(m_srv_descriptor_heap, L"SRVDescriptorHeap");

        m_srv_cpu_heap_start   = m_srv_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
        m_srv_gpu_heap_start   = m_srv_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
        m_srv_descriptor_size  = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_srv_descriptor_count = num_srv_descriptors;
    }

    Descriptor AllocateShaderVisibleDescriptor()
    {
        FASTASSERT(m_srv_descriptor_heap != nullptr);

        if (m_srv_descriptors_used == m_srv_descriptor_count)
        {
            GameInterface::Errorf("Out of SRV descriptors! Max = %u", m_srv_descriptor_count);
        }

        Descriptor d;

        d.cpu_handle = m_srv_cpu_heap_start;
        m_srv_cpu_heap_start.ptr += m_srv_descriptor_size;

        d.gpu_handle = m_srv_gpu_heap_start;
        m_srv_gpu_heap_start.ptr += m_srv_descriptor_size;

        ++m_srv_descriptors_used;

        return d;
    }

    ID3D12DescriptorHeap * const * GetHeapAddr() const { return m_srv_descriptor_heap.GetAddressOf(); }

private:

    ComPtr<ID3D12DescriptorHeap> m_srv_descriptor_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE  m_srv_cpu_heap_start   = {};
    D3D12_GPU_DESCRIPTOR_HANDLE  m_srv_gpu_heap_start   = {};
    uint32_t                     m_srv_descriptor_size  = 0;
    uint32_t                     m_srv_descriptor_count = 0;
    uint32_t                     m_srv_descriptors_used = 0;
};

/*
===============================================================================

    D3D12 VertexBuffers

===============================================================================
*/
template<typename VertexType, uint32_t kNumBuffers>
class VertexBuffers final
{
public:

    VertexBuffers() = default;

    struct DrawBuffer
    {
        VertexBuffer * buffer_ptr;
        int            used_verts;
    };

    void Init(ID3D12Device5 * device, const char * const debug_name, const int max_verts)
    {
        FASTASSERT(device != nullptr);

        m_num_verts  = max_verts;
        m_debug_name = debug_name;

        const uint32_t buffer_size_in_bytes   = sizeof(VertexType) * max_verts;
        const uint32_t vertex_stride_in_bytes = sizeof(VertexType);

        for (unsigned b = 0; b < m_vertex_buffers.size(); ++b)
        {
            if (!m_vertex_buffers[b].Init(device, buffer_size_in_bytes, vertex_stride_in_bytes))
            {
                GameInterface::Errorf("Failed to create %s vertex buffer %u", debug_name, b);
            }
            m_mapped_ptrs[b] = nullptr;
        }

        MemTagsTrackAlloc(buffer_size_in_bytes * kNumBuffers, MemTag::kVertIndexBuffer);
        GameInterface::Printf("%s used %s", debug_name, FormatMemoryUnit(buffer_size_in_bytes * kNumBuffers));
    }

    VertexType * Increment(const int count)
    {
        FASTASSERT(count > 0 && count <= m_num_verts);

        VertexType * verts = m_mapped_ptrs[m_buffer_index];
        FASTASSERT(verts != nullptr);
        FASTASSERT_ALIGN16(verts);

        verts += m_used_verts;
        m_used_verts += count;

        if (m_used_verts > m_num_verts)
        {
            GameInterface::Errorf("%s vertex buffer overflowed! used_verts=%i, num_verts=%i. "
                                  "Increase size.", m_debug_name, m_used_verts, m_num_verts);
        }

        return verts;
    }

    int BufferSize() const
    {
        return m_num_verts;
    }

    int NumVertsRemaining() const
    {
        FASTASSERT((m_num_verts - m_used_verts) > 0);
        return m_num_verts - m_used_verts;
    }

    int CurrentPosition() const
    {
        return m_used_verts;
    }

    VertexType * CurrentVertexPtr() const
    {
        return m_mapped_ptrs[m_buffer_index] + m_used_verts;
    }

    void Begin()
    {
        FASTASSERT(m_used_verts == 0); // Missing End()?

        // Map the current buffer:
        void * const memory = m_vertex_buffers[m_buffer_index].Map();
        if (memory == nullptr)
        {
            GameInterface::Errorf("Failed to map %s vertex buffer %i", m_debug_name, m_buffer_index);
        }

        FASTASSERT_ALIGN16(memory);
        m_mapped_ptrs[m_buffer_index] = static_cast<VertexType *>(memory);
    }

    DrawBuffer End()
    {
        FASTASSERT(m_mapped_ptrs[m_buffer_index] != nullptr); // Missing Begin()?

        VertexBuffer & current_buffer = m_vertex_buffers[m_buffer_index];
        const int current_position = m_used_verts;

        // Unmap current buffer so we can draw with it:
        current_buffer.Unmap();
        m_mapped_ptrs[m_buffer_index] = nullptr;

        // Move to the next buffer:
        m_buffer_index = (m_buffer_index + 1) % kNumBuffers;
        m_used_verts = 0;

        return { &current_buffer, current_position };
    }

private:

    int m_num_verts    = 0;
    int m_used_verts   = 0;
    int m_buffer_index = 0;

    std::array<VertexBuffer, kNumBuffers> m_vertex_buffers{};
    std::array<VertexType *, kNumBuffers> m_mapped_ptrs{};

    const char * m_debug_name = nullptr;
};

/*
===============================================================================

    D3D12 SpriteBatch

===============================================================================
*/
class SpriteBatch final
{
public:

    SpriteBatch() = default;
    void Init(ID3D12Device5 * device, int max_verts);

    void BeginFrame();
	void EndFrame(ID3D12GraphicsCommandList * gfx_cmd_list, ID3D12PipelineState * pipeline_state, const Texture * opt_tex_atlas = nullptr);

    DrawVertex2D * Increment(const int count) { return m_buffers.Increment(count); }

    void PushTriVerts(const DrawVertex2D tri[3]);
    void PushQuadVerts(const DrawVertex2D quad[4]);

    void PushQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  const DirectX::XMFLOAT4A & color);

    void PushQuadTextured(float x, float y, float w, float h,
                          const TextureImage * tex, const DirectX::XMFLOAT4A & color);

    void PushQuadTexturedUVs(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                             const TextureImage * tex, const DirectX::XMFLOAT4A & color);

    // Disallow copy.
    SpriteBatch(const SpriteBatch &) = delete;
    SpriteBatch & operator=(const SpriteBatch &) = delete;

private:

    VertexBuffers<DrawVertex2D, kNumFrameBuffers> m_buffers;

    struct DeferredTexQuad
    {
        int quad_start_vtx;
        const TextureImageImpl * tex;
    };
    std::vector<DeferredTexQuad> m_deferred_textured_quads;
};

} // D3D12
} // MrQ2
