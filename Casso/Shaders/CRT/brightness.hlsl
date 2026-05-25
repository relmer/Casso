// Casso original. Brightness + contrast pass. Used as the first stage
// of the CRT post-process chain so subsequent passes (scanlines, bloom,
// color bleed) operate on already-corrected pixels.
//
// Contrast is applied around the 0.5 mid-grey pivot (1.0 = identity,
// >1.0 stretches the range, <1.0 compresses toward grey). Brightness is
// a final multiplicative scale (1.0 = identity). Output is clamped to
// [0, 1] so a saturated combination doesn't overflow into the next pass.

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
    float  g_contrast;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

float4 main (PSInput i) : SV_TARGET
{
    float4 c = tex.Sample (sam, i.uv);
    c.rgb = saturate (((c.rgb - 0.5) * g_contrast + 0.5) * g_brightness);
    return c;
}
