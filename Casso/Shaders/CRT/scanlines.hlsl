// ATTRIBUTION: Adapted from crt-pi by Davide Berra (MIT)
// Upstream URL: https://github.com/libretro/glsl-shaders/blob/master/crt/shaders/crt-pi.glsl
// Upstream collection SHA: 42fa8a98ab19bdaffb53280746a30819eb21f807
// SPDX-License-Identifier: MIT
// Casso modifications: simplified single-pass HLSL port of the scanline
//   darkening kernel only. The original crt-pi shader also performs subpixel
//   mask emulation and a curvature warp; both are omitted in this v1 port to
//   keep the kernel a single ALU-cheap pass. Brightness is applied upstream
//   in the brightness pass, so this pass only darkens between emulated
//   scanlines based on the fragment's position in *emulated* scanline space.

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

// Apple II native vertical resolution. The CPU framebuffer is 2x this
// (pixel-doubled to 384 rows so the 4:3 aspect math works out), but for
// scanline emulation we want one dark line per ORIGINAL raster line.
static const float kNativeScanlines = 192.0;

float4 main (PSInput i) : SV_TARGET
{
    float4 c = tex.Sample (sam, i.uv);

    // UV runs 0..1 over the visible content rect; uv.y * kNativeScanlines
    // gives the fractional emulated scanline index. A full cosine cycle
    // per emulated line draws one bright phosphor stripe + one dark gap.
    // sin^2 gives a smoother phosphor-glow falloff than a raw cosine.
    float linePos = i.uv.y * kNativeScanlines;
    float gap     = sin (linePos * 3.14159265);     // 0 at top of line, 1 at middle
    float bright  = gap * gap;                       // sharper peak in the middle

    // intensity == 1 means the gap rows go fully black; intensity == 0
    // is a pass-through. No fudge multiplier -- "100%" should actually
    // mean "100% scanlines".
    float darken  = lerp (1.0 - g_scanlineIntensity, 1.0, bright);

    c.rgb *= darken;
    return c;
}
