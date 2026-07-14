#include "Pch.h"

#include "Printer3DScene.h"

#include "Devices/Printer/ObjMeshParser.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Scene constants -- world units; the printer body is ~1.76 wide. Modeled on
//  the real ImageWriter II: a tall lower front face (badge left, control
//  staircase right), a recessed step ledge, the raised hood that lifts off,
//  the smoked platen cover with the print head visible through it, a vented
//  rear deck, round platen end covers, and the platen knob on the right.
//  Everything here is a tuning knob for the look.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr float   s_kPi = 3.14159265f;

    // Case footprint. Width and overall depth are set to match the width:
    // height:depth ratio (3.740 : 1.0 : 2.696) measured off the user's
    // reference CAD model (Tinkercad export, Assets/Imagewriter II.zip); the
    // extra depth lands entirely in the rear deck (no explicit rule ever fixed
    // its depth, unlike the front-portion ratios below).
    constexpr float   s_kBodyHalfW  = 0.907f;
    constexpr float   s_kBodyZFront = 0.42f;
    constexpr float   s_kBodyZBack  = -0.888f;

    // The whole printer is inclined 10 degrees front-to-back (matches the
    // reference model), rotating about the bottom edge of the front face
    // (y=0 at the front plane): the rear lifts and the front face tips
    // toward the camera, like the real machine on its feet. Applied as a
    // model matrix in Render.
    constexpr float   s_kBodyTiltRad = 10.0f * s_kPi / 180.0f;

    // Side profile, front to back: face -> step ledge -> hood -> smoked cover.
    // Reference proportions: the exposed shelf is as deep as the front face is
    // tall, and the cover box is 5% deeper than that shelf.
    constexpr float   s_kFaceTopY      = 0.331f;   // lower front face 0..this
    constexpr float   s_kLedgeZ        = 0.089f;   // hood front edge; shelf depth == s_kFaceTopY
    constexpr float   s_kHoodFrontY    = 0.375f;   // hood riser top (at ledge z)
    constexpr float   s_kHoodBackY     = 0.415f;   // hood top rear edge...
    constexpr float   s_kHoodZBack     = -0.259f;  // ...at this z (cover depth = 1.05 * shelf)
    constexpr float   s_kCoverTopY     = 0.485f;   // smoked cover top edge...
    constexpr float   s_kCoverZTop     = -0.37f;   // ...at this z
    constexpr float   s_kDeckY         = 0.40f;    // vented rear deck height
    constexpr float   s_kDeckZFront    = -0.42f;

    // Platen bay (seen through the smoked cover), its rounded end towers
    // (vertical half-cylinders hugging the body ends), and the paper-advance
    // knob sticking out of the right tower.
    constexpr float   s_kBayHalfW   = 0.70f;

    // Platen end housings: rounded-top blocks running front-to-back at each
    // end of the platen line, merged into the case (NOT freestanding
    // cylinders). Extruded along x with a quarter-round crown; the paper-
    // advance knob emerges from the right one on the same axis. X-span and
    // ground contact match the reference model: the housing's inner/outer
    // edges sit at 0.882/1.118 of the body half-width (measured), and it
    // reaches all the way down to the ground -- NOT floating mid-height.
    constexpr float   s_kEndXIn     = 0.80f;    // housing spans In..Out (mirrored)
    constexpr float   s_kEndXOut    = 1.01f;    // proud of the body side both ways
    constexpr float   s_kEndCy      = 0.44f;    // crown axis (y, z)
    constexpr float   s_kEndCz      = -0.26f;
    constexpr float   s_kEndR       = 0.08f;    // crown radius (top = 0.52)
    constexpr float   s_kEndBaseY   = 0.0f;     // touches the ground, per reference
    constexpr float   s_kKnobX1     = 1.10f;
    constexpr float   s_kKnobR      = 0.050f;

    // Paper strip: leans back from vertical, then curls away over a roll.
    constexpr float   s_kPaperHalfW  = 0.62f;   // clears the end housings
    constexpr float   s_kPaperZ      = -0.40f;
    constexpr float   s_kPaperStartY = 0.46f;   // just below the cover's top edge
    constexpr float   s_kPaperTilt   = 12.0f * s_kPi / 180.0f;
    constexpr float   s_kStraightLen = 0.95f;   // arclength before the curl (fits the tilted frame)
    constexpr float   s_kCurlRadius  = 0.28f;
    constexpr int     s_kPaperSlices = 48;

    // Camera: in front and above, looking slightly down so the printer sits at
    // the bottom of the frame and the paper rises through the middle.
    constexpr float   s_kEye[3]   = { 0.0f, 1.35f, 3.30f };
    constexpr float   s_kAt[3]    = { 0.0f, 0.95f, 0.0f };
    constexpr float   s_kFovY     = 34.0f * s_kPi / 180.0f;

    // Palette (ARGB): warm ImageWriter platinum + accents.
    // The mat is deliberately lighter than the dark platen roller so the
    // roller's silhouette reads against it (they used to be near-identical).
    constexpr uint32_t   s_kArgbMat      = 0xFF4A505A;   // panel mat behind everything
    constexpr uint32_t   s_kArgbCase     = 0xFFD8D3C6;   // platinum case
    constexpr uint32_t   s_kArgbButton   = 0xFFE8E3D6;   // control caps (lighter cream)
    constexpr uint32_t   s_kArgbBay      = 0xFF14161A;   // platen bay interior
    constexpr uint32_t   s_kArgbSlot     = 0xFF17181B;   // paper slot recess
    constexpr uint32_t   s_kArgbRollerHi = 0xFF3F4348;   // platen roller, lit top
    constexpr uint32_t   s_kArgbRollerLo = 0xFF26282C;   // platen roller, shadowed
    constexpr uint32_t   s_kArgbHead     = 0xFF1E2126;   // print head / carriage
    constexpr uint32_t   s_kArgbCover    = 0x99101418;   // smoked cover (translucent)
    constexpr uint32_t   s_kArgbLedOn    = 0xFF2FBF5F;   // lit green LED
    constexpr uint32_t   s_kArgbLedErr   = 0xFF63262A;   // error LED, unlit dark red
    constexpr uint32_t   s_kArgbRibbon[4] = { 0xFF202020, 0xFFF0C810, 0xFFC83030, 0xFF2848A8 };

    // Control cluster (right of the front face): six small caps rising gently
    // toward the right -- paper load/eject, form feed, line feed, print
    // quality, select, on/off -- with tiny LEDs above the right three. Sized
    // from the reference photo: the whole cluster spans ~10% of the body
    // width, so the caps read as switches, not stair treads.
    constexpr int     s_kButtonCount  = 6;
    constexpr float   s_kButtonX0     = 0.62f;    // leftmost cap center
    constexpr float   s_kButtonY0     = 0.155f;
    constexpr float   s_kButtonDx     = 0.036f;
    constexpr float   s_kButtonDy     = 0.0075f;
    constexpr float   s_kButtonW      = 0.030f;
    constexpr float   s_kButtonH      = 0.011f;

    // The Casso cassowary badge (lower-left of the front face, where the real
    // machine wears its apple). Silhouette + rainbow mirror the DriveWidget
    // chrome badge (DrawCassowaryRainbow) -- that painter is the source of
    // truth for the motif; this is its 3D rendition.
    constexpr int       s_kLogoGridW = 36;
    constexpr int       s_kLogoGridH = 54;
    constexpr uint64_t  s_kLogoSilhouette[s_kLogoGridH] = {
        0x0000000000ULL, 0x0000000000ULL, 0x0000000000ULL, 0x0000000000ULL, 0x0000000000ULL,
        0x000000FE00ULL, 0x000001FF80ULL, 0x000003FFC0ULL, 0x000007FFE0ULL, 0x00000FFFC0ULL,
        0x00000FFFC0ULL, 0x00001FFF80ULL, 0x00001FFF00ULL, 0x00003FFF00ULL, 0x00003FFF00ULL,
        0x00007FFE00ULL, 0x00007FFE00ULL, 0x0000FFFE00ULL, 0x0000FFFC00ULL, 0x0000FFFC00ULL,
        0x0000FFFC00ULL, 0x0001FFFC00ULL, 0x0001FFFC00ULL, 0x0001FFFC00ULL, 0x0001FFFC00ULL,
        0x0003FFFE00ULL, 0x0003FFFE00ULL, 0x0003FFFF00ULL, 0x0003FFFF80ULL, 0x0003FFFFC0ULL,
        0x0003FFFFC0ULL, 0x0007FFFFE0ULL, 0x0007FFFFE0ULL, 0x0007FFFFE0ULL, 0x000FFFFFF0ULL,
        0x001FFFFFF0ULL, 0x003FFFFFF0ULL, 0x007FFFFFF0ULL, 0x00FFFFFFF0ULL, 0x01FFFFFFF8ULL,
        0x01FFFFFFF8ULL, 0x03F007FFF8ULL, 0x038000FFF8ULL, 0x0200007FF8ULL, 0x0000007FF8ULL,
        0x0000007FF8ULL, 0x0000007FF8ULL, 0x000000FFF8ULL, 0x000000FFF8ULL, 0x000001FFF8ULL,
        0x000001FFF8ULL, 0x000001FFF8ULL, 0x000001FFF0ULL, 0x000003FFF0ULL
    };
    constexpr uint32_t  s_kLogoStripes[6] = {
        0xFF61BB46, 0xFFFDB827, 0xFFF5821F, 0xFFE03A3E, 0xFF963D97, 0xFF009DDC
    };
    constexpr float     s_kLogoLeft   = -0.82f;
    constexpr float     s_kLogoWidth  = 0.075f;
    constexpr float     s_kLogoTopY   = 0.19f;


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Row-vector matrix helpers (clip = v * view * proj), matching the
    //  renderer's row_major cbuffer. Hand-rolled to keep the 3D path free of
    //  header dependencies -- three small functions is all a fixed scene needs.
    //
    ////////////////////////////////////////////////////////////////////////////

    void Mul44 (const float a[16], const float b[16], float out[16])
    {
        for (int r = 0; r < 4; r++)
        {
            for (int c = 0; c < 4; c++)
            {
                out[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c]
                               + a[r * 4 + 1] * b[1 * 4 + c]
                               + a[r * 4 + 2] * b[2 * 4 + c]
                               + a[r * 4 + 3] * b[3 * 4 + c];
            }
        }
    }


    void LookAtRH (const float eye[3], const float at[3], float out[16])
    {
        float   z[3] = { eye[0] - at[0], eye[1] - at[1], eye[2] - at[2] };
        float   zl   = std::sqrt (z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
        float   x[3] = {};
        float   xl   = 0.0f;
        float   y[3] = {};

        z[0] /= zl; z[1] /= zl; z[2] /= zl;

        // x = normalize(cross(up, z)) with up = (0,1,0)
        x[0] = z[2]; x[1] = 0.0f; x[2] = -z[0];
        xl   = std::sqrt (x[0] * x[0] + x[2] * x[2]);
        x[0] /= xl; x[2] /= xl;

        // y = cross(z, x)
        y[0] = z[1] * x[2] - z[2] * x[1];
        y[1] = z[2] * x[0] - z[0] * x[2];
        y[2] = z[0] * x[1] - z[1] * x[0];

        out[0]  = x[0]; out[1]  = y[0]; out[2]  = z[0]; out[3]  = 0.0f;
        out[4]  = x[1]; out[5]  = y[1]; out[6]  = z[1]; out[7]  = 0.0f;
        out[8]  = x[2]; out[9]  = y[2]; out[10] = z[2]; out[11] = 0.0f;
        out[12] = -(x[0] * eye[0] + x[1] * eye[1] + x[2] * eye[2]);
        out[13] = -(y[0] * eye[0] + y[1] * eye[1] + y[2] * eye[2]);
        out[14] = -(z[0] * eye[0] + z[1] * eye[1] + z[2] * eye[2]);
        out[15] = 1.0f;
    }


    void PerspectiveFovRH (float fovY, float aspect, float zn, float zf, float out[16])
    {
        float   ys = 1.0f / std::tan (fovY * 0.5f);
        float   xs = ys / aspect;

        memset (out, 0, 16 * sizeof (float));
        out[0]  = xs;
        out[5]  = ys;
        out[10] = zf / (zn - zf);
        out[11] = -1.0f;
        out[14] = zn * zf / (zn - zf);
    }


    void IdentityMvp (float out[16])
    {
        memset (out, 0, 16 * sizeof (float));
        out[0] = out[5] = out[10] = out[15] = 1.0f;
    }


    // Model matrix: rotate the whole printer about the x-axis line through the
    // bottom edge of the front face (y = 0, z = pivotZ), tipping tops toward
    // the camera so the rear of the machine lifts by `tiltRad`.
    void TiltAboutFrontBottom (float tiltRad, float pivotZ, float out[16])
    {
        float   c = std::cos (tiltRad);
        float   s = std::sin (tiltRad);

        memset (out, 0, 16 * sizeof (float));
        out[0]  = 1.0f;
        out[5]  = c;
        out[6]  = s;
        out[9]  = -s;
        out[10] = c;
        out[13] = pivotZ * s;
        out[14] = pivotZ * (1.0f - c);
        out[15] = 1.0f;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Printer3DScene::Initialize (ID3D11Device * device, ID3D11DeviceContext * context)
{
    // Default anchors serve the procedural fallback; SetModel re-measures
    // them off the loaded geometry.
    m_meshFrontZ  = s_kBodyZFront;
    m_paperStartY = s_kPaperStartY;
    m_paperZ      = s_kPaperZ;
    m_logoLeft    = s_kLogoLeft;
    m_logoTopY    = s_kLogoTopY;

    return m_renderer.Initialize (device, context);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::SetModel
//
//  Turns the user's CAD export into the scene's body mesh:
//
//   1. Colors remap from Tinkercad's part-identification palette to the real
//      machine's (platinum case, dark roller, cream caps, signal LEDs). The
//      error LED shares Tinkercad's red with the platen housings, so it is
//      picked out by position before the coordinate transform.
//   2. Axes remap from Tinkercad's Z-up (X width, Y depth, Z height) to the
//      scene's Y-up, with +z toward the camera (the model's front is -Y),
//      scaled so the overall width matches the procedural body's footprint.
//   3. The model bakes the same front-to-back incline the scene applies at
//      render time (s_kBodyTiltRad), so it is un-tilted here and re-grounded
//      (feet on y=0, front face at s_kBodyZFront); the runtime model matrix
//      then tilts mesh and procedural overlays together, keeping the paper,
//      head, and badge in one consistent upright frame.
//   4. Flat Lambert lighting from the scene's frontal light is baked per
//      face, and the platen anchors (roller axis, radius, front plane) are
//      measured from the roller's geometry so the paper and paced head land
//      on the printer exactly where the user put its platen.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Printer3DScene::SetModel (const std::string & objText, const std::string & mtlText)
{
    HRESULT                    hr      = S_OK;
    std::vector<ObjTriangle>   tris;
    float                      maxAbsX = 0.0f;
    float                      preMinY = 0.0f, preMaxY = 0.0f, preMaxZ = 0.0f;
    bool                       first   = true;

    // Tinkercad's five part-identification colors (MTL Kd values) -> the real
    // machine's palette. Matched by value with a wide epsilon.
    struct ColorMap { float r, g, b; uint32_t argb; };
    static constexpr ColorMap   s_kPalette[] =
    {
        { 0.8667f, 0.8863f, 0.8941f, s_kArgbCase     },   // cream: body/lid/case
        { 0.9137f, 0.1137f, 0.1765f, s_kArgbCase     },   // red: platen end housings
        { 0.9608f, 0.5137f, 0.1216f, 0xFF3A3D42      },   // orange: platen roller
        { 0.2745f, 0.7176f, 0.2863f, s_kArgbLedOn    },   // green: lit LEDs
        { 0.6549f, 0.6784f, 0.6941f, s_kArgbButton   },   // gray: control caps
    };

    CBREx (ObjMeshParser::Parse (objText, mtlText, tris) && !tris.empty (), E_FAIL);

    m_mesh.clear ();
    m_meshGlass.clear ();
    m_mesh.reserve (tris.size () * 3);

    // Pass 1 (original model coordinates): overall extents for the scale.
    for (const ObjTriangle & t : tris)
    {
        const float *   pts[3] = { t.p0, t.p1, t.p2 };

        for (const float * p : pts)
        {
            maxAbsX = (std::max) (maxAbsX, std::abs (p[0]));
        }
    }
    CBREx (maxAbsX > 0.0f, E_FAIL);

    {
        float   scale   = 0.95f / maxAbsX;               // overall width, with side margins in frame
        float   cUntilt = std::cos (s_kBodyTiltRad);
        float   sUntilt = -std::sin (s_kBodyTiltRad);    // reverse of the runtime tilt
        float   lightL  = std::sqrt (0.18f * 0.18f + 0.88f * 0.88f + 0.44f * 0.44f);
        float   lx      = 0.18f / lightL;                // top-weighted light: top faces bright,
        float   ly      = 0.88f / lightL;                // fronts mid, sides dim -- the profile
        float   lz      = 0.44f / lightL;                // reads instead of washing into a slab

        float   rollerMinY = 0.0f, rollerMaxY = 0.0f, rollerMinZ = 0.0f, rollerMaxZ = 0.0f;
        bool    rollerSeen = false;

        struct XYZ { float x, y, z; };
        std::vector<XYZ>        pos;
        std::vector<uint32_t>   argbs;

        pos.reserve (tris.size () * 3);
        argbs.reserve (tris.size ());

        for (const ObjTriangle & t : tris)
        {
            // Color remap, with the error LED (red like the housings, but a
            // tiny cap on the sloped control deck) picked out by its position
            // in ORIGINAL model coordinates. BLACK parts are the smoked
            // window: they become translucent (alpha < 1) and are routed to
            // the glass pass below so the platen reads through them.
            uint32_t   argb    = 0xFFFFFFFF;
            bool       matched = false;
            bool       roller  = false;

            // Tinkercad's darkest swatch exports as Kd ~0.17-0.19, so the
            // "black means glass" test reaches to 0.25 -- the next-darkest
            // real material (button gray) sits far above at ~0.65.
            if (t.r < 0.25f && t.g < 0.25f && t.b < 0.25f)
            {
                argb    = 0x8C14181D;   // smoked translucent
                matched = true;
            }

            for (const ColorMap & m : s_kPalette)
            {
                if (matched)
                {
                    break;
                }
                if (std::abs (t.r - m.r) < 0.02f && std::abs (t.g - m.g) < 0.02f &&
                    std::abs (t.b - m.b) < 0.02f)
                {
                    argb    = m.argb;
                    matched = true;
                    roller  = (m.argb == 0xFF3A3D42);
                    break;
                }
            }

            if (!matched)
            {
                // Unmapped material: keep the author's color.
                uint32_t   r8 = (uint32_t) std::clamp (t.r * 255.0f, 0.0f, 255.0f);
                uint32_t   g8 = (uint32_t) std::clamp (t.g * 255.0f, 0.0f, 255.0f);
                uint32_t   b8 = (uint32_t) std::clamp (t.b * 255.0f, 0.0f, 255.0f);

                argb = 0xFF000000u | (r8 << 16) | (g8 << 8) | b8;
            }
            else if (argb == s_kArgbCase && std::abs (t.r - 0.9137f) < 0.02f)
            {
                // Red-material triangle: error LED if it lives in the small
                // control-deck cap region (model coords), housing otherwise.
                bool   isLed = true;

                const float *   pts[3] = { t.p0, t.p1, t.p2 };
                for (const float * p : pts)
                {
                    if (p[0] < 150.0f || p[0] > 176.0f ||
                        p[1] < -104.0f || p[1] > -94.0f ||
                        p[2] < 71.0f || p[2] > 77.0f)
                    {
                        isLed = false;
                        break;
                    }
                }

                if (isLed)
                {
                    argb = s_kArgbLedErr;
                }
            }

            // Axis remap + scale + un-tilt, tracking the post-transform bbox.
            const float *   pts[3] = { t.p0, t.p1, t.p2 };
            for (const float * p : pts)
            {
                float   x  = p[0] * scale;
                float   y  = p[2] * scale;            // model Z (height) -> scene y
                float   z  = -p[1] * scale;           // model -Y (front) -> scene +z
                float   zp = 0.6703f;                 // approx front edge; re-grounding absorbs error
                float   dz = z - zp;
                float   ry = y * cUntilt - dz * sUntilt;
                float   rz = zp + y * sUntilt + dz * cUntilt;

                // Only OPAQUE geometry defines the grounding extents: the
                // printer stands on its feet and fronts on its case even if
                // a smoked-glass shape strays past them mid-edit.
                if ((argb >> 24) == 0xFFu)
                {
                    if (first)
                    {
                        preMinY = preMaxY = ry;
                        preMaxZ = rz;
                        first   = false;
                    }
                    preMinY = (std::min) (preMinY, ry);
                    preMaxY = (std::max) (preMaxY, ry);
                    preMaxZ = (std::max) (preMaxZ, rz);
                }

                pos.push_back ({ x, ry, rz });
            }

            argbs.push_back (argb);

            if (roller)
            {
                for (size_t k = pos.size () - 3; k < pos.size (); k++)
                {
                    if (!rollerSeen)
                    {
                        rollerMinY = rollerMaxY = pos[k].y;
                        rollerMinZ = rollerMaxZ = pos[k].z;
                        rollerSeen = true;
                    }
                    rollerMinY = (std::min) (rollerMinY, pos[k].y);
                    rollerMaxY = (std::max) (rollerMaxY, pos[k].y);
                    rollerMinZ = (std::min) (rollerMinZ, pos[k].z);
                    rollerMaxZ = (std::max) (rollerMaxZ, pos[k].z);
                }
            }
        }

        // Re-ground: feet on y=0, front face at the procedural body's plane so
        // the camera framing carries over unchanged.
        float   dy = -preMinY;
        float   dz = s_kBodyZFront - preMaxZ;

        for (XYZ & p : pos)
        {
            p.y += dy;
            p.z += dz;
        }

        // Bake per-face Lambert shading (two-sided: CAD winding is arbitrary).
        for (size_t t = 0; t < argbs.size (); t++)
        {
            const XYZ &   a = pos[t * 3 + 0];
            const XYZ &   b = pos[t * 3 + 1];
            const XYZ &   c = pos[t * 3 + 2];

            float   ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
            float   vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
            float   nx = uy * vz - uz * vy;
            float   ny = uz * vx - ux * vz;
            float   nz2 = ux * vy - uy * vx;
            float   nl = std::sqrt (nx * nx + ny * ny + nz2 * nz2);
            float   shade = 0.72f;

            if (nl > 1e-8f)
            {
                float   d = (nx * lx + ny * ly + nz2 * lz) / nl;

                shade = std::clamp (0.30f + 0.70f * std::abs (d), 0.0f, 1.0f);
            }

            uint32_t   argb = argbs[t];
            float      ca   = (float) ((argb >> 24) & 0xFF) / 255.0f;
            float      cr   = (float) ((argb >> 16) & 0xFF) / 255.0f * shade * ca;
            float      cg   = (float) ((argb >>  8) & 0xFF) / 255.0f * shade * ca;
            float      cb   = (float) ((argb      ) & 0xFF) / 255.0f * shade * ca;

            // Translucent parts (the smoked window) go to the glass pass,
            // drawn after everything they must show through.
            std::vector<Vertex> &   dest = (ca < 1.0f) ? m_meshGlass : m_mesh;

            for (int k = 0; k < 3; k++)
            {
                const XYZ &   p = pos[t * 3 + k];

                dest.push_back ({ p.x, p.y, p.z, 0.0f, 0.0f, cr, cg, cb, ca });
            }
        }

        // Platen anchors from the roller's measured geometry: the paper rises
        // through the roller's back half and the head rides its front.
        if (rollerSeen)
        {
            m_platenY = (rollerMinY + rollerMaxY) * 0.5f + dy;
            m_platenZ = (rollerMinZ + rollerMaxZ) * 0.5f + dz;
            m_platenR = (rollerMaxY - rollerMinY) * 0.5f;

            m_paperZ      = m_platenZ - m_platenR * 0.45f;
            m_paperStartY = m_platenY - 0.05f;
        }

        m_meshFrontZ = s_kBodyZFront;

        // Measure the actual front FACE (the model has a base below it, so
        // "y = 0 upward" is wrong for badge placement): the verts on the
        // front plane give its true extent, and the badge sits lower-left
        // within it -- where the real machine wears its apple.
        {
            float   faceMinX = 0.0f, faceMinY = 0.0f, faceMaxY = 0.0f;
            bool    faceSeen = false;

            for (const XYZ & p : pos)
            {
                if (p.z > m_meshFrontZ - 0.02f)
                {
                    if (!faceSeen)
                    {
                        faceMinX = p.x;
                        faceMinY = faceMaxY = p.y;
                        faceSeen = true;
                    }
                    faceMinX = (std::min) (faceMinX, p.x);
                    faceMinY = (std::min) (faceMinY, p.y);
                    faceMaxY = (std::max) (faceMaxY, p.y);
                }
            }

            if (faceSeen)
            {
                float   faceH = faceMaxY - faceMinY;

                m_logoLeft = faceMinX + 0.06f;
                m_logoTopY = faceMinY + faceH * 0.62f;
            }
        }
    }

Error:
    if (FAILED (hr))
    {
        m_mesh.clear ();   // never leave a half-loaded body: fall back whole
        m_meshGlass.clear ();
    }
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::SetContent
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Printer3DScene::SetContent (const uint32_t * bgra, int width, int height)
{
    HRESULT   hr = m_renderer.UpdateContentTexture (bgra, width, height);

    if (SUCCEEDED (hr))
    {
        m_contentWidth  = width;
        m_contentHeight = height;
    }

    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::SetHeadColumn01
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::SetHeadColumn01 (float x01)
{
    m_head01 = std::clamp (x01, 0.0f, 1.0f);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::AppendQuad
//
//  p00..p11 are the quad corners at (u0,v0) (u1,v0) (u0,v1) (u1,v1); `shade`
//  bakes the lighting into the tint, and colors are emitted premultiplied so
//  translucent pieces (the smoked cover) composite correctly.
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::AppendQuad (std::vector<Vertex> & out,
                                 const float p00[3], const float p10[3],
                                 const float p01[3], const float p11[3],
                                 float u0, float v0, float u1, float v1,
                                 uint32_t argb, float shade)
{
    float   a = (float) ((argb >> 24) & 0xFF) / 255.0f;
    float   r = (float) ((argb >> 16) & 0xFF) / 255.0f * shade * a;
    float   g = (float) ((argb >>  8) & 0xFF) / 255.0f * shade * a;
    float   b = (float) ((argb      ) & 0xFF) / 255.0f * shade * a;

    Vertex   v00 = { p00[0], p00[1], p00[2], u0, v0, r, g, b, a };
    Vertex   v10 = { p10[0], p10[1], p10[2], u1, v0, r, g, b, a };
    Vertex   v01 = { p01[0], p01[1], p01[2], u0, v1, r, g, b, a };
    Vertex   v11 = { p11[0], p11[1], p11[2], u1, v1, r, g, b, a };

    out.push_back (v00);
    out.push_back (v10);
    out.push_back (v01);
    out.push_back (v01);
    out.push_back (v10);
    out.push_back (v11);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::AppendFaceQuad
//
//  Vertical quad on a constant-z plane (the front face and everything on it).
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::AppendFaceQuad (std::vector<Vertex> & out,
                                     float x0, float x1, float y0, float y1, float z,
                                     uint32_t argb, float shade)
{
    float   p00[3] = { x0, y1, z };
    float   p10[3] = { x1, y1, z };
    float   p01[3] = { x0, y0, z };
    float   p11[3] = { x1, y0, z };

    AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, argb, shade);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::AppendLatheX
//
//  Fake cylinder around an x-parallel axis: sweeps `segments` quads through
//  angles [a0, a1] in the y/z plane (angle 0 faces the viewer, pi/2 up), with
//  per-segment Lambert shading from a front-top light. Approximates the round
//  platen end covers and the knob well enough at this scene scale.
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::AppendLatheX (std::vector<Vertex> & out,
                                   float x0, float x1, float cy, float cz, float radius,
                                   float a0, float a1, int segments, uint32_t argb)
{
    float   prevY = cy + radius * std::sin (a0);
    float   prevZ = cz + radius * std::cos (a0);

    for (int i = 1; i <= segments; i++)
    {
        float   a     = a0 + (a1 - a0) * (float) i / (float) segments;
        float   yPos  = cy + radius * std::sin (a);
        float   zPos  = cz + radius * std::cos (a);
        float   mid   = a0 + (a1 - a0) * ((float) i - 0.5f) / (float) segments;
        float   shade = (std::max) (0.42f, 0.62f * std::sin (mid) + 0.78f * std::cos (mid));

        float   p00[3] = { x0, prevY, prevZ };
        float   p10[3] = { x1, prevY, prevZ };
        float   p01[3] = { x0, yPos, zPos };
        float   p11[3] = { x1, yPos, zPos };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, argb, shade);

        prevY = yPos;
        prevZ = zPos;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::AppendDiscX
//
//  Fan in the y/z plane at constant x (a cap facing left or right): the flat
//  inner side of a platen end housing. Angle 0 faces the viewer (+z), +pi/2
//  is up.
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::AppendDiscX (std::vector<Vertex> & out,
                                  float x, float cy, float cz, float radius,
                                  float a0, float a1, int segments,
                                  uint32_t argb, float shade)
{
    float   a = (float) ((argb >> 24) & 0xFF) / 255.0f;
    float   r = (float) ((argb >> 16) & 0xFF) / 255.0f * shade * a;
    float   g = (float) ((argb >>  8) & 0xFF) / 255.0f * shade * a;
    float   b = (float) ((argb      ) & 0xFF) / 255.0f * shade * a;

    float   prevY = cy + radius * std::sin (a0);
    float   prevZ = cz + radius * std::cos (a0);

    for (int i = 1; i <= segments; i++)
    {
        float   ang  = a0 + (a1 - a0) * (float) i / (float) segments;
        float   yPos = cy + radius * std::sin (ang);
        float   zPos = cz + radius * std::cos (ang);

        Vertex   vc = { x, cy,    cz,    0, 0, r, g, b, a };
        Vertex   v0 = { x, prevY, prevZ, 0, 0, r, g, b, a };
        Vertex   v1 = { x, yPos,  zPos,  0, 0, r, g, b, a };

        out.push_back (vc);
        out.push_back (v0);
        out.push_back (v1);

        prevY = yPos;
        prevZ = zPos;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::BuildBackdrop
//
//  Full-viewport mat quad in NDC (drawn with an identity matrix): the scene
//  paints its own background because the panel deliberately does not fill
//  this rect (a painter fill would flush AFTER the hook and cover the scene).
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::BuildBackdrop (std::vector<Vertex> & out) const
{
    float   tl[3] = { -1.0f,  1.0f, 0.0f };
    float   tr[3] = {  1.0f,  1.0f, 0.0f };
    float   bl[3] = { -1.0f, -1.0f, 0.0f };
    float   br[3] = {  1.0f, -1.0f, 0.0f };

    AppendQuad (out, tl, tr, bl, br, 0, 0, 1, 1, s_kArgbMat, 1.0f);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::BuildBodyBack
//
//  Everything BEHIND the paper: the vented rear deck (grooves running to the
//  back of the machine), the slot strip the paper rises through, and the
//  platen bay's back wall. Drawn before the paper so the strip visibly
//  emerges out of the machine.
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::BuildBodyBack (std::vector<Vertex> & out) const
{
    // Rear deck base.
    {
        float   p00[3] = { -s_kBodyHalfW, s_kDeckY, s_kBodyZBack };
        float   p10[3] = {  s_kBodyHalfW, s_kDeckY, s_kBodyZBack };
        float   p01[3] = { -s_kBodyHalfW, s_kDeckY, s_kDeckZFront };
        float   p11[3] = {  s_kBodyHalfW, s_kDeckY, s_kDeckZFront };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.94f);
    }

    // Vent grooves: true recessed slats running front-to-back across the
    // deck -- a shallow trench (floor + two side walls) rather than a flat
    // shading trick, so the recess actually reads as depth under the raking
    // camera angle instead of just a darker stripe.
    {
        constexpr float   kVentRecess = 0.012f;
        constexpr float   kVentW      = 0.010f;
        float              zB         = s_kBodyZBack + 0.02f;
        float              zF         = s_kDeckZFront - 0.015f;
        float              floorY     = s_kDeckY - kVentRecess;

        for (float gx = -0.84f; gx <= 0.84f; gx += 0.06f)
        {
            float   rx = gx + kVentW;

            // Floor of the trench.
            {
                float   p00[3] = { gx, floorY, zB };
                float   p10[3] = { rx, floorY, zB };
                float   p01[3] = { gx, floorY, zF };
                float   p11[3] = { rx, floorY, zF };

                AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.42f);
            }

            // Left wall (facing +x).
            {
                float   p00[3] = { gx, s_kDeckY, zB };
                float   p10[3] = { gx, s_kDeckY, zF };
                float   p01[3] = { gx, floorY,   zB };
                float   p11[3] = { gx, floorY,   zF };

                AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.58f);
            }

            // Right wall (facing -x).
            {
                float   p00[3] = { rx, s_kDeckY, zF };
                float   p10[3] = { rx, s_kDeckY, zB };
                float   p01[3] = { rx, floorY,   zF };
                float   p11[3] = { rx, floorY,   zB };

                AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.58f);
            }
        }
    }

    // Slot strip between the deck and the platen bay.
    {
        float   p00[3] = { -s_kBayHalfW, s_kDeckY, s_kDeckZFront };
        float   p10[3] = {  s_kBayHalfW, s_kDeckY, s_kDeckZFront };
        float   p01[3] = { -s_kBayHalfW, s_kDeckY, s_kCoverZTop - 0.015f };
        float   p11[3] = {  s_kBayHalfW, s_kDeckY, s_kCoverZTop - 0.015f };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbSlot, 1.0f);
    }

    // Platen bay back wall (seen through the smoked cover, behind the roller).
    {
        float   p00[3] = { -s_kBayHalfW, 0.50f, s_kCoverZTop - 0.015f };
        float   p10[3] = {  s_kBayHalfW, 0.50f, s_kCoverZTop - 0.015f };
        float   p01[3] = { -s_kBayHalfW, 0.34f, s_kCoverZTop - 0.015f };
        float   p11[3] = {  s_kBayHalfW, 0.34f, s_kCoverZTop - 0.015f };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbBay, 1.0f);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::BuildPaper
//
//  The fanfold strip: rises from the slot with a slight backward lean, then
//  curls away from the viewer over a roll and heads down behind the printer.
//  The content canvas maps 1:1 by arclength (square texels), canvas bottom --
//  the live row -- at the platen. Slices darken as the surface turns away
//  from the frontal light.
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::BuildPaper (std::vector<Vertex> & out) const
{
    float   aspect   = (m_contentWidth > 0) ? (float) m_contentHeight / (float) m_contentWidth
                                            : 1584.0f / 1368.0f;
    float   total    = 2.0f * s_kPaperHalfW * aspect;   // arclength covering the whole canvas
    float   dy       = std::cos (s_kPaperTilt);
    float   dz       = -std::sin (s_kPaperTilt);
    float   ny       = dz;                              // curl normal: down-and-back
    float   nz       = -dy;
    float   p1y      = m_paperStartY + dy * s_kStraightLen;   // where the curl begins
    float   p1z      = m_paperZ + dz * s_kStraightLen;
    float   cy       = p1y + s_kCurlRadius * ny;        // curl center
    float   cz       = p1z + s_kCurlRadius * nz;

    // The live end of the paper wraps the platen the way the real machine
    // feeds it: the newest row sits at the roller's FRONT (under the head,
    // read through the smoked window), arcs over the top, and disappears
    // behind the roller where the sheet re-emerges as the rising fanfold.
    // The short run hidden behind the roller is simply not built -- it is
    // occluded on the real machine too.
    float   wrapR    = m_platenR + 0.002f;              // just off the rubber, INSIDE the smoked barrel
    float   wrapEnd  = 1.833f;                          // ~105 deg: past the top, into occlusion
    float   arcLen   = wrapR * wrapEnd;

    float   prevY    = m_platenY;                       // theta = 0: the front strike line
    float   prevZ    = m_platenZ + wrapR;
    float   prevV    = 1.0f;
    float   prevSh   = 0.72f;

    for (int i = 1; i <= s_kPaperSlices; i++)
    {
        float   s     = total * (float) i / (float) s_kPaperSlices;
        float   yPos  = 0.0f;
        float   zPos  = 0.0f;
        float   shade = 1.0f;

        if (s <= arcLen)
        {
            float   theta = s / wrapR;

            yPos  = m_platenY + wrapR * std::sin (theta);
            zPos  = m_platenZ + wrapR * std::cos (theta);
            shade = 0.72f + 0.28f * (std::max) (0.0f, std::sin (theta));
        }
        else if (s - arcLen <= s_kStraightLen)
        {
            float   s2 = s - arcLen;

            yPos = m_paperStartY + dy * s2;
            zPos = m_paperZ + dz * s2;
        }
        else
        {
            float   phi = (s - arcLen - s_kStraightLen) / s_kCurlRadius;

            yPos  = cy + s_kCurlRadius * (-ny * std::cos (phi) + dy * std::sin (phi));
            zPos  = cz + s_kCurlRadius * (-nz * std::cos (phi) + dz * std::sin (phi));
            shade = (std::max) (0.38f, std::cos (phi * 0.75f));
        }

        {
            float   v   = 1.0f - s / total;
            float   p00[3] = { -s_kPaperHalfW, prevY, prevZ };
            float   p10[3] = {  s_kPaperHalfW, prevY, prevZ };
            float   p01[3] = { -s_kPaperHalfW, yPos, zPos };
            float   p11[3] = {  s_kPaperHalfW, yPos, zPos };

            // Per-slice shade via two AppendQuads would double vertices; write
            // the quad directly so top/bottom edges carry their own shades.
            size_t   base = out.size ();

            AppendQuad (out, p00, p10, p01, p11, 0.0f, prevV, 1.0f, v, 0xFFFFFFFF, 1.0f);

            for (size_t k = 0; k < 6; k++)
            {
                bool    isTopEdge = (out[base + k].v < prevV);   // smaller v == farther along
                float   sh        = isTopEdge ? shade : prevSh;

                out[base + k].r *= sh;
                out[base + k].g *= sh;
                out[base + k].b *= sh;
            }
        }

        prevY  = yPos;
        prevZ  = zPos;
        prevV  = 1.0f - s / total;
        prevSh = shade;

        // The curl's tail fell behind the printer: the rest is out of sight.
        // (Only the curl -- the platen wrap and straight section legitimately
        // pass below the cover's top edge, inside the machine.)
        if (s - arcLen > s_kStraightLen && yPos < s_kDeckY + 0.05f)
        {
            break;
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::BuildBodyFront
//
//  Everything in front of the paper, in painter's order: the platen roller
//  and the paced print head inside the bay, the smoked cover over them, then
//  the hood, step ledge, front face, badge, controls, platen end drums, and
//  the knob.
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::BuildBodyFront (std::vector<Vertex> & out) const
{
    // Platen roller (dark drum behind the head), lit along its top.
    {
        float   zR = s_kEndCz + 0.02f;

        float   t00[3] = { -s_kBayHalfW, 0.475f, zR };
        float   t10[3] = {  s_kBayHalfW, 0.475f, zR };
        float   t01[3] = { -s_kBayHalfW, 0.435f, zR };
        float   t11[3] = {  s_kBayHalfW, 0.435f, zR };

        AppendQuad (out, t00, t10, t01, t11, 0, 0, 1, 1, s_kArgbRollerHi, 1.0f);

        float   b00[3] = { -s_kBayHalfW, 0.435f, zR };
        float   b10[3] = {  s_kBayHalfW, 0.435f, zR };
        float   b01[3] = { -s_kBayHalfW, 0.385f, zR };
        float   b11[3] = {  s_kBayHalfW, 0.385f, zR };

        AppendQuad (out, b00, b10, b01, b11, 0, 0, 1, 1, s_kArgbRollerLo, 1.0f);
    }

    // Print head riding the platen at the paced column (FR-034), its ribbon
    // cartridge showing the four ink stripes -- all seen through the smoke.
    {
        float   travel = s_kPaperHalfW - 0.10f;
        float   cx     = -travel + m_head01 * 2.0f * travel;
        float   zH     = s_kEndCz + 0.06f;

        float   p00[3] = { cx - 0.075f, 0.455f, zH };
        float   p10[3] = { cx + 0.075f, 0.455f, zH };
        float   p01[3] = { cx - 0.075f, 0.355f, zH };
        float   p11[3] = { cx + 0.075f, 0.355f, zH };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbHead, 1.0f);

        for (int i = 0; i < 4; i++)
        {
            float   rx     = cx - 0.058f + (float) i * 0.030f;
            float   r00[3] = { rx,          0.435f, zH + 0.005f };
            float   r10[3] = { rx + 0.024f, 0.435f, zH + 0.005f };
            float   r01[3] = { rx,          0.410f, zH + 0.005f };
            float   r11[3] = { rx + 0.024f, 0.410f, zH + 0.005f };

            AppendQuad (out, r00, r10, r01, r11, 0, 0, 1, 1, s_kArgbRibbon[i], 1.0f);
        }
    }

    // Smoked cover: angled translucent pane from the hood's rear edge up and
    // back over the bay -- the head and roller read through it, darkened.
    {
        float   p00[3] = { -s_kBayHalfW, s_kCoverTopY, s_kCoverZTop };
        float   p10[3] = {  s_kBayHalfW, s_kCoverTopY, s_kCoverZTop };
        float   p01[3] = { -s_kBayHalfW, s_kHoodBackY, s_kHoodZBack };
        float   p11[3] = {  s_kBayHalfW, s_kHoodBackY, s_kHoodZBack };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCover, 1.0f);
    }

    // Hood top (the raised cover that lifts off), sloping up toward the bay.
    {
        float   p00[3] = { -s_kBodyHalfW, s_kHoodBackY, s_kHoodZBack };
        float   p10[3] = {  s_kBodyHalfW, s_kHoodBackY, s_kHoodZBack };
        float   p01[3] = { -s_kBodyHalfW, s_kHoodFrontY, s_kLedgeZ };
        float   p11[3] = {  s_kBodyHalfW, s_kHoodFrontY, s_kLedgeZ };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 1.0f);
    }

    // Hood riser (its front edge) down to the step ledge.
    {
        float   p00[3] = { -s_kBodyHalfW, s_kHoodFrontY, s_kLedgeZ };
        float   p10[3] = {  s_kBodyHalfW, s_kHoodFrontY, s_kLedgeZ };
        float   p01[3] = { -s_kBodyHalfW, s_kFaceTopY, s_kLedgeZ };
        float   p11[3] = {  s_kBodyHalfW, s_kFaceTopY, s_kLedgeZ };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.82f);
    }

    // Step ledge (horizontal shelf between the hood and the front face).
    {
        float   p00[3] = { -s_kBodyHalfW, s_kFaceTopY, s_kLedgeZ };
        float   p10[3] = {  s_kBodyHalfW, s_kFaceTopY, s_kLedgeZ };
        float   p01[3] = { -s_kBodyHalfW, s_kFaceTopY, s_kBodyZFront };
        float   p11[3] = {  s_kBodyHalfW, s_kFaceTopY, s_kBodyZFront };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.97f);
    }

    // Front face.
    AppendFaceQuad (out, -s_kBodyHalfW, s_kBodyHalfW, 0.0f, s_kFaceTopY, s_kBodyZFront,
                    s_kArgbCase, 0.90f);

    // Wraparound reveal groove: a shallow recessed accent line partway up the
    // front face (floor + two edge walls, drawn over the flat face so it
    // reads as a real inset). Runs the full width here; the badge and control
    // caps paint over it afterward, same as a real reveal line disappearing
    // behind surface-mounted trim.
    {
        constexpr float   kGrooveYRatio = 0.30f;   // fraction up the front-face height
        constexpr float   kGrooveBandH  = 0.012f;
        constexpr float   kGrooveRecess = 0.010f;

        float   gy0    = s_kFaceTopY * kGrooveYRatio;
        float   gy1    = gy0 + kGrooveBandH;
        float   zOuter = s_kBodyZFront;
        float   zInner = s_kBodyZFront - kGrooveRecess;

        AppendFaceQuad (out, -s_kBodyHalfW, s_kBodyHalfW, gy0, gy1, zInner, s_kArgbCase, 0.55f);

        // Top wall (ceiling of the groove, facing down).
        {
            float   p00[3] = { -s_kBodyHalfW, gy1, zOuter };
            float   p10[3] = {  s_kBodyHalfW, gy1, zOuter };
            float   p01[3] = { -s_kBodyHalfW, gy1, zInner };
            float   p11[3] = {  s_kBodyHalfW, gy1, zInner };

            AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.70f);
        }

        // Bottom wall (floor of the approach into the groove, facing up).
        {
            float   p00[3] = { -s_kBodyHalfW, gy0, zInner };
            float   p10[3] = {  s_kBodyHalfW, gy0, zInner };
            float   p01[3] = { -s_kBodyHalfW, gy0, zOuter };
            float   p11[3] = {  s_kBodyHalfW, gy0, zOuter };

            AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.88f);
        }
    }

    BuildCassoLogo (out, s_kBodyZFront);
    BuildControls  (out);

    // Platen end housings: rounded-top blocks running front-to-back at each
    // end, merged into the case. Per side: a short front face rising out of
    // the hood, a quarter-round crown arcing from that face over the top
    // toward the back (an x-axis lathe -- the curve is in the side profile,
    // like the real end covers), and a flat inner side cap facing center.
    for (int side = 0; side < 2; side++)
    {
        float   xIn   = (side == 0) ? -s_kEndXOut : s_kEndXIn;
        float   xOut  = (side == 0) ? -s_kEndXIn : s_kEndXOut;
        float   xCap  = (side == 0) ? -s_kEndXIn : s_kEndXIn;   // inner side faces center
        float   zFace = s_kEndCz + s_kEndR;

        // Front face up to where the crown begins.
        {
            float   p00[3] = { xIn, s_kEndCy, zFace };
            float   p10[3] = { xOut, s_kEndCy, zFace };
            float   p01[3] = { xIn, s_kEndBaseY, zFace };
            float   p11[3] = { xOut, s_kEndBaseY, zFace };

            AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.86f);
        }

        // Crown: front horizon, over the top, down to the back horizon.
        AppendLatheX (out, xIn, xOut, s_kEndCy, s_kEndCz, s_kEndR, 0.0f, s_kPi, 10, s_kArgbCase);

        // Inner side cap: the profile's crown semicircle plus the block below.
        AppendDiscX (out, xCap, s_kEndCy, s_kEndCz, s_kEndR, 0.0f, s_kPi, 8, s_kArgbCase, 0.68f);
        {
            float   p00[3] = { xCap, s_kEndCy, s_kEndCz + s_kEndR };
            float   p10[3] = { xCap, s_kEndCy, s_kEndCz - s_kEndR };
            float   p01[3] = { xCap, s_kEndBaseY, s_kEndCz + s_kEndR };
            float   p11[3] = { xCap, s_kEndBaseY, s_kEndCz - s_kEndR };

            AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.68f);
        }
    }

    // Paper-advance knob: a short barrel on the crown axis, emerging from the
    // right housing, with a dim end-cap sliver (edge-on from this camera).
    AppendLatheX (out, s_kEndXOut, s_kKnobX1, s_kEndCy, s_kEndCz, s_kKnobR,
                  -0.35f, s_kPi, 8, s_kArgbCase);
    {
        float   p00[3] = { s_kKnobX1, s_kEndCy + s_kKnobR * 0.8f, s_kEndCz + 0.01f };
        float   p10[3] = { s_kKnobX1 + 0.012f, s_kEndCy + s_kKnobR * 0.55f, s_kEndCz };
        float   p01[3] = { s_kKnobX1, s_kEndCy - s_kKnobR * 0.8f, s_kEndCz + 0.01f };
        float   p11[3] = { s_kKnobX1 + 0.012f, s_kEndCy - s_kKnobR * 0.55f, s_kEndCz };

        AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbCase, 0.70f);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::BuildControls
//
//  The control staircase on the front face's right: six cream caps rising
//  toward the right (paper load/eject, form feed, line feed, print quality,
//  select, on/off), with status LEDs above the right three -- print quality
//  and select each get a single lit-green window; the on/off pair is a single
//  window split side by side, per the reference model -- unlit error (red)
//  on the left half, lit pwr (green) on the right half, both at the same
//  height (not stacked).
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::BuildControls (std::vector<Vertex> & out) const
{
    float   zWall = s_kBodyZFront + 0.001f;
    float   zCap  = s_kBodyZFront + 0.016f;   // paddles stand proud of the face

    for (int i = 0; i < s_kButtonCount; i++)
    {
        float   cx = s_kButtonX0 + (float) i * s_kButtonDx;
        float   cy = s_kButtonY0 + (float) i * s_kButtonDy;
        float   x0 = cx - s_kButtonW * 0.5f;
        float   x1 = cx + s_kButtonW * 0.5f;
        float   y1 = cy + s_kButtonH;

        // Real switches protrude: a shadow on the wall beneath, the paddle's
        // top face catching the light, and its front face standing proud.
        AppendFaceQuad (out, x0, x1, cy - 0.006f, cy, zWall, s_kArgbCase, 0.52f);

        {
            float   p00[3] = { x0, y1, zWall };
            float   p10[3] = { x1, y1, zWall };
            float   p01[3] = { x0, y1, zCap };
            float   p11[3] = { x1, y1, zCap };

            AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbButton, 1.06f);
        }

        AppendFaceQuad (out, x0, x1, cy, y1, zCap, s_kArgbButton, 0.94f);

        // LED windows flush in the wall above the right three caps.
        if (i == 3 || i == 4)
        {
            // print quality / select: a single lit-green window.
            float   ly = cy + s_kButtonH + 0.012f;

            AppendFaceQuad (out, cx - 0.013f, cx + 0.013f, ly, ly + 0.008f, zWall,
                            s_kArgbLedOn, 1.0f);
        }
        else if (i == 5)
        {
            // on/off: one window, split left(error, unlit)/right(pwr, lit).
            float   ly = cy + s_kButtonH + 0.012f;

            AppendFaceQuad (out, cx - 0.013f, cx,          ly, ly + 0.008f, zWall,
                            s_kArgbLedErr, 1.0f);
            AppendFaceQuad (out, cx,          cx + 0.013f, ly, ly + 0.008f, zWall,
                            s_kArgbLedOn, 1.0f);
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::BuildCassoLogo
//
//  The Casso cassowary badge in rainbow stripes, lower-left of the front face
//  (where the real machine wears its apple). One quad per contiguous bit run
//  per silhouette row, same as the chrome DriveWidget badge.
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::BuildCassoLogo (std::vector<Vertex> & out, float faceZ) const
{
    float   zf     = faceZ + 0.002f;
    float   height = s_kLogoWidth * (float) s_kLogoGridH / (float) s_kLogoGridW;
    float   rowH   = height / (float) s_kLogoGridH;
    float   colW   = s_kLogoWidth / (float) s_kLogoGridW;
    float   left   = m_logoLeft;
    float   topY   = m_logoTopY;
    int     first  = -1;
    int     last   = -1;

    for (int row = 0; row < s_kLogoGridH; row++)
    {
        if (s_kLogoSilhouette[row] != 0)
        {
            if (first < 0) { first = row; }
            last = row;
        }
    }
    if (last < first)
    {
        return;
    }

    for (int row = first; row <= last; row++)
    {
        uint64_t   bits   = s_kLogoSilhouette[row];
        int        stripe = ((row - first) * 6) / (last - first + 1);
        uint32_t   argb   = s_kLogoStripes[stripe];
        float      yTop   = topY - (float) (row - first) * rowH;
        int        col    = 0;

        while (col < s_kLogoGridW)
        {
            if ((bits & (1ULL << col)) == 0)
            {
                col++;
                continue;
            }

            int   runStart = col;

            while (col < s_kLogoGridW && (bits & (1ULL << col)) != 0)
            {
                col++;
            }

            AppendFaceQuad (out,
                            left + (float) runStart * colW,
                            left + (float) col * colW,
                            yTop - rowH, yTop, zf, argb, 1.0f);
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::BuildHeadOverlay
//
//  The paced print head (FR-034) for the loaded-mesh path: a dark carriage
//  with its four ribbon-ink stripes, riding the measured roller's front at
//  the reveal column. (The procedural path keeps its own inline head.)
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::BuildHeadOverlay (std::vector<Vertex> & out) const
{
    float   travel = s_kPaperHalfW - 0.10f;
    float   cx     = -travel + m_head01 * 2.0f * travel;
    float   zH     = m_platenZ + m_platenR + 0.015f;
    float   yTop   = m_platenY + m_platenR * 0.35f;
    float   yBot   = m_platenY - m_platenR * 0.85f;

    float   p00[3] = { cx - 0.075f, yTop, zH };
    float   p10[3] = { cx + 0.075f, yTop, zH };
    float   p01[3] = { cx - 0.075f, yBot, zH };
    float   p11[3] = { cx + 0.075f, yBot, zH };

    AppendQuad (out, p00, p10, p01, p11, 0, 0, 1, 1, s_kArgbHead, 1.0f);

    for (int i = 0; i < 4; i++)
    {
        float   rx     = cx - 0.058f + (float) i * 0.030f;
        float   ry     = m_platenY - m_platenR * 0.25f;
        float   r00[3] = { rx,          ry + 0.025f, zH + 0.005f };
        float   r10[3] = { rx + 0.024f, ry + 0.025f, zH + 0.005f };
        float   r01[3] = { rx,          ry,          zH + 0.005f };
        float   r11[3] = { rx + 0.024f, ry,          zH + 0.005f };

        AppendQuad (out, r00, r10, r01, r11, 0, 0, 1, 1, s_kArgbRibbon[i], 1.0f);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Printer3DScene::Render
//
////////////////////////////////////////////////////////////////////////////////

void Printer3DScene::Render (const RECT & targetPx)
{
    HRESULT          hr = S_OK;
    int              w  = targetPx.right - targetPx.left;
    int              h  = targetPx.bottom - targetPx.top;
    float            model[16];
    float            view[16];
    float            proj[16];
    float            viewProj[16];
    float            mvp[16];
    float            identity[16];
    D3D11_VIEWPORT   vp = {};

    if (!m_renderer.IsInitialized () || w <= 0 || h <= 0)
    {
        return;
    }

    vp.TopLeftX = (float) targetPx.left;
    vp.TopLeftY = (float) targetPx.top;
    vp.Width    = (float) w;
    vp.Height   = (float) h;
    vp.MaxDepth = 1.0f;

    TiltAboutFrontBottom (s_kBodyTiltRad, s_kBodyZFront, model);
    LookAtRH         (s_kEye, s_kAt, view);
    PerspectiveFovRH (s_kFovY, (float) w / (float) h, 0.1f, 20.0f, proj);
    Mul44            (view, proj, viewProj);
    Mul44            (model, viewProj, mvp);
    IdentityMvp      (identity);

    m_backdrop.clear ();
    m_solidBack.clear ();
    m_paper.clear ();
    m_solidFront.clear ();

    BuildBackdrop (m_backdrop);
    BuildPaper    (m_paper);

    IGNORE_RETURN_VALUE (hr, m_renderer.DrawTriangles (m_backdrop.data (), m_backdrop.size (), identity, false, vp));

    if (HasModel ())
    {
        // Loaded CAD body: real depth testing (its triangles arrive in
        // arbitrary order), with the paper, paced head, and badge overlays
        // depth-tested against it so occlusion just works.
        m_solidFront.clear ();
        BuildHeadOverlay (m_solidFront);
        BuildCassoLogo   (m_solidFront, m_meshFrontZ);

        IGNORE_RETURN_VALUE (hr, m_renderer.BeginDepthPass ());
        IGNORE_RETURN_VALUE (hr, m_renderer.DrawTriangles (m_mesh.data (),       m_mesh.size (),       mvp, false, vp, true));
        IGNORE_RETURN_VALUE (hr, m_renderer.DrawTriangles (m_paper.data (),      m_paper.size (),      mvp, true,  vp, true));
        IGNORE_RETURN_VALUE (hr, m_renderer.DrawTriangles (m_solidFront.data (), m_solidFront.size (), mvp, false, vp, true));

        // The smoked window last, over everything it must show through.
        if (!m_meshGlass.empty ())
        {
            IGNORE_RETURN_VALUE (hr, m_renderer.DrawTriangles (m_meshGlass.data (), m_meshGlass.size (), mvp, false, vp, true));
        }
    }
    else
    {
        // Procedural fallback: hand-ordered painter's-algorithm batches.
        BuildBodyBack  (m_solidBack);
        BuildBodyFront (m_solidFront);

        IGNORE_RETURN_VALUE (hr, m_renderer.DrawTriangles (m_solidBack.data (),  m_solidBack.size (),  mvp, false, vp));
        IGNORE_RETURN_VALUE (hr, m_renderer.DrawTriangles (m_paper.data (),      m_paper.size (),      mvp, true,  vp));
        IGNORE_RETURN_VALUE (hr, m_renderer.DrawTriangles (m_solidFront.data (), m_solidFront.size (), mvp, false, vp));
    }
}
