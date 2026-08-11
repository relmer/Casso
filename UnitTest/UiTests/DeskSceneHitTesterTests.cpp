#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Render/SceneCamera.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Ui/Scene/DeskSceneHitTester.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneHitTesterTests
//
//  Screen-ray resolution against the composed scene: glass hits carry the
//  emulated pixel and outrank everything, drives compete on nearest hit with
//  eject-before-body precedence within a drive, and dead space resolves to
//  None. The fixture drives the real DeskSceneLayout composition over the
//  real model dimensions, so the rays exercised here are the shipping
//  geometry's.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeskSceneHitTesterTests)
{
public:

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

    // The Disk II region boxes DeskSceneModel declares (eject wraps
    // slot/door/latch, body wraps the case), in declaration order.
    static std::vector<DeskRegionBox> MakeDriveRegions()
    {
        std::vector<DeskRegionBox>  regions;
        DeskRegionBox               box;

        box.boxMin[0] = 14.0f;   box.boxMin[1] = -5.0f;   box.boxMin[2] = 44.0f;
        box.boxMax[0] = 141.0f;  box.boxMax[1] = 3.0f;    box.boxMax[2] = 63.0f;
        box.region    = DriveWidgetRegion::Eject;
        regions.push_back (box);

        box.boxMin[0] = 0.0f;    box.boxMin[1] = -5.0f;   box.boxMin[2] = 0.0f;
        box.boxMax[0] = 155.0f;  box.boxMax[1] = 222.0f;  box.boxMax[2] = 86.0f;
        box.region    = DriveWidgetRegion::Body;
        regions.push_back (box);

        return regions;
    }

    // Screen position of a model-space point taken through a device's world
    // transform and the composition's camera.
    static void ScreenAt (const DeskSceneComposition & comp,
                          const float                  world[16],
                          float                        mx,
                          float                        my,
                          float                        mz,
                          float                        outScreen[2])
    {
        float   modelPt[3] = { mx, my, mz };
        float   worldPt[3] = {};

        Assert::IsTrue (SceneCamera::TransformPoint (world, modelPt, worldPt));
        Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, worldPt, comp.viewportPx, outScreen));
    }

    TEST_METHOD (Glass_Hit_Carries_The_Emulated_Pixel)
    {
        DeskSceneMetrics      metrics   = MakeMetrics();
        RECT                  vp        = { 0, 0, 1120, 768 };
        DeskSceneComposition  comp;
        float                 screen[2] = {};



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        // The exact center of the glass must resolve to the center pixel.
        {
            CurvedDisplaySurface  glass     = metrics.glass;
            float                 center[3] = {};

            CurvedDisplayMath::ModelPointFromUv (glass, 0.5f, 0.5f, center);
            ScreenAt (comp, comp.monitorWorld, center[0], center[1], center[2], screen);
        }

        {
            SceneHitResult  hit = DeskSceneHitTester::Classify (
                comp, metrics.glass, MakeDriveRegions(), screen[0], screen[1], 560, 384);

            Assert::IsTrue (hit.target == SceneHitResult::Target::Glass);

            // The exact uv 0.5 boundary can quantize to either neighbor
            // under float round trip; one pixel of slack is the spec's bar.
            Assert::IsTrue (std::abs (hit.emulatedPixel.x - 280L) <= 1);
            Assert::IsTrue (std::abs (hit.emulatedPixel.y - 192L) <= 1);
        }
    }

    TEST_METHOD (Slot_Region_Resolves_To_Eject_Before_Body)
    {
        DeskSceneMetrics      metrics   = MakeMetrics();
        RECT                  vp        = { 0, 0, 1120, 768 };
        DeskSceneComposition  comp;
        float                 screen[2] = {};



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        // Center of drive 0's slot area, on the front face.
        ScreenAt (comp, comp.driveWorld[0], 77.5f, -5.0f, 53.0f, screen);

        {
            SceneHitResult  hit = DeskSceneHitTester::Classify (
                comp, metrics.glass, MakeDriveRegions(), screen[0], screen[1], 560, 384);

            Assert::IsTrue   (hit.target == SceneHitResult::Target::Drive);
            Assert::AreEqual (0, hit.driveIndex);
            Assert::IsTrue   (hit.region == DriveWidgetRegion::Eject);
        }
    }

    TEST_METHOD (Body_Clicks_Resolve_Per_Drive)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        RECT                  vp      = { 0, 0, 1120, 768 };
        DeskSceneComposition  comp;



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        for (int drive = 0; drive < 2; drive++)
        {
            float           screen[2] = {};
            SceneHitResult  hit;

            // Low on the front face, outside the eject box.
            ScreenAt (comp, comp.driveWorld[drive], 77.5f, -5.0f, 20.0f, screen);

            hit = DeskSceneHitTester::Classify (
                comp, metrics.glass, MakeDriveRegions(), screen[0], screen[1], 560, 384);

            Assert::IsTrue   (hit.target == SceneHitResult::Target::Drive);
            Assert::AreEqual (drive, hit.driveIndex);
            Assert::IsTrue   (hit.region == DriveWidgetRegion::Body);
        }
    }

    TEST_METHOD (Dead_Space_Resolves_To_None)
    {
        DeskSceneMetrics      metrics = MakeMetrics();
        RECT                  vp      = { 0, 0, 1120, 768 };
        DeskSceneComposition  comp;



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        {
            SceneHitResult  hit = DeskSceneHitTester::Classify (
                comp, metrics.glass, MakeDriveRegions(), 3.0f, 3.0f, 560, 384);

            Assert::IsTrue (hit.target == SceneHitResult::Target::None);
            Assert::AreEqual (-1, hit.driveIndex);
        }
    }

    TEST_METHOD (No_Drives_Means_Only_Glass_Or_None)
    {
        DeskSceneMetrics      metrics   = MakeMetrics();
        RECT                  vp        = { 0, 0, 1120, 768 };
        DeskSceneComposition  comp;
        float                 screen[2] = {};



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 0, metrics, comp));

        // Where a drive WOULD be, nothing resolves.
        ScreenAt (comp, comp.monitorWorld, 124.0f, -100.0f, -60.0f, screen);

        {
            SceneHitResult  hit = DeskSceneHitTester::Classify (
                comp, metrics.glass, MakeDriveRegions(), screen[0], screen[1], 560, 384);

            Assert::IsTrue (hit.target == SceneHitResult::Target::None);
        }
    }

};
