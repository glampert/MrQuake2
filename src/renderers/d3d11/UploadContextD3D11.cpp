//
// UploadContextD3D11.cpp
//

#include "UploadContextD3D11.hpp"
#include "TextureD3D11.hpp"
#include "DeviceD3D11.hpp"

namespace MrQ2
{

void UploadContextD3D11::Init(const DeviceD3D11 & device)
{
    MRQ2_ASSERT(m_device == nullptr);
    m_device = &device;
}

void UploadContextD3D11::Shutdown()
{
    m_device = nullptr;
}

void UploadContextD3D11::UploadTextureImmediate(const TextureUploadD3D11 & upload_info)
{
    MRQ2_ASSERT(m_device != nullptr);
}

} // MrQ2
