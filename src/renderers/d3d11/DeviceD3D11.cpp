//
// DeviceD3D11.cpp
//

#include "DeviceD3D11.hpp"
#include "SwapChainD3D11.hpp"

namespace MrQ2
{

void DeviceD3D11::Init(const SwapChainD3D11 & sc, UploadContextD3D11 & up_ctx, GraphicsContextD3D11 & gfx_ctx, const bool debug)
{
    m_upload_ctx       = &up_ctx;
    m_graphics_ctx     = &gfx_ctx;
    m_device           = sc.Device();
    m_context          = sc.DeviceContext();
    m_debug_validation = debug;

    D11CHECK(sc.Device()->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 1, &m_multisample_quality_levels_rgba));

    // Device creation and shutdown is handled by the SwapChain.
}

void DeviceD3D11::Shutdown()
{
    m_upload_ctx   = nullptr;
    m_graphics_ctx = nullptr;
    m_device       = nullptr;
    m_context      = nullptr;
}

uint32_t DeviceD3D11::MultisampleQualityLevel(const DXGI_FORMAT fmt) const
{
    MRQ2_ASSERT(fmt == DXGI_FORMAT_R8G8B8A8_UNORM); // only format supported at the moment
    (void)fmt;
    return m_multisample_quality_levels_rgba;
}

} // MrQ2
