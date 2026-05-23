// ui_glow.hlsl
//
// Additive radial soft-glow sample for LED active state. The vertex
// shader emits a unit-quad UV; the pixel shader computes distance from
// the quad center and falls off via smoothstep so the glow halo grades
// cleanly to zero alpha at the edge. Output is premultiplied so the
// painter can composite this pass on top of the LED core with a single
// SRC_ONE / DEST_ONE additive blend.

cbuffer GlowConstants : register(b0)
{
    float4 g_coreColor;   // premultiplied
    float4 g_haloColor;   // premultiplied
    float  g_haloFalloff; // 0..1; smaller = sharper edge
    float  g_pad0;
    float  g_pad1;
    float  g_pad2;
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
    float2 centered = input.uv - float2 (0.5f, 0.5f);
    float  dist     = length (centered) * 2.0f;
    float  core     = saturate (1.0f - dist);
    float  halo     = smoothstep (1.0f, g_haloFalloff, dist);

    return g_coreColor * core + g_haloColor * halo;
}
