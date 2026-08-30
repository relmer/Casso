#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ObjMeshParser
//
//  Minimal Wavefront OBJ + MTL parser: text in, a flat triangle list out. Pure
//  and system-free (no file I/O -- the shell reads the embedded resource and
//  hands over the decoded text), so it lives in core and is unit-testable with
//  synthetic strings. Built for exactly what a CAD-tool export needs, not the
//  full OBJ spec: positions (`v`), n-gon faces (`f`, fan-triangulated) with
//  optional `/vt/vn` suffixes (ignored -- solid-color CAD shapes carry no
//  texture), `usemtl` material switches, and MTL `Kd` (diffuse) colors baked
//  per triangle. No normals, UVs, groups, or smoothing groups.
//
////////////////////////////////////////////////////////////////////////////////

struct ObjTriangle
{
    float   p0[3] = {};
    float   p1[3] = {};
    float   p2[3] = {};
    float   r     = 1.0f;
    float   g     = 1.0f;
    float   b     = 1.0f;

    // WHICH PART this triangle belongs to -- an index into the material-name
    // table Parse fills, not the name itself. A std::string here would be one
    // heap allocation per triangle, and a drive alone is ~2700 of them.
    //
    // Identity used to be inferred from the COLOR, which made a shade mean
    // two things at once: two parts could not share one, and recoloring a
    // part to see where it was silently made it a different part. -1 is a
    // triangle declared before any usemtl.
    int     material = -1;
};


class ObjMeshParser
{
public:
    // Parses `objText` (referencing material names via `usemtl`) and
    // `mtlText` (defining `Kd` per `newmtl`) into a flat triangle list with
    // each triangle's color baked from whichever material was active when
    // its face was declared. A face's material is a plain lookup; unknown or
    // not-yet-defined materials fall back to white (1,1,1). Fails only if
    // `objText` contains no vertices at all.
    static HRESULT Parse (const std::string        & objText,
                          const std::string        & mtlText,
                          std::vector<ObjTriangle>  & outTriangles);

    // As above, and reports the material NAMES that ObjTriangle::material
    // indexes -- the part names the generator wrote. Callers that only want
    // geometry use the three-argument form and pay nothing for the table.
    static HRESULT Parse (const std::string         & objText,
                          const std::string         & mtlText,
                          std::vector<ObjTriangle>  & outTriangles,
                          std::vector<std::string>  & outMaterialNames);

    // The name of `tri`'s material, or "" when it has none. A free function
    // over the pair would be the same thing with more places to get the
    // bounds check wrong.
    static const std::string & MaterialName (
                          const ObjTriangle              & tri,
                          const std::vector<std::string> & names);

private:
    struct Rgb { float r, g, b; };

    // `newmtl` / `Kd`-keyed diffuse colors parsed from the MTL text, and the
    // leading vertex index of an OBJ face token (negative == relative).
    static std::unordered_map<std::string, Rgb>  ParseMtl       (const std::string & mtlText);
    static int                                   ParseFaceIndex (const std::string & token, size_t vertexCount);
};
