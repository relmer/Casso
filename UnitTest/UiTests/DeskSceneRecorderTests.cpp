#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Devices/Printer/MeshBlob.h"
#include "Devices/Printer/MeshNormals.h"
#include "Devices/Printer/ObjMeshParser.h"
#include "Render/SceneCamera.h"
#include "Ui/Chrome/DriveWidget.h"
#include "Ui/Scene/DeskScene.h"
#include "Ui/Scene/DeskSceneHitTester.h"
#include "Ui/Scene/DeskSceneLayout.h"
#include "Ui/Scene/DeskSceneModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneRecorderTests
//
//  The cassette recorder beside the drive stack: its model loads with no
//  lamp, door or glass; the layout sets it on the desk to the right of the
//  stack without overlapping it and keeps it inside the framed view; a click
//  on it resolves to the recorder; and a scene loaded without it (a machine
//  with no cassette jacks) composes with no recorder at all.
//
////////////////////////////////////////////////////////////////////////////////

// The RQ-309DS case, model mm: X right, Y back, Z up.
static constexpr float   s_kRecW = 140.0f;
static constexpr float   s_kRecD = 260.0f;
static constexpr float   s_kRecH = 70.0f;





TEST_CLASS (DeskSceneRecorderTests)
{
public:

    // Emits one axis-aligned box (8 vertices, 6 quad faces) under `mtl`.
    static void AppendBox (std::string & obj, int & vertexBase,
                           float x0, float y0, float z0,
                           float x1, float y1, float z1,
                           const char * mtl)
    {
        char   line[128] = {};
        float  xs[2]     = { x0, x1 };
        float  ys[2]     = { y0, y1 };
        float  zs[2]     = { z0, z1 };
        int    b         = vertexBase;
        int    quads[6][4] = { { 0, 1, 3, 2 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 },
                               { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 3, 7, 5 } };



        for (int i = 0; i < 8; i++)
        {
            sprintf_s (line, "v %g %g %g\n", xs[(i >> 0) & 1], ys[(i >> 1) & 1], zs[(i >> 2) & 1]);
            obj += line;
        }

        sprintf_s (line, "usemtl %s\n", mtl);
        obj += line;

        for (int q = 0; q < 6; q++)
        {
            sprintf_s (line, "f %d %d %d %d\n",
                       b + quads[q][0] + 1, b + quads[q][1] + 1,
                       b + quads[q][2] + 1, b + quads[q][3] + 1);
            obj += line;
        }

        vertexBase += 8;
    }

    // A shallow spherical-sag glass sheet, enough for the monitor to load.
    static void AppendGlass (std::string & obj, int & vertexBase)
    {
        constexpr int    kCols     = 4;
        constexpr int    kRows     = 3;
        constexpr float  kX0       = 29.0f;
        constexpr float  kX1       = 219.0f;
        constexpr float  kZ0       = 77.0f;
        constexpr float  kZ1       = 197.0f;
        constexpr float  kBaseY    = 6.0f;
        char             line[128] = {};
        float            cx        = (kX0 + kX1) * 0.5f;
        float            cz        = (kZ0 + kZ1) * 0.5f;
        float            halfW     = (kX1 - kX0) * 0.5f;
        float            halfH     = (kZ1 - kZ0) * 0.5f;
        float            radius    = 3.0f * std::sqrt (halfW * halfW + halfH * halfH);
        float            maxSag    = radius - std::sqrt (radius * radius - (halfW * halfW + halfH * halfH));
        int              b         = vertexBase;



        for (int r = 0; r <= kRows; r++)
        {
            for (int c = 0; c <= kCols; c++)
            {
                float  x   = kX0 + (kX1 - kX0) * (float) c / (float) kCols;
                float  z   = kZ0 + (kZ1 - kZ0) * (float) r / (float) kRows;
                float  rr  = (x - cx) * (x - cx) + (z - cz) * (z - cz);
                float  sag = radius - std::sqrt (radius * radius - rr);

                sprintf_s (line, "v %g %g %g\n", x, kBaseY - (maxSag - sag), z);
                obj += line;
            }
        }

        obj += "usemtl glass\n";

        for (int r = 0; r < kRows; r++)
        {
            for (int c = 0; c < kCols; c++)
            {
                int  v00 = b + r * (kCols + 1) + c + 1;

                sprintf_s (line, "f %d %d %d %d\n", v00, v00 + 1, v00 + kCols + 2, v00 + kCols + 1);
                obj += line;
            }
        }

        vertexBase += (kRows + 1) * (kCols + 1);
    }

    static std::string Mtl()
    {
        return
            "newmtl case\nKd 0.833 0.784 0.659\n"
            "newmtl glass\nKd 0.05 0.09 0.07\n"
            "newmtl lamp\nKd 0.29 0.87 0.38\n"
            "newmtl led\nKd 0.90 0.12 0.10\n"
            "newmtl door\nKd 0.16 0.16 0.18\n"
            "newmtl body\nKd 0.09 0.10 0.15\n"
            "newmtl deck\nKd 0.68 0.685 0.69\n"
            "newmtl window\nKd 0.075 0.095 0.12\n"
            "newmtl keys_0\nKd 0.90 0.88 0.72\n";
    }

    static std::string RecorderObj()
    {
        std::string  obj;
        int          base = 0;

        AppendBox (obj, base, 0.0f, 0.0f, 0.0f, s_kRecW, s_kRecD, s_kRecH, "body");
        AppendBox (obj, base, 6.0f, 30.0f, s_kRecH, s_kRecW - 6.0f, s_kRecD - 3.0f, s_kRecH + 1.2f, "deck");
        AppendBox (obj, base, 14.0f, 105.0f, s_kRecH + 1.2f, 126.0f, 150.0f, s_kRecH + 1.9f, "window");
        AppendBox (obj, base, 10.0f, 26.0f, s_kRecH - 12.0f, 28.0f, 80.0f, s_kRecH + 6.2f, "keys_0");

        return obj;
    }

    static std::string MonitorObj()
    {
        std::string  obj;
        int          base = 0;

        AppendBox (obj, base, 0.0f, 0.0f, 0.0f, 343.0f, 348.0f, 292.0f, "case");
        AppendBox (obj, base, 300.0f, -2.0f, 40.0f, 306.0f, -1.0f, 46.0f, "lamp");
        AppendGlass (obj, base);

        return obj;
    }

    static std::string DriveObj()
    {
        std::string  obj;
        int          base = 0;

        AppendBox (obj, base, 0.0f, 0.0f, 0.0f, 153.0f, 217.0f, 90.0f, "case");
        AppendBox (obj, base, 30.0f, -2.6f, 24.0f, 36.0f, -1.0f, 30.0f, "led");
        AppendBox (obj, base, 56.0f, -3.0f, 44.0f, 96.0f, -1.0f, 78.0f, "door");

        return obj;
    }

    // OBJ text baked into the blob form DeskSceneModel loads, the same pack
    // the build performs on the shipping models.
    static void Bake (const std::string & obj, std::vector<uint8_t> & outBlob)
    {
        std::vector<ObjTriangle>           triangles;
        std::vector<std::string>           names;
        std::vector<std::array<float, 3>>  normals;



        AssertSucceeded (ObjMeshParser::Parse (obj, Mtl(), triangles, names));
        MeshNormals::Compute (triangles, MeshNormals::kDefaultSmoothingDeg, normals);
        AssertSucceeded (MeshBlob::Write (triangles, names, normals, outBlob));
    }

    // Monitor II and Disk II proportions, plus the recorder.
    static DeskSceneMetrics MakeMetrics (bool withRecorder)
    {
        DeskSceneMetrics  metrics;

        metrics.monitorMin[0] = 0.0f;    metrics.monitorMax[0] = 343.0f;
        metrics.monitorMin[1] = -5.0f;   metrics.monitorMax[1] = 348.0f;
        metrics.monitorMin[2] = 0.0f;    metrics.monitorMax[2] = 292.0f;

        metrics.driveMin[0]   = 0.0f;    metrics.driveMax[0]   = 155.0f;
        metrics.driveMin[1]   = -5.0f;   metrics.driveMax[1]   = 222.0f;
        metrics.driveMin[2]   = 0.0f;    metrics.driveMax[2]   = 90.0f;

        metrics.glass.x0      = 60.0f;   metrics.glass.x1      = 280.0f;
        metrics.glass.z0      = 90.0f;   metrics.glass.z1      = 250.0f;
        metrics.glass.baseY   = 6.0f;
        metrics.glass.radius  = 3.0f * std::sqrt (110.0f * 110.0f + 80.0f * 80.0f);

        metrics.drivePadSideMm  = 9.0f;
        metrics.drivePadDepthMm = 34.0f;

        metrics.hasRecorder = withRecorder;

        metrics.recorderMin[0] = 0.0f;    metrics.recorderMax[0] = s_kRecW;
        metrics.recorderMin[1] = -7.0f;   metrics.recorderMax[1] = s_kRecD;
        metrics.recorderMin[2] = 0.0f;    metrics.recorderMax[2] = s_kRecH + 6.0f;

        metrics.recorderPadSideMm  = 9.0f;
        metrics.recorderPadDepthMm = 34.0f;

        return metrics;
    }

    // A model box's eight corners through `world`, as a world-space box.
    static void WorldBox (const float world[16], const float mn[3], const float mx[3],
                          float outLo[3], float outHi[3])
    {
        outLo[0] = outLo[1] = outLo[2] =  FLT_MAX;
        outHi[0] = outHi[1] = outHi[2] = -FLT_MAX;

        for (int c = 0; c < 8; c++)
        {
            float  pt[3]  = { (c & 1) ? mx[0] : mn[0], (c & 2) ? mx[1] : mn[1], (c & 4) ? mx[2] : mn[2] };
            float  out[3] = {};

            Assert::IsTrue (SceneCamera::TransformPoint (world, pt, out));

            for (int a = 0; a < 3; a++)
            {
                outLo[a] = std::min (outLo[a], out[a]);
                outHi[a] = std::max (outHi[a], out[a]);
            }
        }
    }

    //
    //  The recorder has no lamp, no door and no glass, and must load anyway:
    //  the drive and monitor guards that reject a model missing those parts
    //  are about those devices, not this one.
    //
    TEST_METHOD (Recorder_Keys_Are_Kept_Apart_With_Their_Boxes)
    {
        DeskSceneModel        model;
        std::vector<uint8_t>  blob;
        const float *         box = nullptr;



        Bake (RecorderObj(), blob);
        AssertSucceeded (model.Load (DeskDeviceKind::CassetteRecorder, blob));

        Assert::IsFalse (model.KeyVerts (0).empty(), L"the key is its own geometry, so it can be pressed");
        Assert::IsTrue  (model.KeyVerts (1).empty());

        box = model.KeyBoxes();
        Assert::AreEqual (10.0f,           box[0], 0.01f);
        Assert::AreEqual (26.0f,           box[1], 0.01f);
        Assert::AreEqual (28.0f,           box[3], 0.01f);
        Assert::AreEqual (s_kRecH + 6.2f,  box[5], 0.01f);
    }


    TEST_METHOD (Recorder_Loads_Without_Lamp_Door_Or_Glass)
    {
        DeskSceneModel        model;
        std::vector<uint8_t>  blob;
        float                 lo[3] = {};
        float                 hi[3] = {};



        Bake (RecorderObj(), blob);
        AssertSucceeded (model.Load (DeskDeviceKind::CassetteRecorder, blob));

        Assert::IsTrue (model.Kind() == DeskDeviceKind::CassetteRecorder);
        Assert::IsFalse (model.OpaqueVerts().empty());
        Assert::IsTrue (model.LampVerts().empty());
        Assert::IsTrue (model.Lamps().empty());
        Assert::IsTrue (model.DoorVerts().empty());
        Assert::IsFalse (model.HasGlass());
        Assert::IsTrue (model.RegionBoxes().empty());

        model.BoundsMin (lo);
        model.BoundsMax (hi);

        Assert::AreEqual (0.0f,    lo[0], 0.01f);
        Assert::AreEqual (s_kRecW, hi[0], 0.01f);
        Assert::AreEqual (s_kRecD, hi[1], 0.01f);
        Assert::AreEqual (0.0f,    lo[2], 0.01f);
    }

    //
    //  The relaxation is for the recorder only: the same lampless geometry
    //  loaded as a drive is still a broken drive.
    //
    TEST_METHOD (Lampless_Geometry_Still_Fails_As_A_Drive)
    {
        DeskSceneModel        model;
        std::vector<uint8_t>  blob;
        HRESULT               hr    = S_OK;



        Bake (RecorderObj(), blob);

        {
            UnitTestHelpers::ExpectedEhmAssert  expect;

            hr = model.Load (DeskDeviceKind::DiskII, blob);
        }

        AssertFailed (hr);
    }

    //
    //  None of the recorder's colors may be read as a finish marker: within
    //  the loader's tolerance of the pebbled or recessed plate, a part loses
    //  its own color and turns black.
    //
    TEST_METHOD (Recorder_Keeps_Its_Own_Colors)
    {
        DeskSceneModel        model;
        std::vector<uint8_t>  blob;
        bool                  sawWindow = false;



        Bake (RecorderObj(), blob);
        AssertSucceeded (model.Load (DeskDeviceKind::CassetteRecorder, blob));

        for (const Dxui3DRenderer::Vertex & v : model.OpaqueVerts())
        {
            Assert::AreEqual (0.0f, v.pebble);

            if (std::abs (v.b - 0.12f) < 0.001f)
            {
                sawWindow = true;
            }
        }

        Assert::IsTrue (sawWindow);
    }

    //
    //  BESIDE THE STACK, NOT IN IT. The recorder's world box starts to the
    //  right of every drive and of the monitor, rests on the desk, and stays
    //  inside the projected scene the camera was fitted to.
    //
    TEST_METHOD (Layout_Places_Recorder_Beside_The_Stack)
    {
        DeskSceneMetrics      metrics  = MakeMetrics (true);
        RECT                  vp       = { 0, 0, 1280, 800 };
        DeskSceneComposition  comp;
        float                 recLo[3] = {};
        float                 recHi[3] = {};
        float                 boxLo[3] = {};
        float                 boxHi[3] = {};



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        Assert::AreEqual (1, comp.hasRecorder);

        WorldBox (comp.recorderWorld, metrics.recorderMin, metrics.recorderMax, recLo, recHi);

        for (int drive = 0; drive < comp.driveCount; drive++)
        {
            WorldBox (comp.driveWorld[drive], metrics.driveMin, metrics.driveMax, boxLo, boxHi);
            Assert::IsTrue (recLo[0] > boxHi[0], L"recorder overlaps a drive");
        }

        WorldBox (comp.monitorWorld, metrics.monitorMin, metrics.monitorMax, boxLo, boxHi);
        Assert::IsTrue (recLo[0] > boxHi[0], L"recorder overlaps the monitor");

        // On the desk, like the drives.
        Assert::AreEqual (0.0f, recLo[1], 0.01f);

        // Projected, and inside what the camera frames.
        Assert::IsTrue (comp.recorderRectPx.right > comp.recorderRectPx.left);
        Assert::IsTrue (comp.recorderRectPx.left   >= vp.left);
        Assert::IsTrue (comp.recorderRectPx.right  <= vp.right);
        Assert::IsTrue (comp.recorderRectPx.top    >= vp.top);
        Assert::IsTrue (comp.recorderRectPx.bottom <= vp.bottom);
        Assert::IsTrue (comp.recorderRectPx.right  <= comp.sceneRectPx.right);

        // On screen it reads to the right of the right-hand drive. Not by
        // bounds: the recorder is deeper than a drive and its far corner
        // projects in toward the center past the drive's near edge.
        Assert::IsTrue (comp.recorderRectPx.left + comp.recorderRectPx.right >
                        comp.driveRectPx[1].left + comp.driveRectPx[1].right);
    }

    //
    //  THE FULLSCREEN STRIP BRINGS THE RECORDER BACK WITH THE DRIVES: it sits
    //  to the right of them, inside the band, and the row is centered as a
    //  whole. Without one the strip is the drives alone, as before.
    //
    TEST_METHOD (Strip_Carries_The_Recorder_Beside_The_Drives)
    {
        DeskSceneMetrics      metrics = MakeMetrics (true);
        RECT                  vp      = { 0, 0, 2400, 300 };
        DeskSceneComposition  comp;
        DeskSceneComposition  bare;
        LONG                  left    = 0;
        LONG                  right   = 0;



        AssertSucceeded (DeskSceneLayout::ComputeStrip (vp, 96, 2, metrics, comp,
                                                        DeskSceneLayout::kDriveBandGazeDownRad));

        Assert::AreEqual (1, comp.hasRecorder);
        Assert::IsTrue (comp.recorderRectPx.right > comp.recorderRectPx.left);
        Assert::IsTrue (comp.recorderRectPx.left   >= vp.left);
        Assert::IsTrue (comp.recorderRectPx.right  <= vp.right);
        Assert::IsTrue (comp.recorderRectPx.top    >= vp.top);
        Assert::IsTrue (comp.recorderRectPx.bottom <= vp.bottom);
        Assert::IsTrue (comp.recorderRectPx.left + comp.recorderRectPx.right >
                        comp.driveRectPx[1].left + comp.driveRectPx[1].right);

        // Centered as one group: the margins either side are close.
        left  = comp.driveRectPx[0].left - vp.left;
        right = vp.right - comp.recorderRectPx.right;
        Assert::IsTrue (std::abs (left - right) < (vp.right - vp.left) / 20);

        metrics.hasRecorder = false;
        AssertSucceeded (DeskSceneLayout::ComputeStrip (vp, 96, 2, metrics, bare,
                                                        DeskSceneLayout::kDriveBandGazeDownRad));
        Assert::AreEqual (0, bare.hasRecorder);
    }

    //
    //  The camera centers on everything on the desk, not on the monitor: with
    //  the recorder beside the stack, the group sits in the middle of the
    //  frame rather than off to the left of it.
    //
    TEST_METHOD (Layout_Centers_The_Whole_Group_Recorder_Included)
    {
        DeskSceneMetrics      metrics = MakeMetrics (true);
        RECT                  vp      = { 0, 0, 1280, 800 };
        DeskSceneComposition  comp;
        float                 middle  = 0.0f;
        float                 offset  = 0.0f;



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        middle = (comp.sceneRectPx.left + comp.sceneRectPx.right) * 0.5f;
        offset = std::abs (middle - (vp.left + vp.right) * 0.5f);

        Assert::IsTrue (offset < (vp.right - vp.left) * 0.05f, L"the framed group is off center");
    }

    //
    //  A click on the recorder resolves to the recorder; the same click with
    //  no recorder bounds passed in finds nothing there.
    //
    TEST_METHOD (Hit_On_Recorder_Returns_Recorder)
    {
        DeskSceneMetrics            metrics    = MakeMetrics (true);
        RECT                        vp         = { 0, 0, 1280, 800 };
        DeskSceneComposition        comp;
        std::vector<DeskRegionBox>  regions;
        float                       modelPt[3] = { s_kRecW * 0.5f, s_kRecD * 0.3f, s_kRecH };
        float                       worldPt[3] = {};
        float                       screen[2]  = {};



        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, metrics, comp));

        Assert::IsTrue (SceneCamera::TransformPoint (comp.recorderWorld, modelPt, worldPt));
        Assert::IsTrue (SceneCamera::ProjectToScreen (comp.viewProj, worldPt, comp.viewportPx, screen));

        {
            SceneHitResult  hit = DeskSceneHitTester::Classify (
                comp, metrics.glass, regions, screen[0], screen[1], 560, 384,
                true, nullptr, nullptr, metrics.monitorMin, metrics.monitorMax,
                metrics.driveMin, metrics.driveMax, nullptr,
                metrics.recorderMin, metrics.recorderMax);

            Assert::IsTrue (hit.target == SceneHitResult::Target::Recorder);
            Assert::AreEqual (-1, hit.driveIndex);
        }

        {
            SceneHitResult  hit = DeskSceneHitTester::Classify (
                comp, metrics.glass, regions, screen[0], screen[1], 560, 384,
                true, nullptr, nullptr, metrics.monitorMin, metrics.monitorMax,
                metrics.driveMin, metrics.driveMax);

            Assert::IsTrue (hit.target == SceneHitResult::Target::None);
        }
    }

    //
    //  NO JACKS, NO RECORDER. A scene loaded without the recorder's mesh --
    //  what the shell does for the //c -- reports none, hands the layout
    //  none, and the composition places none.
    //
    TEST_METHOD (No_Recorder_Without_A_Cassette_Port)
    {
        DeskScene             scene;
        std::vector<uint8_t>  monitor;
        std::vector<uint8_t>  drive;
        std::vector<uint8_t>  recorder;
        RECT                  vp = { 0, 0, 1280, 800 };
        DeskSceneComposition  comp;



        Bake (MonitorObj(),  monitor);
        Bake (DriveObj(),    drive);
        Bake (RecorderObj(), recorder);

        AssertSucceeded (scene.LoadModels (DeskDeviceKind::Monitor2, monitor, drive, recorder));
        Assert::IsTrue (scene.HasRecorder());
        Assert::IsTrue (scene.Metrics().hasRecorder);

        AssertSucceeded (scene.LoadModels (DeskDeviceKind::Monitor2, monitor, drive));
        Assert::IsFalse (scene.HasRecorder());
        Assert::IsFalse (scene.Metrics().hasRecorder);

        AssertSucceeded (DeskSceneLayout::Compute (vp, 96, 2, scene.Metrics(), comp));
        Assert::AreEqual (0, comp.hasRecorder);
        Assert::IsTrue (IsRectEmpty (&comp.recorderRectPx) != FALSE);
    }
};
