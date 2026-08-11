#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Ui/Chrome/DriveWidget.h"
#include "Ui/Scene/DeskSceneModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModelTests
//
//  Model load and discovery over synthetic OBJ/MTL text: sub-mesh split by
//  material color, glass surface derivation, UV synthesis exactness, lamp
//  discovery, and region box order. The glass fixture is generated with the
//  SAME spherical-sag formula as scripts/modelgen/gen_monitor2c.py (rect,
//  front plane, radius = 2.2 x half-diagonal), so the derivation is exercised
//  against the shipping geometry's shape, just at test-sized resolution --
//  UnitTest carries no resources by design, and the real embedded model goes
//  through this exact code path in the app.
//
////////////////////////////////////////////////////////////////////////////////

// The synthetic monitor glass: the generator's rect and radius.
static constexpr float   s_kGlassX0    = 29.0f;
static constexpr float   s_kGlassX1    = 219.0f;
static constexpr float   s_kGlassZ0    = 77.0f;
static constexpr float   s_kGlassZ1    = 197.0f;
static constexpr float   s_kGlassBaseY = 6.0f;


TEST_CLASS (DeskSceneModelTests)
{
public:

    static float GlassRadius()
    {
        float   halfW = (s_kGlassX1 - s_kGlassX0) * 0.5f;
        float   halfH = (s_kGlassZ1 - s_kGlassZ0) * 0.5f;

        return 2.2f * std::sqrt (halfW * halfW + halfH * halfH);
    }

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

        for (int i = 0; i < 8; i++)
        {
            sprintf_s (line, "v %g %g %g\n", xs[(i >> 0) & 1], ys[(i >> 1) & 1], zs[(i >> 2) & 1]);
            obj += line;
        }

        sprintf_s (line, "usemtl %s\n", mtl);
        obj += line;

        {
            int   quads[6][4] = { { 0, 1, 3, 2 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 },
                                  { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 3, 7, 5 } };

            for (int q = 0; q < 6; q++)
            {
                sprintf_s (line, "f %d %d %d %d\n",
                           b + quads[q][0] + 1, b + quads[q][1] + 1,
                           b + quads[q][2] + 1, b + quads[q][3] + 1);
                obj += line;
            }
        }

        vertexBase += 8;
    }

    // Emits the spherical-sag glass grid with the generator's exact formula.
    static void AppendGlassGrid (std::string & obj, int & vertexBase, int cols, int rows)
    {
        char    line[128] = {};
        float   cx        = (s_kGlassX0 + s_kGlassX1) * 0.5f;
        float   cz        = (s_kGlassZ0 + s_kGlassZ1) * 0.5f;
        float   radius    = GlassRadius();
        float   halfW     = (s_kGlassX1 - s_kGlassX0) * 0.5f;
        float   halfH     = (s_kGlassZ1 - s_kGlassZ0) * 0.5f;
        float   halfDiag  = std::sqrt (halfW * halfW + halfH * halfH);
        float   maxSag    = radius - std::sqrt (radius * radius - halfDiag * halfDiag);
        int     b         = vertexBase;

        for (int r = 0; r <= rows; r++)
        {
            for (int c = 0; c <= cols; c++)
            {
                float   x   = s_kGlassX0 + (s_kGlassX1 - s_kGlassX0) * (float) c / (float) cols;
                float   z   = s_kGlassZ0 + (s_kGlassZ1 - s_kGlassZ0) * (float) r / (float) rows;
                float   rr  = std::sqrt ((x - cx) * (x - cx) + (z - cz) * (z - cz));
                float   sag = radius - std::sqrt (radius * radius - rr * rr);
                float   y   = s_kGlassBaseY - (maxSag - sag);

                sprintf_s (line, "v %g %g %g\n", x, y, z);
                obj += line;
            }
        }

        obj += "usemtl glassM\n";

        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                int   v00 = b + r * (cols + 1) + c + 1;
                int   v01 = v00 + 1;
                int   v10 = v00 + (cols + 1);
                int   v11 = v10 + 1;

                sprintf_s (line, "f %d %d %d %d\n", v00, v01, v11, v10);
                obj += line;
            }
        }

        vertexBase += (rows + 1) * (cols + 1);
    }

    static std::string Mtl()
    {
        return
            "newmtl caseM\nKd 0.833 0.784 0.659\n"
            "newmtl glassM\nKd 0.05 0.09 0.07\n"
            "newmtl monLampM\nKd 0.29 0.87 0.38\n"
            "newmtl ledM\nKd 0.90 0.12 0.10\n";
    }

    static std::string MonitorObj (bool withGlass = true)
    {
        std::string   obj;
        int           base = 0;

        AppendBox (obj, base, 0.0f, 0.0f, 0.0f, 248.0f, 280.0f, 226.0f, "caseM");
        AppendBox (obj, base, 210.0f, -5.0f, 45.0f, 218.0f, -4.0f, 52.0f, "monLampM");

        if (withGlass)
        {
            AppendGlassGrid (obj, base, 8, 6);
        }

        return obj;
    }

    static std::string DriveObj()
    {
        std::string   obj;
        int           base = 0;

        AppendBox (obj, base, 0.0f, 0.0f, 0.0f, 155.0f, 222.0f, 86.0f, "caseM");
        AppendBox (obj, base, 21.0f, -2.6f, 13.0f, 27.0f, -0.6f, 19.0f, "ledM");

        return obj;
    }

    TEST_METHOD (Monitor_Splits_Glass_And_Derives_The_Surface)
    {
        DeskSceneModel   model;



        AssertSucceeded (model.Load (DeskDeviceKind::Monitor2c, MonitorObj(), Mtl()));
        Assert::IsTrue  (model.HasGlass());

        {
            const CurvedDisplaySurface &  surface = model.Surface();

            Assert::AreEqual (s_kGlassX0, surface.x0, 0.01f);
            Assert::AreEqual (s_kGlassX1, surface.x1, 0.01f);
            Assert::AreEqual (s_kGlassZ0, surface.z0, 0.01f);
            Assert::AreEqual (s_kGlassZ1, surface.z1, 0.01f);

            // The loader lifts the glass (verts + surface together) clear of
            // the coplanar cavity front.
            Assert::AreEqual (s_kGlassBaseY - DeskSceneModel::kGlassLiftMm, surface.baseY, 0.01f);

            // The radius is DERIVED from the measured sag; it must land on
            // the generator's parameter within grid-resolution error.
            Assert::AreEqual (GlassRadius(), surface.radius, GlassRadius() * 0.01f);
            Assert::IsTrue   (CurvedDisplayMath::IsValid (surface));
        }
    }

    TEST_METHOD (Glass_Uvs_Are_Exact_At_The_Rect_Corners)
    {
        DeskSceneModel   model;



        AssertSucceeded (model.Load (DeskDeviceKind::Monitor2c, MonitorObj(), Mtl()));

        for (const Dxui3DRenderer::Vertex & v : model.GlassVerts())
        {
            if (std::abs (v.x - s_kGlassX0) < 0.01f && std::abs (v.z - s_kGlassZ1) < 0.01f)
            {
                Assert::AreEqual (0.0f, v.u, 1e-4f);
                Assert::AreEqual (0.0f, v.v, 1e-4f);
            }

            if (std::abs (v.x - s_kGlassX1) < 0.01f && std::abs (v.z - s_kGlassZ0) < 0.01f)
            {
                Assert::AreEqual (1.0f, v.u, 1e-4f);
                Assert::AreEqual (1.0f, v.v, 1e-4f);
            }
        }
    }

    TEST_METHOD (Glass_Tint_Is_White_So_The_Picture_Passes_Through)
    {
        DeskSceneModel   model;



        AssertSucceeded (model.Load (DeskDeviceKind::Monitor2c, MonitorObj(), Mtl()));
        Assert::IsFalse (model.GlassVerts().empty());

        for (const Dxui3DRenderer::Vertex & v : model.GlassVerts())
        {
            Assert::AreEqual (1.0f, v.r);
            Assert::AreEqual (1.0f, v.g);
            Assert::AreEqual (1.0f, v.b);
            Assert::AreEqual (1.0f, v.a);
        }
    }

    TEST_METHOD (Monitor_Missing_Glass_Is_A_Broken_Asset)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;
        DeskSceneModel                      model;



        // Two guards fire: BuildGlassSurface's empty-glass check and Load's
        // CHRA on the propagated failure.
        AssertFailed (model.Load (DeskDeviceKind::Monitor2c, MonitorObj (false), Mtl()));
        expect.RequireCount (2);
    }

    TEST_METHOD (Drive_Finds_Its_Lamp_And_Orders_Regions_Eject_First)
    {
        DeskSceneModel   model;



        AssertSucceeded (model.Load (DeskDeviceKind::DiskII, DriveObj(), Mtl()));
        Assert::IsFalse (model.HasGlass());
        Assert::IsFalse (model.LampVerts().empty());
        Assert::AreEqual ((size_t) 1, model.Lamps().size());

        // The lamp anchor is the LED box's center.
        Assert::AreEqual (24.0f, model.Lamps()[0].center[0], 0.01f);
        Assert::AreEqual (16.0f, model.Lamps()[0].center[2], 0.01f);

        Assert::AreEqual ((size_t) 2, model.RegionBoxes().size());
        Assert::IsTrue (model.RegionBoxes()[0].region == DriveWidgetRegion::Eject);
        Assert::IsTrue (model.RegionBoxes()[1].region == DriveWidgetRegion::Body);

        // The eject box is contained inside the body box -- precedence by
        // declaration order, like the 2D widget's eject-inside-body rects.
        for (int axis = 0; axis < 3; axis++)
        {
            Assert::IsTrue (model.RegionBoxes()[0].boxMin[axis] >= model.RegionBoxes()[1].boxMin[axis]);
            Assert::IsTrue (model.RegionBoxes()[0].boxMax[axis] <= model.RegionBoxes()[1].boxMax[axis]);
        }
    }

    TEST_METHOD (Opaque_Verts_Carry_Baked_Shading)
    {
        DeskSceneModel   model;
        bool             sawVariation = false;
        float            firstR       = -1.0f;



        AssertSucceeded (model.Load (DeskDeviceKind::DiskII, DriveObj(), Mtl()));
        Assert::IsFalse (model.OpaqueVerts().empty());

        for (const Dxui3DRenderer::Vertex & v : model.OpaqueVerts())
        {
            // Shade range: floor 0.30 to full 1.0 of the material color.
            Assert::IsTrue (v.r >= 0.833f * 0.30f - 1e-4f && v.r <= 0.833f + 1e-4f);
            Assert::AreEqual (1.0f, v.a);

            if (firstR < 0.0f)
            {
                firstR = v.r;
            }
            else if (std::abs (v.r - firstR) > 1e-3f)
            {
                sawVariation = true;
            }
        }

        // Different face orientations get different Lambert shades.
        Assert::IsTrue (sawVariation);
    }

};
