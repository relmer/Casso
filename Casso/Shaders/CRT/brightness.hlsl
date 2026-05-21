// Casso original. Simple constant-multiply brightness pass. Used as the
// first stage of the CRT post-process chain so subsequent passes (scanlines,
// bloom, color bleed) operate on already-brightness-corrected pixels.
// No third-party derivation -- no attribution required.

Texture2D    tex : register(t0);
SamplerState sam : register(s0);

cbuffer CrtCb : register(b0)
{
    float  g_brightness;
    float  g_scanlineIntensity;
    float  g_bloomRadius;
    float  g_bloomStrength;
    float  g_colorBleedWidth;
    float  g_outputW;
    float  g_outputH;
    float  g_pad;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

float4 main (PSInput i) : SV_TARGET
{
    float4 c = tex.Sample (sam, i.uv);
    c.rgb *= g_brightness;
    return c;
}
