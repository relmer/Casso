// Casso original. Brightness + contrast pass. Used as the first stage
// of the CRT post-process chain so subsequent passes operate on already-
// corrected pixels.
//
// Photo-style contrast model: scale RGB around mid-grey 0.5 by the
// contrast factor, so contrast<1 pulls values toward 0.5 (more grey,
// less black, less white) and contrast>1 pushes them away (more
// punch). This is what users expect from "contrast" knob behaviour;
// a previous gain-only model (multiply by contrast) just darkened
// uniformly when reduced and was visually indistinguishable from
// brightness.
//
// Brightness is an additive offset (`+ (brightness - 1.0)`), which
// lifts ALL pixels uniformly including blacks. Mirrors a real CRT
// brightness knob (DC offset / cathode bias). The two controls are
// now visibly distinct: contrast scales around mid-grey, brightness
// shifts the whole curve.
//
// Identity at brightness=1.0, contrast=1.0: (c - 0.5)*1 + 0.5 + 0 = c.

cbuffer CrtCb : register(b0) { float g_brightness; float g_scanlineIntensity; float g_bloomRadius; float g_bloomStrength; float g_colorBleedWidth; float g_outputW; float g_outputH; float g_contrast; float g_gamma; float g_persistence; };
Texture2D    tex : register(t0);
SamplerState sam : register(s0);
struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
float4 main (PSInput i) : SV_TARGET
{
    float4 c = tex.Sample (sam, i.uv);
    c.rgb = saturate ((c.rgb - 0.5) * g_contrast + 0.5 + (g_brightness - 1.0));
    return c;
}
