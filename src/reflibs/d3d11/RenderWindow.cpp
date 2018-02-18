//
// RenderWindow.cpp
//  D3D11 rendering window.
//

#include "RenderWindow.hpp"
#include "reflibs/shared/RefShared.hpp"

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "d3dcompiler")

namespace D3D11
{

///////////////////////////////////////////////////////////////////////////////
// RenderWindow
///////////////////////////////////////////////////////////////////////////////

void RenderWindow::InitRenderWindow()
{
    // If the project is in a debug build, enable debugging via SDK Layers with this flag.
    UINT create_device_flags = 0;
    if (debug_validation)
    {
        create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
        GameInterface::Printf("Creating D3D11 Device with debug validation...");
    }

    // Acceptable driver types.
    const D3D_DRIVER_TYPE driver_types[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    const UINT num_driver_types = ARRAYSIZE(driver_types);

    // This array defines the ordering of feature levels that D3D should attempt to create.
    const D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    const UINT num_feature_levels = ARRAYSIZE(feature_levels);

    DXGI_SWAP_CHAIN_DESC sd               = {0};
    sd.BufferCount                        = 1;
    sd.BufferDesc.Width                   = width;
    sd.BufferDesc.Height                  = height;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hwnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = true;

    HRESULT hr;
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

    // Try to create the device and swap chain:
    for (UINT dti = 0; dti < num_driver_types; ++dti)
    {
        auto driver_type = driver_types[dti];
        hr = D3D11CreateDeviceAndSwapChain(nullptr, driver_type, nullptr, create_device_flags,
                                           feature_levels, num_feature_levels, D3D11_SDK_VERSION,
                                           &sd, swap_chain.GetAddressOf(), device.GetAddressOf(),
                                           &feature_level, device_context.GetAddressOf());

        if (hr == E_INVALIDARG)
        {
            // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
            hr = D3D11CreateDeviceAndSwapChain(nullptr, driver_type, nullptr, create_device_flags,
                                               &feature_levels[1], num_feature_levels - 1,
                                               D3D11_SDK_VERSION, &sd, swap_chain.GetAddressOf(),
                                               device.GetAddressOf(), &feature_level,
                                               device_context.GetAddressOf());
        }

        if (SUCCEEDED(hr))
        {
            break;
        }
    }

    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create D3D device or swap chain!");
    }

    // Create a render target view for the framebuffer:
    ID3D11Texture2D * back_buffer = nullptr;
    hr = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to get framebuffer from swap chain!");
    }

    hr = device->CreateRenderTargetView(back_buffer, nullptr, framebuffer_rtv.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create RTV for the framebuffer!");
    }

    framebuffer_texture.Attach(back_buffer);
    device_context->OMSetRenderTargets(1, framebuffer_rtv.GetAddressOf(), nullptr);

    // Setup a default viewport:
    D3D11_VIEWPORT vp;
    vp.Width    = static_cast<float>(width);
    vp.Height   = static_cast<float>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    device_context->RSSetViewports(1, &vp);

    GameInterface::Printf("D3D11 RenderWindow initialized.");
}

///////////////////////////////////////////////////////////////////////////////

} // D3D11
