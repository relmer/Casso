// Textured pass for RmlUi 6.x meshes.
//
// Vertex layout (matches Rml::Vertex):
//   float2 position    (screen-space pixels, top-left origin)
//   byte4  color       (RGBA premultiplied, normalized to 0..1)
//   float2 tex_coord
//
// Constant buffer slot 0:
//   float4x4 mvp                 (row-major)
//   float4   translation_pad     (xy = per-draw screen translation,
//                                 zw = unused)

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
    float2 uv        : TEXCOORD0;
};


Texture2D    g_texture : register (t0);
SamplerState g_sampler : register (s0);


PSIn VSMain (VSIn input)
{
    PSIn output;

    float2 p = input.pos + g_translation_pad.xy;

    output.sv_pos = mul (float4 (p, 0.0f, 1.0f), g_mvp);
    output.color  = input.color;
    output.uv     = input.uv;

    return output;
}


float4 PSMain (PSIn input) : SV_TARGET
{
    float4 t = g_texture.Sample (g_sampler, input.uv);
    return t * input.color;
}
