//
// ModelStructs.hpp
//  Structures and types representing the in-memory versions
//  of 3D models / world geometry used by Quake 2.
//
#pragma once

#include "Common.hpp"
#include "Memory.hpp"
#include "RenderInterface.hpp"

namespace MrQ2
{

class TextureImage;
class TextureStore;

/*
===============================================================================

    Misc constants / flags

===============================================================================
*/

enum PlaneSides : int
{
    kPlaneSide_Front = 0,
    kPlaneSide_Back  = 1,
    kPlaneSide_On    = 2,
};

enum SurfaceFlags : int
{
    // Misc surface flags (same values used by ref_gl)
    kSurf_PlaneBack      = 2,
    kSurf_DrawSky        = 4,
    kSurf_DrawTurb       = 16,
    kSurf_DrawBackground = 64,
    kSurf_Underwater     = 128,
};

enum class ModelType : std::uint8_t
{
    kBrush,    // World geometry.
    kSprite,   // Sprite model.
    kAliasMD2, // MD2/Entity model.

    // Number of items in the enum - not a valid model type.
    kCount,

    // Special flag for ModelStore::Find - not a valid type
    kAny = 0xFF,
};

constexpr float kBackFaceEpsilon = 0.01f;
constexpr int kSubdivideSize = 64;

// Max height in pixels of MD2 model skins.
constexpr int kMaxMD2SkinHeight = 480;

// From q_files.h
constexpr int kMaxMD2Skins  = 32;
constexpr int kMaxLightmaps = 4;

/*
===============================================================================

    In-memory representation of 3D models (world and entities)

===============================================================================
*/

//
// Vertex format used by ModelPoly.
// Has two sets of texture coordinates for lightmapping.
//
struct PolyVertex
{
    // model vertex position:
    vec3_t position;

    // main tex coords:
    float texture_s;
    float texture_t;

    // lightmap tex coords:
    float lightmap_s;
    float lightmap_t;
};

//
// Model vertex position.
//
struct ModelVertex
{
    vec3_t position;
};

//
// Model triangle vertex indexes.
// Limited to 16bits to save space.
//
struct ModelTriangle
{
    std::uint16_t vertexes[3];
};

//
// Edge description.
//
struct ModelEdge
{
    std::uint16_t v[2]; // Vertex numbers/indexes
};

//
// Texture/material description.
//
struct ModelTexInfo
{
    float vecs[2][4];
    int flags;
    int num_frames;
    const TextureImage * teximage;
    const ModelTexInfo * next; // Texture animation chain
};

//
// Model polygon/face.
// List links are for draw-time sorting.
//
struct ModelPoly
{
    int num_verts;             // size of vertexes[], since it's dynamically allocated
    PolyVertex * vertexes;     // array of polygon vertexes. Never null
    ModelTriangle * triangles; // (num_verts - 2) triangles with indexes into vertexes[]
    ModelPoly * next;

    // Range in the ModelInstance index buffer for this polygon (used if r_use_vertex_index_buffers is set).
    struct IbRange {
        int first_index;
        int index_count;
        int base_vertex;
    } index_buffer;
};

//
// Surface description (holds a set of polygons).
//
struct ModelSurface
{
    int vis_frame; // should be drawn when node is crossed

    cplane_s * plane;
    int flags;
    ColorRGBA32 color;

    int first_edge; // look up in model->surf_edges[], negative numbers are backwards edges
    int num_edges;

    std::int16_t texture_mins[2];
    std::int16_t extents[2];

    // lightmap tex coordinates
    int light_s;
    int light_t;

    ModelPoly * polys; // multiple if warped
    const ModelSurface * texture_chain;
    ModelTexInfo * texinfo;

    // dynamic lighting info:
    int dlight_frame;
    int dlight_bits;

    int lightmap_texture_num;          // -1 if not lightmapped
    std::uint8_t styles[kMaxLightmaps];
    float cached_light[kMaxLightmaps]; // values currently used in lightmap
    std::uint8_t * samples;            // [numstyles * surfsize]
};

//
// BSP world node.
//
struct ModelNode
{
    // common with leaf
    int contents;  // -1, to differentiate from leafs
    int vis_frame; // node needs to be traversed if current

    // for bounding box culling
    float minmaxs[6];

    ModelNode * parent;

    // node specific
    cplane_s * plane;
    ModelNode * children[2];

    std::uint16_t first_surface;
    std::uint16_t num_surfaces;
};

//
// Special BSP leaf node (a draw node).
//
struct ModelLeaf
{
    // common with node
    int contents;  // will be a negative contents number
    int vis_frame; // node needs to be traversed if current

    // for bounding box culling
    float minmaxs[6];

    ModelNode * parent;

    // leaf specific
    int cluster;
    int area;

    ModelSurface ** first_mark_surface;
    int num_mark_surfaces;
};

//
// Sub-model mesh information.
//
struct SubModelInfo
{
    vec3_t mins;
    vec3_t maxs;
    vec3_t origin;
    float radius;
    int head_node;
    int vis_leafs;
    int first_face;
    int num_faces;
};

//
// Whole model (world or entity/sprite).
//
class ModelInstance final
{
public:

    struct RenderData
    {
        int num_frames;
        int flags;

        // Volume occupied by the model graphics.
        vec3_t mins;
        vec3_t maxs;
        float radius;

        // Solid volume for clipping.
        bool clipbox;
        vec3_t clipmins;
        vec3_t clipmaxs;

        // Brush model.
        int first_model_surface;
        int num_model_surfaces;
        int lightmap; // Only for submodels

        int num_submodels;
        SubModelInfo * submodels;

        int num_planes;
        cplane_s * planes;

        int num_leafs; // Number of visible leafs, not counting 0
        ModelLeaf * leafs;

        int num_vertexes;
        ModelVertex * vertexes;

        int num_edges;
        ModelEdge * edges;

        int num_nodes;
        int first_node;
        ModelNode * nodes;

        int num_texinfos;
        ModelTexInfo * texinfos;

        int num_surfaces;
        ModelSurface * surfaces;

        int num_surf_edges;
        int * surf_edges;

        int num_mark_surfaces;
        ModelSurface ** mark_surfaces;

        dvis_s * vis;
        std::uint8_t * light_data;

        const TextureImage * skins[kMaxMD2Skins]; // For alias models and skins.
    };

    // File name with path + hash (must be the first field - game code assumes this).
    const PathName name;

    // Model type flag.
    const ModelType type;

    // True if from the inline models pool.
    const bool is_inline;

    // Registration number, so we know if it is currently referenced by the level being played.
    std::uint32_t reg_num;

    // POD data that we can memset to zeros on construction.
    RenderData data;

    // Memory hunk backing the model's data.
    MemHunk hunk;

    // Optional Vertex and Index buffer for static world geometry.
    VertexBuffer vb;
    IndexBuffer  ib;

    // Initializing constructor.
    ModelInstance(const char * const mdl_name, const ModelType mdl_type, const std::uint32_t registration_number, const bool inline_mdl)
        : name{ mdl_name }
        , type{ mdl_type }
        , is_inline{ inline_mdl }
        , reg_num{ registration_number }
    {
        std::memset(&data, 0, sizeof(data));
    }

    ~ModelInstance()
    {
        vb.Shutdown();
        ib.Shutdown();
    }

    // Disallow copy.
    ModelInstance(const ModelInstance &) = delete;
    ModelInstance & operator=(const ModelInstance &) = delete;
};

// ============================================================================

} // MrQ2
