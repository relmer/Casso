// The desk scene's pixel shader.
//
// Compiled at BUILD time by fxc into a bytecode header; nothing compiles
// HLSL at launch. This used to be a C++ string literal in
// Dxui3DRenderer.cpp, where it could not be read as the shader it is.

Texture2D    tex  : register(t0);
SamplerState samp : register(s0);
cbuffer Light : register(b1)
{
    float4 l0;        // xyz light 0
    float4 l1;        // xyz light 1
    float4 eye;       // xyz DIRECTION toward the viewer
    float4 parm;      // x refDist, y span, z gain, w specStrength
    float4 parm2;     // x specPower
    float4 ambUp;     // xyz ceiling bounce
    float4 ambDown;   // xyz desk bounce
    float4 lampPos;   // xyz, w refDist
    float4 lampDir;   // xyz lens facing, w range
    float4 lampCol;   // xyz, zero disables; w = emission wrap
    row_major float4x4 shadow0;
    row_major float4x4 shadow1;
    float4 shadowParm;   // x texel (0 disables), y bias, z strength
    row_major float4x4 lampShadow;
    float4 lampShadowParm;   // x texel (0 disables), y bias, zw throw cone
    float4 parm3;            // x pebble pitch (mm), y pebble amount
// THE SPILL'S CEILING, per channel: the lens's own color, and only as
// much of it as the surface has ROOM for.
//
// A LAMP MAY ONLY FILL THE HEADROOM THAT IS LEFT, tinted by its own
// color. The light term is scaled by the receiving surface's own base
// color, which is what lets one gain read as a lamp on black plastic and
// as a flashlight on cream -- so a cream wall a few millimeters from an
// indicator takes several times full white and blows out to a flat
// supernova while the lens itself sits at its own quiet green. The
// Monitor II's power notch is that case exactly: the first lamp in the
// scene mounted INSIDE a pocket, with the pocket's walls squarely facing
// it at point-blank range.
//
// Capping against the remaining headroom rather than against a flat value
// is what leaves every lamp that was tuned on DARK plastic exactly where
// it was: black has nearly all its range still to climb, so its spill
// never reaches the ceiling and never notices it. Only the pathological
// bright-surface case is clipped, which is the only case that was wrong.
//
// HALF the headroom, not all of it. A lamp whose color runs to 1.0 in a
// channel -- and an indicator green does -- would otherwise drive that
// channel to exactly full on any surface it reaches, which is a clipped
// plateau wearing a tint rather than a light. Half leaves every channel
// room to still be graded by distance and angle.
    float4 lampCap;          // xyz spill ceiling; the lens's own color
};
Texture2D              shadowTex0 : register(t1);
Texture2D              shadowTex1 : register(t2);
Texture2D              lampShadowTex : register(t3);
SamplerComparisonState shadowSamp : register(s1);
struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR;
              float3 nrm : NORMAL;      float3 emi : COLOR1;   float3 wp : TEXCOORD1;
              float  peb : TEXCOORD2; };
// A molded-in pebble finish, computed rather than sampled.
//
// An INTEGER hash of the quantized position, not the usual
// frac(sin(dot(p,k)) * 43758.5453): that one rides on transcendental
// float precision and gives visibly different grain on different GPUs
// and driver versions. Bit ops on integers are exact, so every machine
// renders the same drive -- which also keeps screenshot comparisons
// meaningful.
//
// Nothing here reads a clock or a frame counter. Same point, same value,
// every frame and every run; the only thing it shares with randomness is
// that it looks irregular.
uint HashCell (int3 c)
{
    uint h = (uint) (c.x * 374761393 + c.y * 668265263 + c.z * 1274126177);
    h ^= h >> 13;
    h *= 1274126177u;
    return h ^ (h >> 16);
}
float CellRand (int3 c, int salt)
{
    return (float) (HashCell (c + int3 (salt * 7, salt * 13, salt * 29)) & 0xFFFFu)
         * (1.0f / 65535.0f);
}
// Smooth value noise, INTERPOLATED across the cell rather than constant
// within it. The cell value alone is a cube of one number, so the finish
// rendered as flat blocks a few pixels across -- digital noise, not a
// molded grain. Reading all eight corners and easing between them turns
// the same hash into a continuous undulation, which is what a pebbled
// surface actually is: the mold's texture is smooth at every scale, only
// irregular.
//
// The ease is the standard smoothstep weighting, so the field's slope is
// continuous at the cell walls too -- linear weights leave a crease on
// every boundary, which reads as a faint grid.
float SmoothRand (float3 p, int salt)
{
    int3   c = (int3) floor (p);
    float3 f = p - (float3) c;
    float3 w = f * f * (3.0f - 2.0f * f);
    float  x00 = lerp (CellRand (c + int3 (0,0,0), salt), CellRand (c + int3 (1,0,0), salt), w.x);
    float  x10 = lerp (CellRand (c + int3 (0,1,0), salt), CellRand (c + int3 (1,1,0), salt), w.x);
    float  x01 = lerp (CellRand (c + int3 (0,0,1), salt), CellRand (c + int3 (1,0,1), salt), w.x);
    float  x11 = lerp (CellRand (c + int3 (0,1,1), salt), CellRand (c + int3 (1,1,1), salt), w.x);
    return lerp (lerp (x00, x10, w.y), lerp (x01, x11, w.y), w.z);
}
// THE SHADING IS FLOAT AND THE PLATE IS EIGHT BITS, and this is what stands
// between them.
//
// A room light falls off with the square of distance, so out where the
// backdrop meets the frame the gradient's slope is nearly flat: hundreds of
// pixels share one code value, and the next hundreds share the next. Rounding
// alone turns that into stripes -- a hard edge every time the value crosses a
// half-step -- and the darkest tenth of the range, which is where a black case
// on a dim desk spends all of its time, is exactly where sRGB spaces its codes
// furthest apart. Fullscreen makes each stripe physically wider, and the plate
// cache holds the whole thing perfectly still, so there is nothing left for the
// eye to average away.
//
// Perturbing the value by under one code before it is rounded moves the
// crossing off the contour and scatters it over the pixels around it. The
// error is the same size it always was; it is simply no longer aligned into a
// line. What replaces the stripes is grain a fraction of a code deep, which is
// below what the eye resolves on a dark surface -- the banding is not so.
//
// TRIANGULAR, from two independent draws rather than one. A flat draw leaves
// the residual error correlated with the signal, which keeps a ghost of the
// contour visible right where the gradient is slowest -- which is the case
// this exists for. Summing two decorrelates it outright.
//
// The value comes from the same integer hash the pebble finish uses, for the
// same reason: exact bit arithmetic renders identically on every GPU, and it
// depends on nothing but the pixel's own coordinates. No clock, no frame
// counter. The grain is fixed to the plate, so a cached plate stays byte for
// byte what it was and screenshot comparisons still mean something.
float DitherOffset (float2 pixel)
{
    int3   c = int3 ((int) pixel.x, (int) pixel.y, 0);
    float  a = CellRand (c, 11);
    float  b = CellRand (c, 23);
    // One offset for all three channels, not three. A neutral gray dithered
    // per channel picks up faint color speckle; moved together it stays gray.
    return (a + b - 1.0f) * (1.0f / 255.0f);
}
float4 main (PSIn input) : SV_TARGET
{
    float4 texel = tex.Sample (samp, input.uv);
    float4 base  = texel * input.col;
    float3 lit   = base.rgb;
    if (dot (input.nrm, input.nrm) > 0.5f)
    {
        float3 n    = normalize (input.nrm);
        if (input.peb > 0.0f)
        {
// THREE octaves, and a CAVITY term. Tilting the normal alone cannot read
// as depth on a near-black surface viewed head-on: a matte plastic
// returns almost the same value however it is tilted, so the grain came
// out flat no matter how hard the normal was pushed.
//
// What the eye actually reads as three-dimensional is that the pits are
// DARKER -- less of the room reaches the bottom of a dimple than its rim.
// So the same field that bends the normal also drives an occlusion term,
// which is what turns a shimmer into a texture with a floor and a top.
//
// Three frequencies rather than two because a molded grain is not one
// size of bump: the coarse octave gives the mottle, the middle one the
// grain proper, and the finest keeps it from reading as cells at all.
            float3 p  = input.wp / parm3.x;
            float3 j  = float3 (SmoothRand (p, 1), SmoothRand (p, 2),
                                SmoothRand (p, 3)) * 2.0f - 1.0f;
            float3 j2 = float3 (SmoothRand (p * 2.7f, 4), SmoothRand (p * 2.7f, 5),
                                SmoothRand (p * 2.7f, 6)) * 2.0f - 1.0f;
            float3 j3 = float3 (SmoothRand (p * 6.9f, 7), SmoothRand (p * 6.9f, 8),
                                SmoothRand (p * 6.9f, 9)) * 2.0f - 1.0f;
            n = normalize (n + (j + j2 * 0.45f + j3 * 0.22f)
                             * (parm3.y * input.peb));
// Height at this point, from the same octaves, centered on zero. Pits go
// negative and rims positive, so the base color is scaled by a factor
// straddling 1 -- the surface keeps its average value rather than simply
// going darker.
            float  h  = (SmoothRand (p, 1) - 0.5f)
                      + (SmoothRand (p * 2.7f, 4) - 0.5f) * 0.45f
                      + (SmoothRand (p * 6.9f, 7) - 0.5f) * 0.22f;
// A HORIZON TEST, which is what actually makes a bump look like a bump.
// Cavity darkening alone says "this spot is low"; it never says "this
// spot is low BECAUSE something beside it is in the way", so the finish
// still read as a stain rather than as relief.
//
// The room's fixtures are overhead, so the occluder is whatever sits just
// ABOVE a point -- model +Z. Sampling the height there and darkening when
// it stands higher gives every bump a small shadow on its underside,
// which is the cue the eye uses for depth and the one that was missing.
//
// Two octaves for the probe rather than three: the finest is below a
// shadow's scale anyway, and this is the one term that costs an extra
// pair of noise fetches.
            float3 up = p + float3 (0.0f, 0.0f, 0.55f);
            float  hu = (SmoothRand (up, 1) - 0.5f)
                      + (SmoothRand (up * 2.7f, 4) - 0.5f) * 0.45f;
            float  occ = saturate ((hu - h) * 2.2f);
            base.rgb *= saturate (1.0f + h * parm3.z * input.peb)
                      * saturate (1.0f - occ * parm3.w * input.peb);
        }
        float3 v    = normalize (eye.xyz);
        float  diff = 0.0f;
        float  spec = 0.0f;
        [unroll] for (int k = 0; k < 2; k++)
        {
            float3 lp = (k == 0) ? l0.xyz : l1.xyz;
            float3 d  = lp - input.wp;
            float  r  = max (length (d), 1e-4f);
            float3 L  = d / r;
            float  at = (parm.x * parm.x) / (r * r);
            float  nl = saturate (dot (n, L));
            float  vis = 1.0f;
// Shadow lookup. The texture fetch is written out per light rather than
// indexed because ps_4_0 has no texture arrays and cannot take a
// Texture2D as a parameter; the k loop is unrolled, so both ternaries
// fold away at compile time and this costs nothing at runtime.
            if (shadowParm.x > 0.0f && nl > 0.0f)
            {
                float4 lc = (k == 0) ? mul (float4 (input.wp, 1.0f), shadow0)
                                     : mul (float4 (input.wp, 1.0f), shadow1);
                if (lc.w > 0.0f)
                {
                    float3 p  = lc.xyz / lc.w;
                    float2 uv = float2 (p.x * 0.5f + 0.5f, 0.5f - p.y * 0.5f);
// Outside the map is LIT, never shadowed: a caster's frustum covers the
// scene, so falling outside means nothing was in the way.
                    if (max (abs (p.x), abs (p.y)) <= 1.0f && p.z >= 0.0f && p.z <= 1.0f)
                    {
// Hardware comparison filtering: each tap is already bilinear across
// four texels, so nine of them span an effective six-by-six and the
// edge comes out graded instead of staircased. Doing the compare by
// hand cost the same nine fetches and gave nine hard yes-or-no answers.
                        float lit = 0.0f;
                        float ref = p.z - shadowParm.y;
                        [unroll] for (int sy = -1; sy <= 1; sy++)
                        {
                            [unroll] for (int sx = -1; sx <= 1; sx++)
                            {
                                float2 o = uv + float2 (sx, sy) * shadowParm.x * 1.5f;
                                lit += (k == 0)
                                     ? shadowTex0.SampleCmpLevelZero (shadowSamp, o, ref)
                                     : shadowTex1.SampleCmpLevelZero (shadowSamp, o, ref);
                            }
                        }
                        vis = lerp (1.0f, lit / 9.0f, shadowParm.z);
                    }
                }
            }
            diff += nl * at * vis;
            if (nl > 0.0f)
            {
                spec += pow (saturate (dot (n, normalize (L + v))), parm2.x) * at * vis;
            }
        }
        // Ambient by FACING: ceiling bounce above, desk bounce below.
        float3 amb = lerp (ambDown.rgb, ambUp.rgb, saturate (n.z * 0.5f + 0.5f));
        float  ramp = parm.y * (1.0f - exp (-diff * parm.z));
        lit = base.rgb * (amb + ramp) + spec * parm.w;
// The device's own lamp, with its own occlusion. Facing the lens was once
// taken as proof of seeing it -- "a face inside the notch points at the
// lens and lights" -- and that is wrong wherever something stands between
// the two. The notch floor ahead of the power button is exactly that case:
// it points squarely at the lamp and the button blocks every ray.

        if (dot (lampCol.rgb, lampCol.rgb) > 0.0f)
        {
            float3 d  = lampPos.xyz - input.wp;
            float  r  = length (d);
            if (r < lampDir.w)
            {
                float3 L    = d / max (r, 1e-4f);
                // -L is the direction the lamp SHINES to reach this
                // pixel, so it is measured against the lens facing
                // itself. Negating that facing instead aimed the cone
                // backwards into the case: everything in front of the
                // lens -- the notch walls, the button top, the whole
                // point of having the lamp -- fell on the zero side of
                // the saturate and took no light at all.
// WRAP IS THE CALLER'S, because how far past its own equator a lens throws
// is a fact about the PART. Zero is a true hemisphere -- a flat window
// flush in a panel, which cannot see its own plane and therefore does not
// light it. A domed LED is not flat, does see it, and asks for more.
//
// One hard-coded 0.65 for every lamp is what made a flush window light the
// frame it sits in, and a bezel thirty millimeters away facing somewhere
// else: emission thrown past the equator in a direction the part does not
// emit in.
                float  wrap = lampCol.w;
                float  emit = saturate ((dot (lampDir.xyz, -L) + wrap)
                                        / (1.0f + wrap));
                float  recv = saturate (dot (n, L));
                float  rr   = max (r, lampPos.w);
                float  fade = saturate (1.0f - r / lampDir.w);
                float  lvis = 1.0f;
                if (lampShadowParm.x > 0.0f && emit * recv > 0.0f)
                {
// A LAMP'S FRUSTUM CANNOT COVER A LAMP'S REACH, so off the map means the
// map has no opinion, and no opinion resolves to lit -- which is the same
// rule the room lights use a few lines up, arrived at for a different
// reason. Theirs covers the whole scene, so off it is off the scene. This
// one is a 120 degree pyramid pinned to the lens, and that is SEVEN
// MILLIMETERS WIDE at the two where the Monitor II's power notch stands
// its back wall.
//
// Reading off the map as full shadow instead drew that pyramid on the
// case: a dark wedge flanking the LED and opening forward over the power
// button, apex exactly at the lamp, which is the cone's own edge and not
// any occluder at all. It read as an LED sunk down a deep well.
//
// Nothing is given up. What the map is for is the button, which stands
// eleven millimeters proud of the lens and shadows a notch floor forty
// degrees off axis -- well inside the pyramid, mapped, and correctly
// dark. Only the lamp's own few millimeters fall outside it, and at that
// range there is nothing between lamp and surface to find.
                    float4 lc = mul (float4 (input.wp, 1.0f), lampShadow);
                    if (lc.w > 0.0f)
                    {
                        float3 p  = lc.xyz / lc.w;
                        float2 uv = float2 (p.x * 0.5f + 0.5f, 0.5f - p.y * 0.5f);
                        if (max (abs (p.x), abs (p.y)) <= 1.0f && p.z >= 0.0f && p.z <= 1.0f)
                        {
                            float acc = 0.0f;
                            float ref = p.z - lampShadowParm.y;
                            [unroll] for (int ly = -1; ly <= 1; ly++)
                            {
                                [unroll] for (int lx = -1; lx <= 1; lx++)
                                {
                                    float2 o = uv + float2 (lx, ly) * lampShadowParm.x;
                                    acc += lampShadowTex.SampleCmpLevelZero (shadowSamp, o, ref);
                                }
                            }
                            lvis = acc / 9.0f;
                        }
                    }
                }
// THE THROW ENDS INSIDE THE MAPPED CONE. Off the map every sample resolves the
// same way, so whichever way it resolves draws the frustum's own outline onto
// the model: as shadow it sank the lamp down a dark well, as lit it let the
// lamp shine THROUGH the pocket it sits in and stain the case beside it. Ending
// the light where the map ends leaves nothing out there to resolve, and costs
// nothing the map was for -- the button shadows its notch floor forty degrees
// off axis, well inside this.
                float  cone = 1.0f;
                if (lampShadowParm.z > -1.0f)
                {
                    cone = smoothstep (lampShadowParm.z, lampShadowParm.w,
                                       dot (lampDir.xyz, -L));
                }

                float3 spill = base.rgb * lampCol.rgb * emit * recv
                             * fade * lvis * cone
                             * (lampPos.w * lampPos.w) / (rr * rr);
                float3 head = saturate (1.0f - lit);
                lit += min (spill, head * lampCap.rgb * 0.5f);
            }
        }
    }
    return float4 (lit + input.emi + DitherOffset (input.pos.xy), base.a);
}
