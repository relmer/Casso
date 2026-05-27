// ATTRIBUTION: Adapted from libretro bloom shader by hunterk (Public Domain)
// Upstream URL: https://github.com/libretro/glsl-shaders/blob/master/bloom/shaders/bloom.glsl
// Upstream collection SHA: 42fa8a98ab19bdaffb53280746a30819eb21f807
// SPDX-License-Identifier: CC0-1.0
// Casso modifications: additive composite of the blurred bloom buffer.

cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };
Texture2D    texSrc   : register(t0);
Texture2D    texBloom : register(t1);
SamplerState sam      : register(s0);
struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
float4 main (PSInput i) : SV_TARGET
{
    float3 b = texSrc.Sample   (sam, i.uv).rgb;
    float3 g = texBloom.Sample (sam, i.uv).rgb;
    return float4 (b + g * g_bloomStrength, 1.0);
}
