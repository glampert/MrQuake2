//
// SwapChainVK.hpp
//
#pragma once

#include "UtilsVK.hpp"

namespace MrQ2
{

class SwapChainVK final
{
public:

    SwapChainVK() = default;

    // Disallow copy.
    SwapChainVK(const SwapChainVK &) = delete;
    SwapChainVK & operator=(const SwapChainVK &) = delete;

    void Init(const bool fullscreen, const int width, const int height, const bool debug);
    void Shutdown();
    void Present();
};

class SwapChainRenderTargetsVK final
{
public:

    int render_target_width{ 0 };
    int render_target_height{ 0 };

    SwapChainRenderTargetsVK() = default;

    // Disallow copy.
    SwapChainRenderTargetsVK(const SwapChainRenderTargetsVK &) = delete;
    SwapChainRenderTargetsVK & operator=(const SwapChainRenderTargetsVK &) = delete;

    void Init(const SwapChainVK & sc, const int width, const int height);
    void Shutdown();
};

} // MrQ2
