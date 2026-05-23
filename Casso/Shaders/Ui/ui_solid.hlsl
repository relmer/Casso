// ui_solid.hlsl
//
// Solid-fill (or linear-gradient) rect for the native UI painter. Each
// vertex carries a position in normalized device coordinates plus a
// premultiplied-alpha RGBA color. The pixel shader returns the
// interpolated color directly so the same shader handles both flat
// fills (all four vertex colors equal) and linear gradients (top/bottom
// or left/right vertex colors differ).
//
// Source of truth for the build; the production renderer compiles the
// equivalent inline string in DxUiPainter.cpp to avoid an HLSL build
// dependency.

struct VSInput
{
    float2 pos   : POSITION;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR;
};

VSOutput VSMain (VSInput input)
{
    VSOutput output;
    output.pos   = float4 (input.pos, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}

float4 PSMain (VSOutput input) : SV_TARGET
{
    return input.color;
}
