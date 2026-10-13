//  The disk inspector's platter (FR-022, FR-023, FR-026).
//
//  Each pixel's distance from the center gives a ring, track 0 at the rim,
//  and its angle clockwise from 12 o'clock gives a position along the ring.
//  Kinds has every ring's cells, one byte each, followed by its coarser
//  levels, all laid end to end across rows of kTextureWidth; Rings has,
//  per ring, the cell count, the ring's state and where each level starts.
//  The level drawn is the finest one whose texels are each at least as long
//  as the pixel's arc, so a pixel samples one texel with the most
//  important kind under it.
//
//  Timing has each cell's deviation from the nominal cell, 128 at nominal
//  and 1 and 255 at full scale, at the same texels as its kind.
//
//  Texture changes brightness alone, never a ring's color (FR-149): a grain
//  that varies from texel to texel, a line between cells once each spans
//  two pixels, a dark seam between rings at least kSeamMinPx wide, and a
//  darker rim.
//
//  Must match PlatterRenderer: kTextureWidth, kRingStride, kTimingFlag and
//  the state values.

static const uint  kTextureWidth = 4096;
static const uint  kMaxLevels    = 17;
static const uint  kRingStride   = kMaxLevels + 2;

static const uint  kStateData    = 0;
static const uint  kStateNothing = 1;
static const uint  kStatePending = 2;
static const uint  kStateDamaged = 3;

static const float kTwoPi        = 6.28318530718;
static const float kPatternPx    = 6.0;
static const uint  kTimingFlag   = 0x100;
static const float kFullScale    = 0.25;
static const float kDimShare     = 0.6;

static const float kGrainShare   = 0.16;
static const float kCellLineDark = 0.8;
static const float kSeamDark     = 0.72;
static const float kSeamMinPx    = 3.0;
static const float kRimPx        = 6.0;
static const float kRimDark      = 0.55;

cbuffer Platter : register (b0)
{
    float2  center;
    float   outerRadiusPx;
    float   innerFraction;
    float   ringCount;
    float   rotation;
    float   headLimitRing;
    float   grooveMinPx;
    float4  kindColors[16];
    float4  grooveColor;
    float4  nothingColor;
    float4  pendingColor;
    float4  damagedColor;
    float4  hatchColor;
    float4  beyondColor;
    float4  fastColor;
    float4  nominalColor;
    float4  slowColor;
    float   timingMode;
    float   timingRange;
    float2  padding;
};

Texture2D<uint>  Kinds  : register (t0);
Buffer<uint>     Rings  : register (t1);
Texture2D<uint>  Timing : register (t2);



//  A well-mixed 32-bit hash, so neighboring texels get unrelated grain.
uint Hash (uint x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;

    return x;
}



float4 SampleRing (uint ring, float turn, float radiusPx, float2 position)
{
    uint    base   = ring * kRingStride;
    uint    count  = Rings[base];
    uint    flags  = Rings[base + 1];
    uint    state  = flags & 0xFF;
    float4  color  = nothingColor;
    float   stripe = frac ((position.x + position.y) / (2.0 * kPatternPx));
    float   shade  = 1.0;
    float   cellPx = 0.0;
    float   along  = 0.0;



    if (state == kStatePending)
    {
        color = stripe < 0.5 ? pendingColor : nothingColor;
    }
    else if (state == kStateDamaged)
    {
        color = stripe < 0.25 ? hatchColor : damagedColor;
    }
    else if (state == kStateData && count > 0)
    {
        float  cellsPerPixel = count / max (kTwoPi * radiusPx, 1.0);
        uint   level         = cellsPerPixel <= 1.0 ? 0 : min ((uint) ceil (log2 (cellsPerPixel)), kMaxLevels - 1);
        uint   levelCount    = (count + (1u << level) - 1) >> level;
        uint   index         = min (((uint) (turn * count)) >> level, levelCount - 1);
        uint   texel         = Rings[base + 2 + level] + index;
        int3   at            = int3 (texel % kTextureWidth, texel / kTextureWidth, 0);
        uint   value         = Kinds.Load (at);

        color = kindColors[value & 15];

        //  Timing mode colors a flux cell by its deviation, fast to slow
        //  through nominal, and dims a track that records no timing.
        if (timingMode > 0.5 && (flags & kTimingFlag) != 0)
        {
            float  deviation = ((float) Timing.Load (at) - 128.0) / 127.0 * kFullScale;
            float  t         = clamp (deviation / timingRange, -1.0, 1.0);

            color = (t < 0.0) ? lerp (nominalColor, fastColor, -t) : lerp (nominalColor, slowColor, t);
        }
        else if (timingMode > 0.5)
        {
            color = lerp (color, nothingColor, kDimShare);
        }

        //  Grain fixed to the texel, so it turns with the disk, and once a
        //  cell spans two pixels, a darker line at each cell's edge.
        shade = 1.0 + ((float) (Hash (texel * 2654435761u + ring) & 255) / 255.0 - 0.5) * kGrainShare;

        if (cellsPerPixel <= 0.5)
        {
            cellPx = 1.0 / cellsPerPixel;
            along  = frac (turn * count) * cellPx;
            shade *= (min (along, cellPx - along) < 0.5) ? kCellLineDark : 1.0;
        }

        color.rgb *= shade;
    }

    return color;
}



float4 main (float4 position : SV_Position) : SV_Target
{
    float2  d          = position.xy - center;
    float   radiusPx   = length (d);
    float   r          = radiusPx / outerRadiusPx;
    float   ringPx     = (1.0 - innerFraction) * outerRadiusPx / ringCount;
    float   ringF      = (outerRadiusPx - radiusPx) / ringPx;
    uint    ring       = (uint) max (ringF, 0.0);
    float   turn       = frac (atan2 (d.x, -d.y) / kTwoPi - rotation + 1.0);
    float4  color;



    if (r > 1.0 || r < innerFraction || ring >= (uint) ringCount)
    {
        discard;
    }

    color = SampleRing (ring, turn, radiusPx, position.xy);

    //  A groove at each whole-track boundary, once whole tracks are far
    //  enough apart for one to read as a line, and a dark seam at every other
    //  ring's edge once rings are wide enough for one.
    if (ring % 4 == 0 && frac (ringF) * ringPx < 1.0 && ringPx * 4.0 >= grooveMinPx)
    {
        color = grooveColor;
    }
    else if (ringPx >= kSeamMinPx && frac (ringF) * ringPx < 1.0)
    {
        color.rgb *= kSeamDark;
    }

    //  The rim darkens toward the disk's edge.
    color.rgb *= lerp (kRimDark, 1.0, saturate ((outerRadiusPx - radiusPx) / kRimPx));

    if ((float) ring >= headLimitRing)
    {
        color = float4 (lerp (color.rgb, beyondColor.rgb, beyondColor.a), color.a);
    }

    return color;
}
