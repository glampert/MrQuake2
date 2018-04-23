
///////////////////////////////////////////////////////////////////////////////

Texture2D    g_diffuse_texture : register(t0);
SamplerState g_diffuse_sampler : register(s0);

cbuffer ConstantBufferDataSGeom : register(b0)
{
    matrix g_mvp_matrix;
};

struct VertexInput
{
    float4 position : POSITION;
    float4 rgba     : COLOR;
};

struct VertexOutput
{
    float4 vpos : SV_POSITION;
    float4 rgba : COLOR;
};

///////////////////////////////////////////////////////////////////////////////

VertexOutput VS_main(VertexInput input)
{
    VertexOutput output;
    output.vpos = mul(g_mvp_matrix, input.position);
    output.rgba = input.rgba;
    return output;
}

///////////////////////////////////////////////////////////////////////////////

float4 PS_main(VertexOutput input) : SV_TARGET
{
    return input.rgba;

    //float4 color = g_diffuse_texture.Sample(g_diffuse_sampler, input.uv.xy);
    //return color * input.rgba;
}

///////////////////////////////////////////////////////////////////////////////
