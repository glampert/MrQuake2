//
// UtilsD3D11.hpp
//
#pragma once

#include "../common/Common.hpp"
#include <wrl.h>
#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_1.h>

namespace MrQ2
{

// Triple-buffering
constexpr uint32_t kD11NumFrameBuffers = 3;

template<typename T>
using D11ComPtr = Microsoft::WRL::ComPtr<T>;

enum class PrimitiveTopologyD3D11 : uint8_t
{
    kTriangleList,
    kTriangleStrip,
    kTriangleFan,

    kCount
};

inline void D11CheckImpl(const HRESULT hr, const char * const msg, const char * const file, const int line)
{
    if (FAILED(hr))
    {
        GameInterface::Errorf("D3D11 Error 0x%x: %s - %s(%d)", hr, msg, file, line);
    }
}

#define D11CHECK(expr) D11CheckImpl((expr), #expr, __FILE__, __LINE__)

} // MrQ2
