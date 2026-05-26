// Casso original. Mixes current frame with previous frame's post-bloom output
// for phosphor-style persistence.

cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };
Texture2D    texCurrent : register(t0);
Texture2D    texPrev    : register(t1);
SamplerState sam        : register(s0);
struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
float4 main (PSInput i) : SV_TARGET
{
    float3 cur  = texCurrent.Sample (sam, i.uv).rgb;
    float3 prev = texPrev.Sample    (sam, i.uv).rgb;
    float3 decayed = max (prev * saturate (g_persistence) - (1.5 / 255.0), 0.0);
    return float4 (max (cur, decayed), 1.0);
}
