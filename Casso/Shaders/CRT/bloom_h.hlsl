// ATTRIBUTION: Adapted from libretro bloom shader by hunterk (Public Domain)
// Upstream URL: https://github.com/libretro/glsl-shaders/blob/master/bloom/shaders/bloom.glsl
// Upstream collection SHA: 42fa8a98ab19bdaffb53280746a30819eb21f807
// SPDX-License-Identifier: CC0-1.0
// Casso modifications: simplified separable horizontal Gaussian. The upstream
//   pass first thresholds against a luminance floor to isolate highlights; we
//   skip the threshold pass for v1 (every pixel contributes) which yields a
//   softer global glow rather than a true HDR bloom. The radius is exposed as
//   a uniform `bloomRadius` in pixels; sample count is fixed at 9 taps.

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

static const float kWeights[5] =
{
    0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216
};

float4 main (PSInput i) : SV_TARGET
{
    float  texelX = 1.0 / max (g_outputW, 1.0);
    float  step   = texelX * max (g_bloomRadius, 0.001);
    float3 acc    = tex.Sample (sam, i.uv).rgb * kWeights[0];

    [unroll] for (int k = 1; k < 5; ++k)
    {
        float2 off = float2 (step * (float) k, 0.0);
        acc += tex.Sample (sam, i.uv + off).rgb * kWeights[k];
        acc += tex.Sample (sam, i.uv - off).rgb * kWeights[k];
    }

    return float4 (acc, 1.0);
}
