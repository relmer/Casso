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

    //
    //  THE ORBIT TURNS THE MODELS, NOT THE EYE -- so from half a turn away
    //  the same two drives project mirrored: the one on the left appears on
    //  the right. The mirroring is what a user sees, and it is pinned first.
    //
    //  WHICH SIDE MOVES IS ALSO PART OF THE CONTRACT. The camera stays where
    //  it is and the devices' world transforms carry the rotation, because
    //  everything lit and shadowed is anchored in WORLD space: turning the
    //  eye instead leaves the room's lights fixed relative to the machines,
    //  so the shading and both shadow passes never change as the scene
    //  spins. A composition whose world matrices came back unchanged under
    //  yaw would be the camera-orbit regression, and is failed here.
    //
    //  And an absurd pitch must stay finite rather than blow up: ten radians
    //  is a legal number and the symptom of mishandling it is NaN rects, not
    //  an exception.
    //
    TEST_METHOD (Orbit_Yaw_Mirrors_The_View_And_Pitch_Clamps)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        RECT                  vp      = { 0, 0, 1120, 900 };
        DeskSceneComposition  front;
        DeskSceneComposition  behind;
        DeskSceneComposition  above;
        DeskSceneView         spun;
        DeskSceneView         wound;



        spun.orbitYawRad    = 3.1415927f;
        wound.orbitPitchRad = 10.0f;

        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, front));
        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, behind, 0, spun));
        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, above, 0, wound));

        // From the front, drive 0 is left of drive 1; from behind, mirrored.
        Assert::IsTrue (front.driveRectPx[0].right <= front.driveRectPx[1].left);
        Assert::IsTrue (behind.driveRectPx[1].right <= behind.driveRectPx[0].left);

        // The MODELS carried the turn: their world transforms differ, and
        // the camera's did not move.
        {
            bool  driveTurned   = false;
            bool  monitorTurned = false;

            for (int i = 0; i < 16; i++)
            {
                driveTurned   = driveTurned ||
                                std::abs (front.driveWorld[0][i] - behind.driveWorld[0][i]) > 1e-4f;
                monitorTurned = monitorTurned ||
                                std::abs (front.monitorWorld[i] - behind.monitorWorld[i]) > 1e-4f;
            }

            Assert::IsTrue (driveTurned);
            Assert::IsTrue (monitorTurned);
        }

        for (int i = 0; i < 16; i++)
        {
            Assert::AreEqual (front.viewProj[i], behind.viewProj[i], 1e-4f);
        }

        // Ten radians of pitch clamps to a steep look-down and still projects
        // finite geometry.
        for (int i = 0; i < 2; i++)
        {
            Assert::IsTrue (above.driveRectPx[i].right >= above.driveRectPx[i].left);
            Assert::IsTrue (above.driveRectPx[i].right - above.driveRectPx[i].left < 100000);
        }
    }

    //
    //  A MODERATE ORBIT KEEPS THE SCENE IN FRAME. The eye pivots about the
    //  gaze target, so a half-radian of yaw must leave the monitor's rect
    //  overlapping the viewport's middle third horizontally -- if it slides
    //  to an edge, the pivot is not the target and inspection is a fight.
    //
    TEST_METHOD (Orbit_Moderate_Yaw_Keeps_The_Monitor_Near_Center)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        RECT                  vp      = { 0, 0, 1120, 900 };
        DeskSceneComposition  comp;
        DeskSceneView         view;



        view.orbitYawRad   = 0.6f;
        view.orbitPitchRad = 0.2f;

        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp, 0, view));

        {
            long  cx = (comp.monitorRectPx.left + comp.monitorRectPx.right) / 2;

            Assert::IsTrue (cx > vp.right / 3,     L"monitor slid to the left edge");
            Assert::IsTrue (cx < vp.right * 2 / 3, L"monitor slid to the right edge");
        }
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



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        // Top edge of the drive, front (model y min) vs back (model y max),
        // both through the left drive's world transform.
        {
            float   frontModel[3] = { 77.5f, metrics.driveMin[1], metrics.driveMax[2] };
            float   backModel[3]  = { 77.5f, metrics.driveMax[1], metrics.driveMax[2] };

            Assert::IsTrue (SceneCamera::TransformPoint (comp.driveWorld[0], frontModel, frontTop));
            Assert::IsTrue (SceneCamera::TransformPoint (comp.driveWorld[0], backModel, backTop));
        }

        Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, frontTop, vp, frontPx));
        Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, backTop, vp, backPx));

        // Seen from above: the back of the lid projects HIGHER on screen than
        // the front edge, so the top face has visible area (FR-016 parallax).
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



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        // The LEFT drive's RIGHT (inward) side: front and back bottom
        // corners of that flank.
        {
            float   frontModel[3] = { metrics.driveMax[0], metrics.driveMin[1], 40.0f };
            float   backModel[3]  = { metrics.driveMax[0], metrics.driveMax[1], 40.0f };

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

    // Projects a billboard's four world corners and returns their screen
    // bounds, which is the whole of what the sizing contract is about.
    static void MeasureQuadPx (const DeskSceneComposition & comp,
                               const float                  corners[4][3],
                               RECT                       & outPx)
    {
        float  pxMin[2] = {};
        float  pxMax[2] = {};

        Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, corners[0], comp.viewportPx, pxMin));

        pxMax[0] = pxMin[0];
        pxMax[1] = pxMin[1];

        for (int corner = 1; corner < 4; corner++)
        {
            float  px[2] = {};

            Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, corners[corner], comp.viewportPx, px));

            pxMin[0] = std::min (pxMin[0], px[0]);  pxMax[0] = std::max (pxMax[0], px[0]);
            pxMin[1] = std::min (pxMin[1], px[1]);  pxMax[1] = std::max (pxMax[1], px[1]);
        }

        outPx.left   = (LONG) std::lround (pxMin[0]);
        outPx.top    = (LONG) std::lround (pxMin[1]);
        outPx.right  = (LONG) std::lround (pxMax[0]);
        outPx.bottom = (LONG) std::lround (pxMax[1]);
    }


    // The clip w of a world point: its distance in front of the eye, which
    // is what the billboard's four corners must all agree on.
    static float MeasureClipW (const DeskSceneComposition & comp, const float worldPt[3])
    {
        return worldPt[0] * comp.viewProj[3]  + worldPt[1] * comp.viewProj[7] +
               worldPt[2] * comp.viewProj[11] + comp.viewProj[15];
    }


    TEST_METHOD (Camera_Basis_Is_Orthonormal_And_Square_To_The_Gaze)
    {
        DeskSceneMetrics      metrics  = MakeMetrics();
        DeskSceneComposition  comp;
        RECT                  viewport = { 0, 0, 1600, 1000 };
        float                 right[3] = {};
        float                 up[3]    = {};
        float                 rl       = 0.0f;
        float                 ul       = 0.0f;



        Assert::AreEqual (S_OK, DeskSceneLayout::Compute (viewport, 96, 2, metrics, comp));

        DeskSceneLayout::GetCameraBasis (comp.view, right, up);

        rl = std::sqrt (right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
        ul = std::sqrt (up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);

        Assert::AreEqual (1.0f, rl, 1e-5f, L"camera right must be unit length");
        Assert::AreEqual (1.0f, ul, 1e-5f, L"camera up must be unit length");

        Assert::AreEqual (0.0f, right[0] * up[0] + right[1] * up[1] + right[2] * up[2], 1e-5f,
            L"right and up must be square to each other, or the quad shears");
    }


    TEST_METHOD (World_Per_Pixel_Grows_With_Distance)
    {
        DeskSceneMetrics      metrics    = MakeMetrics();
        DeskSceneComposition  comp;
        RECT                  viewport   = { 0, 0, 1600, 1000 };
        float                 anchor[3]  = {};
        float                 farther[3] = {};
        float                 nearX      = 0.0f;
        float                 nearY      = 0.0f;
        float                 farX       = 0.0f;
        float                 farY       = 0.0f;



        Assert::AreEqual (S_OK, DeskSceneLayout::Compute (viewport, 96, 2, metrics, comp));

        anchor[0] = comp.driveLabelWorld[0][0];
        anchor[1] = comp.driveLabelWorld[0][1];
        anchor[2] = comp.driveLabelWorld[0][2];

        // Straight back along the gaze, which in this world frame is -Z.
        farther[0] = anchor[0];
        farther[1] = anchor[1];
        farther[2] = anchor[2] - 500.0f;

        Assert::IsTrue (DeskSceneLayout::GetWorldPerPixel (comp, anchor,  nearX, nearY));
        Assert::IsTrue (DeskSceneLayout::GetWorldPerPixel (comp, farther, farX,  farY));

        Assert::IsTrue (farY > nearY, L"a pixel must cover more world further away");
        Assert::IsTrue (farX > nearX, L"and the same across as down");
    }


    TEST_METHOD (World_Per_Pixel_Follows_The_Zoom)
    {
        DeskSceneMetrics      metrics  = MakeMetrics();
        DeskSceneComposition  plain;
        DeskSceneComposition  zoomed;
        DeskSceneView         view;
        RECT                  viewport = { 0, 0, 1600, 1000 };
        float                 plainX   = 0.0f;
        float                 plainY   = 0.0f;
        float                 zoomX    = 0.0f;
        float                 zoomY    = 0.0f;



        view.zoom = 2.0f;

        Assert::AreEqual (S_OK, DeskSceneLayout::Compute (viewport, 96, 2, metrics, plain));
        Assert::AreEqual (S_OK, DeskSceneLayout::Compute (viewport, 96, 2, metrics, zoomed, 0, view));

        Assert::IsTrue (DeskSceneLayout::GetWorldPerPixel (plain,  plain.driveLabelWorld[0],  plainX, plainY));
        Assert::IsTrue (DeskSceneLayout::GetWorldPerPixel (zoomed, zoomed.driveLabelWorld[0], zoomX,  zoomY));

        // A doubled zoom halves the world a pixel covers. Reading the scale
        // off kFovY instead of the composition's own proj would miss this and
        // size the name wrong at every zoom but 1.0.
        Assert::AreEqual (plainY * 0.5f, zoomY, plainY * 0.01f,
            L"a doubled zoom must halve the world per pixel");
        Assert::AreEqual (plainX * 0.5f, zoomX, plainX * 0.01f,
            L"and the same across");
    }


    TEST_METHOD (World_Per_Pixel_Rejects_A_Point_Behind_The_Eye)
    {
        DeskSceneMetrics      metrics   = MakeMetrics();
        DeskSceneComposition  comp;
        RECT                  viewport  = { 0, 0, 1600, 1000 };
        float                 behind[3] = {};
        float                 perPxX    = 1.0f;
        float                 perPxY    = 1.0f;



        Assert::AreEqual (S_OK, DeskSceneLayout::Compute (viewport, 96, 2, metrics, comp));

        // Well behind the camera, which sits forward of the whole scene.
        behind[0] = comp.driveLabelWorld[0][0];
        behind[1] = comp.driveLabelWorld[0][1];
        behind[2] = comp.driveLabelWorld[0][2] + 100000.0f;

        Assert::IsFalse (DeskSceneLayout::GetWorldPerPixel (comp, behind, perPxX, perPxY));

        Assert::AreEqual (0.0f, perPxX, L"a refused query must not leave a stale scale behind");
        Assert::AreEqual (0.0f, perPxY, L"and the same down");
    }


    TEST_METHOD (Label_Quad_Covers_The_Requested_Pixels)
    {
        DeskSceneMetrics      metrics       = MakeMetrics();
        DeskSceneComposition  comp;
        RECT                  viewport      = { 0, 0, 1600, 1000 };
        SIZE                  labelPx       = { 120, 16 };
        int                   gapPx         = 4;
        float                 corners[4][3] = {};
        RECT                  quadPx        = {};



        Assert::AreEqual (S_OK, DeskSceneLayout::Compute (viewport, 96, 2, metrics, comp));

        Assert::IsTrue (DeskSceneLayout::TryMakeDriveLabelQuad (comp, 0, labelPx, gapPx, corners));

        MeasureQuadPx (comp, corners, quadPx);

        Assert::AreEqual (labelPx.cx, quadPx.right - quadPx.left,
            L"the quad must be exactly as wide as it was asked for");
        Assert::AreEqual (labelPx.cy, quadPx.bottom - quadPx.top,
            L"and exactly as tall");

        // Centered on the anchor and hung the gap below it: the placement the
        // chrome strip had, which is what keeps the tooltip rect honest.
        Assert::AreEqual (comp.driveLabelPx[0].x, (quadPx.left + quadPx.right) / 2,
            L"centered on the drive's anchor");
        Assert::AreEqual (comp.driveLabelPx[0].y + gapPx, quadPx.top,
            L"hung the gap below that anchor");
    }


    TEST_METHOD (Label_Quad_Keeps_Its_Pixel_Size_Through_The_Orbit)
    {
        DeskSceneMetrics      metrics          = MakeMetrics();
        DeskSceneComposition  front;
        DeskSceneComposition  turned;
        DeskSceneView         view;
        RECT                  viewport         = { 0, 0, 1600, 1000 };
        SIZE                  labelPx          = { 120, 16 };
        float                 frontQuad[4][3]  = {};
        float                 turnedQuad[4][3] = {};
        RECT                  frontPx          = {};
        RECT                  turnedPx         = {};



        // Forty degrees of yaw is where the bleed showed, and where the
        // reverted in-scene quad had foreshortened the name away.
        view.orbitYawRad = 0.6981317f;

        Assert::AreEqual (S_OK, DeskSceneLayout::Compute (viewport, 96, 2, metrics, front));
        Assert::AreEqual (S_OK, DeskSceneLayout::Compute (viewport, 96, 2, metrics, turned, 0, view));

        Assert::IsTrue (DeskSceneLayout::TryMakeDriveLabelQuad (front,  0, labelPx, 4, frontQuad));
        Assert::IsTrue (DeskSceneLayout::TryMakeDriveLabelQuad (turned, 0, labelPx, 4, turnedQuad));

        MeasureQuadPx (front,  frontQuad,  frontPx);
        MeasureQuadPx (turned, turnedQuad, turnedPx);

        Assert::AreEqual (frontPx.right - frontPx.left, turnedPx.right - turnedPx.left,
            L"the name must not narrow as the scene turns");
        Assert::AreEqual (frontPx.bottom - frontPx.top, turnedPx.bottom - turnedPx.top,
            L"nor shorten");
    }


    TEST_METHOD (Label_Quad_Keeps_Its_Pixel_Size_At_Any_Zoom)
    {
        DeskSceneMetrics      metrics       = MakeMetrics();
        DeskSceneComposition  comp;
        DeskSceneView         view;
        RECT                  viewport      = { 0, 0, 1600, 1000 };
        SIZE                  labelPx       = { 120, 16 };
        float                 corners[4][3] = {};
        RECT                  quadPx        = {};



        view.zoom = 2.5f;
        view.panY = -0.3f;

        Assert::AreEqual (S_OK, DeskSceneLayout::Compute (viewport, 96, 2, metrics, comp, 0, view));

        Assert::IsTrue (DeskSceneLayout::TryMakeDriveLabelQuad (comp, 0, labelPx, 4, corners));

        MeasureQuadPx (comp, corners, quadPx);

        Assert::AreEqual (labelPx.cx, quadPx.right - quadPx.left,
            L"leaning in must not enlarge the type");
        Assert::AreEqual (labelPx.cy, quadPx.bottom - quadPx.top,
            L"nor stretch it");
    }


    TEST_METHOD (Label_Quad_Corners_Share_The_Anchor_Depth)
    {
        DeskSceneMetrics      metrics       = MakeMetrics();
        DeskSceneComposition  comp;
        DeskSceneView         view;
        RECT                  viewport      = { 0, 0, 1600, 1000 };
        SIZE                  labelPx       = { 200, 20 };
        float                 corners[4][3] = {};
        float                 anchorW       = 0.0f;



        view.orbitYawRad   = 0.6f;
        view.orbitPitchRad = 0.2f;

        Assert::AreEqual (S_OK, DeskSceneLayout::Compute (viewport, 96, 2, metrics, comp, 0, view));

        Assert::IsTrue (DeskSceneLayout::TryMakeDriveLabelQuad (comp, 1, labelPx, 4, corners));

        anchorW = MeasureClipW (comp, comp.driveLabelWorld[1]);

        // ONE depth across the whole quad is what makes the occlusion exact.
        // A quad tilted in depth would be cut by the case along a line that
        // has nothing to do with where the case crosses the name on screen.
        for (int corner = 0; corner < 4; corner++)
        {
            Assert::AreEqual (anchorW, MeasureClipW (comp, corners[corner]), anchorW * 1e-4f,
                L"every corner must sit at the drive's own distance");
        }
    }


    TEST_METHOD (Label_Quad_Refuses_An_Absent_Drive)
    {
        DeskSceneMetrics      metrics       = MakeMetrics();
        DeskSceneComposition  comp;
        RECT                  viewport      = { 0, 0, 1600, 1000 };
        SIZE                  labelPx       = { 120, 16 };
        SIZE                  emptyPx       = { 0, 0 };
        float                 corners[4][3] = {};



        Assert::AreEqual (S_OK, DeskSceneLayout::Compute (viewport, 96, 1, metrics, comp));

        Assert::IsFalse (DeskSceneLayout::TryMakeDriveLabelQuad (comp, 1, labelPx, 4, corners),
            L"a drive the composition never placed has no name to hang");
        Assert::IsFalse (DeskSceneLayout::TryMakeDriveLabelQuad (comp, -1, labelPx, 4, corners));
        Assert::IsFalse (DeskSceneLayout::TryMakeDriveLabelQuad (comp, 0, emptyPx, 4, corners),
            L"an empty measurement is not a quad");
    }

};
