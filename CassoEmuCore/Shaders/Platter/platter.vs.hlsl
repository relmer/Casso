//  One triangle that covers the viewport; the pixel shader works from each
//  pixel's position in the target, so the triangle carries nothing else.

float4 main (uint id : SV_VertexID) : SV_Position
{
    float2  uv = float2 ((id << 1) & 2, id & 2);

    return float4 (uv * float2 (2.0, -2.0) + float2 (-1.0, 1.0), 0.0, 1.0);
}
