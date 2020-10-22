
///////////////////////////////////////////////////////////////////////////////
// Inputs
///////////////////////////////////////////////////////////////////////////////

struct DrawVertex2D
{
    float2 position   : POSITION;
    float2 texture_uv : TEXCOORD;
    float4 rgba       : COLOR;
};

struct VertexShaderOutput
{
    float4 vpos       : SV_POSITION;
    float4 texture_uv : TEXCOORD;
    float4 rgba       : COLOR;
};

cbuffer PerFrameShaderConstants : register(b0)
{
    float2 screen_dimensions;

    // Debugging flags (unused in this shader)
    uint   debug_mode;
    float  forced_mip_level;
    float4 texture_color_scaling;
    float4 vertex_color_scaling;
};

Texture2D    glyphs_texture : register(t3);
SamplerState glyphs_sampler : register(s3);

///////////////////////////////////////////////////////////////////////////////
// Vertex Shader
///////////////////////////////////////////////////////////////////////////////

VertexShaderOutput VS_main(DrawVertex2D input)
{
    // Map to normalized clip coordinates
    float x = ((2.0 * (input.position.x - 0.5)) / screen_dimensions.x) - 1.0;
    float y = 1.0 - ((2.0 * (input.position.y - 0.5)) / screen_dimensions.y);

    VertexShaderOutput output;
    output.vpos       = float4(x, y, 0.0, 1.0);
    output.texture_uv = float4(input.texture_uv.x, input.texture_uv.y, 0.0, 0.0);
    output.rgba       = input.rgba;
    return output;
}

///////////////////////////////////////////////////////////////////////////////
// Pixel Shader
///////////////////////////////////////////////////////////////////////////////

float4 PS_main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = glyphs_texture.Sample(glyphs_sampler, input.texture_uv.xy);
    return color * input.rgba;
}
