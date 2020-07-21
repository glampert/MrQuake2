//
// DeviceD3D11.hpp
//
#pragma once

#include "UtilsD3D11.hpp"
#include <string>

namespace MrQ2
{

class UploadContextD3D11;
class GraphicsContextD3D11;

class DeviceD3D11 final
{
    UploadContextD3D11   * m_upload_ctx{ nullptr };
    GraphicsContextD3D11 * m_graphics_ctx{ nullptr };

public:

    bool debug_validation{ false }; // With D3D11 debug validation layer?

    DeviceD3D11() = default;

    // Disallow copy.
    DeviceD3D11(const DeviceD3D11 &) = delete;
    DeviceD3D11 & operator=(const DeviceD3D11 &) = delete;

    void Init(const bool debug, UploadContextD3D11 & up_ctx, GraphicsContextD3D11 & gfx_ctx);
    void Shutdown();

    // Public to renderers/common
    UploadContextD3D11   & UploadContext()   const { return *m_upload_ctx;   }
    GraphicsContextD3D11 & GraphicsContext() const { return *m_graphics_ctx; }
};

} // MrQ2
