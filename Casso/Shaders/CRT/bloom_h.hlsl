// ATTRIBUTION: Adapted from libretro bloom shader by hunterk (Public Domain)
// Upstream URL: https://github.com/libretro/glsl-shaders/blob/master/bloom/shaders/bloom.glsl
// Upstream collection SHA: 42fa8a98ab19bdaffb53280746a30819eb21f807
// SPDX-License-Identifier: CC0-1.0
// Casso modifications: simplified separable horizontal Gaussian.

cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; float g_pixelScaleX; float g_pixelScaleY; float g_pictureV0; float g_pictureV1; float g_bloomThreshold; };
Texture2D    tex : register(t0);
SamplerState sam : register(s0);
struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
static const float W[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

//  Only bright pixels scatter. A phosphor has to emit light before the
//  faceplate can bounce it around, so a pixel below the threshold contributes
//  nothing to the halo. Cutting hard at the threshold would band as content
//  crosses it, so the low end is a quadratic knee: contribution rises from
//  zero over a band below the threshold instead of switching on.
//
//  Applied here rather than in a pass of its own. The vertical leg blurs what
//  this one writes, so thresholding the taps here yields the separable blur of
//  the bright-passed picture and costs no extra fullscreen pass.
float3 BrightPass (float3 c)
{
    float  knee = max (g_bloomThreshold * 0.5, 0.0001);
    float  lum  = max (c.r, max (c.g, c.b));
    float  soft = clamp (lum - g_bloomThreshold + knee, 0.0, 2.0 * knee);
    float  over = max (soft * soft / (4.0 * knee), lum - g_bloomThreshold);

    return c * (over / max (lum, 0.0001));
}

float4 main (PSInput i) : SV_TARGET
{
    // g_pixelScaleX is target texels per emulated pixel, so the radius is
    // read as a distance across the PICTURE rather than across the target.
    float  tx   = 1.0 / max (g_outputW, 1.0);
    float  step = tx * max (g_pixelScaleX, 0.001) * max (g_bloomRadius, 0.001);
    float3 acc  = BrightPass (tex.Sample (sam, i.uv).rgb) * W[0];
    [unroll] for (int k = 1; k < 5; ++k)
    {
        float2 off = float2 (step * (float) k, 0.0);
        acc += BrightPass (tex.Sample (sam, i.uv + off).rgb) * W[k];
        acc += BrightPass (tex.Sample (sam, i.uv - off).rgb) * W[k];
    }
    return float4 (acc, 1.0);
}
