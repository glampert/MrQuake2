//
// UploadContextD3D11.hpp
//
#pragma once

#include "UtilsD3D11.hpp"

namespace MrQ2
{

class DeviceD3D11;
class TextureD3D11;

struct TextureUploadD3D11 final
{
    const TextureD3D11 * texture;
    bool is_scrap;

    struct {
        uint32_t             num_mip_levels;
        const ColorRGBA32 ** mip_init_data;
        const Vec2u16 *      mip_dimensions;
    } mipmaps;
};

class UploadContextD3D11 final
{
public:

    UploadContextD3D11() = default;

    // Disallow copy.
    UploadContextD3D11(const UploadContextD3D11 &) = delete;
    UploadContextD3D11 & operator=(const UploadContextD3D11 &) = delete;

    void Init(const DeviceD3D11 & device);
    void Shutdown();

    void UploadTextureImmediate(const TextureUploadD3D11 & upload_info);

private:

    const DeviceD3D11 * m_device{ nullptr };
};

} // MrQ2
