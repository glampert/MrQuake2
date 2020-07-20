
///////////////////////////////////////////////////////////////////////////////
// Inputs
///////////////////////////////////////////////////////////////////////////////

struct DrawVertex2D
{
    float4 xy_uv : POSITION;
    float4 rgba  : COLOR;
};

struct VertexShaderOutput
{
    float4 vpos : SV_POSITION;
    float4 uv   : TEXCOORD;
    float4 rgba : COLOR;
};

cbuffer PerFrameShaderConstants : register(b0)
{
    float4 screen_dimensions;
    float4 texture_color_scaling;
    float4 vertex_color_scaling;
};

Texture2D    glyphs_texture : register(t0);
SamplerState glyphs_sampler : register(s0);

///////////////////////////////////////////////////////////////////////////////
// Vertex Shader
///////////////////////////////////////////////////////////////////////////////

VertexShaderOutput VS_main(DrawVertex2D input)
{
    // Map to normalized clip coordinates
    float x = ((2.0 * (input.xy_uv.x - 0.5)) / screen_dimensions.x) - 1.0;
    float y = 1.0 - ((2.0 * (input.xy_uv.y - 0.5)) / screen_dimensions.y);

    VertexShaderOutput output;
    output.vpos = float4(x, y, 0.0, 1.0);
    output.uv   = float4(input.xy_uv.z, input.xy_uv.w, 0.0, 0.0);
    output.rgba = input.rgba;
    return output;
}

///////////////////////////////////////////////////////////////////////////////
// Pixel Shader
///////////////////////////////////////////////////////////////////////////////

float4 PS_main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = glyphs_texture.Sample(glyphs_sampler, input.uv.xy);
    return color * input.rgba;
}
