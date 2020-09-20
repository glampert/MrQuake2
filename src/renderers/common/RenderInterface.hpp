//
// RenderInterface.hpp
//  Exposed the renderer back-end header for the DLL we're building.
//
#pragma once

#if defined(MRQ2_RENDERER_DLL_D3D12)
    #include "../d3d12/RenderInterfaceD3D12.hpp"
#elif defined(MRQ2_RENDERER_DLL_D3D11)
    #include "../d3d11/RenderInterfaceD3D11.hpp"
#elif defined(MRQ2_RENDERER_DLL_VULKAN)
    #include "../vulkan/RenderInterfaceVK.hpp"
#else
    #error "Missing renderer DLL switch?"
#endif
