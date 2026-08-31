#include "Pch.h"

#include "Devices/Printer/MeshNormals.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Vertex-normal averaging, which is what lets a coarse mesh shade as the
// curved surface it approximates. The rule has exactly three jobs: fold
// gentle neighbors together, leave sharp ones alone, and never reach across
// a material boundary.
namespace MeshNormalsTests
{
    // One triangle from three corners, all on the same material.
    static ObjTriangle Tri (float a[3], float b[3], float c[3], int material)
    {
        ObjTriangle   tri;

        memcpy (tri.p0, a, sizeof (float) * 3);
        memcpy (tri.p1, b, sizeof (float) * 3);
        memcpy (tri.p2, c, sizeof (float) * 3);
        tri.material = material;

        return tri;
    }


    // A hinge: two quads meeting along the y axis at `foldDeg` from flat.
    // Four triangles, so both sides of the shared edge have real area.
    static std::vector<ObjTriangle> Hinge (float foldDeg, int materialA, int materialB)
    {
        std::vector<ObjTriangle>   tris;
        float                      rad = foldDeg * 3.14159265358979323846f / 180.0f;

        float   x  = std::cos (rad);
        float   z  = std::sin (rad);

        float   l0[3] = { -1.0f, 0.0f, 0.0f };
        float   l1[3] = { -1.0f, 1.0f, 0.0f };
        float   m0[3] = {  0.0f, 0.0f, 0.0f };
        float   m1[3] = {  0.0f, 1.0f, 0.0f };
        float   r0[3] = {     x, 0.0f,    z };
        float   r1[3] = {     x, 1.0f,    z };

        tris.push_back (Tri (l0, m0, m1, materialA));
        tris.push_back (Tri (l0, m1, l1, materialA));
        tris.push_back (Tri (m0, r0, r1, materialB));
        tris.push_back (Tri (m0, r1, m1, materialB));

        return tris;
    }


    static float Dot (const std::array<float, 3> & a, const std::array<float, 3> & b)
    {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }


    TEST_CLASS (MeshNormalsTests)
    {
    public:

        TEST_METHOD (ProducesThreeNormalsPerTriangle)
        {
            std::vector<ObjTriangle>            tris = Hinge (0.0f, 0, 0);
            std::vector<std::array<float, 3>>   normals;



            MeshNormals::Compute (tris, MeshNormals::kDefaultSmoothingDeg, normals);

            Assert::AreEqual (tris.size() * 3, normals.size());
        }


        // A flat sheet is one surface: every corner ends up with the sheet's
        // own normal, whatever the triangulation.
        TEST_METHOD (LeavesAFlatSheetAlone)
        {
            std::vector<ObjTriangle>            tris = Hinge (0.0f, 0, 0);
            std::vector<std::array<float, 3>>   normals;



            MeshNormals::Compute (tris, MeshNormals::kDefaultSmoothingDeg, normals);

            for (const std::array<float, 3> & n : normals)
            {
                Assert::AreEqual (1.0f, std::abs (n[2]), 0.001f, L"flat sheet faces z");
            }
        }


        // A gentle fold is a curve. The corners ON the fold blend the two
        // sides, so neither keeps its own face normal and the shading runs
        // across the seam instead of stepping at it.
        TEST_METHOD (SmoothsAcrossAGentleFold)
        {
            std::vector<ObjTriangle>           tris    = Hinge (20.0f, 0, 0);
            std::vector<std::array<float, 3>>  normals;
            std::array<float, 3>               faceA   = {};
            float                              length  = 0.0f;



            MeshNormals::Compute (tris, MeshNormals::kDefaultSmoothingDeg, normals);

            faceA  = MeshNormals::FaceNormal (tris[0]);
            length = std::sqrt (Dot (faceA, faceA));
            faceA  = { faceA[0] / length, faceA[1] / length, faceA[2] / length };

            // Corner 1 of triangle 0 is on the fold, so it must have moved
            // off that triangle's own face normal.
            Assert::IsTrue (Dot (normals[1], faceA) < 0.999f, L"fold corner blended");
        }


        // A sharp fold is an edge. Its corners keep their own side's normal,
        // so the crease stays a crease.
        TEST_METHOD (KeepsASharpFoldHard)
        {
            std::vector<ObjTriangle>           tris    = Hinge (90.0f, 0, 0);
            std::vector<std::array<float, 3>>  normals;
            std::array<float, 3>               faceA   = {};
            float                              length  = 0.0f;



            MeshNormals::Compute (tris, MeshNormals::kDefaultSmoothingDeg, normals);

            faceA  = MeshNormals::FaceNormal (tris[0]);
            length = std::sqrt (Dot (faceA, faceA));
            faceA  = { faceA[0] / length, faceA[1] / length, faceA[2] / length };

            for (size_t c = 0; c < 3; c++)
            {
                Assert::AreEqual (1.0f, Dot (normals[c], faceA), 0.001f, L"edge stays hard");
            }
        }


        // Two parts that touch are still two parts. The same gentle fold
        // that smooths within one material must not smooth across a seam
        // between separate mouldings.
        TEST_METHOD (DoesNotSmoothAcrossMaterials)
        {
            std::vector<ObjTriangle>           tris    = Hinge (20.0f, 0, 1);
            std::vector<std::array<float, 3>>  normals;
            std::array<float, 3>               faceA   = {};
            float                              length  = 0.0f;



            MeshNormals::Compute (tris, MeshNormals::kDefaultSmoothingDeg, normals);

            faceA  = MeshNormals::FaceNormal (tris[0]);
            length = std::sqrt (Dot (faceA, faceA));
            faceA  = { faceA[0] / length, faceA[1] / length, faceA[2] / length };

            for (size_t c = 0; c < 3; c++)
            {
                Assert::AreEqual (1.0f, Dot (normals[c], faceA), 0.001f, L"seam not smoothed");
            }
        }


        // The threshold is the caller's, not a constant baked into the rule:
        // the same fold smooths under a wide angle and does not under a
        // narrow one.
        TEST_METHOD (HonorsTheSmoothingAngle)
        {
            std::vector<ObjTriangle>           tris   = Hinge (30.0f, 0, 0);
            std::vector<std::array<float, 3>>  wide;
            std::vector<std::array<float, 3>>  narrow;
            std::array<float, 3>               faceA  = {};
            float                              length = 0.0f;



            MeshNormals::Compute (tris, 45.0f, wide);
            MeshNormals::Compute (tris, 10.0f, narrow);

            faceA  = MeshNormals::FaceNormal (tris[0]);
            length = std::sqrt (Dot (faceA, faceA));
            faceA  = { faceA[0] / length, faceA[1] / length, faceA[2] / length };

            Assert::IsTrue   (Dot (wide[1], faceA) < 0.999f, L"45 deg smooths a 30 deg fold");
            Assert::AreEqual (1.0f, Dot (narrow[1], faceA), 0.001f, L"10 deg does not");
        }


        TEST_METHOD (HandlesAnEmptyMesh)
        {
            std::vector<std::array<float, 3>>   normals;



            MeshNormals::Compute ({}, MeshNormals::kDefaultSmoothingDeg, normals);

            Assert::AreEqual ((size_t) 0, normals.size());
        }
    };
}
