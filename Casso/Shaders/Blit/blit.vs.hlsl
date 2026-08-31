// The framebuffer blit's vertex shader.
//
// Positions arrive already in clip space and the pixel shader returns the
// texel unchanged: this pair does NOTHING but sample. Every visual effect
// lives in the CRT post-process chain instead.
//
// Compiled at build time by fxc into a bytecode header; nothing compiles
// HLSL at launch.

struct VSInput  { float2 pos : POSITION; float2 uv : TEXCOORD; };
struct VSOutput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };

VSOutput main (VSInput i)
{
    VSOutput o;
    o.pos = float4 (i.pos, 0.0f, 1.0f);
    o.uv  = i.uv;
    return o;
}
