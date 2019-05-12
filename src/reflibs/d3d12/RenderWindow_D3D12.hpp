//
// RenderWindow_D3D12.hpp
//  D3D12 rendering window.
//
#pragma once

#include "reflibs/shared/RefShared.hpp"
#include "reflibs/shared/OSWindow.hpp"

namespace MrQ2
{
namespace D3D12
{

/*
===============================================================================

    D3D12 RenderWindow

===============================================================================
*/
class RenderWindow final
    : public OSWindow
{
public:

    // Enable D3D-level debug validation?
    bool debug_validation = false;

private:

    void InitRenderWindow() override;
};

} // D3D12
} // MrQ2
