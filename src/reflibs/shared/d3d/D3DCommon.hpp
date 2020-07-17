//
// D3DCommon.hpp
//  Common Ref API for the D3D back-ends (Dx11 & Dx12).
//
#pragma once

#include "reflibs/shared/MiniImBatch.hpp"
#include "reflibs/shared/TextureStore.hpp"
#include "reflibs/shared/RenderDocUtils.hpp"
#include <DirectXMath.h>

/* TODO
#if MRQ2_D3D11_DLL
	#include "RenderInterfaceD3D11.hpp"
#elif MRQ2_D3D12_DLL
	#include "RenderInterfaceD3D12.hpp"
#else
	#error "Missing renderer compilation switch!"
#endif
*/

namespace MrQ2
{

/*
// this uses the lower-level API, lower-level API does not know about TextureImage/ModelInstance, etc
class Quake2RendererDllInterface
{
private:

	static constexpr auto kSpriteBatchCount = size_t(SpriteBatchIdx::kCount);

	struct State
	{
		RenderInterface m_renderer;
		SpriteBatch m_sprite_batches[kSpriteBatchCount];
		TextureStore m_texture_store;
		ModelStore m_model_store;
		ViewDrawState m_view_state;
	};

	static State sm_state;

public:

	// static functions for the DLL
};
*/

// Code shared by both the D3D11 and D3D12 back-ends goes here.
// This header is only included in the RefAPI_D3D[X].cpp files.
template<typename RendererBackEndType>
struct D3DCommon
{
    using RB = RendererBackEndType;

    static int Init(void * hInstance, void * wndproc, int is_fullscreen)
    {
        #if defined(DEBUG) || defined(_DEBUG)
        constexpr bool debug_validation = true;
        #else // DEBUG
        constexpr bool debug_validation = false;
        #endif // DEBUG

        auto vid_mode = GameInterface::Cvar::Get("vid_mode", "6", CvarWrapper::kFlagArchive);
        auto vid_width = GameInterface::Cvar::Get("vid_width", "1024", CvarWrapper::kFlagArchive);
        auto vid_height = GameInterface::Cvar::Get("vid_height", "768", CvarWrapper::kFlagArchive);
        auto r_renderdoc = GameInterface::Cvar::Get("r_renderdoc", "0", CvarWrapper::kFlagArchive);

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

        RB::Init((HINSTANCE)hInstance, (WNDPROC)wndproc, w, h, (bool)is_fullscreen, debug_validation);
        return true;
    }

    static void Shutdown()
    {
        RB::Shutdown();
        RenderDocUtils::Shutdown();
        GameInterface::Shutdown();
    }

    static void BeginRegistration(const char * const map_name)
    {
        GameInterface::Printf("**** D3D::BeginRegistration ****");

        RB::ViewState()->BeginRegistration();
        RB::TexStore()->BeginRegistration();
        RB::MdlStore()->BeginRegistration(map_name);

        MemTagsPrintAll();
    }

    static void EndRegistration()
    {
        GameInterface::Printf("**** D3D::EndRegistration ****");

        RB::MdlStore()->EndRegistration();
        RB::TexStore()->EndRegistration();
        RB::TexStore()->UploadScrapIfNeeded();
        RB::ViewState()->EndRegistration();

        MemTagsPrintAll();
    }

    static void AppActivate(int activate)
    {
        // TODO: Anything to be done here???
    }

    static model_s * RegisterModel(const char * const name)
    {
        // Returned as an opaque handle.
        return (model_s *)RB::MdlStore()->FindOrLoad(name, ModelType::kAny);
    }

    static image_s * RegisterSkin(const char * const name)
    {
        // Returned as an opaque handle.
        return (image_s *)RB::TexStore()->FindOrLoad(name, TextureType::kSkin);
    }

    static image_s * RegisterPic(const char * const name)
    {
        // Returned as an opaque handle.
        return (image_s *)RB::TexStore()->FindOrLoad(name, TextureType::kPic);
    }

    static void SetSky(const char * const name, const float rotate, vec3_t axis)
    {
        RB::ViewState()->Sky() = SkyBox(*RB::TexStore(), name, rotate, axis);
    }

    static void DrawGetPicSize(int * out_w, int * out_h, const char * const name)
    {
        // This can be called outside Begin/End frame.
        const TextureImage * const tex = RB::TexStore()->FindOrLoad(name, TextureType::kPic);
        if (tex == nullptr)
        {
            GameInterface::Printf("WARNING: Can't find or load pic: '%s'", name);
            *out_w = *out_h = -1;
            return;
        }

        *out_w = tex->width;
        *out_h = tex->height;
    }

    static void DrawFpsCounter()
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
        static FpsCounter fps;

        const uint32_t timeMillisec = GameInterface::GetTimeMilliseconds(); // Real time clock
        const uint32_t frameTime = timeMillisec - fps.previousTime;

        fps.previousTimes[fps.index++] = frameTime;
        fps.previousTime = timeMillisec;

        if (fps.index == kMaxFrames)
        {
            uint32_t total = 0;
            for (uint32_t i = 0; i < kMaxFrames; ++i)
            {
                total += fps.previousTimes[i];
            }

            if (total == 0)
            {
                total = 1;
            }

            fps.count = (10000 * kMaxFrames / total);
            fps.count = (fps.count + 5) / 10;
            fps.index = 0;
        }

        char text[128];
        sprintf_s(text, "FPS:%u", fps.count);

        // Draw it at the top-left corner of the screen
        DrawAltString(10, 10, text);
    }

    static void BeginFrame(float /*camera_separation*/)
    {
        MRQ2_ASSERT(!RB::FrameStarted());
        RB::BeginFrame();
    }

    static void EndFrame()
    {
        MRQ2_ASSERT(RB::FrameStarted());
        DrawFpsCounter();
        RB::EndFrame();
    }

    static void RenderFrame(refdef_t * const view_def)
    {
        MRQ2_ASSERT(view_def != nullptr);
        MRQ2_ASSERT(RB::FrameStarted());

        // A world map should have been loaded already by BeginRegistration().
        if (RB::MdlStore()->WorldModel() == nullptr && !(view_def->rdflags & RDF_NOWORLDMODEL))
        {
            GameInterface::Errorf("RenderFrame: Null world model!");
        }

        RB::RenderView(*view_def);
    }

    static void DrawPic(const int x, const int y, const char * const name)
    {
        MRQ2_ASSERT(RB::FrameStarted());

        const TextureImage * const tex = RB::TexStore()->FindOrLoad(name, TextureType::kPic);
        if (tex == nullptr)
        {
            GameInterface::Printf("WARNING: Can't find or load pic: '%s'", name);
            return;
        }

        const auto fx = float(x);
        const auto fy = float(y);
        const auto fw = float(tex->width);
        const auto fh = float(tex->height);

        static const DirectX::XMFLOAT4A kColorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };

        if (tex->from_scrap)
        {
            const auto u0 = float(tex->scrap_uv0.x) / TextureStore::kScrapSize;
            const auto v0 = float(tex->scrap_uv0.y) / TextureStore::kScrapSize;
            const auto u1 = float(tex->scrap_uv1.x) / TextureStore::kScrapSize;
            const auto v1 = float(tex->scrap_uv1.y) / TextureStore::kScrapSize;

            RB::SBatch(RB::SpriteBatchIdx::kDrawPics)->PushQuadTexturedUVs(
                fx, fy, fw, fh, u0, v0, u1, v1, tex, kColorWhite);
        }
        else
        {
            RB::SBatch(RB::SpriteBatchIdx::kDrawPics)->PushQuadTextured(
                fx, fy, fw, fh, tex, kColorWhite);
        }
    }

    static void DrawStretchPic(const int x, const int y, const int w, const int h, const char * const name)
    {
        MRQ2_ASSERT(RB::FrameStarted());

        const TextureImage * const tex = RB::TexStore()->FindOrLoad(name, TextureType::kPic);
        if (tex == nullptr)
        {
            GameInterface::Printf("WARNING: Can't find or load pic: '%s'", name);
            return;
        }

        const auto fx = float(x);
        const auto fy = float(y);
        const auto fw = float(w);
        const auto fh = float(h);

        static const DirectX::XMFLOAT4A kColorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };

        if (tex->from_scrap)
        {
            const auto u0 = float(tex->scrap_uv0.x) / TextureStore::kScrapSize;
            const auto v0 = float(tex->scrap_uv0.y) / TextureStore::kScrapSize;
            const auto u1 = float(tex->scrap_uv1.x) / TextureStore::kScrapSize;
            const auto v1 = float(tex->scrap_uv1.y) / TextureStore::kScrapSize;

            RB::SBatch(RB::SpriteBatchIdx::kDrawPics)->PushQuadTexturedUVs(
                fx, fy, fw, fh, u0, v0, u1, v1, tex, kColorWhite);
        }
        else
        {
            RB::SBatch(RB::SpriteBatchIdx::kDrawPics)->PushQuadTextured(
                fx, fy, fw, fh, tex, kColorWhite);
        }
    }

    static void DrawChar(const int x, const int y, int c)
    {
        MRQ2_ASSERT(RB::FrameStarted());

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

        static const DirectX::XMFLOAT4A kColorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };

        RB::SBatch(RB::SpriteBatchIdx::kDrawChar)->PushQuad(float(x), float(y), kGlyphSize, kGlyphSize,
            fcol, frow, fcol + kGlyphUVScale, frow + kGlyphUVScale, kColorWhite);
    }

    static void DrawString(int x, int y, const char * s)
    {
        while (*s)
        {
            DrawChar(x, y, *s);
            x += 8; // kGlyphSize
            ++s;
        }
    }

    static void DrawAltString(int x, int y, const char * s)
    {
        while (*s)
        {
            DrawChar(x, y, *s ^ 0x80);
            x += 8; // kGlyphSize
            ++s;
        }
    }

    static void DrawNumberBig(int x, int y, int color, int width, int value)
    {
        // Draw a big number using one of the 0-9 textures
        // color=0: normal color
        // color=1: alternate color (red numbers)
        // width: 3 is a good default

        constexpr int kStatMinus = 10; // num frame for '-' stats digit
        constexpr int kCharWidth = 16;

        static const char * const sb_nums[2][11] = {
            { "num_0",  "num_1",  "num_2",  "num_3",  "num_4",  "num_5",  "num_6",  "num_7",  "num_8",  "num_9",  "num_minus"  },
            { "anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5", "anum_6", "anum_7", "anum_8", "anum_9", "anum_minus" }
        };

        MRQ2_ASSERT(color == 0 || color == 1);

        if (width < 1) width = 1;
        if (width > 5) width = 5;

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

            DrawPic(x, y, sb_nums[color][frame]);
            x += kCharWidth;
            ptr++;
            l--;
        }
    }

    static void DrawTileClear(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const char * /*name*/)
    {
        MRQ2_ASSERT(RB::FrameStarted());

        // TODO - Only used when letterboxing the screen for SW rendering, so not required ???
        // ACTUALLY you can control the letterboxing with the -,+ keys
        GameInterface::Errorf("DrawTileClear() not implemented!");
    }

    static void DrawFill(const int x, const int y, const int w, const int h, const int c)
    {
        MRQ2_ASSERT(RB::FrameStarted());

        const ColorRGBA32 color = TextureStore::ColorForIndex(c & 0xFF);
        const std::uint8_t r = (color & 0xFF);
        const std::uint8_t g = (color >> 8)  & 0xFF;
        const std::uint8_t b = (color >> 16) & 0xFF;

        constexpr float scale = 1.0f / 255.0f;
        const DirectX::XMFLOAT4A normalized_color{ r * scale, g * scale, b * scale, 1.0f };
        const TextureImage * const dummy_tex = RB::TexStore()->tex_white2x2;

        RB::SBatch(RB::SpriteBatchIdx::kDrawPics)->PushQuadTextured(
            float(x), float(y), float(w), float(h), dummy_tex, normalized_color);
    }

    static void DrawFadeScreen()
    {
        MRQ2_ASSERT(RB::FrameStarted());

        // was 0.8 on ref_gl Draw_FadeScreen
        constexpr float fade_alpha = 0.5f;

        // Use a dummy white texture as base
        const TextureImage * dummy_tex = RB::TexStore()->tex_white2x2;

        // Full screen quad with alpha
        RB::SBatch(RB::SpriteBatchIdx::kDrawPics)->PushQuadTextured(
            0.0f, 0.0f, RB::Width(), RB::Height(), dummy_tex,
            DirectX::XMFLOAT4A{ 0.0f, 0.0f, 0.0f, fade_alpha });
    }

    static void DrawStretchRaw(const int x, const int y, int w, int h, const int cols, const int rows, const qbyte * const data)
    {
        MRQ2_ASSERT(RB::FrameStarted());

        //
        // This function is only used by Quake2 to draw the cinematic frames, nothing else,
        // so it could have a better name... We'll optimize for that and assume this is not
        // a generic "draw pixels" kind of function.
        //

        const TextureImage * const cin_tex = RB::TexStore()->tex_cinframe;
        MRQ2_ASSERT(cin_tex != nullptr);

        ColorRGBA32 * const cinematic_buffer = const_cast<ColorRGBA32 *>(cin_tex->pixels);
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
        RB::UploadTexture(cin_tex);

        // Draw a fullscreen quadrilateral with the cinematic texture applied to it
        static const DirectX::XMFLOAT4A kColorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
        RB::SBatch(RB::SpriteBatchIdx::kDrawPics)->PushQuadTextured(
            float(x), float(y), float(w), float(h), cin_tex, kColorWhite);
    }

    static void CinematicSetPalette(const qbyte * const palette)
    {
        TextureStore::SetCinematicPaletteFromRaw(palette);
    }
};

// Debug helper: Sends all draw calls to outer space but still doest all the rest.
template<typename RendererBackEndType>
struct D3DCommon_NullDraw final : public D3DCommon<RendererBackEndType>
{
    static void RenderFrame(refdef_t * const) {}
    static void DrawPic(int, int, const char *) {}
    static void DrawStretchPic(int, int, int, int, const char *) {}
    static void DrawChar(int, int, int) {}
    static void DrawTileClear(int, int, int, int, const char *) {}
    static void DrawFill(int, int, int, int, int) {}
    static void DrawFadeScreen() {}
    static void DrawStretchRaw(int, int, int, int, int, int, const qbyte *) {}
};

} // MrQ2
