// ui_textured.hlsl
//
// Textured-quad and 9-slice rect for the native UI painter. The vertex
// shader passes through clip-space position plus a UV; the pixel shader
// samples the bound SRV with bilinear filtering and multiplies by a
// per-draw color tint (set to white for an untinted blit).
//
// Source of truth for the build; the production renderer compiles the
// equivalent inline string in DxUiPainter.cpp.

Texture2D    g_tex : register(t0);
SamplerState g_sam : register(s0);

cbuffer Tint : register(b0)
{
    float4 g_tint;
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
    return g_tex.Sample (g_sam, input.uv) * g_tint;
}
