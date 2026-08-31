// The chrome painter's vertex shader: vertex color, no transform.
//
// Positions arrive already in clip space, so this only passes them through
// along with the tint the caller baked per vertex.
//
// Compiled at build time by fxc into a bytecode header; nothing compiles
// HLSL at launch.

struct VSIn  { float2 pos : POSITION; float4 col : COLOR; };
struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; };

VSOut main (VSIn input)
{
    VSOut output;
    output.pos = float4 (input.pos, 0.0f, 1.0f);
    output.col = input.col;
    return output;
}
