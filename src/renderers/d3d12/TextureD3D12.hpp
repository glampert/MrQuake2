//
// TextureD3D12.hpp
//
#pragma once

#include "DescriptorHeapD3D12.hpp"

namespace MrQ2
{

/*
struct SamplerD3D12 final
{
    // TODO: need this at all???
    // might make it an internal detail of Texture...
    // in D3D12 static samplers are part of the RootSig
};
*/

class TextureD3D12 final
{
    friend class UploadContextD3D12;
    friend class GraphicsContextD3D12;

public:

    void Init(const DeviceD3D12 & device, const uint32_t width, const uint32_t height, const bool is_scrap, const ColorRGBA32* const init_data);
    void Init(const TextureD3D12 & other); // Init from existing texture sharing the resource and descriptor (for the scrap texture)
    void Shutdown();

private:

    D12ComPtr<ID3D12Resource> m_resource;
    DescriptorD3D12           m_srv_descriptor{};
    const DeviceD3D12 *       m_device{ nullptr };
    bool                      m_is_descriptor_shared{ false };
};

} // MrQ2
