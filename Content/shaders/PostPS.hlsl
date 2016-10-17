struct PixelShaderInput
{
    float4 pos      : SV_POSITION;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
};

Texture2D texDiffuse;
SamplerState samPoint;

cbuffer constants
{
    float4 params0;
    float4 params1;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
    // texturing
    float4 texColor = texDiffuse.Sample(samPoint, input.uv);
    float3 rgb = texColor.rgb*input.color.rgb;
    rgb += params0.rgb;
    float4 color = float4(rgb, 1.0f);
    return color;
}
