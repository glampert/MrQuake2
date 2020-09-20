//
// UploadContextVK.hpp
//
#pragma once

#include "UtilsVK.hpp"

namespace MrQ2
{

class DeviceVK;
class TextureVK;

struct TextureUploadVK final
{
    const TextureVK * texture;
    bool is_scrap;

    struct {
        uint32_t             num_mip_levels;
        const ColorRGBA32 ** mip_init_data;
        const Vec2u16 *      mip_dimensions;
    } mipmaps;
};

class UploadContextVK final
{
public:

    UploadContextVK() = default;

    // Disallow copy.
    UploadContextVK(const UploadContextVK &) = delete;
    UploadContextVK & operator=(const UploadContextVK &) = delete;

    void Init(const DeviceVK & device);
    void Shutdown();

    void UploadTexture(const TextureUploadVK & upload_info);

private:

    const DeviceVK * m_device{ nullptr };
};

} // MrQ2
