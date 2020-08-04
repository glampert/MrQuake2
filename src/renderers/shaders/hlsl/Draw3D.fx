
///////////////////////////////////////////////////////////////////////////////
// Inputs
///////////////////////////////////////////////////////////////////////////////

struct DrawVertex3D
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD;
    float4 rgba     : COLOR;
};

struct VertexShaderOutput
{
    float4 vpos : SV_POSITION;
    float2 uv   : TEXCOORD;
    float4 rgba : COLOR;
};

cbuffer PerFrameShaderConstants : register(b0)
{
    float2 screen_dimensions;

    // Debugging flags
    bool   debug_mode;
    float  forced_mip_level;
    float4 texture_color_scaling;
    float4 vertex_color_scaling;
};

cbuffer PerViewShaderConstants : register(b1)
{
    matrix view_proj_matrix;
};

cbuffer PerDrawShaderConstants : register(b2)
{
    matrix model_matrix;
};

Texture2D    diffuse_texture : register(t0);
SamplerState diffuse_sampler : register(s0);

///////////////////////////////////////////////////////////////////////////////
// Vertex Shader
///////////////////////////////////////////////////////////////////////////////

VertexShaderOutput VS_main(DrawVertex3D input)
{
    VertexShaderOutput output;
    output.vpos = mul(mul(view_proj_matrix, model_matrix), float4(input.position, 1.0f));
    output.uv   = input.uv;
    output.rgba = input.rgba;
    return output;
}

///////////////////////////////////////////////////////////////////////////////
// Pixel Shader
///////////////////////////////////////////////////////////////////////////////

// Handles both opaque and translucent geometry
float4 PS_main(VertexShaderOutput input) : SV_TARGET
{
    float4 pixel_color;

    if (!debug_mode)
    {
        float4 tex_color = diffuse_texture.Sample(diffuse_sampler, input.uv);
        pixel_color = tex_color * input.rgba;
    }
    else // Debug shader
    {
        float4 tex_color;
        if (forced_mip_level >= 0.0f)
        {
            // Sample specific mipmap level for debugging (r_force_mip_level)
            tex_color = diffuse_texture.SampleLevel(diffuse_sampler, input.uv, forced_mip_level);
        }
        else
        {
            tex_color = diffuse_texture.Sample(diffuse_sampler, input.uv);
        }

        // Surface debugging (r_disable_texturing/r_blend_debug_color)
        tex_color *= texture_color_scaling;

        float4 vert_color = input.rgba;
        vert_color *= vertex_color_scaling;

        pixel_color = saturate(tex_color + vert_color);
        pixel_color.a = input.rgba.a * tex_color.a; // Preserve the incoming alpha values for transparencies
    }

    return pixel_color;
}
