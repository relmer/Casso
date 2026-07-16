#include "Pch.h"

#include "Devices/Printer/ObjMeshParser.h"




namespace
{
    struct Rgb { float r, g, b; };


    ////////////////////////////////////////////////////////////////////////////
    //
    //  ParseMtl
    //
    //  `newmtl <name>` starts a material; a `Kd r g b` line inside it sets
    //  the diffuse color. Any other MTL directive (Ka, d, illum, ...) is
    //  ignored -- this scene bakes flat per-triangle color, nothing else.
    //
    ////////////////////////////////////////////////////////////////////////////

    std::unordered_map<std::string, Rgb> ParseMtl (const std::string & mtlText)
    {
        std::unordered_map<std::string, Rgb>   materials;
        std::istringstream                     stream (mtlText);
        std::string                            line;
        std::string                            curName;

        while (std::getline (stream, line))
        {
            std::istringstream   ls (line);
            std::string          tag;

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


    ////////////////////////////////////////////////////////////////////////////
    //
    //  ParseFaceIndex
    //
    //  A face token is "v", "v/vt", "v/vt/vn", or "v//vn" -- only the leading
    //  vertex index matters here (no UVs/normals in a flat-shaded CAD scene).
    //  Negative indices are relative to the current vertex count (OBJ spec);
    //  Tinkercad emits plain positive 1-based indices, but this costs nothing
    //  to honor.
    //
    ////////////////////////////////////////////////////////////////////////////

    int ParseFaceIndex (const std::string & token, size_t vertexCount)
    {
        int   vi = std::atoi (token.c_str());

        if (vi < 0)
        {
            vi = (int) vertexCount + vi + 1;
        }

        return vi;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ObjMeshParser::Parse
//
////////////////////////////////////////////////////////////////////////////////

bool ObjMeshParser::Parse (const std::string        & objText,
                           const std::string        & mtlText,
                           std::vector<ObjTriangle>  & outTriangles)
{
    std::unordered_map<std::string, Rgb>   materials = ParseMtl (mtlText);
    std::vector<std::array<float, 3>>      verts;
    Rgb                                    curColor = { 1.0f, 1.0f, 1.0f };
    std::istringstream                     stream (objText);
    std::string                            line;

    outTriangles.clear();

    while (std::getline (stream, line))
    {
        std::istringstream   ls (line);
        std::string          tag;

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

                outTriangles.push_back (tri);
            }
        }
    }

    return !verts.empty();
}
