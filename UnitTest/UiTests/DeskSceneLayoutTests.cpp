#include "Pch.h"
#include "../EhmTestHelper.h"

#include "CrtPostProcess.h"
#include "Render/SceneCamera.h"
#include "Ui/Scene/DeskSceneLayout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayoutTests
//
//  Composition rules for the desk scene: deterministic placement, drive count
//  from machine config, containment at extreme aspects, the sceneScale
//  formula's DPI behavior, and the FR-016 single-camera / position-derived
//  parallax invariants -- a below-center drive must show its top face, an
//  off-center drive its inward flank, purely from placement under the one
//  shared camera.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeskSceneLayoutTests)
{
public:

    // Metrics mirroring the real models: Monitor //c 248x280x226 with the
    // generated glass rect, Disk II 155x222x86 (model space X/Y-back/Z-up).
    static DeskSceneMetrics MakeMetrics()
    {
        DeskSceneMetrics  metrics;

        metrics.monitorMin[0] = 0.0f;    metrics.monitorMax[0] = 248.0f;
        metrics.monitorMin[1] = -5.0f;   metrics.monitorMax[1] = 280.0f;
        metrics.monitorMin[2] = -2.0f;   metrics.monitorMax[2] = 226.0f;

        metrics.driveMin[0]   = 0.0f;    metrics.driveMax[0]   = 155.0f;
        metrics.driveMin[1]   = -5.0f;   metrics.driveMax[1]   = 222.0f;
        metrics.driveMin[2]   = -2.0f;   metrics.driveMax[2]   = 86.0f;

        metrics.glass.x0      = 29.0f;   metrics.glass.x1      = 219.0f;
        metrics.glass.z0      = 77.0f;   metrics.glass.z1      = 197.0f;
        metrics.glass.baseY   = 6.0f;
        metrics.glass.radius  = 2.2f * std::sqrt (95.0f * 95.0f + 60.0f * 60.0f);

        return metrics;
    }

    // Projects every world-space corner of a device's remapped model box and
    // asserts it lands inside the viewport.
    static void AssertDeviceContained (const DeskSceneComposition & comp,
                                       const float                  world[16],
                                       const float                  modelMin[3],
                                       const float                  modelMax[3])
    {
        for (int corner = 0; corner < 8; corner++)
        {
            float   modelPt[3] = { (corner & 1) ? modelMax[0] : modelMin[0],
                                   (corner & 2) ? modelMax[1] : modelMin[1],
                                   (corner & 4) ? modelMax[2] : modelMin[2] };
            float   worldPt[3] = {};
            float   screen[2]  = {};

            Assert::IsTrue (SceneCamera::TransformPoint (world, modelPt, worldPt));
            Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, worldPt, comp.viewportPx, screen));

            Assert::IsTrue (screen[0] >= comp.viewportPx.left - 1.0f);
            Assert::IsTrue (screen[0] <= comp.viewportPx.right + 1.0f);
            Assert::IsTrue (screen[1] >= comp.viewportPx.top - 1.0f);
            Assert::IsTrue (screen[1] <= comp.viewportPx.bottom + 1.0f);
        }
    }

    // How much of drive 0's lid the composition shows: the projected drop
    // from the back-top edge to the front-top edge, over the drive's whole
    // projected height. Zero looking straight on, growing with the gaze.
    static float ProjectedTopFaceFraction (const DeskSceneComposition & comp,
                                           const DeskSceneMetrics     & metrics)
    {
        float  cx            = (metrics.driveMin[0] + metrics.driveMax[0]) * 0.5f;
        float  frontTop[3]   = { cx, metrics.driveMin[1], metrics.driveMax[2] };
        float  backTop[3]    = { cx, metrics.driveMax[1], metrics.driveMax[2] };
        float  frontLow[3]   = { cx, metrics.driveMin[1], metrics.driveMin[2] };
        float  world[3]      = {};
        float  pxFrontTop[2] = {};
        float  pxBackTop[2]  = {};
        float  pxFrontLow[2] = {};


        Assert::IsTrue (SceneCamera::TransformPoint (comp.driveWorld[0], frontTop, world));
        Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, world, comp.viewportPx, pxFrontTop));

        Assert::IsTrue (SceneCamera::TransformPoint (comp.driveWorld[0], backTop, world));
        Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, world, comp.viewportPx, pxBackTop));

        Assert::IsTrue (SceneCamera::TransformPoint (comp.driveWorld[0], frontLow, world));
        Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, world, comp.viewportPx, pxFrontLow));

        return (pxFrontTop[1] - pxBackTop[1]) / (pxFrontLow[1] - pxBackTop[1]);
    }

    TEST_METHOD (Compute_Is_Deterministic)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        RECT                  vp      = { 0, 0, 1120, 768 };
        DeskSceneComposition  a;
        DeskSceneComposition  b;



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, a));
        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, b));

        Assert::AreEqual (0, memcmp (&a, &b, sizeof (a)));
    }

    TEST_METHOD (Empty_Viewport_Reports_S_FALSE)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        RECT                  vp      = { 10, 10, 10, 300 };
        DeskSceneComposition  comp;



        Assert::AreEqual (S_FALSE, DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));
    }

    TEST_METHOD (Invalid_Drive_Count_Asserts)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;
        DeskSceneMetrics                    metrics = MakeMetrics();
        RECT                                vp      = { 0, 0, 800, 600 };
        DeskSceneComposition                comp;



        AssertFailed (DeskSceneLayout::Compute (vp, 96, 3, metrics, comp));
        expect.RequireCount (1);
    }

    TEST_METHOD (Drive_Rects_Project_Disjoint_And_Ordered)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        RECT                  vp      = { 0, 0, 1120, 900 };
        DeskSceneComposition  comp;



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        // Both drives project to real rects inside the viewport, drive 0
        // left of drive 1 with a gap between them (the tooltip anchors and
        // drop targets the 2D widgets' OuterRects used to provide).
        for (int i = 0; i < 2; i++)
        {
            Assert::IsTrue (comp.driveRectPx[i].right > comp.driveRectPx[i].left);
            Assert::IsTrue (comp.driveRectPx[i].bottom > comp.driveRectPx[i].top);
            Assert::IsTrue (comp.driveRectPx[i].left >= vp.left);
            Assert::IsTrue (comp.driveRectPx[i].right <= vp.right);
        }

        Assert::IsTrue (comp.driveRectPx[0].right <= comp.driveRectPx[1].left);
    }

    TEST_METHOD (Drive_Count_Maps_To_Placements)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        RECT                  vp      = { 0, 0, 1120, 768 };
        DeskSceneComposition  zero;
        DeskSceneComposition  one;
        DeskSceneComposition  two;



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 0, metrics, zero));
        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 1, metrics, one));
        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, two));

        Assert::AreEqual (0, zero.driveCount);
        Assert::AreEqual (1, one.driveCount);
        Assert::AreEqual (2, two.driveCount);

        // A single drive centers on the scene axis; two flank it
        // symmetrically. Translation lives in row 3 of the world matrix,
        // offset by the model's own center -- devices are placed at true
        // size, so that offset is the whole correction.
        {
            float   driveCx = (metrics.driveMin[0] + metrics.driveMax[0]) * 0.5f;

            Assert::AreEqual (-driveCx, one.driveWorld[0][12], 0.01f);
            Assert::IsTrue   (two.driveWorld[0][12] < two.driveWorld[1][12]);
            Assert::AreEqual (-(two.driveWorld[0][12] + driveCx),
                              two.driveWorld[1][12] + driveCx, 0.01f);
        }
    }

    TEST_METHOD (Scene_Is_Contained_At_Extreme_Aspects)
    {
        DeskSceneMetrics  metrics = MakeMetrics();
        RECT              shapes[] = { { 0, 0, 1120, 768 },
                                       { 0, 0, 3000, 420 },
                                       { 0, 0, 420, 3000 },
                                       { 40, 60, 1000, 700 } };



        for (const RECT & vp : shapes)
        {
            DeskSceneComposition  comp;

            AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

            AssertDeviceContained (comp, comp.monitorWorld, metrics.monitorMin, metrics.monitorMax);
            AssertDeviceContained (comp, comp.driveWorld[0], metrics.driveMin, metrics.driveMax);
            AssertDeviceContained (comp, comp.driveWorld[1], metrics.driveMin, metrics.driveMax);
        }
    }

    TEST_METHOD (Strip_Row_Is_Contained_At_Every_Gaze)
    {
        DeskSceneMetrics  metrics  = MakeMetrics();
        RECT              shapes[] = { { 0, 0, 1120, 300 },
                                       { 0, 0, 900, 500 },
                                       { 0, 0, 3000, 200 },
                                       { 20, 40, 700, 460 } };
        float             gazes[]  = { 0.0f,
                                       DeskSceneLayout::kGazeDownRad,
                                       DeskSceneLayout::kDriveBandGazeDownRad,
                                       0.7f };



        // The standoff is solved in the gaze's frame, so a steep look-down
        // contains the row as exactly as a straight-on one -- solving in
        // world axes cropped the near-front edge off the band.
        for (const RECT & vp : shapes)
        {
            for (float gaze : gazes)
            {
                DeskSceneComposition  comp;

                AssertSucceeded (DeskSceneLayout::ComputeStrip (vp, 96, 2, metrics, comp, gaze));

                AssertDeviceContained (comp, comp.driveWorld[0], metrics.driveMin, metrics.driveMax);
                AssertDeviceContained (comp, comp.driveWorld[1], metrics.driveMin, metrics.driveMax);
            }
        }
    }

    TEST_METHOD (Steeper_Gaze_Shows_More_Of_The_Drive_Top)
    {
        DeskSceneMetrics      metrics    = MakeMetrics();
        RECT                  vp         = { 0, 0, 1120, 300 };
        DeskSceneComposition  shallow;
        DeskSceneComposition  steep;
        float                 topShallow = 0.0f;
        float                 topSteep   = 0.0f;



        AssertSucceeded (DeskSceneLayout::ComputeStrip (vp, 96, 2, metrics, shallow,
                                                        DeskSceneLayout::kGazeDownRad));
        AssertSucceeded (DeskSceneLayout::ComputeStrip (vp, 96, 2, metrics, steep,
                                                        DeskSceneLayout::kDriveBandGazeDownRad));

        // How much lid the viewer sees: the projected gap between the drive's
        // front-top edge and the back-top edge, as a fraction of the whole
        // drive's projected height. The band's gaze is calibrated to the 2D
        // widget's 56 dp case top over its 104 dp faceplate.
        topShallow = ProjectedTopFaceFraction (shallow, metrics);
        topSteep   = ProjectedTopFaceFraction (steep, metrics);

        Assert::IsTrue (topSteep > topShallow * 1.5f);
    }

    TEST_METHOD (SceneScale_Halves_When_Dpi_Doubles)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        RECT                  vp      = { 0, 0, 1120, 768 };
        DeskSceneComposition  at96;
        DeskSceneComposition  at192;



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, at96));
        AssertSucceeded (DeskSceneLayout::Compute (vp, 192, 2, metrics, at192));

        Assert::IsTrue   (at96.sceneScale > 0.0f);
        Assert::AreEqual (at96.sceneScale, at192.sceneScale * 2.0f, 1e-4f);
    }

    TEST_METHOD (CenterSizeForDisplayPx_Round_Trips_Through_Compute)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        SIZE                  center  = DeskSceneLayout::CenterSizeForDisplayPx (1120, 768, 96, 2, metrics);
        RECT                  vp      = { 0, 0, center.cx, center.cy };
        DeskSceneComposition  comp;



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        // The Ctrl+0 contract: at the solved center, the picture's ACTUAL
        // on-screen height -- measured through the band placement, sag, and
        // gaze keystone -- lands at the requested size within quantization
        // slack.
        {
            float   measured = DeskSceneLayout::MeasurePictureHeightPx (comp, metrics.glass, 1120, 768);

            Assert::IsTrue (std::abs (measured - 768.0f) <= 3.0f);
        }

        // And the glass rect sits inside the viewport.
        Assert::IsTrue (comp.glassRectPx.left >= 0 && comp.glassRectPx.top >= 0);
        Assert::IsTrue (comp.glassRectPx.right <= center.cx && comp.glassRectPx.bottom <= center.cy);
    }

    TEST_METHOD (Below_Center_Drive_Shows_Its_Top_Face)
    {
        DeskSceneMetrics      metrics     = MakeMetrics();
        RECT                  vp          = { 0, 0, 1120, 768 };
        DeskSceneComposition  comp;
        float                 frontTop[3] = {};
        float                 backTop[3]  = {};
        float                 frontPx[2]  = {};
        float                 backPx[2]   = {};
        // *** TEMPORARY: the drives are turned over for inspection (see
        // *** kDriveInspectFlip), which swaps which face points up and which
        // *** end of it is the deeper. What is under test does not change --
        // *** the upward face has visible area -- so the test asks for it in
        // *** whichever pose the scene is currently built in.
        bool                  flipped     = DeskSceneLayout::kDriveInspectFlip;
        float                 upZ         = flipped ? metrics.driveMin[2] : metrics.driveMax[2];
        float                 deepY       = flipped ? metrics.driveMin[1] : metrics.driveMax[1];
        float                 nearY       = flipped ? metrics.driveMax[1] : metrics.driveMin[1];



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        // The upward face's near and deep edges, both through the left
        // drive's world transform.
        {
            float   frontModel[3] = { 77.5f, nearY, upZ };
            float   backModel[3]  = { 77.5f, deepY, upZ };

            Assert::IsTrue (SceneCamera::TransformPoint (comp.driveWorld[0], frontModel, frontTop));
            Assert::IsTrue (SceneCamera::TransformPoint (comp.driveWorld[0], backModel, backTop));
        }

        Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, frontTop, vp, frontPx));
        Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, backTop, vp, backPx));

        // Seen from above: the deeper edge of that face projects HIGHER on
        // screen than the near one, so the face has visible area (FR-016
        // parallax).
        Assert::IsTrue (backPx[1] < frontPx[1] - 1.0f);
    }

    TEST_METHOD (Off_Center_Drive_Shows_Its_Inward_Flank)
    {
        DeskSceneMetrics      metrics      = MakeMetrics();
        RECT                  vp           = { 0, 0, 1120, 768 };
        DeskSceneComposition  comp;
        float                 frontEdge[3] = {};
        float                 backEdge[3]  = {};
        float                 frontPx[2]   = {};
        float                 backPx[2]    = {};
        // *** TEMPORARY: turning the drives over for inspection swaps which
        // *** end is the deeper -- see kDriveInspectFlip. The flank's visible
        // *** width is what is under test either way.
        bool                  flipped      = DeskSceneLayout::kDriveInspectFlip;
        float                 deepY        = flipped ? metrics.driveMin[1] : metrics.driveMax[1];
        float                 nearY        = flipped ? metrics.driveMax[1] : metrics.driveMin[1];



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        // The LEFT drive's RIGHT (inward) side: the near and deep corners of
        // that flank.
        {
            float   frontModel[3] = { metrics.driveMax[0], nearY, 40.0f };
            float   backModel[3]  = { metrics.driveMax[0], deepY, 40.0f };

            Assert::IsTrue (SceneCamera::TransformPoint (comp.driveWorld[0], frontModel, frontEdge));
            Assert::IsTrue (SceneCamera::TransformPoint (comp.driveWorld[0], backModel, backEdge));
        }

        Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, frontEdge, vp, frontPx));
        Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, backEdge, vp, backPx));

        // The deeper edge pulls toward the screen center (rightward for the
        // left drive): the inward flank has visible projected width.
        Assert::IsTrue (backPx[0] > frontPx[0] + 1.0f);
    }


    //
    //  The framing is a lens on top of the fitted composition, so an identity
    //  view has to leave the composition bit-for-bit alone. Anything else and
    //  every existing expectation about the scene silently shifts.
    //
    TEST_METHOD (Identity_View_Changes_Nothing)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        RECT                  vp      = { 0, 0, 1120, 768 };
        DeskSceneComposition  plain;
        DeskSceneComposition  framed;
        DeskSceneView         view;



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, plain));
        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, framed, 0, view));

        Assert::AreEqual (0, memcmp (&plain, &framed, sizeof (plain)),
            L"a default DeskSceneView must be a no-op");
    }


    //
    //  Zoom magnifies about the CENTER of the viewport, so a point already at
    //  the center stays put while everything else moves outward from it.
    //
    TEST_METHOD (Zoom_Magnifies_About_The_Viewport_Center)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        RECT                  vp      = { 0, 0, 1120, 768 };
        DeskSceneComposition  plain;
        DeskSceneComposition  zoomed;
        DeskSceneView         view;
        float                 plainW  = 0.0f;
        float                 zoomedW = 0.0f;

        view.zoom = 2.0f;

        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, plain));
        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, zoomed, 0, view));

        plainW  = (float) (plain.glassRectPx.right  - plain.glassRectPx.left);
        zoomedW = (float) (zoomed.glassRectPx.right - zoomed.glassRectPx.left);

        Assert::IsTrue (zoomedW > plainW * 1.8f,
            L"2x zoom must roughly double the projected glass width -- and the "
            L"GLASS RECT proves the projected bounds followed the lens, not "
            L"just the camera, so the CRT still lands where the glass is.");
    }


    //
    //  Pan shifts the composition without resizing it. Both have to hold: a
    //  transform that moved things by scaling them would pass a position
    //  check alone.
    //
    TEST_METHOD (Pan_Shifts_Without_Resizing)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        RECT                  vp      = { 0, 0, 1120, 768 };
        DeskSceneComposition  plain;
        DeskSceneComposition  panned;
        DeskSceneView         view;
        int                   plainW  = 0;
        int                   pannedW = 0;

        view.panX = 0.5f;      // half an NDC unit right == a quarter viewport

        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, plain));
        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, panned, 0, view));

        plainW  = plain.glassRectPx.right  - plain.glassRectPx.left;
        pannedW = panned.glassRectPx.right - panned.glassRectPx.left;

        Assert::IsTrue (panned.glassRectPx.left > plain.glassRectPx.left,
            L"+panX moves the scene right");
        Assert::IsTrue (std::abs (pannedW - plainW) <= 1,
            L"and must not change its size while doing so");
    }


    //
    //  Depth is deliberately untouched by the lens. The shadow maps are built
    //  against the same world, so a transform that shifted z would slide every
    //  shadow off the thing casting it.
    //
    TEST_METHOD (View_Transform_Leaves_Depth_Alone)
    {
        float          plain[16]  = {};
        float          framed[16] = {};
        DeskSceneView  view;
        int            row        = 0;

        view.zoom = 3.0f;
        view.panX = 0.4f;
        view.panY = -0.2f;

        SceneCamera::PerspectiveFovRH (0.8f, 1.5f, 1.0f, 5000.0f, plain);
        memcpy (framed, plain, sizeof (plain));

        DeskSceneLayout::ApplyViewTransform (view, framed);

        for (row = 0; row < 4; row++)
        {
            Assert::AreEqual (plain[row * 4 + 2], framed[row * 4 + 2], 1e-6f,
                L"the z column must survive the lens untouched");
            Assert::AreEqual (plain[row * 4 + 3], framed[row * 4 + 3], 1e-6f,
                L"and so must w, or the perspective divide changes meaning");
        }
    }

};
