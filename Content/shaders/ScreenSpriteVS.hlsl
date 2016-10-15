// A constant buffer that stores the three basic column-major matrices for composing geometry.
cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
    matrix model;
};

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
    float3 pos      : SV_POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
    uint2  tindex   : TEXINDEX;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 pos      : SV_POSITION;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
};

// Simple shader to do vertex processing on the GPU.
PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
    output.pos = mul(float4(input.pos, 1.0f), model);
    output.color = input.color;
    output.uv = input.uv;

	return output;
}
