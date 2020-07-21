//
// DLLInterface.cpp
//

#include "DLLInterface.hpp"
#include "RenderDocUtils.hpp"

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////
// DLLInterface
///////////////////////////////////////////////////////////////////////////////

RenderInterface DLLInterface::sm_renderer;
SpriteBatches   DLLInterface::sm_sprite_batches;
TextureStore    DLLInterface::sm_texture_store;
ModelStore      DLLInterface::sm_model_store;
ViewDrawState   DLLInterface::sm_view_state;

// Constant buffers:
ConstBuffers<DLLInterface::PerFrameShaderConstants> DLLInterface::sm_per_frame_shader_consts;
ConstBuffers<DLLInterface::PerViewShaderConstants>  DLLInterface::sm_per_view_shader_consts;

// Cached Cvars:
CvarWrapper DLLInterface::sm_disable_texturing;
CvarWrapper DLLInterface::sm_blend_debug_color;
CvarWrapper DLLInterface::sm_draw_fps_counter;

///////////////////////////////////////////////////////////////////////////////

int DLLInterface::Init(void * hInst, void * wndProc, int fullscreen)
{
    auto vid_mode    = GameInterface::Cvar::Get("vid_mode",    "6",    CvarWrapper::kFlagArchive);
    auto vid_width   = GameInterface::Cvar::Get("vid_width",   "1024", CvarWrapper::kFlagArchive);
    auto vid_height  = GameInterface::Cvar::Get("vid_height",  "768",  CvarWrapper::kFlagArchive);
    auto r_renderdoc = GameInterface::Cvar::Get("r_renderdoc", "0",    CvarWrapper::kFlagArchive);
    auto r_debug     = GameInterface::Cvar::Get("r_debug",     "0",    CvarWrapper::kFlagArchive);

    sm_disable_texturing = GameInterface::Cvar::Get("r_disable_texturing", "0", 0);
    sm_blend_debug_color = GameInterface::Cvar::Get("r_blend_debug_color", "0", 0);
    sm_draw_fps_counter  = GameInterface::Cvar::Get("r_draw_fps_counter",  "0", CvarWrapper::kFlagArchive);

    int w, h;
    if (!GameInterface::Video::GetModeInfo(w, h, vid_mode.AsInt()))
    {
        // An invalid vid_mode (i.e.: -1) uses the explicit size
        w = vid_width.AsInt();
        h = vid_height.AsInt();
    }

    if (r_renderdoc.IsSet())
    {
        RenderDocUtils::Initialize();
    }

    // Low-level renderer back-end initialization
    const bool debug = r_debug.IsSet();
    sm_renderer.Init(reinterpret_cast<HINSTANCE>(hInst), reinterpret_cast<WNDPROC>(wndProc), w, h, static_cast<bool>(fullscreen), debug);

    // 2D sprite/UI batch setup
    sm_sprite_batches.Init(sm_renderer.Device());

    // Stores/view:
    sm_texture_store.Init(sm_renderer.Device());
    sm_model_store.Init(sm_texture_store);
    sm_view_state.Init(sm_renderer.Device(), sm_texture_store);

    // Constant buffers:
    sm_per_frame_shader_consts.Init(sm_renderer.Device());
    sm_per_view_shader_consts.Init(sm_renderer.Device());

    return true;
}

void DLLInterface::Shutdown()
{
    sm_per_view_shader_consts.Shutdown();
    sm_per_frame_shader_consts.Shutdown();
    sm_view_state.Shutdown();
    sm_model_store.Shutdown();
    sm_texture_store.Shutdown();
    sm_sprite_batches.Shutdown();
    sm_renderer.Shutdown();

    RenderDocUtils::Shutdown();
    GameInterface::Shutdown();
}

void DLLInterface::BeginRegistration(const char * const map_name)
{
    GameInterface::Printf("**** DLLInterface::BeginRegistration ****");

    sm_view_state.BeginRegistration();
    sm_texture_store.BeginRegistration();
    sm_model_store.BeginRegistration(map_name);

    MemTagsPrintAll();
}

void DLLInterface::EndRegistration()
{
    GameInterface::Printf("**** DLLInterface::EndRegistration ****");

    sm_model_store.EndRegistration();
    sm_texture_store.EndRegistration();
    sm_texture_store.UploadScrapIfNeeded();
    sm_view_state.EndRegistration();

    MemTagsPrintAll();
}

void DLLInterface::AppActivate(int activate)
{
    // TODO: Anything to be done here???
}

model_s * DLLInterface::RegisterModel(const char * const name)
{
    // Returned as an opaque handle.
    return (model_s *)sm_model_store.FindOrLoad(name, ModelType::kAny);
}

image_s * DLLInterface::RegisterSkin(const char * const name)
{
    // Returned as an opaque handle.
    return (image_s *)sm_texture_store.FindOrLoad(name, TextureType::kSkin);
}

image_s * DLLInterface::RegisterPic(const char * const name)
{
    // Returned as an opaque handle.
    return (image_s *)sm_texture_store.FindOrLoad(name, TextureType::kPic);
}

void DLLInterface::SetSky(const char * const name, const float rotate, vec3_t axis)
{
    sm_view_state.Sky() = SkyBox{ sm_texture_store, name, rotate, axis };
}

void DLLInterface::GetPicSize(int * out_w, int * out_h, const char * const name)
{
    // This can be called outside Begin/End frame.
    const TextureImage * const tex = sm_texture_store.FindOrLoad(name, TextureType::kPic);
    if (tex == nullptr)
    {
        GameInterface::Printf("WARNING: Can't find or load pic: '%s'", name);
        *out_w = *out_h = -1;
        return;
    }

    *out_w = tex->width;
    *out_h = tex->height;
}

void DLLInterface::BeginFrame(float /*camera_separation*/)
{
    const float   clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // RGBA
    const float   clear_depth    = 1.0f;
    const uint8_t clear_stencil  = 0;

    sm_renderer.BeginFrame(clear_color, clear_depth, clear_stencil);

    sm_per_frame_shader_consts.data.screen_dimensions[0] = static_cast<float>(sm_renderer.RenderWidth());
    sm_per_frame_shader_consts.data.screen_dimensions[1] = static_cast<float>(sm_renderer.RenderHeight());

    if (sm_disable_texturing.IsSet()) // Use only debug vertex color
    {
        VecSplatN(sm_per_frame_shader_consts.data.texture_color_scaling, 0.0f);
        VecSplatN(sm_per_frame_shader_consts.data.vertex_color_scaling,  1.0f);
    }
    else if (sm_blend_debug_color.IsSet()) // Blend debug vertex color with texture
    {
        VecSplatN(sm_per_frame_shader_consts.data.texture_color_scaling, 1.0f);
        VecSplatN(sm_per_frame_shader_consts.data.vertex_color_scaling,  1.0f);
    }
    else // Normal rendering
    {
        VecSplatN(sm_per_frame_shader_consts.data.texture_color_scaling, 1.0f);
        VecSplatN(sm_per_frame_shader_consts.data.vertex_color_scaling,  0.0f);
    }

    auto & context = sm_renderer.Device().GraphicsContext();
    MRQ2_PUSH_GPU_MARKER(context, "BeginFrame");

    sm_per_frame_shader_consts.Upload();
    sm_sprite_batches.BeginFrame();
}

void DLLInterface::EndFrame()
{
    if (sm_draw_fps_counter.IsSet())
    {
        DrawFpsCounter();
    }

    auto & context = sm_renderer.Device().GraphicsContext();
    {
        MRQ2_SCOPED_GPU_MARKER(context, "Draw2DSprites");
        sm_sprite_batches.EndFrame(context, sm_per_frame_shader_consts.CurrentBuffer(), sm_texture_store.tex_conchars);
    }

    MRQ2_POP_GPU_MARKER(context); // "EndFrame"

    sm_per_frame_shader_consts.MoveToNextFrame();
    sm_per_view_shader_consts.MoveToNextFrame();

    sm_renderer.EndFrame();
}

void DLLInterface::RenderView(refdef_t * const view_def)
{
    MRQ2_ASSERT(view_def != nullptr);
    MRQ2_ASSERT(sm_renderer.IsFrameStarted());

    auto & context = sm_renderer.Device().GraphicsContext();
    MRQ2_SCOPED_GPU_MARKER(context, "RenderView");

    // A world map should have been loaded already by BeginRegistration().
    if (sm_model_store.WorldModel() == nullptr && !(view_def->rdflags & RDF_NOWORLDMODEL))
    {
        GameInterface::Errorf("RenderView: Null world model!");
    }

    ViewDrawState::FrameData frame_data{ sm_texture_store, *sm_model_store.WorldModel(), *view_def };

    // Set up camera/view (fills frame_data)
    sm_view_state.RenderViewSetup(frame_data);

    // Update the constant buffers for this view
    sm_per_view_shader_consts.data.view_proj_matrix = frame_data.view_proj_matrix;
    sm_per_view_shader_consts.Upload();

    FixedSizeArray<const ConstantBuffer *, 2> cbuffers;
    cbuffers.push_back(&sm_per_frame_shader_consts.CurrentBuffer()); // slot(0)
    cbuffers.push_back(&sm_per_view_shader_consts.CurrentBuffer());  // slot(1)

    sm_view_state.DoRenderView(frame_data, context, cbuffers);
}

void DLLInterface::DrawPic(const int x, const int y, const char * const name)
{
    MRQ2_ASSERT(sm_renderer.IsFrameStarted());

    const TextureImage * const tex = sm_texture_store.FindOrLoad(name, TextureType::kPic);
    if (tex == nullptr)
    {
        GameInterface::Printf("WARNING: Can't find or load pic: '%s'", name);
        return;
    }

    const auto fx = float(x);
    const auto fy = float(y);
    const auto fw = float(tex->width);
    const auto fh = float(tex->height);
    const ColorRGBA32 kColorWhite = 0xFFFFFFFF;

    if (tex->from_scrap)
    {
        const auto u0 = float(tex->scrap_uv0.x) / TextureStore::kScrapSize;
        const auto v0 = float(tex->scrap_uv0.y) / TextureStore::kScrapSize;
        const auto u1 = float(tex->scrap_uv1.x) / TextureStore::kScrapSize;
        const auto v1 = float(tex->scrap_uv1.y) / TextureStore::kScrapSize;
        sm_sprite_batches.Get(SpriteBatch::kDrawPics).PushQuadTexturedUVs(fx, fy, fw, fh, u0, v0, u1, v1, tex, kColorWhite);
    }
    else
    {
        sm_sprite_batches.Get(SpriteBatch::kDrawPics).PushQuadTextured(fx, fy, fw, fh, tex, kColorWhite);
    }
}

void DLLInterface::DrawStretchPic(const int x, const int y, const int w, const int h, const char * const name)
{
    MRQ2_ASSERT(sm_renderer.IsFrameStarted());

    const TextureImage * const tex = sm_texture_store.FindOrLoad(name, TextureType::kPic);
    if (tex == nullptr)
    {
        GameInterface::Printf("WARNING: Can't find or load pic: '%s'", name);
        return;
    }

    const auto fx = float(x);
    const auto fy = float(y);
    const auto fw = float(w);
    const auto fh = float(h);
    const ColorRGBA32 kColorWhite = 0xFFFFFFFF;

    if (tex->from_scrap)
    {
        const auto u0 = float(tex->scrap_uv0.x) / TextureStore::kScrapSize;
        const auto v0 = float(tex->scrap_uv0.y) / TextureStore::kScrapSize;
        const auto u1 = float(tex->scrap_uv1.x) / TextureStore::kScrapSize;
        const auto v1 = float(tex->scrap_uv1.y) / TextureStore::kScrapSize;
        sm_sprite_batches.Get(SpriteBatch::kDrawPics).PushQuadTexturedUVs(fx, fy, fw, fh, u0, v0, u1, v1, tex, kColorWhite);
    }
    else
    {
        sm_sprite_batches.Get(SpriteBatch::kDrawPics).PushQuadTextured(fx, fy, fw, fh, tex, kColorWhite);
    }
}

void DLLInterface::DrawChar(const int x, const int y, int c)
{
    MRQ2_ASSERT(sm_renderer.IsFrameStarted());

    // Draws one 8*8 console graphic character with 0 being transparent.
    // It can be clipped to the top of the screen to allow the console
    // to be smoothly scrolled off. Based on Draw_Char() from ref_gl.
    constexpr int kGlyphSize = 8;
    constexpr int kGlyphTextureSize = 128;
    constexpr float kGlyphUVScale = float(kGlyphSize) / float(kGlyphTextureSize);

    c &= 255;

    if ((c & 127) == ' ')
    {
        return; // Whitespace
    }
    if (y <= -kGlyphSize)
    {
        return; // Totally off screen
    }

    const int row = c >> 4;
    const int col = c & 15;
    const float frow = row * kGlyphUVScale;
    const float fcol = col * kGlyphUVScale;
    const ColorRGBA32 kColorWhite = 0xFFFFFFFF;

    sm_sprite_batches.Get(SpriteBatch::kDrawChar).PushQuad(float(x), float(y), kGlyphSize, kGlyphSize, fcol, frow, fcol + kGlyphUVScale, frow + kGlyphUVScale, kColorWhite);
}

void DLLInterface::DrawString(int x, int y, const char * s)
{
    while (*s)
    {
        DrawChar(x, y, *s);
        x += 8; // kGlyphSize
        ++s;
    }
}

void DLLInterface::DrawTileClear(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const char * /*name*/)
{
    MRQ2_ASSERT(sm_renderer.IsFrameStarted());

    // TODO - Only used when letterboxing the screen for SW rendering, so not required ???
    // ACTUALLY you can control the letterboxing with the -,+ keys. Implement?
    GameInterface::Printf("WARNING: DrawTileClear() not implemented!");
}

void DLLInterface::DrawFill(const int x, const int y, const int w, const int h, const int c)
{
    MRQ2_ASSERT(sm_renderer.IsFrameStarted());

    const ColorRGBA32 color = TextureStore::ColorForIndex(c & 0xFF);
    const auto* white_tex = sm_texture_store.tex_white2x2;

    sm_sprite_batches.Get(SpriteBatch::kDrawPics).PushQuadTextured(float(x), float(y), float(w), float(h), white_tex, color);
}

void DLLInterface::DrawFadeScreen()
{
    MRQ2_ASSERT(sm_renderer.IsFrameStarted());

    // Fade alpha was 0.8 on ref_gl Draw_FadeScreen
    const auto fade_alpha = BytesToColor(0, 0, 0, 128);

    // Use a dummy white texture as base
    const auto * white_tex = sm_texture_store.tex_white2x2;

    // Full screen quad with alpha
    sm_sprite_batches.Get(SpriteBatch::kDrawPics).PushQuadTextured(0.0f, 0.0f, sm_renderer.RenderWidth(), sm_renderer.RenderHeight(), white_tex, fade_alpha);
}

void DLLInterface::DrawStretchRaw(const int x, const int y, int w, int h, const int cols, const int rows, const qbyte * const data)
{
    MRQ2_ASSERT(sm_renderer.IsFrameStarted());

    //
    // This function is only used by Quake2 to draw the cinematic frames, nothing else,
    // so it could have a better name... We'll optimize for that and assume this is not
    // a generic "draw pixels" kind of function.
    //

    const TextureImage * const cinematic_tex = sm_texture_store.tex_cinframe;
    MRQ2_ASSERT(cinematic_tex != nullptr);

    ColorRGBA32 * const cinematic_buffer = const_cast<ColorRGBA32 *>(cinematic_tex->pixels);
    MRQ2_ASSERT(cinematic_buffer != nullptr);

    const ColorRGBA32 * const cinematic_palette = TextureStore::CinematicPalette();
    float hscale;
    int num_rows;

    if (rows <= kQuakeCinematicImgSize)
    {
        hscale = 1.0f;
        num_rows = rows;
    }
    else
    {
        hscale = float(rows) / float(kQuakeCinematicImgSize);
        num_rows = kQuakeCinematicImgSize;
    }

    // Good idea to clear the buffer first, in case the
    // following upsampling doesn't fill the whole thing.
    for (int p = 0; p < (kQuakeCinematicImgSize * kQuakeCinematicImgSize); ++p)
    {
        //                        0xAABBGGRR
        const ColorRGBA32 black = 0xFF000000;
        cinematic_buffer[p] = black;
    }

    // Upsample to fill our 256*256 cinematic buffer.
    // This is based on the algorithm applied by ref_gl.
    for (int i = 0; i < num_rows; ++i)
    {
        const int row = static_cast<int>(i * hscale);
        if (row > rows)
        {
            break;
        }

        const qbyte * source = &data[cols * row];
        ColorRGBA32 * dest = &cinematic_buffer[i * kQuakeCinematicImgSize];

        const int fracstep = (cols * 65536 / kQuakeCinematicImgSize);
        int frac = fracstep >> 1;

        for (int j = 0; j < kQuakeCinematicImgSize; ++j)
        {
            const ColorRGBA32 color = cinematic_palette[source[frac >> 16]];
            dest[j] = color;
            frac += fracstep;
        }
    }

    h += 45; // FIXME HACK - Image scaling is probably broken.
             // Cinematics are not filling up the buffer as they should...

    // Update the cinematic GPU texture from our CPU buffer
    TextureUpload upload_info{};
    upload_info.texture  = &cinematic_tex->texture;
    upload_info.pixels   = cinematic_tex->pixels;
    upload_info.width    = cinematic_tex->width;
    upload_info.height   = cinematic_tex->height;
    upload_info.is_scrap = true; // This texture is a temporary, do not transition to shader resource in the D3D12 backend.
    sm_renderer.Device().UploadContext().UploadTextureImmediate(upload_info);

    // Draw a fullscreen quadrilateral with the cinematic texture applied to it
    const ColorRGBA32 kColorWhite = 0xFFFFFFFF;
    sm_sprite_batches.Get(SpriteBatch::kDrawPics).PushQuadTextured(float(x), float(y), float(w), float(h), cinematic_tex, kColorWhite);
}

void DLLInterface::CinematicSetPalette(const qbyte * const palette)
{
    TextureStore::SetCinematicPaletteFromRaw(palette);
}

void DLLInterface::DrawAltString(int x, int y, const char * s)
{
    while (*s)
    {
        DrawChar(x, y, *s ^ 0x80);
        x += 8; // kGlyphSize
        ++s;
    }
}

void DLLInterface::DrawNumberBig(int x, int y, int color, int width, int value)
{
    // Draw a big number using one of the 0-9 textures
    // color=0: normal color
    // color=1: alternate color (red numbers)
    // width: 3 is a good default

    constexpr int kStatMinus = 10; // num frame for '-' stats digit
    constexpr int kCharWidth = 16;

    static const char * const s_nums[2][11] = {
        { "num_0",  "num_1",  "num_2",  "num_3",  "num_4",  "num_5",  "num_6",  "num_7",  "num_8",  "num_9",  "num_minus"  },
        { "anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5", "anum_6", "anum_7", "anum_8", "anum_9", "anum_minus" }
    };

    MRQ2_ASSERT(color == 0 || color == 1);

    if (width < 1)
        width = 1;
    if (width > 5)
        width = 5;

    char num[16];
    sprintf_s(num, "%i", value);
    int l = (int)strlen(num);
    if (l > width)
        l = width;
    x += 2 + kCharWidth * (width - l);

    char * ptr = num;
    while (*ptr && l)
    {
        int frame;
        if (*ptr == '-')
            frame = kStatMinus;
        else
            frame = *ptr - '0';

        DrawPic(x, y, s_nums[color][frame]);
        x += kCharWidth;
        ptr++;
        l--;
    }
}

void DLLInterface::DrawFpsCounter()
{
    // Average multiple frames together to smooth changes out a bit.
    static constexpr uint32_t kMaxFrames = 4;
    struct FpsCounter
    {
        uint32_t previousTimes[kMaxFrames];
        uint32_t previousTime;
        uint32_t count;
        uint32_t index;
    };
    static FpsCounter s_fps;

    const uint32_t timeMillisec = GameInterface::GetTimeMilliseconds(); // Real time clock
    const uint32_t frameTime = timeMillisec - s_fps.previousTime;

    s_fps.previousTimes[s_fps.index++] = frameTime;
    s_fps.previousTime = timeMillisec;

    if (s_fps.index == kMaxFrames)
    {
        uint32_t total = 0;
        for (uint32_t i = 0; i < kMaxFrames; ++i)
        {
            total += s_fps.previousTimes[i];
        }

        if (total == 0)
        {
            total = 1;
        }

        s_fps.count = (10000 * kMaxFrames / total);
        s_fps.count = (s_fps.count + 5) / 10;
        s_fps.index = 0;
    }

    char text[128];
    sprintf_s(text, "FPS:%u", s_fps.count);

    // Draw it at the top-left corner of the screen
    DrawAltString(10, 10, text);
}

} // namespace MrQ2
