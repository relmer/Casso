// Casso original. Settings preview compose shader.
//
// Per-pixel logic in order of precedence:
//   1. Inside focused control's feathered rect -> sharp source. Wins
//      over emu clip so the slider the user is dragging stays fully
//      visible even when it overlaps the emulator.
//   2. Inside emulator rect -> fully transparent (see-through to the
//      Casso back buffer behind the popup).
//   3. Otherwise -> blurred + dimmed.
//
// Feathering: the focused rect's edges ramp from "fully sharp" inside
// to "fully blurred" `g_featherPx` pixels outside the rect, so the
// focus pop doesn't have a harsh boundary.

cbuffer SettingsComposeCb : register(b0)
{
    float4 g_emuRectClient;
    float4 g_focusRectClient;
    float  g_outputW;
    float  g_outputH;
    float  g_dimFactor;
    float  g_featherPx;
};

Texture2D    texSharp : register(t0);
Texture2D    texBlur  : register(t1);
SamplerState sam      : register(s0);
struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };

bool InRect (float2 pos, float4 rect)
{
    return pos.x >= rect.x && pos.x < rect.z && pos.y >= rect.y && pos.y < rect.w;
}

// Returns 1.0 inside the rect (with feather inset 0), falling off
// smoothly to 0.0 at `feather` pixels outside the rect. When feather
// is 0 this collapses to a hard inside/outside test.
float FocusWeight (float2 pos, float4 rect, float feather)
{
    float dx = max (0.0, max (rect.x - pos.x, pos.x - rect.z));
    float dy = max (0.0, max (rect.y - pos.y, pos.y - rect.w));
    float d  = sqrt (dx * dx + dy * dy);
    if (feather <= 0.0)
    {
        return (dx == 0.0 && dy == 0.0) ? 1.0 : 0.0;
    }
    return 1.0 - saturate (d / feather);
}

float4 main (PSInput i) : SV_TARGET
{
    float2  pos      = i.uv * float2 (g_outputW, g_outputH);
    float4  sharp    = texSharp.Sample (sam, i.uv);
    float4  blurred  = texBlur.Sample  (sam, i.uv);
    float   focusW   = FocusWeight (pos, g_focusRectClient, g_featherPx);
    bool    inEmu    = InRect (pos, g_emuRectClient);

    // Blurred + dimmed is the baseline.
    float4  dimmed = blurred;
    dimmed.rgb *= saturate (g_dimFactor);

    // Inside emu zone the BASELINE collapses to fully transparent,
    // but the focused-control sharp source still wins via focusW.
    float4  baseline = inEmu ? float4 (0.0, 0.0, 0.0, 0.0) : dimmed;

    // Premultiplied-alpha lerp: sharp's rgb is already premultiplied
    // by its alpha, so straight lerp on the float4 is correct.
    return lerp (baseline, sharp, focusW);
}
