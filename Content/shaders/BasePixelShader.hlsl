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
    float4 texAtlasSize;  // xy=atlas size, z=global Time, w=aspect ratio
    float4 other; // x=color.rgb multiplier, y=0|1 all lit, z=denstity mult
    float4 modulate;
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
    float alpha = texColor.a*input.color.a;
    //if (alpha < 0.02f) discard; // hmm not sure

    float4 color = alpha*float4(texColor.rgb*input.color.rgb, 1.0f);
    const float aspect = texAtlasSize.w;
    float dist = length(input.viewSpace); // range based

    if ( (texColor.r!=1 || texColor.g!=0 || texColor.b!=0) &&
         (texColor.r!=0 || texColor.g!=1 || texColor.b!=0) ) // red bypass fog
    {
        // Changing fog density depending on circle from origin 
        const float2 xy = float2(input.sPos.x*aspect, input.sPos.y);
        const float levelTime = texAtlasSize.z;
        const float l = length(xy) * other.z;
        float val = l*saturate(2 / dist); // origin and depth
        fogDensity *= val*other.y;

        // Fog 1/exp2
        const float dfd = dist*fogDensity;
        const float fogFactor = saturate(1.0f / exp(dfd*dfd));
        color.rgb = lerp(color.rgb, fogColor.rgb, (1 - fogFactor)) * other.x;
        //color.a *= 5 / dist;
    }
    else
    {
        //color.rgb *= saturate(1.0f - input.sPos.z / 6.0f);
    }
    return color*modulate;
}

// blocky
/*
float4 main(PixelShaderInput input) : SV_TARGET
{
    const float4 fogColor = float4(0, 0, 0, 1.0f);
    float fogDensity = 1.0f;

    // texturing
    float2 texAtlasFac = 1.0f / texAtlasSize.xy;
    float2 uv = input.tindex*texAtlasFac;
    const float dxy = float2(0.07, 0.07);
    uv += input.uv*texAtlasFac;
    float4 texColor = texDiffuse.Sample(samPoint, uv);
    float alpha = texColor.a;
    float4 color = alpha*float4(texColor.rgb*input.color.rgb, 1.0f);    
    float dist = length(input.viewSpace); // range based

    // Changing fog density depending on circle from origin 
    const float aspect = texAtlasSize.w;
    const float2 xy = float2(input.sPos.x*aspect, input.sPos.y);
    const float l = length(floor(xy/dxy))*0.02;
    float val = l;// *saturate(2 / dist); // origin and depth
    fogDensity *= val;

    // Fog 1/exp2
    {
        const float dfd = dist*fogDensity;
        const float fogFactor = saturate(1.0f / exp(dfd*dfd));
        color.rgb = lerp(color.rgb, fogColor.rgb, (1 - fogFactor));
        
    }

    return color;
}

*/
