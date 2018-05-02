
///////////////////////////////////////////////////////////////////////////////

Texture2D    g_diffuse_texture : register(t0);
SamplerState g_diffuse_sampler : register(s0);

cbuffer ConstantBufferDataSGeomVS : register(b0)
{
    matrix g_mvp_matrix;
};

cbuffer ConstantBufferDataSGeomPS : register(b1)
{
    float4 g_texture_color_scaling;
    float4 g_vertex_color_scaling;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
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

VertexOutput VS_main(VertexInput input)
{
    VertexOutput output;
    output.vpos = mul(g_mvp_matrix, float4(input.position, 1.0f));
    output.uv   = input.uv;
    output.rgba = input.rgba;
    return output;
}

///////////////////////////////////////////////////////////////////////////////

float4 PS_main(VertexOutput input) : SV_TARGET
{
    float4 tex_color = g_diffuse_texture.Sample(g_diffuse_sampler, input.uv);
    tex_color *= g_texture_color_scaling;

    float4 vert_color = input.rgba;
    vert_color *= g_vertex_color_scaling;

    return saturate(tex_color + vert_color);
}

///////////////////////////////////////////////////////////////////////////////
