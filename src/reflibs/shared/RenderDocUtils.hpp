//
// RenderDocUtils.hpp
//  RenderDoc integration to trigger captures from code.
//
#pragma once

namespace MrQ2
{
namespace RenderDocUtils
{

// Global init/shutdown:
bool Initialize();
void Shutdown();
bool IsInitialized();

// Single frame capture triggering:
void TriggerCapture();

} // RenderDocUtils
} // MrQ2
