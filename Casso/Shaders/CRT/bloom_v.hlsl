// ATTRIBUTION: Adapted from libretro bloom shader by hunterk (Public Domain)
// Upstream URL: https://github.com/libretro/glsl-shaders/blob/master/bloom/shaders/bloom.glsl
// Upstream collection SHA: 42fa8a98ab19bdaffb53280746a30819eb21f807
// SPDX-License-Identifier: CC0-1.0
// Casso modifications: vertical leg of the separable Gaussian.

cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };
Texture2D    tex : register(t0);
SamplerState sam : register(s0);
struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
static const float W[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };
float4 main (PSInput i) : SV_TARGET
{
    float  ty   = 1.0 / max (g_outputH, 1.0);
    float  step = ty * max (g_bloomRadius, 0.001);
    float3 acc  = tex.Sample (sam, i.uv).rgb * W[0];
    [unroll] for (int k = 1; k < 5; ++k)
    {
        float2 off = float2 (0.0, step * (float) k);
        acc += tex.Sample (sam, i.uv + off).rgb * W[k];
        acc += tex.Sample (sam, i.uv - off).rgb * W[k];
    }
    return float4 (acc, 1.0);
}
