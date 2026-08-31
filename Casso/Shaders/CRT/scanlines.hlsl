// ATTRIBUTION: Adapted from crt-pi by Davide Berra (MIT)
// Upstream URL: https://github.com/libretro/glsl-shaders/blob/master/crt/shaders/crt-pi.glsl
// Upstream collection SHA: 42fa8a98ab19bdaffb53280746a30819eb21f807
// SPDX-License-Identifier: MIT
// Casso modifications: simplified single-pass HLSL port of the scanline
//   darkening kernel only. The original crt-pi shader also performs subpixel
//   mask emulation and a curvature warp; both are omitted in this v1 port.
//   The kernel is also AREA-AVERAGED rather than point sampled -- see below.

cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };
Texture2D    tex : register(t0);
SamplerState sam : register(s0);
struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };

static const float kNativeScanlines = 192.0;
static const float kPi              = 3.14159265;

//  A //e draws 192 scanlines, and this pass lays down 192 cycles across the
//  target however many pixels tall it is. When the target is comfortably
//  taller than 384 those cycles are well resolved and the point-sampled
//  sin^2 that used to live here looked right. When it is NOT -- and the
//  picture is often only ~370 px tall -- 192 cycles land under two pixels
//  apart, below Nyquist, and point sampling turns them into a moire beat
//  tens of pixels wide that crawls as the window resizes.
//
//  So integrate the kernel over the pixel instead of sampling it at a point.
//  For sin^2(pi*L) == (1 - cos(2*pi*L)) / 2, the mean over a pixel spanning
//  dL cycles has a closed form:
//
//      mean = 1/2 - 1/2 * cos(2*pi*L) * sinc(dL),   sinc(x) = sin(pi*x)/(pi*x)
//
//  which is the original kernel scaled by sinc. Many pixels per cycle leaves
//  sinc at 1 and nothing changes; one pixel per cycle drives it to 0 and the
//  scanlines fade to flat rather than aliasing. Between those it rolls off
//  smoothly, so resizing dims the lines instead of making them shimmer.
float4 main (PSInput i) : SV_TARGET
{
    float4 c       = tex.Sample (sam, i.uv);
    float  linePos = i.uv.y * kNativeScanlines;
    float  perPix  = max (abs (ddy (linePos)), 1e-6);
    float  rolloff = max (sin (kPi * perPix) / (kPi * perPix), 0.0);
    float  bright  = 0.5 - 0.5 * cos (2.0 * kPi * linePos) * rolloff;
    float  lum     = max (c.r, max (c.g, c.b));
    float  weight  = saturate (lum * 4.0);
    float  darken  = lerp (1.0, lerp (1.0 - g_scanlineIntensity, 1.0, bright), weight);
    c.rgb *= darken;
    return c;
}
