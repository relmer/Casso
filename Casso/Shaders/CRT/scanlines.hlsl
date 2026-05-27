// ATTRIBUTION: Adapted from crt-pi by Davide Berra (MIT)
// Upstream URL: https://github.com/libretro/glsl-shaders/blob/master/crt/shaders/crt-pi.glsl
// Upstream collection SHA: 42fa8a98ab19bdaffb53280746a30819eb21f807
// SPDX-License-Identifier: MIT
// Casso modifications: simplified single-pass HLSL port of the scanline
//   darkening kernel only. The original crt-pi shader also performs subpixel
//   mask emulation and a curvature warp; both are omitted in this v1 port.

cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };
Texture2D    tex : register(t0);
SamplerState sam : register(s0);
struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
static const float kNativeScanlines = 192.0;
float4 main (PSInput i) : SV_TARGET
{
    float4 c       = tex.Sample (sam, i.uv);
    float  linePos = i.uv.y * kNativeScanlines;
    float  gap     = sin (linePos * 3.14159265);
    float  bright  = gap * gap;
    float  lum     = max (c.r, max (c.g, c.b));
    float  weight  = saturate (lum * 4.0);
    float  darken  = lerp (1.0, lerp (1.0 - g_scanlineIntensity, 1.0, bright), weight);
    c.rgb *= darken;
    return c;
}
