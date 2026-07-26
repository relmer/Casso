#include "Pch.h"

#include "Devices/Printer/ObjMeshParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Wavefront OBJ+MTL parsing for the printer panel's reference CAD model
// (FR-032): synthetic text in, flat color-baked triangle list out. Covers
// exactly the subset a Tinkercad export uses plus the spec quirks that cost
// nothing to honor (slash suffixes, negative indices, n-gon fans).
namespace ObjMeshParserTests
{
    TEST_CLASS (ObjMeshParserTests)
    {
    public:

        TEST_METHOD (ParsesTriangleWithMaterialColor)
        {
            std::string   mtl = "newmtl red\nKd 1 0 0\n";
            std::string   obj =
                "v 0 0 0\n"
                "v 1 0 0\n"
                "v 0 1 0\n"
                "usemtl red\n"
                "f 1 2 3\n";

            std::vector<ObjTriangle>   tris;

            Assert::IsTrue  (ObjMeshParser::Parse (obj, mtl, tris));
            Assert::AreEqual ((size_t) 1, tris.size());
            Assert::AreEqual (1.0f, tris[0].r);
            Assert::AreEqual (0.0f, tris[0].g);
            Assert::AreEqual (0.0f, tris[0].b);
            Assert::AreEqual (1.0f, tris[0].p1[0]);   // second vertex is (1,0,0)
            Assert::AreEqual (1.0f, tris[0].p2[1]);   // third vertex is (0,1,0)
        }


        TEST_METHOD (FanTriangulatesQuad)
        {
            std::string   obj =
                "v 0 0 0\n"
                "v 1 0 0\n"
                "v 1 1 0\n"
                "v 0 1 0\n"
                "f 1 2 3 4\n";

            std::vector<ObjTriangle>   tris;

            Assert::IsTrue  (ObjMeshParser::Parse (obj, "", tris));
            Assert::AreEqual ((size_t) 2, tris.size());

            // Fan: (v1,v2,v3) and (v1,v3,v4) -- both share the first vertex.
            Assert::AreEqual (0.0f, tris[0].p0[0]);
            Assert::AreEqual (0.0f, tris[1].p0[0]);
            Assert::AreEqual (1.0f, tris[1].p1[0]);   // v3 = (1,1,0)
            Assert::AreEqual (1.0f, tris[1].p1[1]);
            Assert::AreEqual (0.0f, tris[1].p2[0]);   // v4 = (0,1,0)
        }


        TEST_METHOD (SlashSuffixesAreIgnored)
        {
            std::string   obj =
                "v 0 0 0\n"
                "v 1 0 0\n"
                "v 0 1 0\n"
                "f 1/1/1 2/2/2 3//3\n";

            std::vector<ObjTriangle>   tris;

            Assert::IsTrue  (ObjMeshParser::Parse (obj, "", tris));
            Assert::AreEqual ((size_t) 1, tris.size());
            Assert::AreEqual (1.0f, tris[0].p1[0]);
        }


        TEST_METHOD (UnknownMaterialFallsBackToWhite)
        {
            std::string   obj =
                "v 0 0 0\n"
                "v 1 0 0\n"
                "v 0 1 0\n"
                "usemtl no_such_material\n"
                "f 1 2 3\n";

            std::vector<ObjTriangle>   tris;

            Assert::IsTrue  (ObjMeshParser::Parse (obj, "newmtl other\nKd 0 0 1\n", tris));
            Assert::AreEqual ((size_t) 1, tris.size());
            Assert::AreEqual (1.0f, tris[0].r);
            Assert::AreEqual (1.0f, tris[0].g);
            Assert::AreEqual (1.0f, tris[0].b);
        }


        TEST_METHOD (NegativeIndicesResolveRelative)
        {
            std::string   obj =
                "v 0 0 0\n"
                "v 1 0 0\n"
                "v 0 1 0\n"
                "f -3 -2 -1\n";

            std::vector<ObjTriangle>   tris;

            Assert::IsTrue  (ObjMeshParser::Parse (obj, "", tris));
            Assert::AreEqual ((size_t) 1, tris.size());
            Assert::AreEqual (1.0f, tris[0].p1[0]);
            Assert::AreEqual (1.0f, tris[0].p2[1]);
        }


        TEST_METHOD (MaterialSwitchBakesPerFace)
        {
            std::string   mtl =
                "newmtl red\nKd 1 0 0\n"
                "newmtl blue\nKd 0 0 1\n";
            std::string   obj =
                "v 0 0 0\n"
                "v 1 0 0\n"
                "v 0 1 0\n"
                "usemtl red\n"
                "f 1 2 3\n"
                "usemtl blue\n"
                "f 3 2 1\n";

            std::vector<ObjTriangle>   tris;

            Assert::IsTrue  (ObjMeshParser::Parse (obj, mtl, tris));
            Assert::AreEqual ((size_t) 2, tris.size());
            Assert::AreEqual (1.0f, tris[0].r);
            Assert::AreEqual (0.0f, tris[0].b);
            Assert::AreEqual (0.0f, tris[1].r);
            Assert::AreEqual (1.0f, tris[1].b);
        }


        TEST_METHOD (EmptyObjReturnsFalse)
        {
            std::vector<ObjTriangle>   tris;

            Assert::IsFalse (ObjMeshParser::Parse ("", "", tris));
            Assert::IsFalse (ObjMeshParser::Parse ("# just a comment\n", "", tris));
        }


        TEST_METHOD (OutOfRangeIndicesAreDropped)
        {
            // A face referencing a vertex that doesn't exist contributes
            // nothing rather than reading garbage.
            std::string   obj =
                "v 0 0 0\n"
                "v 1 0 0\n"
                "f 1 2 9\n";

            std::vector<ObjTriangle>   tris;

            Assert::IsTrue  (ObjMeshParser::Parse (obj, "", tris));
            Assert::AreEqual ((size_t) 0, tris.size());
        }
    };
}
