#include "Pch.h"

#include "Devices/Printer/ObjMeshParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ObjMeshParser::ParseMtl
//
//  `newmtl <name>` starts a material; a `Kd r g b` line inside it sets the
//  diffuse color. Any other MTL directive (Ka, d, illum, ...) is ignored --
//  this scene bakes flat per-triangle color, nothing else.
//
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, ObjMeshParser::Rgb> ObjMeshParser::ParseMtl (const std::string & mtlText)
{
    std::unordered_map<std::string, Rgb>   materials;
    std::string                            line;
    std::string                            curName;
    std::istringstream                     stream (mtlText);



    while (std::getline (stream, line))
    {
        std::string          tag;

        std::istringstream   ls (line);

        ls >> tag;

        if (tag == "newmtl")
        {
            ls >> curName;
        }
        else if (tag == "Kd" && !curName.empty())
        {
            Rgb   c = { 1.0f, 1.0f, 1.0f };

            ls >> c.r >> c.g >> c.b;
            materials[curName] = c;
        }
    }

    return materials;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ObjMeshParser::ParseFaceIndex
//
//  A face token is "v", "v/vt", "v/vt/vn", or "v//vn" -- only the leading
//  vertex index matters here (no UVs/normals in a flat-shaded CAD scene).
//  Negative indices are relative to the current vertex count (OBJ spec);
//  Tinkercad emits plain positive 1-based indices, but this costs nothing to
//  honor.
//
////////////////////////////////////////////////////////////////////////////////

int ObjMeshParser::ParseFaceIndex (const std::string & token, size_t vertexCount)
{
    int   vi = std::atoi (token.c_str());



    if (vi < 0)
    {
        vi = (int) vertexCount + vi + 1;
    }

    return vi;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ObjMeshParser::Parse
//
//  Parses the subset of Wavefront OBJ the printer model actually uses:
//  vertices, faces, and material colors.
//
//  A DELIBERATE SUBSET. Normals, texture coordinates, groups, and smoothing
//  are all ignored -- the 3D scene lights per-face and applies no texture, so
//  parsing them would be work whose results are discarded. The parser is here
//  to load one known asset, not to be a general OBJ importer.
//
//  Materials are resolved to a COLOR at the point of use rather than kept as
//  names, so each triangle carries its own color and the renderer needs no
//  material state or per-material batching.
//
//  Faces are triangulated as they are read, since OBJ permits arbitrary
//  polygons and the renderer takes triangle lists only.
//
//  Unrecognized tags are skipped silently, which is what makes the parser
//  tolerant of whatever a modelling tool emits alongside the geometry.
//
//  An unknown material falls back to white rather than failing, so a mesh
//  referencing a material its MTL omits still loads and is visibly plain
//  rather than absent.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ObjMeshParser::Parse (const std::string        & objText,
                              const std::string        & mtlText,
                              std::vector<ObjTriangle>  & outTriangles)
{
    std::vector<std::string>   names;

    return Parse (objText, mtlText, outTriangles, names);
}


HRESULT ObjMeshParser::Parse (const std::string         & objText,
                              const std::string         & mtlText,
                              std::vector<ObjTriangle>  & outTriangles,
                              std::vector<std::string>  & outMaterialNames)
{
    HRESULT                                hr        = S_OK;
    std::unordered_map<std::string, Rgb>   materials = ParseMtl (mtlText);
    std::vector<std::array<float, 3>>      verts;
    Rgb                                    curColor  = { 1.0f, 1.0f, 1.0f };
    int                                    curMat    = -1;
    std::string                            line;
    bool                                   hasVerts  = false;
    std::istringstream                     stream (objText);



    outTriangles.clear();
    outMaterialNames.clear();

    while (std::getline (stream, line))
    {
        std::string          tag;

        std::istringstream   ls (line);

        ls >> tag;

        if (tag == "v")
        {
            std::array<float, 3>   p = {};

            ls >> p[0] >> p[1] >> p[2];
            verts.push_back (p);
        }
        else if (tag == "usemtl")
        {
            std::string   name;

            ls >> name;

            auto   it = materials.find (name);
            curColor  = (it != materials.end()) ? it->second : Rgb { 1.0f, 1.0f, 1.0f };

            // The name is interned once per usemtl RUN, not per triangle, so
            // the table stays the length of the parts list. A repeat of the
            // same name reuses its index rather than adding a second entry --
            // one part split across two usemtl runs is still one part.
            {
                auto   found = std::find (outMaterialNames.begin(),
                                          outMaterialNames.end(), name);

                if (found == outMaterialNames.end())
                {
                    curMat = (int) outMaterialNames.size();
                    outMaterialNames.push_back (name);
                }
                else
                {
                    curMat = (int) (found - outMaterialNames.begin());
                }
            }
        }
        else if (tag == "f")
        {
            std::vector<int>   faceVerts;
            std::string        token;

            while (ls >> token)
            {
                int   vi = ParseFaceIndex (token, verts.size());

                if (vi >= 1 && (size_t) vi <= verts.size())
                {
                    faceVerts.push_back (vi - 1);   // OBJ is 1-indexed
                }
            }

            // Fan-triangulate any n-gon (n>=3): (0,1,2), (0,2,3), (0,3,4), ...
            for (size_t k = 1; k + 1 < faceVerts.size(); k++)
            {
                ObjTriangle   tri;

                const auto &   a = verts[faceVerts[0]];
                const auto &   b = verts[faceVerts[k]];
                const auto &   c = verts[faceVerts[k + 1]];

                tri.p0[0] = a[0]; tri.p0[1] = a[1]; tri.p0[2] = a[2];
                tri.p1[0] = b[0]; tri.p1[1] = b[1]; tri.p1[2] = b[2];
                tri.p2[0] = c[0]; tri.p2[1] = c[1]; tri.p2[2] = c[2];
                tri.r = curColor.r; tri.g = curColor.g; tri.b = curColor.b;
                tri.material = curMat;

                outTriangles.push_back (tri);
            }
        }
    }

    hasVerts = !verts.empty();
    CBR (hasVerts);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ObjMeshParser::MaterialName
//
//  Bounds-checked so a triangle from a mesh parsed WITHOUT the name table --
//  the three-argument overload, or a mesh with no usemtl at all -- answers
//  "" rather than reading off the end of an empty vector.
//
////////////////////////////////////////////////////////////////////////////////

const std::string & ObjMeshParser::MaterialName (
    const ObjTriangle              & tri,
    const std::vector<std::string> & names)
{
    static const std::string  s_kNone;

    if (tri.material < 0 || (size_t) tri.material >= names.size())
    {
        return s_kNone;
    }

    return names[(size_t) tri.material];
}
