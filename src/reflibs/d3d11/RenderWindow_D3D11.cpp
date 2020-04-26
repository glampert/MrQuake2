//
// RenderWindow_D3D11.cpp
//  D3D11 rendering window.
//

#include "RenderWindow_D3D11.hpp"
#include "reflibs/shared/RefShared.hpp"

namespace MrQ2
{
namespace D3D11
{

///////////////////////////////////////////////////////////////////////////////
// RenderWindow
///////////////////////////////////////////////////////////////////////////////

void RenderWindow::InitRenderWindow()
{
    // If the project is in a debug build, enable debugging via SDK Layers with this flag.
    UINT create_device_flags = 0;
    if (m_debug_validation)
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

    DXGI_SWAP_CHAIN_DESC sd               = {};
    sd.BufferCount                        = kNumFrameBuffers;
    sd.BufferDesc.Width                   = m_width;
    sd.BufferDesc.Height                  = m_height;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = m_hWnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = !m_fullscreen;

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

    //
    // Create a render target view for the framebuffer:
    //

    ID3D11Texture2D * back_buffer_tex = nullptr;
    hr = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer_tex);
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to get framebuffer from swap chain!");
    }

    hr = device->CreateRenderTargetView(back_buffer_tex, nullptr, framebuffer_rtv.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create RTV for the framebuffer!");
    }

    framebuffer_texture.Attach(back_buffer_tex);

    //
    // Set up the Depth and Stencil buffer:
    //

    // Create depth stencil texture:
    D3D11_TEXTURE2D_DESC desc_depth = {};
    desc_depth.Width                = m_width;
    desc_depth.Height               = m_height;
    desc_depth.MipLevels            = 1;
    desc_depth.ArraySize            = 1;
    desc_depth.Format               = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc_depth.SampleDesc.Count     = 1;
    desc_depth.SampleDesc.Quality   = 0;
    desc_depth.Usage                = D3D11_USAGE_DEFAULT;
    desc_depth.BindFlags            = D3D11_BIND_DEPTH_STENCIL;

    hr = device->CreateTexture2D(&desc_depth, nullptr, depth_stencil_texture.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create depth/stencil buffer!");
    }

    // Create the depth stencil view:
    D3D11_DEPTH_STENCIL_VIEW_DESC desc_dsv = {};
    desc_dsv.Format        = desc_depth.Format;
    desc_dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;

    hr = device->CreateDepthStencilView(depth_stencil_texture.Get(), &desc_dsv, depth_stencil_view.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("CreateDepthStencilView failed!");
    }

    // Set the frame/depth buffers as current:
    device_context->OMSetRenderTargets(1, framebuffer_rtv.GetAddressOf(), depth_stencil_view.Get());

    // Setup a default viewport:
    D3D11_VIEWPORT vp;
    vp.Width    = static_cast<float>(m_width);
    vp.Height   = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    device_context->RSSetViewports(1, &vp);

    GameInterface::Printf("D3D11 RenderWindow initialized.");
}

///////////////////////////////////////////////////////////////////////////////

} // D3D11
} // MrQ2
