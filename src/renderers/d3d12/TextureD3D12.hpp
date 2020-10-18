//
// TextureD3D12.hpp
//
#pragma once

#include "DescriptorHeapD3D12.hpp"

namespace MrQ2
{

enum class TextureType : std::uint8_t;

class TextureD3D12 final
{
    friend class UploadContextD3D12;
    friend class GraphicsContextD3D12;

public:

    TextureD3D12() = default;
    ~TextureD3D12() { Shutdown(); }

    // Disallow copy.
    TextureD3D12(const TextureD3D12 &) = delete;
    TextureD3D12 & operator=(const TextureD3D12 &) = delete;

    void Init(const DeviceD3D12 & device, const TextureType type, const bool is_scrap,
              const ColorRGBA32 * mip_init_data[], const Vec2u16 mip_dimensions[],
              const uint32_t num_mip_levels, const char * const debug_name);

    // Init from existing texture sharing the resource and descriptor (for the scrap texture)
    void Init(const TextureD3D12 & other);

    void Shutdown();

private:

    D12ComPtr<ID3D12Resource> m_resource;
    DescriptorD3D12           m_srv_descriptor{};
    DescriptorD3D12           m_sampler_descriptor{};
    const DeviceD3D12 *       m_device{ nullptr };
    bool                      m_shared_descriptors{ false };

#ifndef NDEBUG
    wchar_t m_debug_name[64] = {};
#endif // NDEBUG

    static D3D12_FILTER FilterForTextureType(const TextureType type);
};

} // MrQ2
