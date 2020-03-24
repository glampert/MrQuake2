//
// D3DCommon.hpp
//  Common Ref API for the D3D back-ends (Dx11 & Dx12).
//
#pragma once

#include "reflibs/shared/MiniImBatch.hpp"
#include "reflibs/shared/TextureStore.hpp"
#include "reflibs/shared/RenderDocUtils.hpp"
#include <DirectXMath.h>

namespace MrQ2
{

// Code shared by both the D3D11 and D3D12 back-ends goes here.
// This header is only included in the RefAPI_D3D[X].cpp files.
template<typename RendererBackEndType>
struct D3DCommon
{
    using RB = RendererBackEndType;

    static int Init(void * hInstance, void * wndproc, int fullscreen)
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

        RB::Init((HINSTANCE)hInstance, (WNDPROC)wndproc, w, h, !!fullscreen, debug_validation);
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

    static void BeginFrame(float /*camera_separation*/)
    {
        FASTASSERT(!RB::FrameStarted());
        RB::BeginFrame();
    }

    static void EndFrame()
    {
        FASTASSERT(RB::FrameStarted());
        RB::EndFrame();
    }

    static void RenderFrame(refdef_t * const view_def)
    {
        FASTASSERT(view_def != nullptr);
        FASTASSERT(RB::FrameStarted());

        // A world map should have been loaded already by BeginRegistration().
        if (RB::MdlStore()->WorldModel() == nullptr && !(view_def->rdflags & RDF_NOWORLDMODEL))
        {
            GameInterface::Errorf("RenderFrame: Null world model!");
        }

        RB::RenderView(*view_def);
    }

    static void DrawPic(const int x, const int y, const char * const name)
    {
        FASTASSERT(RB::FrameStarted());

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

            RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTexturedUVs(
                fx, fy, fw, fh, u0, v0, u1, v1, tex, kColorWhite);
        }
        else
        {
            RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTextured(
                fx, fy, fw, fh, tex, kColorWhite);
        }
    }

    static void DrawStretchPic(const int x, const int y, const int w, const int h, const char * const name)
    {
        FASTASSERT(RB::FrameStarted());

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

            RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTexturedUVs(
                fx, fy, fw, fh, u0, v0, u1, v1, tex, kColorWhite);
        }
        else
        {
            RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTextured(
                fx, fy, fw, fh, tex, kColorWhite);
        }
    }

    static void DrawChar(const int x, const int y, int c)
    {
        FASTASSERT(RB::FrameStarted());

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

        RB::SBatch(SpriteBatchIdx::kDrawChar)->PushQuad(float(x), float(y), kGlyphSize, kGlyphSize,
            fcol, frow, fcol + kGlyphUVScale, frow + kGlyphUVScale, kColorWhite);
    }

    static void DrawTileClear(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const char * /*name*/)
    {
        FASTASSERT(RB::FrameStarted());

        // TODO - Only used when letterboxing the screen for SW rendering, so not required ???
        GameInterface::Errorf("DrawTileClear() not implemented!");
    }

    static void DrawFill(const int x, const int y, const int w, const int h, const int c)
    {
        FASTASSERT(RB::FrameStarted());

        const ColorRGBA32 color = TextureStore::ColorForIndex(c & 0xFF);
        const std::uint8_t r = (color & 0xFF);
        const std::uint8_t g = (color >> 8)  & 0xFF;
        const std::uint8_t b = (color >> 16) & 0xFF;

        constexpr float scale = 1.0f / 255.0f;
        const DirectX::XMFLOAT4A normalized_color{ r * scale, g * scale, b * scale, 1.0f };
        const TextureImage * const dummy_tex = RB::TexStore()->tex_white2x2;

        RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTextured(
            float(x), float(y), float(w), float(h), dummy_tex, normalized_color);
    }

    static void DrawFadeScreen()
    {
        FASTASSERT(RB::FrameStarted());

        // was 0.8 on ref_gl Draw_FadeScreen
        constexpr float fade_alpha = 0.5f;

        // Use a dummy white texture as base
        const TextureImage * dummy_tex = RB::TexStore()->tex_white2x2;

        // Full screen quad with alpha
        RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTextured(
            0.0f, 0.0f, RB::Width(), RB::Height(), dummy_tex,
            DirectX::XMFLOAT4A{ 0.0f, 0.0f, 0.0f, fade_alpha });
    }

    static void DrawStretchRaw(const int x, const int y, int w, int h, const int cols, const int rows, const qbyte * const data)
    {
        FASTASSERT(RB::FrameStarted());

        //
        // This function is only used by Quake2 to draw the cinematic frames, nothing else,
        // so it could have a better name... We'll optimize for that and assume this is not
        // a generic "draw pixels" kind of function.
        //

        const TextureImage * const cin_tex = RB::TexStore()->tex_cinframe;
        FASTASSERT(cin_tex != nullptr);

        ColorRGBA32 * const cinematic_buffer = const_cast<ColorRGBA32 *>(cin_tex->pixels);
        FASTASSERT(cinematic_buffer != nullptr);

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
        RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTextured(
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
