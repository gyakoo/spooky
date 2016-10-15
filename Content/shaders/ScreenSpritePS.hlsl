struct PixelShaderInput
{
    float4 pos      : SV_POSITION;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
};

Texture2D texDiffuse;
SamplerState samPoint;

float4 main(PixelShaderInput input) : SV_TARGET
{
    // texturing
    float4 texColor = texDiffuse.Sample(samPoint, input.uv);
    float alpha = texColor.a;
    float4 color = alpha*float4(texColor.rgb*input.color.rgb, 1.0f);
    return color;
}
