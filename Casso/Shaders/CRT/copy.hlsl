// Casso original. Pass-through copy used as the final blit from the
// post-process ping-pong RT to the swap chain back buffer.
//
// Pass-through except for one thing: this is where the chain's ten-bit
// scratch becomes the back buffer's eight, so this is the only place the
// rounding is ever seen, and the only place worth dithering.

Texture2D    tex : register(t0);
SamplerState sam : register(s0);
struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };

// A hash of the pixel's own integer coordinates, and nothing else.
//
// INTEGER bit arithmetic rather than the usual frac(sin(dot(p,k)) * 43758),
// which rides on transcendental float precision and grains differently on
// different GPUs and driver versions. This is exact everywhere, so a
// screenshot taken on one machine still compares against one taken on
// another. Nothing here reads a clock or a frame counter either: a still
// picture stays byte for byte still, and the grain never crawls.
uint DitherHash (int2 c)
{
    uint h = (uint) (c.x * 374761393 + c.y * 668265263);
    h ^= h >> 13;
    h *= 1274126177u;
    return h ^ (h >> 16);
}
float DitherRand (int2 c, int salt)
{
    return (float) (DitherHash (c + int2 (salt * 7, salt * 13)) & 0xFFFFu)
         * (1.0f / 65535.0f);
}
// Under one code of noise, added before the hardware rounds.
//
// A gradient this shallow crosses a half-step every few hundred pixels, and
// rounding puts that crossing on a single contour: one hard edge running the
// width of the picture, which is exactly what the eye is built to find.
// Scattering the crossing over the pixels either side of it spends the same
// error without ever lining it up. The banding goes; what replaces it is
// grain a fraction of a code deep, well under what is visible on a dark
// screen.
//
// TRIANGULAR, from two independent draws rather than one. A single flat draw
// leaves the residual error correlated with the signal, so a ghost of the
// contour survives right where the gradient is slowest -- which is the case
// this exists for. Summing two decorrelates it.
//
// One offset for all three channels, not three: a neutral gray dithered per
// channel picks up faint color speckle, and moved together it stays gray.
float DitherOffset (float2 pixel)
{
    int2   c = int2 ((int) pixel.x, (int) pixel.y);
    float  a = DitherRand (c, 11);
    float  b = DitherRand (c, 23);
    return (a + b - 1.0f) * (1.0f / 255.0f);
}
float4 main (PSInput i) : SV_TARGET
{
    float4 c = tex.Sample (sam, i.uv);
    c.rgb += DitherOffset (i.pos.xy);
    return c;
}
