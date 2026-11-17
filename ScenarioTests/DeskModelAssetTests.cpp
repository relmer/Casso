#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/Printer/MeshBlob.h"
#include "Devices/Printer/MeshNormals.h"
#include "Devices/Printer/ObjMeshParser.h"
#include "Ui/Scene/DeskSceneModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeskModelAssetTests
//
//  The models of the drives other than the Disk II pair, from
//  Resources/Models and baked as the build bakes them: each loads as the
//  desk loads it, with its lamp, and each 3.5-inch drive with its slot.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeskModelAssetTests)
{
public:

    TEST_METHOD (EachDrive_LoadsWithItsLampAndSlot)
    {
        struct Asset
        {
            const char *  name;
            bool          hasSlot;
        };

        constexpr Asset  kAssets[] =
        {
            { "UniDisk35",    true  },
            { "Apple35Drive", true  },
            { "HardDisk20SC", false },
            { "ProFile",      false },
        };



        for (const Asset & asset : kAssets)
        {
            DeskSceneModel        model;
            std::vector<uint8_t>  mesh = ReadMesh (asset.name);
            std::wstring          name (asset.name, asset.name + strlen (asset.name));

            Assert::IsFalse (mesh.empty(), (name + L" has a baked mesh").c_str());
            Assert::AreEqual (S_OK, model.Load (DeskDeviceKind::HardDisk20SC, mesh), (name + L" loads").c_str());
            Assert::IsFalse (model.Lamps().empty(), (name + L" has its lamp").c_str());
            Assert::AreEqual (asset.hasSlot, model.HasSlot(), (name + L" has a slot only if it takes a disk").c_str());

            if (asset.hasSlot)
            {
                AssertDiskShowsInSlot (model, name);
            }
        }
    }


    //  A FINISH IS MATCHED BY COLOR: a part in the pebbled or recessed
    //  black takes that finish whatever its name. Every color in each of
    //  these models is either a finish marker or clear of both by more than
    //  kKdEpsilon on a channel, so no part takes a finish by drifting near it.
    TEST_METHOD (EveryModelColor_IsAFinishOrClearOfOne)
    {
        constexpr const char *  kModels[]   = { "UniDisk35", "Apple35Drive", "HardDisk20SC", "ProFile" };
        constexpr float         kExactly    = 0.001f;
        const float           * kFinishes[] = { DeskSceneModel::kPlatePebbledKd, DeskSceneModel::kPlateRecessKd };
        std::wstring            drifted;



        for (const char * model : kModels)
        {
            std::istringstream  mtl (ReadText (ModelPath (model) + ".mtl"));
            std::string         line;
            std::string         part;

            while (std::getline (mtl, line))
            {
                std::istringstream  fields (line);
                std::string         key;
                float               kd[3] = {};

                fields >> key;

                if (key == "newmtl")
                {
                    fields >> part;
                }
                else if (key == "Kd" && (fields >> kd[0] >> kd[1] >> kd[2]))
                {
                    for (const float * finish : kFinishes)
                    {
                        float  farthest = 0.0f;

                        for (int c = 0; c < 3; c++)
                        {
                            farthest = std::max (farthest, std::fabs (kd[c] - finish[c]));
                        }

                        if (farthest > kExactly && farthest <= DeskSceneModel::kKdEpsilon)
                        {
                            drifted += std::format (L"{} {}: Kd {} {} {}\n", TextEncoding::NarrowToWide (model), TextEncoding::NarrowToWide (part), kd[0], kd[1], kd[2]);
                        }
                    }
                }
            }
        }

        Assert::IsTrue (drifted.empty(), (L"within kKdEpsilon of a finish:\n" + drifted).c_str());
    }


private:

    //  The disk in the slot is a couple of millimeters behind the case's
    //  front, with most of the slot's depth between it and the back of the
    //  slot, so it reads against the dark back.
    static void AssertDiskShowsInSlot (const DeskSceneModel & model, const std::wstring & name)
    {
        std::vector<Dxui3DRenderer::Vertex>  disk;
        float                                lo[3] = {};
        float                                front = FLT_MAX;



        model.BuildDiskInSlot (disk);
        model.BoundsMin (lo);

        Assert::IsFalse (disk.empty(), (name + L" draws a disk in its slot").c_str());

        for (const Dxui3DRenderer::Vertex & v : disk)
        {
            front = std::min (front, v.y);
        }

        Assert::IsTrue (front > lo[1] && front < lo[1] + 4.0f,
                        std::format (L"{}: the disk's front is {:.1f} mm behind the case's front", name, front - lo[1]).c_str());
        Assert::IsTrue (front < model.SlotBox()[1] - 3.0f, (name + L": the disk is well in front of the back of the slot").c_str());
    }


    static std::string ModelPath (const std::string & name)
    {
        std::string  thisFile = __FILE__;



        return thisFile.substr (0, thisFile.find_last_of ("\\/")) + "\\..\\Resources\\Models\\" + name + "\\" + name;
    }


    static std::string ReadText (const std::string & path)
    {
        std::ifstream  file (path, std::ios::binary);
        std::string    text;



        if (file.is_open())
        {
            text.assign (std::istreambuf_iterator<char> (file), std::istreambuf_iterator<char>());
        }

        return text;
    }


    //  The model's OBJ and MTL text, baked as MeshCreator bakes it for the
    //  build.
    static std::vector<uint8_t> ReadMesh (const std::string & name)
    {
        std::string                        folder   = ModelPath (name);
        std::vector<ObjTriangle>           triangles;
        std::vector<std::string>           names;
        std::vector<std::array<float, 3>>  normals;
        std::vector<uint8_t>               blob;
        HRESULT                            hr       = S_OK;



        hr = ObjMeshParser::Parse (ReadText (folder + ".mesh"), ReadText (folder + ".mtl"), triangles, names);
        Assert::AreEqual (S_OK, hr, L"the model parses");

        MeshNormals::Compute (triangles, MeshNormals::kDefaultSmoothingDeg, normals);

        hr = MeshBlob::Write (triangles, names, normals, blob);
        Assert::AreEqual (S_OK, hr, L"the model bakes");

        return blob;
    }
};
