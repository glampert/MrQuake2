
///////////////////////////////////////////////////////////////////////////////

Texture2D    g_diffuse_texture : register(t0);
SamplerState g_diffuse_sampler : register(s0);

cbuffer ConstantBufferDataSGeomVS : register(b0)
{
    matrix g_mvp_matrix;
};

cbuffer ConstantBufferDataSGeomPS : register(b1)
{
    bool g_disable_texturing;
    bool g_blend_debug_color;
};

struct VertexInput
{
    float4 position : POSITION;
    float4 uv       : TEXCOORD;
    float4 rgba     : COLOR;
};

struct VertexOutput
{
    float4 vpos : SV_POSITION;
    float4 uv   : TEXCOORD;
    float4 rgba : COLOR;
};

///////////////////////////////////////////////////////////////////////////////

VertexOutput VS_main(VertexInput input)
{
    VertexOutput output;
    output.vpos = mul(g_mvp_matrix, input.position);
    output.uv   = input.uv;
    output.rgba = input.rgba;
    return output;
}

///////////////////////////////////////////////////////////////////////////////

float4 PS_main(VertexOutput input) : SV_TARGET
{
    float4 pixel_color;
    float4 tex_color = g_diffuse_texture.Sample(g_diffuse_sampler, input.uv.xy);

    if (g_disable_texturing) 
    {
        pixel_color = input.rgba;
    }
    else if (g_blend_debug_color)
    {
        pixel_color = tex_color * input.rgba;
    }
    else
    {
        pixel_color = tex_color;
    }

    return pixel_color;
}

///////////////////////////////////////////////////////////////////////////////
