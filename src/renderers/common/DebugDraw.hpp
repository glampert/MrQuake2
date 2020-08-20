//
// DebugDraw.hpp
//
#pragma once

#include "RenderInterface.hpp"

namespace MrQ2
{
namespace DebugDraw
{

void Init(const RenderDevice & device);
void Shutdown();

// In world coordinates.
void AddLine(const vec3_t from, const vec3_t to, const ColorRGBA32 color);
void AddAABB(const vec3_t bbox[8], const ColorRGBA32 color);
void AddAABB(const vec3_t mins, const vec3_t maxs, const ColorRGBA32 color);

void BeginFrame();
void EndFrame(GraphicsContext & context, const ConstantBuffer & per_view_constants);

} // DebugDraw
} // MrQ2
