// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
    float4 pos          : SV_POSITION;
    float3 normal       : NORMAL;
    float4 color        : COLOR0;
    float2 uv           : TEXCOORD0;
    float4 viewSpace    : VIEWSPACE;
    float4 sPos         : TEXCOORD1;
    uint2  tindex       : TEXINDEX;
};

Texture2D texDiffuse;
SamplerState samPoint;

cbuffer constants
{
    float4 texAtlasSize;  // xy=atlas size, z=<undefined>, w=aspect ratio
};


#define FOG_TYPE 2
#define SPOTLIGHT 1

// You won't like the 'lighting model' below.
// It's basically a range based FOG and the fog density depends on the screen space distance
// from the origin (0,0) and depth
float4 main(PixelShaderInput input) : SV_TARGET
{
    const float4 fogColor = float4(0, 0, 0, 1.0f);
    const float fogStart = 0.5f;
    const float fogEnd = 7.0f;

#if FOG_TYPE==1
    float fogDensity = 0.7f;
#else
    float fogDensity = 1.0f;
#endif

    float2 texAtlasFac = 1.0f / texAtlasSize.xy;
    float2 uv = input.tindex*texAtlasFac;
    float4 texColor = texDiffuse.Sample(samPoint, uv+input.uv*texAtlasFac);
    float alpha = texColor.a;

    float4 color = alpha*float4(texColor.rgb*input.color.rgb, 1.0f);
    
    //float dist = abs(input.viewSpace.z); // dist = input.pos.z / input.pos.w; // plane based
    //float dist = length(input.viewSpace.xzw); // range based
    float dist = length(input.viewSpace); // range based

#if SPOTLIGHT==1
    if (texAtlasSize.z>0.0f)
    {
        const float aspect = texAtlasSize.w;
        const float2 xy = float2(input.sPos.x*aspect, input.sPos.y);
        float val = length(xy)*0.15f;
        fogDensity *= val;
    }
    else
    {
        fogDensity = 0.6f;
    }
#endif

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
        color.rgb = lerp(color.rgb, fogColor.rgb, (1 - fogFactor));
    }

    return color;
}
