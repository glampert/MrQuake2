//
// DLLInterfaceD3D12.cpp
//  Exposes GetRefAPI as the DLL entry point for Quake and the function pointers
//  required by refexport_t. Sets up the D3D12 refresh DLL.
//

#include "reflibs/shared/Common.hpp"
#include "reflibs/shared/RenderDocUtils.hpp"
#include "reflibs/shared/TextureStore.hpp"
#include "reflibs/shared/ModelStore.hpp"
#include "reflibs/shared/MiniImBatch.hpp"
#include "RenderInterfaceD3D12.hpp"

#include <vector>

// Quake includes
#include "client/ref.h"
#include "client/vid.h"

// Refresh common lib
#pragma comment(lib, "RefShared.lib")

// Dx12
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "d3dcompiler")

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace MrQ2
{

///////////////////////////////////////////////////////////////////////////////

template<typename VertexType, uint32_t kNumBuffers>
class VertexBuffers final
{
public:

    VertexBuffers() = default;

    struct DrawBuffer
    {
        VertexBuffer * buffer_ptr;
        uint32_t       used_verts;
    };

    void Init(const RenderDevice & device, const uint32_t max_verts)
    {
        MRQ2_ASSERT(max_verts != 0);
        m_max_verts = max_verts;

        const uint32_t buffer_size_in_bytes   = sizeof(VertexType) * m_max_verts;
        const uint32_t vertex_stride_in_bytes = sizeof(VertexType);

        for (uint32_t b = 0; b < kNumBuffers; ++b)
        {
            if (!m_vertex_buffers[b].Init(device, buffer_size_in_bytes, vertex_stride_in_bytes))
            {
                GameInterface::Errorf("Failed to create vertex buffer %u", b);
            }
            m_mapped_ptrs[b] = nullptr;
        }

        MemTagsTrackAlloc(buffer_size_in_bytes * kNumBuffers, MemTag::kVertIndexBuffer);
        GameInterface::Printf("VertexBuffers used memory: %s", FormatMemoryUnit(buffer_size_in_bytes * kNumBuffers));
    }

    void Shutdown()
    {
        m_max_verts    = 0;
        m_used_verts   = 0;
        m_buffer_index = 0;

        for (uint32_t b = 0; b < kNumBuffers; ++b)
        {
            m_mapped_ptrs[b] = nullptr;
            m_vertex_buffers[b].Shutdown();
        }
    }

    VertexType * Increment(const uint32_t count)
    {
        MRQ2_ASSERT(count != 0 && count <= m_max_verts);

        VertexType * verts = m_mapped_ptrs[m_buffer_index];
        MRQ2_ASSERT(verts != nullptr);
        MRQ2_ASSERT_ALIGN16(verts);

        verts += m_used_verts;
        m_used_verts += count;

        if (m_used_verts > m_max_verts)
        {
            GameInterface::Errorf("Vertex buffer overflowed! Used=%u, Max=%u. Increase size.", m_used_verts, m_max_verts);
        }

        return verts;
    }

    uint32_t BufferSize() const
    {
        return m_max_verts;
    }

    uint32_t NumVertsRemaining() const
    {
        MRQ2_ASSERT((m_max_verts - m_used_verts) > 0);
        return m_max_verts - m_used_verts;
    }

    uint32_t CurrentPosition() const
    {
        return m_used_verts;
    }

    VertexType * CurrentVertexPtr() const
    {
        return m_mapped_ptrs[m_buffer_index] + m_used_verts;
    }

    void Begin()
    {
        MRQ2_ASSERT(m_used_verts == 0); // Missing End()?

        // Map the current buffer:
        void * const memory = m_vertex_buffers[m_buffer_index].Map();
        if (memory == nullptr)
        {
            GameInterface::Errorf("Failed to map vertex buffer %u", m_buffer_index);
        }

        MRQ2_ASSERT_ALIGN16(memory);
        m_mapped_ptrs[m_buffer_index] = static_cast<VertexType *>(memory);
    }

    DrawBuffer End()
    {
        MRQ2_ASSERT(m_mapped_ptrs[m_buffer_index] != nullptr); // Missing Begin()?

        VertexBuffer & current_buffer = m_vertex_buffers[m_buffer_index];
        const uint32_t current_position = m_used_verts;

        // Unmap current buffer so we can draw with it:
        current_buffer.Unmap();
        m_mapped_ptrs[m_buffer_index] = nullptr;

        // Move to the next buffer:
        m_buffer_index = (m_buffer_index + 1) % kNumBuffers;
        m_used_verts = 0;

        return { &current_buffer, current_position };
    }

private:

    uint32_t m_max_verts{ 0 };
    uint32_t m_used_verts{ 0 };
    uint32_t m_buffer_index{ 0 };

    VertexType * m_mapped_ptrs[kNumBuffers] = {};
    VertexBuffer m_vertex_buffers[kNumBuffers] = {};
};

///////////////////////////////////////////////////////////////////////////////

class SpriteBatch final
{
public:

    enum BatchIndex : uint32_t
    {
        kDrawChar,  // Only used to draw console chars
        kDrawPics,  // Used by DrawPic, DrawStretchPic, etc
        kBatchCount // Number of items in the enum - not a valid index.
    };

    SpriteBatch() = default;

    void Init(const RenderDevice & device, const uint32_t max_verts)
    {
        m_buffers.Init(device, max_verts);
    }

    void Shutdown()
    {
        m_deferred_textured_quads.clear();
        m_deferred_textured_quads.shrink_to_fit();
        m_buffers.Shutdown();
    }

    void BeginFrame()
    {
        m_buffers.Begin();
    }

    void EndFrame()
    {
        const auto draw_buf = m_buffers.End();
        // TODO - actually draw 2d prims
    }

    DrawVertex2D * Increment(const uint32_t count)
    {
        return m_buffers.Increment(count);
    }

    void PushTriVerts(const DrawVertex2D tri[3])
    {
        DrawVertex2D * verts = Increment(3);
        std::memcpy(verts, tri, sizeof(DrawVertex2D) * 3);
    }

    void PushQuadVerts(const DrawVertex2D quad[4])
    {
        DrawVertex2D * tri = Increment(6);           // Expand quad into 2 triangles
        const int indexes[6] = { 0, 1, 2, 2, 3, 0 }; // CW winding
        for (int i = 0; i < 6; ++i)
        {
            tri[i] = quad[indexes[i]];
        }
    }

    void PushQuad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, const ColorRGBA32 color)
    {
        float r, g, b, a;
        ColorFloats(color, r, g, b, a);

        DrawVertex2D quad[4];
        quad[0].xy_uv[0] = x;
        quad[0].xy_uv[1] = y;
        quad[0].xy_uv[2] = u0;
        quad[0].xy_uv[3] = v0;
        quad[0].rgba[0]  = r;
        quad[0].rgba[1]  = g;
        quad[0].rgba[2]  = b;
        quad[0].rgba[3]  = a;
        quad[1].xy_uv[0] = x + w;
        quad[1].xy_uv[1] = y;
        quad[1].xy_uv[2] = u1;
        quad[1].xy_uv[3] = v0;
        quad[1].rgba[0]  = r;
        quad[1].rgba[1]  = g;
        quad[1].rgba[2]  = b;
        quad[1].rgba[3]  = a;
        quad[2].xy_uv[0] = x + w;
        quad[2].xy_uv[1] = y + h;
        quad[2].xy_uv[2] = u1;
        quad[2].xy_uv[3] = v1;
        quad[2].rgba[0]  = r;
        quad[2].rgba[1]  = g;
        quad[2].rgba[2]  = b;
        quad[2].rgba[3]  = a;
        quad[3].xy_uv[0] = x;
        quad[3].xy_uv[1] = y + h;
        quad[3].xy_uv[2] = u0;
        quad[3].xy_uv[3] = v1;
        quad[3].rgba[0]  = r;
        quad[3].rgba[1]  = g;
        quad[3].rgba[2]  = b;
        quad[3].rgba[3]  = a;
        PushQuadVerts(quad);
    }

    void PushQuadTextured(float x, float y, float w, float h, const Texture * tex, const ColorRGBA32 color)
    {
        MRQ2_ASSERT(tex != nullptr);
        const auto quad_start_vtx = m_buffers.CurrentPosition();
        PushQuad(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color);
        m_deferred_textured_quads.push_back({ tex, quad_start_vtx });
    }

    void PushQuadTexturedUVs(float x, float y, float w, float h, float u0, float v0, float u1, float v1, const Texture * tex, const ColorRGBA32 color)
    {
        MRQ2_ASSERT(tex != nullptr);
        const auto quad_start_vtx = m_buffers.CurrentPosition();
        PushQuad(x, y, w, h, u0, v0, u1, v1, color);
        m_deferred_textured_quads.push_back({ tex, quad_start_vtx });
    }

private:

    struct DeferredTexQuad
    {
        const Texture * tex;
        uint32_t quad_start_vtx;
    };

    using QuadList = std::vector<DeferredTexQuad>;
    using VBuffers = VertexBuffers<DrawVertex2D, RenderInterface::kNumFrameBuffers>;

    QuadList m_deferred_textured_quads;
    VBuffers m_buffers;
};

class SpriteBatches final
{
    SpriteBatch m_batches[SpriteBatch::kBatchCount];

public:

    void Init(const RenderDevice & device)
    {
        m_batches[SpriteBatch::kDrawChar].Init(device, 6 * 6000); // 6 verts per quad (expanded to 2 triangles each)
        m_batches[SpriteBatch::kDrawPics].Init(device, 6 * 128);
    }

    void Shutdown()
    {
        for (auto & sb : m_batches)
        {
            sb.Shutdown();
        }
    }

    SpriteBatch & Get(const SpriteBatch::BatchIndex index)
    {
        return m_batches[index];
    }
};

///////////////////////////////////////////////////////////////////////////////

//TODO: This will be common for all render back-ends
class DLLInterface final
{
    static RenderInterface sm_renderer;
    static SpriteBatches   sm_sprite_batches;

public:

    static int Init(void * hInst, void * wndProc, int fullscreen)
    {
        auto vid_mode    = GameInterface::Cvar::Get("vid_mode",    "6",    CvarWrapper::kFlagArchive);
        auto vid_width   = GameInterface::Cvar::Get("vid_width",   "1024", CvarWrapper::kFlagArchive);
        auto vid_height  = GameInterface::Cvar::Get("vid_height",  "768",  CvarWrapper::kFlagArchive);
        auto r_renderdoc = GameInterface::Cvar::Get("r_renderdoc", "0",    CvarWrapper::kFlagArchive);
        auto r_debug     = GameInterface::Cvar::Get("r_debug",     "0",    CvarWrapper::kFlagArchive);

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

        return true;
    }

    static void Shutdown()
    {
        sm_sprite_batches.Shutdown();
        sm_renderer.Shutdown();

        RenderDocUtils::Shutdown();
        GameInterface::Shutdown();
    }

    static void BeginRegistration(const char * const map_name)
    {
        GameInterface::Printf("**** DLLInterface::BeginRegistration ****");

        //RB::ViewState()->BeginRegistration();
        //RB::TexStore()->BeginRegistration();
        //RB::MdlStore()->BeginRegistration(map_name);

        MemTagsPrintAll();
    }

    static void EndRegistration()
    {
        GameInterface::Printf("**** DLLInterface::EndRegistration ****");

        //RB::MdlStore()->EndRegistration();
        //RB::TexStore()->EndRegistration();
        //RB::TexStore()->UploadScrapIfNeeded();
        //RB::ViewState()->EndRegistration();

        MemTagsPrintAll();
    }

    static void AppActivate(int activate)
    {
        // TODO: Anything to be done here???
    }

    static model_s * RegisterModel(const char * const name)
    {
        // Returned as an opaque handle.
        //return (model_s *)RB::MdlStore()->FindOrLoad(name, ModelType::kAny);
        return nullptr;
    }

    static image_s * RegisterSkin(const char * const name)
    {
        // Returned as an opaque handle.
        //return (image_s *)RB::TexStore()->FindOrLoad(name, TextureType::kSkin);
        return nullptr;
    }

    static image_s * RegisterPic(const char * const name)
    {
        // Returned as an opaque handle.
        //return (image_s *)RB::TexStore()->FindOrLoad(name, TextureType::kPic);
        return nullptr;
    }

    static void SetSky(const char * const name, const float rotate, vec3_t axis)
    {
        //RB::ViewState()->Sky() = SkyBox(*RB::TexStore(), name, rotate, axis);
    }

    static void DrawGetPicSize(int * out_w, int * out_h, const char * const name)
    {
		/*
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
		*/
    }

    static void BeginFrame(float /*camera_separation*/)
    {
        sm_renderer.BeginFrame();
    }

    static void EndFrame()
    {
        DrawFpsCounter();
        sm_renderer.EndFrame();
    }

    static void RenderFrame(refdef_t * const view_def)
    {
        MRQ2_ASSERT(view_def != nullptr);
        MRQ2_ASSERT(sm_renderer.IsFrameStarted());

		/*
        // A world map should have been loaded already by BeginRegistration().
        if (RB::MdlStore()->WorldModel() == nullptr && !(view_def->rdflags & RDF_NOWORLDMODEL))
        {
            GameInterface::Errorf("RenderFrame: Null world model!");
        }

        RB::RenderView(*view_def);
		*/
    }

    static void DrawPic(const int x, const int y, const char * const name)
    {
        MRQ2_ASSERT(sm_renderer.IsFrameStarted());

		/*
        //const TextureImage * const tex = RB::TexStore()->FindOrLoad(name, TextureType::kPic);
        const TextureImage * const tex = nullptr;
        if (tex == nullptr)
        {
            //GameInterface::Printf("WARNING: Can't find or load pic: '%s'", name);
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

            //RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTexturedUVs(
            //    fx, fy, fw, fh, u0, v0, u1, v1, tex, kColorWhite);
        }
        else
        {
            //RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTextured(
            //    fx, fy, fw, fh, tex, kColorWhite);
        }
		*/
    }

    static void DrawStretchPic(const int x, const int y, const int w, const int h, const char * const name)
    {
        MRQ2_ASSERT(sm_renderer.IsFrameStarted());

		/*
        //const TextureImage * const tex = RB::TexStore()->FindOrLoad(name, TextureType::kPic);
        const TextureImage * const tex = nullptr;
        if (tex == nullptr)
        {
            //GameInterface::Printf("WARNING: Can't find or load pic: '%s'", name);
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

            //RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTexturedUVs(
            //    fx, fy, fw, fh, u0, v0, u1, v1, tex, kColorWhite);
        }
        else
        {
            //RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTextured(
            //    fx, fy, fw, fh, tex, kColorWhite);
        }
		*/
    }

    static void DrawChar(const int x, const int y, int c)
    {
        MRQ2_ASSERT(sm_renderer.IsFrameStarted());

		/*
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

        //RB::SBatch(SpriteBatchIdx::kDrawChar)->PushQuad(float(x), float(y), kGlyphSize, kGlyphSize,
        //    fcol, frow, fcol + kGlyphUVScale, frow + kGlyphUVScale, kColorWhite);
		*/
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

    static void DrawTileClear(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const char * /*name*/)
    {
        MRQ2_ASSERT(sm_renderer.IsFrameStarted());

        // TODO - Only used when letterboxing the screen for SW rendering, so not required ???
        // ACTUALLY you can control the letterboxing with the -,+ keys
        GameInterface::Errorf("DrawTileClear() not implemented!");
    }

    static void DrawFill(const int x, const int y, const int w, const int h, const int c)
    {
        MRQ2_ASSERT(sm_renderer.IsFrameStarted());

		/*
        const ColorRGBA32 color = TextureStore::ColorForIndex(c & 0xFF);
        const std::uint8_t r = (color & 0xFF);
        const std::uint8_t g = (color >> 8)  & 0xFF;
        const std::uint8_t b = (color >> 16) & 0xFF;

        constexpr float scale = 1.0f / 255.0f;
        const DirectX::XMFLOAT4A normalized_color{ r * scale, g * scale, b * scale, 1.0f };
        const TextureImage * const dummy_tex = RB::TexStore()->tex_white2x2;

        RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTextured(
            float(x), float(y), float(w), float(h), dummy_tex, normalized_color);
		*/
    }

    static void DrawFadeScreen()
    {
        MRQ2_ASSERT(sm_renderer.IsFrameStarted());

		/*
        // was 0.8 on ref_gl Draw_FadeScreen
        constexpr float fade_alpha = 0.5f;

        // Use a dummy white texture as base
        const TextureImage * dummy_tex = RB::TexStore()->tex_white2x2;

        // Full screen quad with alpha
        RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTextured(
            0.0f, 0.0f, RB::Width(), RB::Height(), dummy_tex,
            DirectX::XMFLOAT4A{ 0.0f, 0.0f, 0.0f, fade_alpha });
			*/
    }

    static void DrawStretchRaw(const int x, const int y, int w, int h, const int cols, const int rows, const qbyte * const data)
    {
        MRQ2_ASSERT(sm_renderer.IsFrameStarted());

        //
        // This function is only used by Quake2 to draw the cinematic frames, nothing else,
        // so it could have a better name... We'll optimize for that and assume this is not
        // a generic "draw pixels" kind of function.
        //

		/*
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
        RB::SBatch(SpriteBatchIdx::kDrawPics)->PushQuadTextured(
            float(x), float(y), float(w), float(h), cin_tex, kColorWhite);
			*/
    }

    static void CinematicSetPalette(const qbyte * const palette)
    {
        TextureStore::SetCinematicPaletteFromRaw(palette);
    }

    // Not part of the Quake2 DLL renderer interface

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
};

RenderInterface DLLInterface::sm_renderer;
SpriteBatches DLLInterface::sm_sprite_batches;

} // MrQ2

///////////////////////////////////////////////////////////////////////////////
// GetRefAPI()
///////////////////////////////////////////////////////////////////////////////

extern "C" MRQ2_RENDERLIB_DLL_EXPORT refexport_t GetRefAPI(refimport_t ri)
{
    MrQ2::GameInterface::Initialize(ri, "D3D12");

    refexport_t re;
    re.api_version         = REF_API_VERSION;
    re.vidref              = VIDREF_D3D12;
    re.Init                = &MrQ2::DLLInterface::Init;
    re.Shutdown            = &MrQ2::DLLInterface::Shutdown;
    re.BeginRegistration   = &MrQ2::DLLInterface::BeginRegistration;
    re.RegisterModel       = &MrQ2::DLLInterface::RegisterModel;
    re.RegisterSkin        = &MrQ2::DLLInterface::RegisterSkin;
    re.RegisterPic         = &MrQ2::DLLInterface::RegisterPic;
    re.SetSky              = &MrQ2::DLLInterface::SetSky;
    re.EndRegistration     = &MrQ2::DLLInterface::EndRegistration;
    re.RenderFrame         = &MrQ2::DLLInterface::RenderFrame;
    re.DrawGetPicSize      = &MrQ2::DLLInterface::DrawGetPicSize;
    re.DrawPic             = &MrQ2::DLLInterface::DrawPic;
    re.DrawStretchPic      = &MrQ2::DLLInterface::DrawStretchPic;
    re.DrawChar            = &MrQ2::DLLInterface::DrawChar;
    re.DrawTileClear       = &MrQ2::DLLInterface::DrawTileClear;
    re.DrawFill            = &MrQ2::DLLInterface::DrawFill;
    re.DrawFadeScreen      = &MrQ2::DLLInterface::DrawFadeScreen;
    re.DrawStretchRaw      = &MrQ2::DLLInterface::DrawStretchRaw;
    re.CinematicSetPalette = &MrQ2::DLLInterface::CinematicSetPalette;
    re.BeginFrame          = &MrQ2::DLLInterface::BeginFrame;
    re.EndFrame            = &MrQ2::DLLInterface::EndFrame;
    re.AppActivate         = &MrQ2::DLLInterface::AppActivate;
    return re;
}

///////////////////////////////////////////////////////////////////////////////
