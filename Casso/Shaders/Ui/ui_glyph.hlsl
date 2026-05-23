// ui_glyph.hlsl
//
// Reference glyph shader for the native UI painter. Production text
// rendering goes through Direct2D / DirectWrite on top of the same
// swap-chain back buffer (see DwriteTextRenderer.cpp), which owns its
// own ClearType-aware glyph pipeline. This file documents the fallback
// path used by smoke tests / LED labels that want a single textured
// glyph sheet without spinning up DirectWrite.

Texture2D    g_glyphAtlas : register(t0);
SamplerState g_sam        : register(s0);

cbuffer GlyphConstants : register(b0)
{
    float4 g_color;       // premultiplied
};

struct VSInput
{
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOutput VSMain (VSInput input)
{
    VSOutput output;
    output.pos = float4 (input.pos, 0.0f, 1.0f);
    output.uv  = input.uv;
    return output;
}

float4 PSMain (VSOutput input) : SV_TARGET
{
    float coverage = g_glyphAtlas.Sample (g_sam, input.uv).r;
    return g_color * coverage;
}
