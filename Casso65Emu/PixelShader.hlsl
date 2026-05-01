// PixelShader.hlsl — Full-window textured quad pixel shader with point sampling

Texture2D    txDiffuse : register(t0);
SamplerState samPoint  : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

float4 main (PSInput input) : SV_TARGET
{
    return txDiffuse.Sample (samPoint, input.texcoord);
}
