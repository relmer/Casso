// The chrome painter's vertex shader: vertex color, no transform.
//
// Positions arrive already in clip space, so this only passes them through
// along with the tint the caller baked per vertex and the shape description
// the pixel shader turns into edge coverage.
//
// Compiled at build time by fxc into a bytecode header; nothing compiles
// HLSL at launch.

struct VSIn
{
    float2  pos   : POSITION;
    float4  col   : COLOR;
    float2  local : TEXCOORD0;
    float4  shape : TEXCOORD1;
    float   kind  : TEXCOORD2;
    float4  edges : TEXCOORD3;
};

struct VSOut
{
    float4                  pos   : SV_POSITION;
    float4                  col   : COLOR;
    float2                  local : TEXCOORD0;
    nointerpolation float4  shape : TEXCOORD1;
    nointerpolation float   kind  : TEXCOORD2;
    float4                  edges : TEXCOORD3;
};

VSOut main (VSIn input)
{
    VSOut output;
    output.pos   = float4 (input.pos, 0.0f, 1.0f);
    output.col   = input.col;
    output.local = input.local;
    output.shape = input.shape;
    output.kind  = input.kind;
    output.edges = input.edges;
    return output;
}
