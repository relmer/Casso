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
};


class ObjMeshParser
{
public:
    // Parses `objText` (referencing material names via `usemtl`) and
    // `mtlText` (defining `Kd` per `newmtl`) into a flat triangle list with
    // each triangle's color baked from whichever material was active when
    // its face was declared. A face's material is a plain lookup; unknown or
    // not-yet-defined materials fall back to white (1,1,1). Returns false
    // only if `objText` contains no vertices at all.
    static bool Parse (const std::string        & objText,
                       const std::string        & mtlText,
                       std::vector<ObjTriangle>  & outTriangles);
};
