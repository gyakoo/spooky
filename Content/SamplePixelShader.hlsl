// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float4 viewSpace : VIEWSPACE;
    float4 sPos : TEXCOORD1;
};

Texture2D texDiffuse;
SamplerState samPoint
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = WRAP;
    AddressV = WRAP;
};


#define FOG_TYPE 1

// A pass-through function for the (interpolated) color data.
float4 main(PixelShaderInput input) : SV_TARGET
{
    const float3 lightDir=float3(0.4,-1,-0.4);
    const float4 fogColor = float4(0, 0, 0, 1.0f);
    const float fogStart = 0.5f;
    const float fogEnd = 7.0f;
    const float fogDensity = 0.04f;

    float3 L = normalize(-lightDir);
    float d = abs(dot(input.normal, L));
    float4 color = float4(input.color.xyz*d, 1.0f);


    //float dist = abs(input.viewSpace.z); // dist = input.pos.z / input.pos.w; // plane based
    float dist = length(input.viewSpace); // range based

    {
        float fogFactor = 0;
#if FOG_TYPE==0
        fogFactor = saturate((fogEnd - dist) / (fogEnd - fogStart));
#elif FOG_TYPE==1
        fogFactor = saturate(1.0f / exp(dist*fogDensity));
#elif FOG_TYPE==2
        float dfd = dist*fogDensity;
        fogFactor = saturate(1.0f / exp(dfd*dfd));
#endif
        color.rgb = lerp(color.rgb, fogColor, (1 - fogFactor));
    }

	return color;
}
