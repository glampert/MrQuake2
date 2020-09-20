//
// SwapChainVK.cpp
//

#include "../common/Win32Window.hpp"
#include "SwapChainVK.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// SwapChainVK
///////////////////////////////////////////////////////////////////////////////

void SwapChainVK::Init(const bool fullscreen, const int width, const int height, const bool debug)
{
}

void SwapChainVK::Shutdown()
{
}

void SwapChainVK::Present()
{
}

///////////////////////////////////////////////////////////////////////////////
// SwapChainRenderTargetsVK
///////////////////////////////////////////////////////////////////////////////

void SwapChainRenderTargetsVK::Init(const SwapChainVK & sc, const int width, const int height)
{
    MRQ2_ASSERT((width + height) > 0);

    render_target_width  = width;
    render_target_height = height;
}

void SwapChainRenderTargetsVK::Shutdown()
{
}

} // MrQ2
