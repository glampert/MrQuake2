//
// DLLInterface.hpp
//
#pragma once

#include "RenderInterface.hpp"
#include "TextureStore.hpp"
#include "ModelStore.hpp"
#include "ViewDraw.hpp"

// Quake includes
#include "client/ref.h"
#include "client/vid.h"

namespace MrQ2
{

/*
===============================================================================

    DLLInterface

===============================================================================
*/
class DLLInterface final
{
public:

    static int Init(void * hInst, void * wndProc, int fullscreen);
    static void Shutdown();

    static void BeginRegistration(const char * const map_name);
    static void EndRegistration();
    static void AppActivate(int activate);

    static model_s * RegisterModel(const char * const name);
    static image_s * RegisterSkin(const char * const name);
    static image_s * RegisterPic(const char * const name);

    static void SetSky(const char * const name, const float rotate, vec3_t axis);
    static void DrawGetPicSize(int * out_w, int * out_h, const char * const name);

    static void BeginFrame(float camera_separation);
    static void EndFrame();
    static void RenderView(refdef_t * const view_def);

    static void DrawPic(const int x, const int y, const char * const name);
    static void DrawStretchPic(const int x, const int y, const int w, const int h, const char * const name);
    static void DrawChar(const int x, const int y, int c);
    static void DrawString(int x, int y, const char * s);
    static void DrawTileClear(int x, int y, int w, int h, const char * name);
    static void DrawFill(const int x, const int y, const int w, const int h, const int c);
    static void DrawFadeScreen();
    static void DrawStretchRaw(const int x, const int y, int w, int h, const int cols, const int rows, const qbyte * const data);
    static void CinematicSetPalette(const qbyte * const palette);

    // Not part of the Quake2 DLL renderer interface
    static void DrawAltString(int x, int y, const char * s);
    static void DrawNumberBig(int x, int y, int color, int width, int value);
    static void DrawFpsCounter();

private:

    static RenderInterface sm_renderer;
    static SpriteBatches   sm_sprite_batches;
    static TextureStore    sm_texture_store;
    static ModelStore      sm_model_store;
    static ViewDrawState   sm_view_state;

    struct PerFrameShaderConstants
    {
        vec4_t screen_dimensions;     // Only XY used.
        vec4_t texture_color_scaling; // [debug] multiplied with texture color
        vec4_t vertex_color_scaling;  // [debug] multiplied with vertex color
    };
    static PerFrameShaderConstants sm_per_frame_shader_consts;
    static ScratchConstantBuffers  sm_per_frame_const_buffers;

    struct PerViewShaderConstants
    {
        RenderMatrix view_proj_matrix;
    };
    static PerViewShaderConstants sm_per_view_shader_consts;
    static ScratchConstantBuffers sm_per_view_const_buffers;

    // Cached Cvars:
    static CvarWrapper sm_disable_texturing;
    static CvarWrapper sm_blend_debug_color;
    static CvarWrapper sm_draw_fps_counter;
};

} // MrQ2
