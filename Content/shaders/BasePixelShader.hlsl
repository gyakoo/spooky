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


// You won't like the 'lighting model' below.
// It's basically a range based FOG and the fog density depends on the screen space distance
// from the origin (0,0) and depth
float4 main(PixelShaderInput input) : SV_TARGET
{
    const float4 fogColor = float4(0, 0, 0, 1.0f);
    float fogDensity = 1.0f;

    // texturing
    float2 texAtlasFac = 1.0f / texAtlasSize.xy;
    float2 uv = input.tindex*texAtlasFac;
    float4 texColor = texDiffuse.Sample(samPoint, uv+input.uv*texAtlasFac);
    float alpha = texColor.a;

    float4 color = alpha*float4(texColor.rgb*input.color.rgb, 1.0f);
    
    float dist = length(input.viewSpace); // range based

    // Changing fog density depending on circle from origin 
    {
        const float aspect = texAtlasSize.w;
        const float2 xy = float2(input.sPos.x*aspect, input.sPos.y);
        const float l = length(xy)*0.4f;
        float val = val = l*saturate(2/dist); // origin and depth
        fogDensity *= val;
    }
//    else
//    {
//        fogDensity = 0.5f;
//   }

    // Fog 1/exp2
    {
        const float dfd = dist*fogDensity;
        const float fogFactor = saturate(1.0f / exp(dfd*dfd));
        color.rgb = lerp(color.rgb, fogColor.rgb, (1 - fogFactor));
    }

    return color;
}
