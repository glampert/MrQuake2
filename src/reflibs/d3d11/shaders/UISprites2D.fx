
///////////////////////////////////////////////////////////////////////////////

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
	// TODO make this a constant buffer
    float2 screen_dimensions = float2(1024.0, 768.0);

	// Map to normalized clip coordinates
    float x = ((2.0 * (input.xy_uv.x - 0.5)) / screen_dimensions.x) - 1.0;
    float y = 1.0 - ((2.0 * (input.xy_uv.y - 0.5)) / screen_dimensions.y);

    VertexOutput output;
    output.vpos = float4(x, y, 0.0, 1.0);
    output.uv   = float4(input.xy_uv.z, input.xy_uv.w, 0.0, 0.0);
    output.rgba = input.rgba;
    return output;
}

///////////////////////////////////////////////////////////////////////////////

float4 PS_main(VertexOutput input) : SV_TARGET
{
    //return input.rgba;
    return input.uv;
}

///////////////////////////////////////////////////////////////////////////////
