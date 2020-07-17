//
// UtilsD3D12.hpp
//
#pragma once

#include "reflibs/shared/Common.hpp"
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>

namespace MrQ2
{

// Triple-buffering
constexpr uint32_t kD12NumFrameBuffers = 3;

template<typename T>
using D12ComPtr = Microsoft::WRL::ComPtr<T>;

inline uint32_t D12Align(const uint32_t alignment, const uint32_t value)
{
    const uint32_t a = alignment - 1u;
    return (value + a) & ~a;
}

template<typename D3D12ClassPtr>
inline void D12SetDebugName(D3D12ClassPtr & obj_ptr, const wchar_t * const str)
{
    obj_ptr->SetName(str);
}

inline void D12CheckImpl(const HRESULT hr, const char * const msg, const char * const file, const int line)
{
    if (FAILED(hr))
    {
        GameInterface::Errorf("D3D12 Error 0x%x: %s - %s(%d)", hr, msg, file, line);
    }
}

#define D12Check(expr) D12CheckImpl((expr), #expr, __FILE__, __LINE__)

} // MrQ2
