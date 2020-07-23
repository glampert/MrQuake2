//
// DeviceD3D11.hpp
//
#pragma once

#include "UtilsD3D11.hpp"

namespace MrQ2
{

class SwapChainD3D11;
class UploadContextD3D11;
class GraphicsContextD3D11;

class DeviceD3D11 final
{
    UploadContextD3D11   * m_upload_ctx{ nullptr };
    GraphicsContextD3D11 * m_graphics_ctx{ nullptr };

public:

    // These are actually owned by the SwapChain as a ComPtr
    ID3D11Device * device{ nullptr };
    ID3D11DeviceContext * context{ nullptr };

    // With D3D11 debug validation layer?
    bool debug_validation{ false };

    DeviceD3D11() = default;

    // Disallow copy.
    DeviceD3D11(const DeviceD3D11 &) = delete;
    DeviceD3D11 & operator=(const DeviceD3D11 &) = delete;

    void Init(const SwapChainD3D11 & sc, const bool debug, UploadContextD3D11 & up_ctx, GraphicsContextD3D11 & gfx_ctx);
    void Shutdown();

    // Public to renderers/common
    UploadContextD3D11   & UploadContext()   const { return *m_upload_ctx;   }
    GraphicsContextD3D11 & GraphicsContext() const { return *m_graphics_ctx; }
};

} // MrQ2
