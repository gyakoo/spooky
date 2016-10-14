struct PixelShaderInput
{
    float4 pos      : SV_POSITION;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
    uint2  tindex   : TEXINDEX;
};

Texture2D texDiffuse;
SamplerState samPoint;

cbuffer constants
{
    float4 texAtlasSize;  // xy=atlas size, z=<undefined>, w=aspect ratio
};

float4 main(PixelShaderInput input) : SV_TARGET
{
    // texturing
    float2 texAtlasFac = 1.0f / texAtlasSize.xy;
    float2 uv = input.tindex*texAtlasFac;
    float4 texColor = texDiffuse.Sample(samPoint, uv+input.uv*texAtlasFac);
    float alpha = texColor.a;
    float4 color = alpha*float4(texColor.rgb*input.color.rgb, 1.0f);
    return color;
}
