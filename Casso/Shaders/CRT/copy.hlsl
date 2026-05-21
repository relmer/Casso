// Casso original. Pass-through copy used as the final blit from the
// post-process ping-pong RT to the swap chain back buffer.

Texture2D    tex : register(t0);
SamplerState sam : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

float4 main (PSInput i) : SV_TARGET
{
    return tex.Sample (sam, i.uv);
}
