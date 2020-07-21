//
// DeviceD3D11.cpp
//

#include "DeviceD3D11.hpp"

namespace MrQ2
{

void DeviceD3D11::Init(const bool debug, UploadContextD3D11 & up_ctx, GraphicsContextD3D11 & gfx_ctx)
{
    m_upload_ctx   = &up_ctx;
    m_graphics_ctx = &gfx_ctx;
}

void DeviceD3D11::Shutdown()
{
    m_upload_ctx   = nullptr;
    m_graphics_ctx = nullptr;
}

} // MrQ2
