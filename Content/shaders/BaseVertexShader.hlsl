// A constant buffer that stores the three basic column-major matrices for composing geometry.
cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	matrix model;
	matrix view;
	matrix projection;
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
    float3 normal   : NORMAL;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
    float4 viewSpace: VIEWSPACE;
    float4 sPos     : TEXCOORD1;
    uint2  tindex   : TEXINDEX;
};

// Simple shader to do vertex processing on the GPU.
PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	float4 pos = float4(input.pos, 1.0f);

	// Transform the vertex position into projected space.
	pos = mul(pos, model);
	pos = mul(pos, view);    
    //output.sPos.w = pos.z;
    pos = mul(pos, projection);
	output.pos = pos;

    float4 normal = float4(input.normal,0.0f);
    normal = mul(normal, model);
    normal = mul(normal, view);
    output.normal = normal.xyz;

    // Pass through without modification.
    output.uv = input.uv;
	output.color = input.color;
    output.sPos = pos;
    output.viewSpace = pos;
    output.tindex = input.tindex;

	return output;
}
