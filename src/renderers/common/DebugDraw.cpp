//
// DebugDraw.cpp
//

#include "DebugDraw.hpp"
#include "TextureStore.hpp"
#include "ImmediateModeBatching.hpp"

namespace MrQ2
{
namespace DebugDraw
{

///////////////////////////////////////////////////////////////////////////////

constexpr uint32_t kMaxDebugLines = 16386;

struct LineVertex
{
    vec3_t position;
    vec4_t rgba;
};

///////////////////////////////////////////////////////////////////////////////

static VertexBuffers<LineVertex> s_lines_buffer;
static ShaderProgram             s_shader_prog;
static PipelineState             s_pipeline_state;
static bool                      s_initialised = false;

///////////////////////////////////////////////////////////////////////////////

void Init(const RenderDevice & device)
{
    MRQ2_ASSERT(!s_initialised);

    s_lines_buffer.Init(device, kMaxDebugLines * 2); // 2 verts per line

    const VertexInputLayout vertex_input_layout = {
        // LineVertex
        {
            { VertexInputLayout::kVertexPosition, VertexInputLayout::kFormatFloat3, offsetof(LineVertex, position) },
            { VertexInputLayout::kVertexColor,    VertexInputLayout::kFormatFloat4, offsetof(LineVertex, rgba) },
        }
    };

    if (!s_shader_prog.LoadFromFile(device, vertex_input_layout, "DrawDebug"))
    {
        GameInterface::Errorf("Failed to load DrawDebug shader!");
    }

    s_pipeline_state.Init(device);
    s_pipeline_state.SetPrimitiveTopology(PrimitiveTopology::kLineList);
    s_pipeline_state.SetShaderProgram(s_shader_prog);
    s_pipeline_state.SetAlphaBlendingEnabled(false);
    s_pipeline_state.SetDepthTestEnabled(true);
    s_pipeline_state.SetDepthWritesEnabled(true);
    s_pipeline_state.SetCullEnabled(false);
    s_pipeline_state.Finalize();

    s_initialised = true;
}

///////////////////////////////////////////////////////////////////////////////

void Shutdown()
{
    if (s_initialised)
    {
        s_lines_buffer.Shutdown();
        s_shader_prog.Shutdown();
        s_pipeline_state.Shutdown();
        s_initialised = false;
    }
}

///////////////////////////////////////////////////////////////////////////////

void AddLine(const vec3_t from, const vec3_t to, const ColorRGBA32 color)
{
    if (!s_initialised)
        return;

    LineVertex * const verts = s_lines_buffer.Increment(2);

    Vec3Copy(from, verts[0].position);
    ColorFloats(color, verts[0].rgba[0], verts[0].rgba[1], verts[0].rgba[2], verts[0].rgba[3]);

    Vec3Copy(to, verts[1].position);
    ColorFloats(color, verts[1].rgba[0], verts[1].rgba[1], verts[1].rgba[2], verts[1].rgba[3]);
}

///////////////////////////////////////////////////////////////////////////////

void AddAABB(const vec3_t bbox[8], const ColorRGBA32 color)
{
    if (!s_initialised)
        return;

    // top lines
    AddLine(bbox[0], bbox[1], color);
    AddLine(bbox[1], bbox[3], color);
    AddLine(bbox[3], bbox[2], color);
    AddLine(bbox[2], bbox[0], color);

    // bottom lines
    AddLine(bbox[4], bbox[5], color);
    AddLine(bbox[5], bbox[7], color);
    AddLine(bbox[7], bbox[6], color);
    AddLine(bbox[6], bbox[4], color);

    // right lines
    AddLine(bbox[4], bbox[0], color);
    AddLine(bbox[6], bbox[2], color);

    // left lines
    AddLine(bbox[5], bbox[1], color);
    AddLine(bbox[7], bbox[3], color);
}

///////////////////////////////////////////////////////////////////////////////

void AddAABB(const vec3_t mins, const vec3_t maxs, const ColorRGBA32 color)
{
    if (!s_initialised)
        return;

    vec3_t bbox[8];

    // top points
    Vec3Copy(mins, bbox[0]);

    Vec3Copy(mins, bbox[1]);
    bbox[1][0] = maxs[0];

    Vec3Copy(maxs, bbox[2]);
    bbox[2][0] = mins[0];
    bbox[2][1] = mins[1];

    Vec3Copy(maxs, bbox[3]);
    bbox[3][1] = mins[1];

    // bottom points
    Vec3Copy(mins, bbox[4]);
    bbox[4][1] = maxs[1];

    Vec3Copy(mins, bbox[5]);
    bbox[5][0] = maxs[0];
    bbox[5][1] = maxs[1];

    Vec3Copy(maxs, bbox[6]);
    bbox[6][0] = mins[0];

    Vec3Copy(maxs, bbox[7]);

    AddAABB(bbox, color);
}

///////////////////////////////////////////////////////////////////////////////

void BeginFrame()
{
    if (!s_initialised)
        return;

    s_lines_buffer.BeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void EndFrame(GraphicsContext & context, const ConstantBuffer & per_view_constants)
{
    if (!s_initialised)
        return;

    const auto draw_buf = s_lines_buffer.EndFrame();

    if (draw_buf.used_verts > 0)
    {
        MRQ2_SCOPED_GPU_MARKER(context, "DebugDraw");

        context.SetPipelineState(s_pipeline_state);
        context.SetVertexBuffer(*draw_buf.buffer_ptr);
        context.SetConstantBuffer(per_view_constants, 0);
        context.SetPrimitiveTopology(PrimitiveTopology::kLineList);
        context.Draw(0, draw_buf.used_verts);
    }
}

///////////////////////////////////////////////////////////////////////////////

} // DebugDraw
} // MrQ2
