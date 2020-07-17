//
// DeviceD3D12.hpp
//
#pragma once

#include "UtilsD3D12.hpp"
#include <string>

namespace MrQ2
{

class DeviceD3D12 final
{
public:

    D12ComPtr<IDXGIFactory6> factory;
    D12ComPtr<IDXGIAdapter3> adapter;
    D12ComPtr<ID3D12Device5> device;

    size_t dedicated_video_memory{ 0 };
    size_t dedicated_system_memory{ 0 };
    size_t shared_system_memory{ 0 };

    bool supports_rtx{ false };     // Does our graphics card support HW RTX ray tracing?
    bool debug_validation{ false }; // With D3D12 debug validation layer?
    std::string adapter_info{};     // Vendor string.

    void Init(const bool debug);
    void Shutdown();
};

} // MrQ2
