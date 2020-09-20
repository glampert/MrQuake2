//
// ShaderProgramVK.hpp
//
#pragma once

#include "UtilsVK.hpp"

namespace MrQ2
{

class DeviceVK;
class PipelineStateVK;
class GraphicsContextVK;

struct VertexInputLayoutVK final
{
    enum ElementType : uint8_t
    {
        kInvalidElementType = 0,

        kVertexPosition,
        kVertexTexCoords,
        kVertexLmCoords,
        kVertexColor,

        kElementTypeCount
    };

    enum ElementFormat : uint8_t
    {
        kInvalidElementFormat = 0,

        kFormatFloat2,
        kFormatFloat3,
        kFormatFloat4,

        kElementFormatCount
    };

    static constexpr uint32_t kMaxVertexElements = 4;

    struct VertexElement
    {
        ElementType   type;
        ElementFormat format;
        uint32_t      offset;
    } elements[kMaxVertexElements];
};

class ShaderProgramVK final
{
    friend PipelineStateVK;
    friend GraphicsContextVK;

public:

    ShaderProgramVK() = default;

    // Disallow copy.
    ShaderProgramVK(const ShaderProgramVK &) = delete;
    ShaderProgramVK & operator=(const ShaderProgramVK &) = delete;

    // Defaults to VS_main/PS_main and debug if the Device has debug validation on.
    bool LoadFromFile(const DeviceVK & device,
                      const VertexInputLayoutVK & input_layout,
                      const char * filename);

    bool LoadFromFile(const DeviceVK & device,
                      const VertexInputLayoutVK & input_layout,
                      const char * filename,
                      const char * vs_entry, const char * ps_entry,
                      const bool debug);

    void Shutdown();
};

} // MrQ2
