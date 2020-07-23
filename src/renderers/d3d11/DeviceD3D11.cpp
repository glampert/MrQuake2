//
// DeviceD3D11.cpp
//

#include "DeviceD3D11.hpp"
#include "SwapChainD3D11.hpp"

namespace MrQ2
{

void DeviceD3D11::Init(const SwapChainD3D11 & sc, const bool debug, UploadContextD3D11 & up_ctx, GraphicsContextD3D11 & gfx_ctx)
{
    m_upload_ctx     = &up_ctx;
    m_graphics_ctx   = &gfx_ctx;
    device           = sc.device.Get();
    context          = sc.context.Get();
    debug_validation = debug;

    // Device creation and shutdown is handled by the SwapChain.
}

void DeviceD3D11::Shutdown()
{
    m_upload_ctx   = nullptr;
    m_graphics_ctx = nullptr;
    device         = nullptr;
    context        = nullptr;
}

} // MrQ2
