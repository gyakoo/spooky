// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D texDiffuse;
SamplerState samPoint
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = WRAP;
    AddressV = WRAP;
};

// A pass-through function for the (interpolated) color data.
float4 main(PixelShaderInput input) : SV_TARGET
{
    float3 lightDir=float3(0.4,-1,-0.4);
    float3 L = normalize(-lightDir);
    float d = abs(dot(input.normal, L));
	return float4(input.color.xyz*d, 1.0f);
}
