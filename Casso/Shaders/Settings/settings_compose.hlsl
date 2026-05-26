// Casso original. Settings preview compose shader with per-pixel emulator clip.

cbuffer SettingsComposeCb : register(b0) { float4 g_emuRectClient; float4 g_focusRectClient; float g_outputW; float g_outputH; float g_dimFactor; float g_pad; };
Texture2D    texSharp : register(t0);
Texture2D    texBlur  : register(t1);
SamplerState sam      : register(s0);
struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };

bool InRect (float2 pos, float4 rect)
{
    return pos.x >= rect.x && pos.x < rect.z && pos.y >= rect.y && pos.y < rect.w;
}

float4 main (PSInput i) : SV_TARGET
{
    float2 pos = i.uv * float2 (g_outputW, g_outputH);

    if (InRect (pos, g_emuRectClient))
    {
        return float4 (0.0, 0.0, 0.0, 0.0);
    }

    float4 sharp = texSharp.Sample (sam, i.uv);
    if (InRect (pos, g_focusRectClient))
    {
        return sharp;
    }

    float4 blurred = texBlur.Sample (sam, i.uv);
    blurred.rgb *= saturate (g_dimFactor);
    return blurred;
}
