// ATTRIBUTION: Adapted from crt-pi by Davide Berra (MIT)
// Upstream URL: https://github.com/libretro/glsl-shaders/blob/master/crt/shaders/crt-pi.glsl
// Upstream collection SHA: 42fa8a98ab19bdaffb53280746a30819eb21f807
// SPDX-License-Identifier: MIT
// Casso modifications: simplified single-pass HLSL port of the scanline
//   darkening kernel only. The original crt-pi shader also performs subpixel
//   mask emulation and a curvature warp; both are omitted in this v1 port to
//   keep the kernel a single ALU-cheap pass. Brightness is applied upstream
//   in the brightness pass, so this pass only darkens per scanline based on
//   the fragment row in *output* pixel space.

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

    // Output-space row index. pos.y is the rasterized pixel center.
    // Darken every other row by (1 - intensity*amp). amp follows a
    // cosine so the bright lines don't get aliased hard edges.
    float row    = i.pos.y;
    float wave   = 0.5 + 0.5 * cos (row * 3.14159265);
    float darken = 1.0 - g_scanlineIntensity * wave * 0.6;

    c.rgb *= darken;
    return c;
}
