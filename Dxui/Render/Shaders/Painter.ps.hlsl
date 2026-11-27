// The chrome painter's pixel shader.
//
// Solid quads (rects, gradients, spans) return the interpolated vertex color
// unchanged, so anything axis-aligned stays exactly as crisp as the geometry.
//
// Shaped quads (rounded rects, rings, circles, ellipses, line capsules) carry
// their shape in the vertex: a position relative to the shape's own origin,
// interpolated across the quad, plus shape parameters held constant per quad.
// A convex quad carries its four edge distances directly, interpolated.
// The shader takes the signed distance to the shape's edge at the pixel
// center and turns it into coverage over a one-pixel ramp. An edge that falls
// on a pixel boundary puts the neighboring centers at exactly -0.5 and +0.5,
// so straight runs on whole pixels come out fully covered or fully empty.
//
// The color is premultiplied, so scaling all four channels by coverage is the
// correct partial-alpha result for source-over.
//
// Compiled at build time by fxc into a bytecode header; nothing compiles
// HLSL at launch.

struct PSIn
{
    float4                  pos   : SV_POSITION;
    float4                  col   : COLOR;
    float2                  local : TEXCOORD0;
    nointerpolation float4  shape : TEXCOORD1;
    nointerpolation float   kind  : TEXCOORD2;
    float4                  edges : TEXCOORD3;
    nointerpolation float3  band  : TEXCOORD4;
    nointerpolation float2  disk  : TEXCOORD5;
    nointerpolation float4  ring  : TEXCOORD6;
};

static const int kKindSolid       = 0;
static const int kKindRoundedBox  = 1;
static const int kKindRoundedRing = 2;
static const int kKindEllipse     = 3;
static const int kKindCapsule     = 4;
static const int kKindConvexQuad  = 5;
static const int kKindTaperedCapsule = 6;
static const int kKindRingSector     = 7;
static const int kKindRingStroke     = 23;

static const float kHardEdge   = 1.0e6f;

static const float kTinyLength = 1.0e-6f;


// Rounded box centered on the origin: half-size h, corner radius r.
float DistanceToRoundedBox (float2 p, float2 h, float r)
{
    float2 q = abs (p) - h + r;

    return length (max (q, 0.0f)) + min (max (q.x, q.y), 0.0f) - r;
}


// Ring between a rounded box and the same box inset by thickness t.
float DistanceToRoundedRing (float2 p, float2 h, float r, float t)
{
    float  distance = DistanceToRoundedBox (p, h, r);
    float2 hIn      = h - t;

    // A stroke as thick as the box has no hole left to cut.
    if (hIn.x > 0.0f && hIn.y > 0.0f)
    {
        distance = max (distance, -DistanceToRoundedBox (p, hIn, max (r - t, 0.0f)));
    }

    return distance;
}


// Axis-aligned ellipse centered on the origin. The implicit function divided
// by its gradient length: exact on the edge, which is the only place the
// one-pixel coverage ramp looks.
float DistanceToEllipse (float2 p, float2 radii)
{
    float k0 = length (p / radii);
    float k1 = length (p / (radii * radii));

    // At the center the gradient vanishes; the center is deep inside anyway.
    return (k1 < kTinyLength) ? -min (radii.x, radii.y) : k0 * (k0 - 1.0f) / max (k1, kTinyLength);
}


// Segment from the origin to b, thickened by halfThickness with round caps.
float DistanceToCapsule (float2 p, float2 b, float halfThickness)
{
    float along = saturate (dot (p, b) / max (dot (b, b), kTinyLength));

    return length (p - b * along) - halfThickness;
}


// Two circles, radius ra at the origin and rb at b, joined by the lines
// tangent to both: an uneven capsule. A hole of radius hole at the origin is
// cut from it, and a rim greater than 0 limits it to a band that thick
// inside its edge.
float DistanceToTaperedCapsule (float2 p, float2 b, float ra, float rb, float hole, float rim)
{
    float  h        = max (dot (b, b), kTinyLength);
    float2 q        = float2 (dot (p, float2 (b.y, -b.x)), dot (p, b)) / h;
    float  d        = ra - rb;
    float2 c        = float2 (sqrt (max (h - d * d, kTinyLength)), d);
    float  k        = 0.0f;
    float  distance = 0.0f;

    q.x = abs (q.x);
    k   = c.x * q.y - c.y * q.x;

    if (k < 0.0f)
    {
        distance = sqrt (h * dot (q, q)) - ra;
    }
    else if (k > c.x)
    {
        distance = sqrt (h * (dot (q, q) + 1.0f - 2.0f * q.y)) - rb;
    }
    else
    {
        distance = dot (c, q) - ra;
    }

    if (rim > 0.0f)
    {
        distance = max (distance, -distance - rim);
    }

    return (hole > 0.0f) ? max (distance, hole - length (p)) : distance;
}


// A plane distance in pixels, by how fast it changes across the screen.
float ToScreenDistance (float distance)
{
    return distance / max (length (float2 (ddx (distance), ddy (distance))), kTinyLength);
}


// A pixel's offset from a tilted disk's center on screen, taken back onto
// the disk.
float2 ToDisk (float2 p, float eye, float tilt)
{
    float  cosT = cos (tilt);
    float  sinT = sin (tilt);
    float  dy   = p.y * eye / max (eye * cosT + p.y * sinT, kTinyLength);

    return float2 (p.x * (eye - dy * sinT) / eye, dy);
}


// The larger of the distances in pixels past the two ends of a stretch,
// each hard as flags bits 0 and 1 have it.
//
// Where two stretches abut, one's start and the other's end are the same
// line, so a pixel there must come out on the same side of it in both. Far
// from the center each product is many times the pixel's distance from the
// line, and the driver may fuse a multiply and an add, keeping one product
// unrounded; shader model 4 has no way to forbid it. Both ends come from one
// vector multiply-add, so each is computed the same way.
float DistancePastEnds (float2 d, float4 ends, int flags)
{
    float2  past   = ends.xz * d.y + ends.yw * d.x;
    float   start  = -past.x;
    float   end    =  past.y;
    float   startD = ToScreenDistance (start);
    float   endD   = ToScreenDistance (end);

    if ((flags & 1) != 0)
    {
        startD = (start <= 0.0f) ? -kHardEdge : kHardEdge;
    }

    if ((flags & 2) != 0)
    {
        endD = (end < 0.0f) ? -kHardEdge : kHardEdge;
    }

    return max (startD, endD);
}


// A stretch of a ring on a tilted disk. p is the pixel's offset from the
// disk's center on screen; shape is (r0, r1, eye distance, tilt) and ends is
// (sin, cos) of the start angle then of the end angle, not interpolated so
// two stretches that meet test their shared end against the same values.
// The pixel is taken back onto the disk, then measured against both radii
// and both ends, each in pixels. A hard edge (flags bit 0 the start, 1 the
// end, 2 the inner radius, 3 the outer) is all or nothing: the start and
// the inner radius include their line and the end and the outer radius
// exclude it, so stretches that meet there cover each pixel once.
float DistanceToRingSector (float2 p, float4 shape, float4 ends, int flags)
{
    float2 d      = ToDisk (p, shape.z, shape.w);
    float  r      = length (d);
    float  inner  = ToScreenDistance (shape.x - r);
    float  outer  = ToScreenDistance (r - shape.y);
    float  past   = DistancePastEnds (d, ends, flags);

    if ((flags & 4) != 0)
    {
        inner = (r >= shape.x) ? -kHardEdge : kHardEdge;
    }

    if ((flags & 8) != 0)
    {
        outer = (r < shape.y) ? -kHardEdge : kHardEdge;
    }

    return max (max (inner, outer), past);
}


// A line along a circle of a tilted disk, as for a ring stretch: shape is
// (radius, half the line's width in pixels, eye distance, tilt). Its width is
// measured on screen, so it is even all the way around.
float DistanceToRingStroke (float2 p, float4 shape, float4 ends, int flags)
{
    float2 d     = ToDisk (p, shape.z, shape.w);
    float  along = ToScreenDistance (length (d) - shape.x);

    return max (abs (along) - shape.y, DistancePastEnds (d, ends, flags));
}


// The share of the color a pixel has under a band of light at 45 degrees:
// band is (the band's center along x + y, its width, the share away from
// it). A width of 0 is no band, the whole color everywhere.
float GetShimmer (float2 pos, float3 band)
{
    float  distance = (pos.x + pos.y - band.x) / max (band.y, kTinyLength);

    return (band.y > 0.0f) ? band.z + (1.0f - band.z) * exp (-distance * distance) : 1.0f;
}


float4 main (PSIn input) : SV_TARGET
{
    int   kind     = (int) (input.kind + 0.5f);
    float distance = 0.0f;

    if (kind == kKindSolid)
    {
        return input.col;
    }

    if (kind == kKindRoundedBox)
    {
        distance = DistanceToRoundedBox (input.local, input.shape.xy, input.shape.z);
    }
    else if (kind == kKindRoundedRing)
    {
        distance = DistanceToRoundedRing (input.local, input.shape.xy, input.shape.z, input.shape.w);
    }
    else if (kind == kKindEllipse)
    {
        distance = DistanceToEllipse (input.local, input.shape.xy);
    }
    else if (kind == kKindCapsule)
    {
        distance = DistanceToCapsule (input.local, input.shape.xy, input.shape.z);
    }
    else if (kind >= kKindRingSector)
    {
        //  The disk's center is the same in every stretch, not interpolated,
        //  so a pixel's offset from it is too, and stretches that abut on a
        //  hard edge cover each pixel there once.
        distance = (kind >= kKindRingStroke) ? DistanceToRingStroke (input.pos.xy - input.disk, input.ring, input.shape, kind - kKindRingStroke)
                                             : DistanceToRingSector (input.pos.xy - input.disk, input.ring, input.shape, kind - kKindRingSector);
    }
    else if (kind == kKindTaperedCapsule)
    {
        //  Its position is in a plane's units, so the distance is turned
        //  into pixels by how fast it changes across the screen.
        distance = DistanceToTaperedCapsule (input.local, input.shape.xy, input.shape.z, input.shape.w, input.edges.x, input.edges.y);
        distance = distance / max (length (float2 (ddx (distance), ddy (distance))), kTinyLength);
    }
    else
    {
        // Convex quad: the interpolated distances to its four edge lines,
        // positive outside. Inside is where all four are negative.
        distance = max (max (input.edges.x, input.edges.y), max (input.edges.z, input.edges.w));
    }

    return input.col * saturate (0.5f - distance) * GetShimmer (input.pos.xy, input.band);
}
