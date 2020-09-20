//
// TextureVK.hpp
//
#pragma once

#include "UtilsVK.hpp"

namespace MrQ2
{

class DeviceVK;
enum class TextureType : std::uint8_t;

class TextureVK final
{
    friend class UploadContextVK;
    friend class GraphicsContextVK;

public:

    void Init(const DeviceVK & device, const TextureType type, const bool is_scrap,
              const ColorRGBA32 * mip_init_data[], const Vec2u16 mip_dimensions[],
              const uint32_t num_mip_levels, const char * const debug_name);

    // Init from existing texture sharing the resource and sampler/SRV (for the scrap texture)
    void Init(const TextureVK & other);

    void Shutdown();

private:

    const DeviceVK * m_device{ nullptr };
};

} // MrQ2
