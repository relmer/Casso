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
        // symmetrically (translation lives in row 3 of the world matrix).
        {
            float   driveCx = (metrics.driveMin[0] + metrics.driveMax[0]) * 0.5f;

            Assert::AreEqual (-driveCx, one.driveWorld[0][12], 0.01f);
            Assert::IsTrue   (two.driveWorld[0][12] < two.driveWorld[1][12]);
            Assert::AreEqual (-(two.driveWorld[0][12] + driveCx), two.driveWorld[1][12] + driveCx, 0.01f);
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

};
