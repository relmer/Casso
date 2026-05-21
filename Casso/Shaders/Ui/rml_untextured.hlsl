// Untextured pass for RmlUi 6.x meshes. See rml_textured.hlsl for
// the shared constant-buffer + vertex-layout contract.

cbuffer Constants : register (b0)
{
    row_major float4x4 g_mvp;
    float4             g_translation_pad;
};


struct VSIn
{
    float2 pos       : POSITION;
    float4 color     : COLOR;
    float2 uv        : TEXCOORD0;
};

struct PSIn
{
    float4 sv_pos    : SV_POSITION;
    float4 color     : COLOR;
};


PSIn VSMain (VSIn input)
{
    PSIn output;

    float2 p = input.pos + g_translation_pad.xy;

    output.sv_pos = mul (float4 (p, 0.0f, 1.0f), g_mvp);
    output.color  = input.color;

    return output;
}


float4 PSMain (PSIn input) : SV_TARGET
{
    return input.color;
}
