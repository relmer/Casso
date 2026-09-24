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
};

static const int kKindSolid       = 0;
static const int kKindRoundedBox  = 1;
static const int kKindRoundedRing = 2;
static const int kKindEllipse     = 3;
static const int kKindCapsule     = 4;
static const int kKindConvexQuad  = 5;

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
    else
    {
        // Convex quad: the interpolated distances to its four edge lines,
        // positive outside. Inside is where all four are negative.
        distance = max (max (input.edges.x, input.edges.y), max (input.edges.z, input.edges.w));
    }

    return input.col * saturate (0.5f - distance);
}
