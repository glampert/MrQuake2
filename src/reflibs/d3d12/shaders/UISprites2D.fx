
///////////////////////////////////////////////////////////////////////////////

Texture2D    g_glyphs_texture : register(t0);
SamplerState g_glyphs_sampler : register(s0);

cbuffer ConstantBufferDataUIVS : register(b0)
{
    float4 g_screen_dimensions;
};

struct VertexInput
{
    float4 xy_uv : POSITION;
    float4 rgba  : COLOR;
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
    // Map to normalized clip coordinates
    float x = ((2.0 * (input.xy_uv.x - 0.5)) / g_screen_dimensions.x) - 1.0;
    float y = 1.0 - ((2.0 * (input.xy_uv.y - 0.5)) / g_screen_dimensions.y);

    VertexOutput output;
    output.vpos = float4(x, y, 0.0, 1.0);
    output.uv   = float4(input.xy_uv.z, input.xy_uv.w, 0.0, 0.0);
    output.rgba = input.rgba;
    return output;
}

///////////////////////////////////////////////////////////////////////////////

float4 PS_main(VertexOutput input) : SV_TARGET
{
    float4 color = g_glyphs_texture.Sample(g_glyphs_sampler, input.uv.xy);
    return color * input.rgba;
}

///////////////////////////////////////////////////////////////////////////////
