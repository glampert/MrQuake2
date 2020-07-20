
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
    float4 screen_dimensions;
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

float4 PS_main(VertexShaderOutput input) : SV_TARGET
{
    // Handles both opaque and translucent geometry

    float4 tex_color = diffuse_texture.Sample(diffuse_sampler, input.uv);
    tex_color *= texture_color_scaling;

    float4 vert_color = input.rgba;
    vert_color *= vertex_color_scaling;

    float4 pixel_color = saturate(tex_color + vert_color);
    pixel_color.a = input.rgba.a * tex_color.a; // Preserve the incoming alpha values for transparencies

    return pixel_color;
}
