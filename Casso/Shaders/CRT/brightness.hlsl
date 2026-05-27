// Casso original. Brightness + contrast pass. Used as the first stage
// of the CRT post-process chain so subsequent passes operate on already-
// corrected pixels.
//
// Contrast formula uses the "gain" model -- multiply RGB directly --
// rather than the photo-style "(in-0.5)*c + 0.5". Real CRT contrast
// knobs adjust the WHITE-point gain; they don't lift blacks toward
// grey. With the old photo formula, contrast=0.9 made pure black emit
// at 5%% grey -- distractingly visible on the amber preset.

cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };
Texture2D    tex : register(t0);
SamplerState sam : register(s0);
struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
float4 main (PSInput i) : SV_TARGET
{
    float4 c = tex.Sample (sam, i.uv);
    c.rgb = saturate (c.rgb * g_contrast * g_brightness);
    return c;
}
