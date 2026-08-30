#include "Pch.h"

#include "Render/CurvedDisplayMath.h"
#include "Render/SceneCamera.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CurvedDisplayMathTests
//
//  The glass mappings both directions: UV <-> model point (with sag), ray
//  intersection, and screen px <-> emulated pixel round trips. The forward
//  transform is the oracle for the inverse, so the one-pixel accuracy bar
//  (FR-002) is pinned in math before any rendering exists.
//
//  The fixture mirrors the generated Monitor //c glass (rect ~190 x 120 mm,
//  radius 2.2x the half-diagonal) under a front camera with the model mounted
//  by the scene's Z-up -> Y-up axis remap, so the numbers exercised here are
//  the shipping geometry's, not toy values.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr int  s_kDisplayW = 560;
static constexpr int  s_kDisplayH = 384;


TEST_CLASS (CurvedDisplayMathTests)
{
public:

    // Glass rect from the generated Monitor2c model: x 29..219, z 77..197,
    // front plane y=6, radius = 2.2 * half-diagonal.
    static CurvedDisplaySurface MakeSurface()
    {
        CurvedDisplaySurface  surface;

        surface.x0     = 29.0f;
        surface.x1     = 219.0f;
        surface.z0     = 77.0f;
        surface.z1     = 197.0f;
        surface.baseY  = 6.0f;
        surface.radius = 2.2f * std::sqrt (95.0f * 95.0f + 60.0f * 60.0f);

        return surface;
    }

    // Model space (X right, Y back, Z up) -> world (X right, Y up, Z toward
    // viewer): the same axis remap the scene applies when mounting a model.
    static void MakeWorld (float out[16])
    {
        memset (out, 0, 16 * sizeof (float));
        out[0]  = 1.0f;    // x -> x
        out[6]  = -1.0f;   // model y (back) -> world -z
        out[9]  = 1.0f;    // model z (up)   -> world y
        out[15] = 1.0f;
    }

    static void MakeViewProj (const CurvedDisplaySurface & surface,
                              float                        eyeOffsetX,
                              float                        out[16])
    {
        float  cx       = (surface.x0 + surface.x1) * 0.5f;
        float  cyWorld  = (surface.z0 + surface.z1) * 0.5f;   // model z -> world y
        float  eye[3]   = { cx + eyeOffsetX, cyWorld, 700.0f };
        float  at[3]    = { cx, cyWorld, 0.0f };
        float  view[16] = {};
        float  proj[16] = {};

        SceneCamera::LookAtRH         (eye, at, view);
        SceneCamera::PerspectiveFovRH (0.35f, 1120.0f / 768.0f, 1.0f, 2000.0f, proj);
        SceneCamera::Mul44            (view, proj, out);
    }

    TEST_METHOD (Corners_Sit_On_The_Front_Plane_And_Center_Bulges)
    {
        CurvedDisplaySurface  surface   = MakeSurface();
        float                 corner[3] = {};
        float                 center[3] = {};
        float                 maxSag    = CurvedDisplayMath::MaxSag (surface);



        Assert::IsTrue (CurvedDisplayMath::IsValid (surface));
        Assert::IsTrue (maxSag > 0.0f && maxSag < surface.radius);

        CurvedDisplayMath::ModelPointFromUv (surface, 0.0f, 0.0f, corner);
        CurvedDisplayMath::ModelPointFromUv (surface, 0.5f, 0.5f, center);

        Assert::AreEqual (surface.baseY, corner[1], 0.01f);
        Assert::AreEqual (surface.baseY - maxSag, center[1], 0.01f);

        // The bulge is toward the viewer: center y is LESS than corner y.
        Assert::IsTrue (center[1] < corner[1]);
    }

    TEST_METHOD (Uv_ModelPoint_Round_Trips)
    {
        CurvedDisplaySurface  surface = MakeSurface();



        for (float u = 0.0f; u <= 1.0f; u += 0.25f)
        {
            for (float v = 0.0f; v <= 1.0f; v += 0.25f)
            {
                float   pt[3]  = {};
                float   uOut   = 0.0f;
                float   vOut   = 0.0f;

                CurvedDisplayMath::ModelPointFromUv (surface, u, v, pt);
                CurvedDisplayMath::UvFromModelPoint (surface, pt, uOut, vOut);

                Assert::AreEqual (u, uOut, 1e-5f);
                Assert::AreEqual (v, vOut, 1e-5f);
            }
        }
    }

    TEST_METHOD (Straight_On_Ray_Hits_The_Center_At_Full_Sag)
    {
        CurvedDisplaySurface  surface   = MakeSurface();
        float                 cx        = (surface.x0 + surface.x1) * 0.5f;
        float                 cz        = (surface.z0 + surface.z1) * 0.5f;
        float                 origin[3] = { cx, -500.0f, cz };
        float                 dir[3]    = { 0.0f, 1.0f, 0.0f };
        float                 hit[3]    = {};



        Assert::IsTrue  (CurvedDisplayMath::IntersectRay (surface, origin, dir, hit));
        Assert::AreEqual (cx, hit[0], 1e-3f);
        Assert::AreEqual (surface.baseY - CurvedDisplayMath::MaxSag (surface), hit[1], 1e-3f);
        Assert::AreEqual (cz, hit[2], 1e-3f);
    }

    TEST_METHOD (Ray_Outside_The_Rect_Misses)
    {
        CurvedDisplaySurface  surface   = MakeSurface();
        float                 origin[3] = { surface.x1 + 30.0f, -500.0f, (surface.z0 + surface.z1) * 0.5f };
        float                 dir[3]    = { 0.0f, 1.0f, 0.0f };
        float                 hit[3]    = {};



        Assert::IsFalse (CurvedDisplayMath::IntersectRay (surface, origin, dir, hit));
    }

    TEST_METHOD (Pixel_Round_Trips_Exactly_Across_The_Grid)
    {
        CurvedDisplaySurface  surface   = MakeSurface();
        float                 world[16] = {};
        float                 vp[16]    = {};
        RECT                  viewport  = { 0, 0, 1120, 768 };
        POINT                 samples[] = { { 0, 0 }, { s_kDisplayW - 1, 0 },
                                            { 0, s_kDisplayH - 1 },
                                            { s_kDisplayW - 1, s_kDisplayH - 1 },
                                            { s_kDisplayW / 2, 0 }, { 0, s_kDisplayH / 2 },
                                            { s_kDisplayW - 1, s_kDisplayH / 2 },
                                            { s_kDisplayW / 2, s_kDisplayH - 1 },
                                            { s_kDisplayW / 2, s_kDisplayH / 2 },
                                            { 137, 291 } };



        MakeWorld    (world);
        MakeViewProj (surface, 0.0f, vp);

        for (const POINT & sample : samples)
        {
            float   screen[2] = {};
            POINT   back      = {};

            Assert::IsTrue (CurvedDisplayMath::ScreenPxFromEmulatedPixel (
                surface, world, vp, viewport, sample.x, sample.y, s_kDisplayW, s_kDisplayH, screen));
            Assert::IsTrue (CurvedDisplayMath::EmulatedPixelFromScreenPx (
                surface, world, vp, viewport, screen[0], screen[1], s_kDisplayW, s_kDisplayH, back));

            Assert::AreEqual (sample.x, back.x);
            Assert::AreEqual (sample.y, back.y);
        }
    }

    TEST_METHOD (Off_Axis_Camera_Still_Round_Trips_Glancing_Edges)
    {
        CurvedDisplaySurface  surface   = MakeSurface();
        float                 world[16] = {};
        float                 vp[16]    = {};
        RECT                  viewport  = { 0, 0, 1120, 768 };
        POINT                 samples[] = { { 0, 0 }, { s_kDisplayW - 1, 0 },
                                            { 0, s_kDisplayH - 1 },
                                            { s_kDisplayW - 1, s_kDisplayH - 1 },
                                            { 0, s_kDisplayH / 2 },
                                            { s_kDisplayW - 1, s_kDisplayH / 2 } };



        MakeWorld (world);

        // Eye shifted well off axis: edge pixels are hit at a glancing angle.
        MakeViewProj (surface, 260.0f, vp);

        for (const POINT & sample : samples)
        {
            float   screen[2] = {};
            POINT   back      = {};

            Assert::IsTrue (CurvedDisplayMath::ScreenPxFromEmulatedPixel (
                surface, world, vp, viewport, sample.x, sample.y, s_kDisplayW, s_kDisplayH, screen));
            Assert::IsTrue (CurvedDisplayMath::EmulatedPixelFromScreenPx (
                surface, world, vp, viewport, screen[0], screen[1], s_kDisplayW, s_kDisplayH, back));

            Assert::AreEqual (sample.x, back.x);
            Assert::AreEqual (sample.y, back.y);
        }
    }

    TEST_METHOD (Picture_Edge_Maps_To_The_Outermost_Pixel)
    {
        CurvedDisplaySurface  surface   = MakeSurface();
        float                 world[16] = {};
        float                 vp[16]    = {};
        RECT                  viewport  = { 0, 0, 1120, 768 };
        float                 screen[2] = {};
        float                 center[2] = {};
        POINT                 pixel     = {};



        MakeWorld    (world);
        MakeViewProj (surface, 0.0f, vp);

        // Project the corner PIXEL and the display center, step a quarter
        // pixel inward from the corner: that point is visually on the
        // picture's edge region and must map to pixel (0,0), never miss.
        Assert::IsTrue (CurvedDisplayMath::ScreenPxFromEmulatedPixel (
            surface, world, vp, viewport, 0, 0, s_kDisplayW, s_kDisplayH, screen));
        Assert::IsTrue (CurvedDisplayMath::ScreenPxFromEmulatedPixel (
            surface, world, vp, viewport, s_kDisplayW / 2, s_kDisplayH / 2, s_kDisplayW, s_kDisplayH, center));

        {
            float   toCenter[2] = { center[0] - screen[0], center[1] - screen[1] };
            float   len         = std::sqrt (toCenter[0] * toCenter[0] + toCenter[1] * toCenter[1]);
            float   edgeX       = screen[0] + toCenter[0] / len * 0.25f;
            float   edgeY       = screen[1] + toCenter[1] / len * 0.25f;

            Assert::IsTrue  (CurvedDisplayMath::EmulatedPixelFromScreenPx (
                surface, world, vp, viewport, edgeX, edgeY, s_kDisplayW, s_kDisplayH, pixel));
            Assert::AreEqual ((LONG) 0, pixel.x);
            Assert::AreEqual ((LONG) 0, pixel.y);
        }
    }

    TEST_METHOD (Glass_Outside_The_Picture_Band_Misses)
    {
        CurvedDisplaySurface  surface    = MakeSurface();
        float                 world[16]  = {};
        float                 vp[16]     = {};
        RECT                  viewport   = { 0, 0, 1120, 768 };
        float                 corner[3]  = {};
        float                 worldPt[3] = {};
        float                 screen[2]  = {};
        POINT                 pixel      = {};



        MakeWorld    (world);
        MakeViewProj (surface, 0.0f, vp);

        // The glass corner (uv 0,0) lies in the pillarbox margin outside the
        // aspect-fitted picture band: it is tube, not display, and must not
        // report a pixel.
        CurvedDisplayMath::ModelPointFromUv (surface, 0.0f, 0.0f, corner);
        Assert::IsTrue  (SceneCamera::TransformPoint (world, corner, worldPt));
        Assert::IsTrue  (SceneCamera::ProjectToScreen (vp, worldPt, viewport, screen));
        Assert::IsFalse (CurvedDisplayMath::EmulatedPixelFromScreenPx (
            surface, world, vp, viewport, screen[0] + 0.25f, screen[1] + 0.25f,
            s_kDisplayW, s_kDisplayH, pixel));
    }

    TEST_METHOD (Screen_Point_Off_The_Glass_Misses)
    {
        CurvedDisplaySurface  surface   = MakeSurface();
        float                 world[16] = {};
        float                 vp[16]    = {};
        RECT                  viewport  = { 0, 0, 1120, 768 };
        POINT                 pixel     = {};



        MakeWorld    (world);
        MakeViewProj (surface, 0.0f, vp);

        // The viewport corner is dead space well outside the projected glass.
        Assert::IsFalse (CurvedDisplayMath::EmulatedPixelFromScreenPx (
            surface, world, vp, viewport, 2.0f, 2.0f, s_kDisplayW, s_kDisplayH, pixel));
    }

};
