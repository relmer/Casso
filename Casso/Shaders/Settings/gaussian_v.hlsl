// Casso original. Vertical separable Gaussian blur for Settings preview composition.

cbuffer SettingsBlurCb : register(b0) { float g_radiusPx; float g_outputW; float g_outputH; float g_pad; };
Texture2D    tex : register(t0);
SamplerState sam : register(s0);
struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
static const float W[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };
float4 main (PSInput i) : SV_TARGET
{
    float  ty     = 1.0 / max (g_outputH, 1.0);
    float  stepPx = max (g_radiusPx, 1.0) * 0.25;
    float4 acc    = tex.Sample (sam, i.uv) * W[0];
    [unroll] for (int k = 1; k < 5; ++k)
    {
        float2 off = float2 (0.0, ty * stepPx * (float) k);
        acc += tex.Sample (sam, i.uv + off) * W[k];
        acc += tex.Sample (sam, i.uv - off) * W[k];
    }
    return acc;
}
