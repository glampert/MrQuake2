
///////////////////////////////////////////////////////////////////////////////

struct VertexInput
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD;
    float4 rgba     : COLOR;
};

struct VertexOutput
{
    float4 vpos : SV_POSITION;
    float2 uv   : TEXCOORD;
    float4 rgba : COLOR;
};

///////////////////////////////////////////////////////////////////////////////
// Vertex Shader:

cbuffer VertexShaderConstants : register(b0)
{
    matrix mvp_matrix;
};

VertexOutput VS_main(VertexInput input)
{
    VertexOutput output;
    output.vpos = mul(mvp_matrix, float4(input.position, 1.0f));
    output.uv   = input.uv;
    output.rgba = input.rgba;
    return output;
}

///////////////////////////////////////////////////////////////////////////////
// Pixel Shader:

/*
cbuffer PixelShaderConstants : register(b1)
{
    float4 texture_color_scaling;
    float4 vertex_color_scaling;
};
*/

Texture2D    diffuse_texture : register(t0);
SamplerState diffuse_sampler : register(s0);

float4 PS_main(VertexOutput input) : SV_TARGET
{
    float4 tex_color = diffuse_texture.Sample(diffuse_sampler, input.uv);
    float4 c = tex_color;
    c.a = input.rgba.a * tex_color.a;
    return c;

// debug code disabled for now.

//    float4 tex_color = diffuse_texture.Sample(diffuse_sampler, input.uv);
//    tex_color *= texture_color_scaling;

//    float4 vert_color = input.rgba;
//    vert_color *= vertex_color_scaling;

//    float4 frag_color = saturate(tex_color + vert_color);
//    frag_color.a = input.rgba.a * tex_color.a; // Preserve the incoming alpha values for transparencies

//    return frag_color;
}

///////////////////////////////////////////////////////////////////////////////
