// The desk scene's vertex shader.
//
// Compiled at BUILD time by fxc into a bytecode header; nothing compiles
// HLSL at launch. This used to be a C++ string literal in
// Dxui3DRenderer.cpp, where it could not be read as the shader it is.
//
// Row-vector convention (clip = v * M) with a row_major cbuffer matrix, so
// the CPU-side float[16] goes into the constant buffer untransposed.

cbuffer Mvp : register(b0) { row_major float4x4 mvp; };
struct VSIn  { float3 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR;
               float3 nrm : NORMAL;   float3 emi : COLOR1;   float peb : TEXCOORD1; };
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR;
               float3 nrm : NORMAL;     float3 emi : COLOR1; float3 wp : TEXCOORD1;
               float  peb : TEXCOORD2; };
VSOut main (VSIn input)
{
    VSOut output;
    output.pos = mul (float4 (input.pos, 1.0f), mvp);
    output.uv  = input.uv;
    output.col = input.col;
    output.nrm = input.nrm;
    output.emi = input.emi;
    output.wp  = input.pos;   // pre-transform: lights live in this space
    output.peb = input.peb;
    return output;
}
