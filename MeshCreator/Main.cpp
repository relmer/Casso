#include "Pch.h"

#include "Devices/Printer/MeshBlob.h"
#include "Devices/Printer/MeshNormals.h"
#include "Devices/Printer/ObjMeshParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MeshCreator
//
//  Turns a model's OBJ/MTL pair into the baked blob the desk scene loads.
//
//  It exists so that the shipping app never parses text. The Monitor II is
//  1.3 million lines of it, and re-reading them at every launch was most of
//  Casso's startup; the text is still the reviewed source under Resources,
//  and this runs once per build to produce what the parser would have.
//
//  It uses ObjMeshParser ITSELF rather than a second reader written to match
//  it. A baker with its own idea of the dialect is a baker that can disagree
//  with the app about what a model says, and the disagreement would show up
//  as geometry rather than as an error.
//
//  Errors go to stderr and come back as a non-zero exit code, which is what
//  a custom build step reads. A model that fails to bake has to fail the
//  build: the alternative is an executable whose scene is quietly missing a
//  device.
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  ReadWholeFile
//
////////////////////////////////////////////////////////////////////////////////

static bool ReadWholeFile (const char * path, std::string & outText)
{
    std::ifstream       file (path, std::ios::binary);
    std::stringstream   buffer;



    if (!file.good())
    {
        return false;
    }

    buffer << file.rdbuf();
    outText = buffer.str();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteWholeFile
//
////////////////////////////////////////////////////////////////////////////////

static bool WriteWholeFile (const char * path, const std::vector<uint8_t> & bytes)
{
    std::ofstream   file (path, std::ios::binary | std::ios::trunc);



    if (!file.good())
    {
        return false;
    }

    if (!bytes.empty())
    {
        file.write (reinterpret_cast<const char *> (bytes.data()),
                    (std::streamsize) bytes.size());
    }

    return file.good();
}





////////////////////////////////////////////////////////////////////////////////
//
//  main
//
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char ** argv)
{
    std::string                        objText;
    std::string                        mtlText;
    std::vector<ObjTriangle>           triangles;
    std::vector<std::string>           materialNames;
    std::vector<uint8_t>               blob;
    std::vector<std::array<float, 3>>  normals;
    HRESULT                            hr            = S_OK;



    if (argc != 4)
    {
        std::cerr << "usage: MeshCreator <model.mesh> <model.mtl> <model.dmesh>\n";
        return 1;
    }

    if (!ReadWholeFile (argv[1], objText))
    {
        std::cerr << "MeshCreator: cannot read " << argv[1] << "\n";
        return 1;
    }

    // A model with no MTL is legal: its faces take the parser's white. Only
    // the geometry is required, so a missing material file is not an error.
    if (!ReadWholeFile (argv[2], mtlText))
    {
        mtlText.clear();
    }

    hr = ObjMeshParser::Parse (objText, mtlText, triangles, materialNames);

    if (FAILED (hr))
    {
        std::cerr << "MeshCreator: no geometry in " << argv[1] << "\n";
        return 1;
    }

    // Averaged at BUILD time, so the app never pays for it and a coarser
    // mesh can still shade as the smooth surface it approximates.
    MeshNormals::Compute (triangles, MeshNormals::kDefaultSmoothingDeg, normals);

    hr = MeshBlob::Write (triangles, materialNames, normals, blob);

    if (FAILED (hr))
    {
        std::cerr << "MeshCreator: cannot pack " << argv[1] << "\n";
        return 1;
    }

    if (!WriteWholeFile (argv[3], blob))
    {
        std::cerr << "MeshCreator: cannot write " << argv[3] << "\n";
        return 1;
    }

    std::cout << "MeshCreator: " << argv[1]
              << " -- " << triangles.size() << " triangles, "
              << materialNames.size() << " parts, "
              << objText.size() << " -> " << blob.size() << " bytes\n";

    return 0;
}
