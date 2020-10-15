//
// SwapChainD3D11.cpp
//

#include "../common/Win32Window.hpp"
#include "SwapChainD3D11.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// SwapChainD3D11
///////////////////////////////////////////////////////////////////////////////

void SwapChainD3D11::Init(HWND hWnd, const bool fullscreen, const int width, const int height, const bool debug)
{
    // If the project is in a debug build, enable debugging via SDK Layers with this flag.
    UINT create_device_flags = 0;
    if (debug)
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
    const UINT num_driver_types = ArrayLength(driver_types);

    // This array defines the ordering of feature levels that D3D should attempt to create.
    const D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    const UINT num_feature_levels = ArrayLength(feature_levels);

    DXGI_SWAP_CHAIN_DESC sd               = {};
    sd.BufferCount                        = kD11NumFrameBuffers;
    sd.BufferDesc.Width                   = width;
    sd.BufferDesc.Height                  = height;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
    sd.OutputWindow                       = hWnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = !fullscreen;

    HRESULT hr;
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

    // Try to create the device and swap chain:
    for (UINT dti = 0; dti < num_driver_types; ++dti)
    {
        auto driver_type = driver_types[dti];
        hr = D3D11CreateDeviceAndSwapChain(nullptr, driver_type, nullptr, create_device_flags,
                                           feature_levels, num_feature_levels, D3D11_SDK_VERSION,
                                           &sd, m_swap_chain.GetAddressOf(), m_device.GetAddressOf(),
                                           &feature_level, m_context.GetAddressOf());

        if (hr == E_INVALIDARG)
        {
            // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
            hr = D3D11CreateDeviceAndSwapChain(nullptr, driver_type, nullptr, create_device_flags,
                                               &feature_levels[1], num_feature_levels - 1,
                                               D3D11_SDK_VERSION, &sd, m_swap_chain.GetAddressOf(),
                                               m_device.GetAddressOf(), &feature_level,
                                               m_context.GetAddressOf());
        }

        if (SUCCEEDED(hr))
        {
            break;
        }
    }

    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create D3D11 Device or SwapChain!");
    }
    else
    {
        GameInterface::Printf("D3D11 SwapChain initialized.");
    }
}

void SwapChainD3D11::Shutdown()
{
    m_context    = nullptr;
    m_device     = nullptr;
    m_swap_chain = nullptr;
}

void SwapChainD3D11::Present()
{
    auto present_result = m_swap_chain->Present(0, 0);
    if (FAILED(present_result))
    {
        GameInterface::Errorf("SwapChain Present failed: %s", Win32Window::ErrorToString(present_result).c_str());
    }
}

///////////////////////////////////////////////////////////////////////////////
// SwapChainRenderTargetsD3D11
///////////////////////////////////////////////////////////////////////////////

void SwapChainRenderTargetsD3D11::Init(const SwapChainD3D11 & sc, const int width, const int height)
{
    MRQ2_ASSERT((width + height) > 0);

    m_render_target_width  = width;
    m_render_target_height = height;

    //
    // Create a render target view for the framebuffer:
    //

    ID3D11Texture2D * back_buffer_tex = nullptr;
    HRESULT hr = sc.SwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer_tex);
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to get framebuffer from SwapChain!");
    }

    hr = sc.Device()->CreateRenderTargetView(back_buffer_tex, nullptr, m_framebuffer_rtv.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create RTV for the SwapChain framebuffer!");
    }

    m_framebuffer_texture.Attach(back_buffer_tex);

    //
    // Set up the Depth and Stencil buffers:
    //

    // Create depth stencil texture:
    D3D11_TEXTURE2D_DESC desc_depth = {};
    desc_depth.Width                = m_render_target_width;
    desc_depth.Height               = m_render_target_height;
    desc_depth.MipLevels            = 1;
    desc_depth.ArraySize            = 1;
    desc_depth.Format               = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc_depth.SampleDesc.Count     = 1;
    desc_depth.SampleDesc.Quality   = 0;
    desc_depth.Usage                = D3D11_USAGE_DEFAULT;
    desc_depth.BindFlags            = D3D11_BIND_DEPTH_STENCIL;

    hr = sc.Device()->CreateTexture2D(&desc_depth, nullptr, m_depth_stencil_texture.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("Failed to create SwapChain depth/stencil buffer!");
    }

    // Create the depth stencil view:
    D3D11_DEPTH_STENCIL_VIEW_DESC desc_dsv = {};
    desc_dsv.Format        = desc_depth.Format;
    desc_dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;

    hr = sc.Device()->CreateDepthStencilView(m_depth_stencil_texture.Get(), &desc_dsv, m_depth_stencil_view.GetAddressOf());
    if (FAILED(hr))
    {
        GameInterface::Errorf("SwapChain CreateDepthStencilView failed!");
    }

    // Set the frame/depth buffers as current:
    sc.DeviceContext()->OMSetRenderTargets(1, m_framebuffer_rtv.GetAddressOf(), m_depth_stencil_view.Get());

    // Setup a default viewport:
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(m_render_target_width);
    vp.Height   = static_cast<float>(m_render_target_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    sc.DeviceContext()->RSSetViewports(1, &vp);
}

void SwapChainRenderTargetsD3D11::Shutdown()
{
    m_framebuffer_rtv       = nullptr;
    m_framebuffer_texture   = nullptr;
    m_depth_stencil_view    = nullptr;
    m_depth_stencil_texture = nullptr;
}

} // MrQ2
