// VertexShader.hlsl — Full-window textured quad vertex shader

struct VSInput
{
    float2 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

VSOutput main (VSInput input)
{
    VSOutput output;
    output.position = float4 (input.position, 0.0f, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}
