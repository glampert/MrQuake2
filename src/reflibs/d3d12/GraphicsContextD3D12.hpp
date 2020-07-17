//
// GraphicsContextD3D12.hpp
//
#pragma once

namespace MrQ2
{

class DeviceD3D12;

class GraphicsContextD3D12 final
{
public:

    GraphicsContextD3D12() = default;

    // Disallow copy.
    GraphicsContextD3D12(const GraphicsContextD3D12 &) = delete;
    GraphicsContextD3D12 & operator=(const GraphicsContextD3D12 &) = delete;

    void Init(const DeviceD3D12 & device) {}
    void Shutdown() {}
};

} // MrQ2
