#pragma once

#include "Pch.h"

#include "Devices/Printer/ObjMeshParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MeshNormals
//
//  Per-vertex normals, averaged across the faces that meet gently enough to
//  be one surface.
//
//  THE MESH IS FLAT-SHADED WITHOUT THIS, and flat shading is why the models
//  are tessellated as finely as they are. A curved surface built from flat
//  triangles, each lit by its own face normal, shades in bands: one value per
//  triangle, and the eye reads the steps. The only remedy available was more
//  triangles, so the Monitor II's funnel needed 926,142 of them to stop
//  banding, and 178 MB of vertices followed from that.
//
//  Averaging decouples the two. Where neighboring faces meet below the
//  smoothing angle they are treated as samples of one curved surface and
//  share an averaged normal, so the shading runs continuously across them
//  however few there are. Above it they are an edge, and each keeps its own
//  face normal, so a box stays crisp.
//
//  SAME MATERIAL ONLY. Two parts that happen to touch are still two parts:
//  the bezel meeting the case is a seam between separate mouldings, not a
//  curve, and averaging across it would light them as one piece. The
//  parser's material index is what says which part a face belongs to.
//
//  Computed at BUILD time by MeshCreator and carried in the baked mesh, so
//  this costs the app nothing at launch.
//
////////////////////////////////////////////////////////////////////////////////

class MeshNormals
{
public:

    // Faces meeting below this are one surface; above it they are an edge.
    //
    // A fillet is tangent to what it adjoins, so its junction with a flat
    // face is a fraction of a degree and smooths whatever the tessellation.
    // A moulded box corner is 90 and stays hard. The value only has to
    // separate those two populations, and anything from about 30 to 60 does.
    static constexpr float  kDefaultSmoothingDeg = 40.0f;

    // Three normals per triangle, in the same order as `triangles`, so
    // outNormals[t * 3 + c] belongs to corner c of triangle t.
    //
    // A degenerate triangle contributes nothing to its neighbors and takes
    // the zero normal, which the scene shader already reads as "unlit".
    static void  Compute (const std::vector<ObjTriangle>         & triangles,
                          float                                    smoothingDeg,
                          std::vector<std::array<float, 3>>      & outNormals);

    // The face's own normal, NOT normalized: its length is twice the
    // triangle's area, which is the weight a vertex average wants. A big
    // face should pull the shared normal further than a sliver does.
    static std::array<float, 3>  FaceNormal (const ObjTriangle & tri);

private:

    // What counts as "the same vertex" for averaging: an exact position, on
    // one material. Exact rather than within a tolerance because the
    // generator emits a shared corner as the same text every time, so the
    // sharing the model was built with is already recoverable bit for bit;
    // anything looser would weld vertices the model kept apart on purpose.
    struct CornerKey
    {
        uint32_t  x;
        uint32_t  y;
        uint32_t  z;
        int32_t   material;

        bool operator== (const CornerKey & other) const noexcept;
    };


    struct CornerHash
    {
        size_t operator() (const CornerKey & key) const noexcept;
    };


    static CornerKey  KeyOf (const float * position, int material);
};
