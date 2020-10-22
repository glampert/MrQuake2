
///////////////////////////////////////////////////////////////////////////////
// Inputs/Constants
///////////////////////////////////////////////////////////////////////////////

static const uint kDebugMode_None             = 0;
static const uint kDebugMode_ForcedMipLevel   = 1;
static const uint kDebugMode_DisableTexturing = 2;
static const uint kDebugMode_BlendDebugColor  = 3;
static const uint kDebugMode_ViewLightmaps    = 4;

struct DrawVertex3D
{
    float3 position    : POSITION;
    float2 texture_uv  : TEXCOORD0;
    float2 lightmap_uv : TEXCOORD1;
    float4 rgba        : COLOR;
};

struct VertexShaderOutput
{
    float4 vpos        : SV_POSITION;
    float2 texture_uv  : TEXCOORD0;
    float2 lightmap_uv : TEXCOORD1;
    float4 rgba        : COLOR;
};

cbuffer PerFrameShaderConstants : register(b0)
{
    float2 screen_dimensions;

    // Debugging flags
    uint   debug_mode; // Set to one of the kDebugMode_* values.
    float  forced_mip_level;
    float4 texture_color_scaling;
    float4 vertex_color_scaling;
};

cbuffer PerViewShaderConstants : register(b1)
{
    matrix view_proj_matrix;
};

///////////////////////////////////////////////////////////////////////////////
#if VULKAN

// On Vulkan the per-draw constants are supplied via PushConstants.
struct PerDrawShaderConstants
{
    matrix model_matrix;
};

[[vk::push_constant]] PerDrawShaderConstants push_consts;

float4 TransformVertex(float3 position)
{
    float4 vpos = mul(mul(view_proj_matrix, push_consts.model_matrix), float4(position, 1.0f));
    return vpos;
}

#else // !VULKAN

// Implemented as a regular cbuffer for D3D:
// - Set from RootSignature inline constants on D3D12.
// - Just a const buffer updated with UpdateSubresource for D3D11.
cbuffer PerDrawShaderConstants : register(b2)
{
    matrix model_matrix;
};

float4 TransformVertex(float3 position)
{
    float4 vpos = mul(mul(view_proj_matrix, model_matrix), float4(position, 1.0f));
    return vpos;
}

#endif // VULKAN
///////////////////////////////////////////////////////////////////////////////

Texture2D    diffuse_texture  : register(t3);
SamplerState diffuse_sampler  : register(s3);

Texture2D    lightmap_texture : register(t4);
SamplerState lightmap_sampler : register(s4);

///////////////////////////////////////////////////////////////////////////////
// Vertex Shader
///////////////////////////////////////////////////////////////////////////////

VertexShaderOutput VS_main(DrawVertex3D input)
{
    VertexShaderOutput output;
    output.vpos        = TransformVertex(input.position);
    output.texture_uv  = input.texture_uv;
    output.lightmap_uv = input.lightmap_uv;
    output.rgba        = input.rgba;
    return output;
}

///////////////////////////////////////////////////////////////////////////////
// Pixel Shader
///////////////////////////////////////////////////////////////////////////////

// Handles both opaque and translucent geometry
float4 PS_main(VertexShaderOutput input) : SV_TARGET
{
    float4 pixel_color;

    if (debug_mode == kDebugMode_None)
    {
        float4 diffuse_tex_color  = diffuse_texture.Sample(diffuse_sampler, input.texture_uv);
        float4 lightmap_tex_color = lightmap_texture.Sample(lightmap_sampler, input.lightmap_uv);
        float4 vertex_color       = input.rgba;

        pixel_color = diffuse_tex_color * lightmap_tex_color * vertex_color;
    }
    else // Debug shader
    {
        float4 diffuse_tex_color;

        if (forced_mip_level >= 0.0f)
        {
            // Sample specific mipmap level for debugging (r_force_mip_level)
            if (debug_mode == kDebugMode_ViewLightmaps)
            {
                diffuse_tex_color = lightmap_texture.SampleLevel(lightmap_sampler, input.lightmap_uv, forced_mip_level);
            }
            else
            {
                diffuse_tex_color = diffuse_texture.SampleLevel(diffuse_sampler, input.texture_uv, forced_mip_level);
            }
        }
        else
        {
            if (debug_mode == kDebugMode_ViewLightmaps) // Lightmap color only
            {
                diffuse_tex_color = lightmap_texture.Sample(lightmap_sampler, input.lightmap_uv);
            }
            else // Base texture
            {
                diffuse_tex_color = diffuse_texture.Sample(diffuse_sampler, input.texture_uv);
            }
        }

        // Surface debugging (r_disable_texturing/r_blend_debug_color)
        diffuse_tex_color *= texture_color_scaling;

        float4 vertex_color = input.rgba;
        vertex_color *= vertex_color_scaling;

        pixel_color = saturate(diffuse_tex_color + vertex_color);
        pixel_color.a = input.rgba.a * diffuse_tex_color.a; // Preserve the incoming alpha values for transparencies
    }

    return pixel_color;
}
