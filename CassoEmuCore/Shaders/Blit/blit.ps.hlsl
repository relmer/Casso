// The framebuffer blit's pixel shader: sample, and nothing else.
//
// Compiled at build time by fxc into a bytecode header; nothing compiles
// HLSL at launch.

Texture2D    tex : register(t0);
SamplerState sam : register(s0);

struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };

float4 main (PSInput i) : SV_TARGET
{
    return tex.Sample (sam, i.uv);
}
