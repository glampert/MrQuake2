//
// ShaderProgramVK.cpp
//

#include "../common/Win32Window.hpp"
#include "ShaderProgramVK.hpp"
#include "DeviceVK.hpp"

namespace MrQ2
{

bool ShaderProgramVK::LoadFromFile(const DeviceVK & device, const VertexInputLayoutVK & input_layout, const char * filename)
{
    return true;
}

bool ShaderProgramVK::LoadFromFile(const DeviceVK & device, const VertexInputLayoutVK & input_layout,
                                   const char * filename, const char * vs_entry, const char * ps_entry, const bool debug)
{
    return true;
}

void ShaderProgramVK::Shutdown()
{
}

} // MrQ2
