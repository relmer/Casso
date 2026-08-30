#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Printer/MeshBlob.h"
#include "Devices/Printer/MeshNormals.h"
#include "Devices/Printer/ObjMeshParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The baked mesh format the desk scene loads instead of re-parsing a
// modeling tool's text export at every launch. What matters is that a
// round trip is INDISTINGUISHABLE from the parse it replaces, so most of
// these run text through ObjMeshParser, bake, unbake, and compare the two
// triangle lists field for field.
namespace MeshBlobTests
{
    static void AssertSameTriangles (const std::vector<ObjTriangle> & expected,
                                     const std::vector<ObjTriangle> & actual)
    {
        Assert::AreEqual (expected.size(), actual.size(), L"triangle count");

        for (size_t i = 0; i < expected.size(); i++)
        {
            const ObjTriangle &   e = expected[i];
            const ObjTriangle &   a = actual[i];

            for (size_t c = 0; c < 3; c++)
            {
                Assert::AreEqual (e.p0[c], a.p0[c], L"p0");
                Assert::AreEqual (e.p1[c], a.p1[c], L"p1");
                Assert::AreEqual (e.p2[c], a.p2[c], L"p2");
            }

            Assert::AreEqual (e.r, a.r, L"r");
            Assert::AreEqual (e.g, a.g, L"g");
            Assert::AreEqual (e.b, a.b, L"b");
            Assert::AreEqual (e.material, a.material, L"material");
        }
    }


    static void RoundTrip (const std::string & obj, const std::string & mtl)
    {
        std::vector<ObjTriangle>   parsed;
        std::vector<std::string>   parsedNames;
        std::vector<ObjTriangle>   restored;
        std::vector<std::string>   restoredNames;
        std::vector<uint8_t>       blob;
        HRESULT                    hr = S_OK;



        hr = ObjMeshParser::Parse (obj, mtl, parsed, parsedNames);
        Assert::IsTrue (SUCCEEDED (hr), L"parse");

        hr = MeshBlob::Write (parsed, parsedNames, {}, blob);
        Assert::IsTrue (SUCCEEDED (hr), L"write");

        hr = MeshBlob::Read (blob, restored, restoredNames);
        Assert::IsTrue (SUCCEEDED (hr), L"read");

        AssertSameTriangles (parsed, restored);
        Assert::IsTrue (parsedNames == restoredNames, L"material names");
    }


    TEST_CLASS (MeshBlobTests)
    {
    public:

        TEST_METHOD (RoundTripsColorsAndPositions)
        {
            RoundTrip ("v 0 0 0\n"
                       "v 1 0 0\n"
                       "v 0 1 0\n"
                       "usemtl red\n"
                       "f 1 2 3\n",
                       "newmtl red\nKd 1 0 0\n");
        }


        TEST_METHOD (RoundTripsSeveralMaterials)
        {
            RoundTrip ("v 0 0 0\n"
                       "v 1 0 0\n"
                       "v 0 1 0\n"
                       "v 1 1 0\n"
                       "usemtl red\n"
                       "f 1 2 3\n"
                       "usemtl blue\n"
                       "f 2 3 4\n",
                       "newmtl red\nKd 1 0 0\n"
                       "newmtl blue\nKd 0 0 1\n");
        }


        // A face declared before any usemtl is ObjTriangle::material == -1,
        // which the blob spells as its own sentinel rather than as index 0.
        TEST_METHOD (RoundTripsTriangleWithNoMaterial)
        {
            RoundTrip ("v 0 0 0\n"
                       "v 1 0 0\n"
                       "v 0 1 0\n"
                       "f 1 2 3\n",
                       "");
        }


        // The parser falls back to white for a material the MTL never
        // defined, and that white has to survive the trip: the name is
        // interned either way, so the blob cannot tell the two cases apart
        // from the index alone.
        TEST_METHOD (RoundTripsUnknownMaterialAsWhite)
        {
            std::vector<ObjTriangle>   restored;
            std::vector<std::string>   names;
            std::vector<ObjTriangle>   parsed;
            std::vector<std::string>   parsedNames;
            std::vector<uint8_t>       blob;



            ObjMeshParser::Parse ("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                  "usemtl nowhere\n"
                                  "f 1 2 3\n",
                                  "", parsed, parsedNames);

            MeshBlob::Write (parsed, parsedNames, {}, blob);
            MeshBlob::Read  (blob, restored, names);

            Assert::AreEqual ((size_t) 1, restored.size());
            Assert::AreEqual (1.0f, restored[0].r);
            Assert::AreEqual (1.0f, restored[0].g);
            Assert::AreEqual (1.0f, restored[0].b);
        }


        // The point of the format. Two triangles cut from one quad meet
        // along a diagonal and share two corners, so the blob stores the
        // four positions the OBJ declared rather than the six the flattened
        // triangle list repeats.
        TEST_METHOD (SharesPositionsBetweenFaces)
        {
            std::vector<ObjTriangle>   parsed;
            std::vector<std::string>   parsedNames;
            std::vector<uint8_t>       blob;
            std::string                obj;



            obj = "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                  "f 1 2 3\n"
                  "f 1 3 4\n";

            ObjMeshParser::Parse (obj, "", parsed, parsedNames);

            HRESULT   hr = MeshBlob::Write (parsed, parsedNames, {}, blob);

            Assert::IsTrue (SUCCEEDED (hr));
            Assert::AreEqual ((size_t) 2, parsed.size());

            // 32-byte header, no materials, no names, no normals, four
            // shared positions at 12 bytes, two faces at 28.
            Assert::AreEqual ((size_t) (32 + 0 + 0 + 4 * 12 + 2 * 28), blob.size());
        }


        TEST_METHOD (RoundTripsAnEmptyMesh)
        {
            std::vector<ObjTriangle>   restored;
            std::vector<std::string>   names;
            std::vector<uint8_t>       blob;
            HRESULT                    hr = S_OK;



            hr = MeshBlob::Write ({}, {}, {}, blob);
            Assert::IsTrue (SUCCEEDED (hr), L"write");

            hr = MeshBlob::Read (blob, restored, names);
            Assert::IsTrue (SUCCEEDED (hr), L"read");
            Assert::AreEqual ((size_t) 0, restored.size());
        }


        // Normals ride along per corner, and survive the trip exactly. The
        // scene reads them instead of computing a face normal, so a drift
        // here would be a shading change nothing else would catch.
        TEST_METHOD (RoundTripsSmoothedNormals)
        {
            std::vector<ObjTriangle>   parsed;
            std::vector<std::string>   parsedNames;
            std::vector<ObjTriangle>   restored;
            std::vector<std::string>   names;
            std::vector<uint8_t>       blob;

            std::vector<std::array<float, 3>>   normals;
            std::vector<std::array<float, 3>>   restoredNormals;



            ObjMeshParser::Parse ("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                                  "f 1 2 3\n"
                                  "f 1 3 4\n",
                                  "", parsed, parsedNames);

            MeshNormals::Compute (parsed, MeshNormals::kDefaultSmoothingDeg, normals);

            Assert::AreEqual ((size_t) 6, normals.size(), L"three per triangle");

            MeshBlob::Write (parsed, parsedNames, normals, blob);
            MeshBlob::Read  (blob, restored, names, restoredNormals);

            Assert::AreEqual (normals.size(), restoredNormals.size(), L"normal count");

            for (size_t i = 0; i < normals.size(); i++)
            {
                for (size_t c = 0; c < 3; c++)
                {
                    Assert::AreEqual (normals[i][c], restoredNormals[i][c], L"normal");
                }
            }
        }


        // The whole quad is one plane, so every corner of both triangles
        // shares one normal and the table holds exactly that one.
        TEST_METHOD (SharesNormalsBetweenFaces)
        {
            std::vector<ObjTriangle>   parsed;
            std::vector<std::string>   parsedNames;
            std::vector<uint8_t>       blob;

            std::vector<std::array<float, 3>>   normals;



            ObjMeshParser::Parse ("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                                  "f 1 2 3\n"
                                  "f 1 3 4\n",
                                  "", parsed, parsedNames);

            MeshNormals::Compute (parsed, MeshNormals::kDefaultSmoothingDeg, normals);
            MeshBlob::Write (parsed, parsedNames, normals, blob);

            // Header, four positions at 12, ONE normal at 12, two faces at 28.
            Assert::AreEqual ((size_t) (32 + 4 * 12 + 1 * 12 + 2 * 28), blob.size());
        }


        // A blob written with normals and one written without are different
        // lengths, and the reader reports which it got rather than inventing
        // normals for the second.
        TEST_METHOD (ReadsBackNoNormalsWhenNoneWereWritten)
        {
            std::vector<ObjTriangle>   parsed;
            std::vector<std::string>   parsedNames;
            std::vector<ObjTriangle>   restored;
            std::vector<std::string>   names;
            std::vector<uint8_t>       blob;

            std::vector<std::array<float, 3>>   restoredNormals;



            ObjMeshParser::Parse ("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",
                                  "", parsed, parsedNames);

            MeshBlob::Write (parsed, parsedNames, {}, blob);
            MeshBlob::Read  (blob, restored, names, restoredNormals);

            Assert::AreEqual ((size_t) 1, restored.size());
            Assert::AreEqual ((size_t) 0, restoredNormals.size(), L"no normals");
        }


        // Three normals per triangle or none: a partial set is a caller bug.
        TEST_METHOD (RefusesAPartialNormalSet)
        {
            std::vector<ObjTriangle>   parsed;
            std::vector<std::string>   parsedNames;
            std::vector<uint8_t>       blob;
            HRESULT                    hr = S_OK;

            std::vector<std::array<float, 3>>   normals (2);



            ObjMeshParser::Parse ("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",
                                  "", parsed, parsedNames);

            {
                UnitTestHelpers::ExpectedEhmAssert   expected;

                hr = MeshBlob::Write (parsed, parsedNames, normals, blob);
            }

            Assert::IsTrue (FAILED (hr));
        }

        TEST_METHOD (RefusesATruncatedBlob)
        {
            std::vector<ObjTriangle>   parsed;
            std::vector<std::string>   parsedNames;
            std::vector<ObjTriangle>   restored;
            std::vector<std::string>   names;
            std::vector<uint8_t>       blob;
            HRESULT                    hr = S_OK;



            ObjMeshParser::Parse ("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",
                                  "", parsed, parsedNames);
            MeshBlob::Write (parsed, parsedNames, {}, blob);

            blob.pop_back();

            {
                UnitTestHelpers::ExpectedEhmAssert   expected;

                hr = MeshBlob::Read (blob, restored, names);
            }

            Assert::IsTrue (FAILED (hr));
            Assert::AreEqual ((size_t) 0, restored.size());
        }


        TEST_METHOD (RefusesAForeignBlob)
        {
            std::vector<ObjTriangle>   restored;
            std::vector<std::string>   names;
            std::vector<uint8_t>       blob (64, 0);
            HRESULT                    hr = S_OK;



            {
                UnitTestHelpers::ExpectedEhmAssert   expected;

                hr = MeshBlob::Read (blob, restored, names);
            }

            Assert::IsTrue (FAILED (hr));
        }
    };
}
