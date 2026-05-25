// ATTRIBUTION: Adapted from libretro ntsc-adaptive (chroma stage) by Themaister and hunterk (MIT)
// Upstream URL: https://github.com/libretro/glsl-shaders/blob/master/ntsc/shaders/ntsc-adaptive/ntsc-pass1.glsl
// Upstream collection SHA: 42fa8a98ab19bdaffb53280746a30819eb21f807
// SPDX-License-Identifier: MIT
// Casso modifications: the full NTSC-adaptive pipeline is several hundred LOC
//   of frequency-domain trickery (IQ modulation, lowpass filters, comb
//   decoding). This v1 port keeps only the *visible consequence* on a CRT:
//   horizontal lateral spread of chroma. We decompose to Y'CbCr (BT.601),
//   horizontally blur Cb+Cr with a triangle kernel of radius
//   `colorBleedWidth` pixels, leave luma untouched, and recombine. Captures
//   ~80% of the perceived effect for ~10% of the code.

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

float3 RgbToYCbCr (float3 c)
{
    float y  =  0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
    float cb = -0.168736 * c.r - 0.331264 * c.g + 0.5 * c.b;
    float cr =  0.5 * c.r - 0.418688 * c.g - 0.081312 * c.b;
    return float3 (y, cb, cr);
}

float3 YCbCrToRgb (float3 c)
{
    float r = c.x + 1.402   * c.z;
    float g = c.x - 0.344136 * c.y - 0.714136 * c.z;
    float b = c.x + 1.772    * c.y;
    return float3 (r, g, b);
}

float4 main (PSInput i) : SV_TARGET
{
    float  texelX = 1.0 / max (g_outputW, 1.0);
    float  radius = max (g_colorBleedWidth, 0.0);
    float  step   = texelX;

    // Sample the center pixel for luma reference.
    float3 centerRgb   = tex.Sample (sam, i.uv).rgb;
    float3 centerYcbcr = RgbToYCbCr (centerRgb);

    // Triangle-weighted horizontal blur on Cb+Cr only. radius==0 makes the
    // loop body never execute, leaving chroma == center chroma (pass-through).
    float2 chromaAcc = centerYcbcr.yz;
    float  weightSum = 1.0;

    int iRadius = (int) ceil (radius);
    [unroll(8)] for (int k = 1; k <= 8; ++k)
    {
        if (k > iRadius)
        {
            break;
        }

        float  w  = (radius - (float) (k - 1)) / radius;
        if (w < 0.0)
        {
            w = 0.0;
        }
        float2 off = float2 (step * (float) k, 0.0);

        float3 sampP = RgbToYCbCr (tex.Sample (sam, i.uv + off).rgb);
        float3 sampM = RgbToYCbCr (tex.Sample (sam, i.uv - off).rgb);

        chromaAcc += sampP.yz * w + sampM.yz * w;
        weightSum += 2.0 * w;
    }

    float3 outYcbcr = float3 (centerYcbcr.x, chromaAcc / weightSum);
    float3 outRgb   = YCbCrToRgb (outYcbcr);
    return float4 (outRgb, 1.0);
}
