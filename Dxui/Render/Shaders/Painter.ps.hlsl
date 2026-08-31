// The chrome painter's pixel shader: return the interpolated vertex color.
//
// Compiled at build time by fxc into a bytecode header; nothing compiles
// HLSL at launch.

struct PSIn { float4 pos : SV_POSITION; float4 col : COLOR; };

float4 main (PSIn input) : SV_TARGET
{
    return input.col;
}
