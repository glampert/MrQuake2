//
// TextureD3D11.cpp
//

#include "TextureD3D11.hpp"
#include "DeviceD3D11.hpp"
#include "UploadContextD3D11.hpp"

namespace MrQ2
{

void TextureD3D11::Init(const DeviceD3D11 & device, const uint32_t width, const uint32_t height, const bool is_scrap, const ColorRGBA32 * const init_data)
{
    MRQ2_ASSERT((width + height) != 0);
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first
    m_device = &device;
}

void TextureD3D11::Init(const TextureD3D11 & other)
{
    MRQ2_ASSERT(m_device == nullptr); // Shutdown first
}

void TextureD3D11::Shutdown()
{
    m_device = nullptr;
}

} // namespace MrQ2
