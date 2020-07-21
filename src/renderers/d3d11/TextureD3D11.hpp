//
// TextureD3D11.hpp
//
#pragma once

#include "UtilsD3D11.hpp"

namespace MrQ2
{

class DeviceD3D11;

class TextureD3D11 final
{
    friend class UploadContextD3D11;
    friend class GraphicsContextD3D11;

public:

    void Init(const DeviceD3D11 & device, const uint32_t width, const uint32_t height, const bool is_scrap, const ColorRGBA32 * const init_data);
    void Init(const TextureD3D11 & other); // Init from existing texture sharing the resource and descriptor (for the scrap texture)
    void Shutdown();

private:

    const DeviceD3D11 * m_device{ nullptr };
};

} // MrQ2
