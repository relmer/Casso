#pragma once

#include "Pch.h"

#include "Devices/Printer/ObjMeshParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MeshBlob
//
//  The BAKED form of a model, and the only form the shipping app parses.
//
//  ObjMeshParser reads a modeling tool's text export, which is the right
//  input for a generator and the wrong one for a launch: the Monitor II is
//  1.3 million lines of ASCII, and turning them back into floats cost most of
//  Casso's startup. The text stays the reviewed source under Resources, the
//  build bakes it once, and the app reads what the parser would have
//  produced.
//
//  Indexed, because the flat triangle list is the wrong thing to store. An
//  ObjTriangle carries its three positions and its color outright, so writing
//  the parser's output verbatim would be larger than the text it replaces.
//  Positions are shared here and each face names three of them, which is how
//  the model was written before triangulation flattened it.
//
//  Color lives PER MATERIAL rather than per triangle. The parser bakes a
//  triangle's color from whichever material was active when its face was
//  declared, and that lookup is a function of the material alone, so storing
//  it per triangle stores the same three floats a hundred thousand times.
//  Unpacking restores exactly what the parser produced, including the white
//  a face gets when it names a material the MTL never defined.
//
//  Read is a build-defect check, not input validation: the blob is compiled
//  into the executable, so a header that does not agree with its own body
//  means the baker and the loader disagree, which no user can cause and no
//  runtime recovery can help.
//
////////////////////////////////////////////////////////////////////////////////

class MeshBlob
{
public:

    // Bumped whenever the layout below changes. Read refuses anything else
    // rather than misreading a stale blob as a current one.
    static constexpr uint32_t  kVersion = 1;

    // Packs `triangles` and their `materialNames` into `outBytes`, sharing
    // positions that are bit-identical. Lossless: Read returns triangles
    // equal to these, field for field.
    static HRESULT  Write (const std::vector<ObjTriangle>   & triangles,
                           const std::vector<std::string>   & materialNames,
                           std::vector<uint8_t>             & outBytes);

    // Unpacks a blob written by Write. Fails, and asserts, on a truncated or
    // mislabeled blob.
    static HRESULT  Read  (std::span<const uint8_t>           bytes,
                           std::vector<ObjTriangle>         & outTriangles,
                           std::vector<std::string>         & outMaterialNames);

private:

    // Position sharing is by BIT PATTERN, not by proximity. Two vertices the
    // generator emitted from one corner print identical text and parse to
    // identical floats, so exact equality already recovers the sharing the OBJ
    // had before triangulation flattened it. Welding within a tolerance would
    // be a different operation with a different name, and it would change the
    // mesh rather than repack it.
    using PositionKey = std::array<uint32_t, 3>;


    struct PositionHash
    {
        size_t operator() (const PositionKey & key) const noexcept;
    };


    // The three coordinates as the words that spell them, so the map keys on
    // what was written rather than on what compares equal.
    static PositionKey  KeyOf       (const float * position);

    static void         AppendBytes (std::vector<uint8_t> & out,
                                     const void           * data,
                                     size_t                 count);


    // Eight bytes so the tag survives a hex dump, then the counts. Every
    // field is fixed-width and little-endian, which both target
    // architectures are.
    struct Header
    {
        char      magic[8];        // kMagic, not NUL-terminated
        uint32_t  version;
        uint32_t  materialCount;
        uint32_t  positionCount;
        uint32_t  faceCount;
        uint32_t  nameBytes;       // NUL-terminated names, concatenated
        uint32_t  reserved;        // zero; keeps the body 8-byte aligned
    };

    static constexpr char      kMagic[8]     = { 'C', 'A', 'S', 'S', 'O', 'M', 'S', 'H' };

    // A face whose material index is this belongs to no material, which is
    // ObjTriangle::material == -1: a triangle declared before any usemtl.
    static constexpr uint32_t  kNoMaterial   = 0xFFFFFFFFu;
};
