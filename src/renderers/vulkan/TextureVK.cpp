//
// TextureVK.cpp
//

#include "../common/TextureStore.hpp"
#include "TextureVK.hpp"
#include "DeviceVK.hpp"

namespace MrQ2
{

void TextureVK::Init(const DeviceVK & device, const TextureType type, const bool is_scrap,
                     const ColorRGBA32 * mip_init_data[], const Vec2u16 mip_dimensions[],
                     const uint32_t num_mip_levels, const char * const debug_name)
{
    MRQ2_ASSERT(num_mip_levels >= 1 && num_mip_levels <= TextureImage::kMaxMipLevels);
    MRQ2_ASSERT((mip_dimensions[0].x + mip_dimensions[0].y) != 0);
    MRQ2_ASSERT(mip_init_data[0] != nullptr);
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first
    (void)debug_name; // unused

    int max_anisotropy = Config::r_max_anisotropy.AsInt();

    if (max_anisotropy > 16)
        max_anisotropy = 16;

    if (max_anisotropy < 1)
        max_anisotropy = 1;

    // TODO

    m_device = &device;
}

void TextureVK::Init(const TextureVK & other)
{
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first
}

void TextureVK::Shutdown()
{
    m_device = nullptr;
}

} // namespace MrQ2
