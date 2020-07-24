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
    MRQ2_ASSERT(upload_info.pixels != nullptr);
    MRQ2_ASSERT(upload_info.width != 0);
    MRQ2_ASSERT(upload_info.texture->m_resource != nullptr);

    const UINT sub_rsrc  = 0; // no mips/slices
    const UINT row_pitch = upload_info.width * 4; // RGBA
    m_device->context->UpdateSubresource(upload_info.texture->m_resource.Get(), sub_rsrc, nullptr, upload_info.pixels, row_pitch, 0);
}

} // MrQ2
