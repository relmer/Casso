#include "Pch.h"

#include "Devices/Printer/MeshBlob.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MeshBlob::VectorHash::operator()
//
//  FNV-1a over the three words. The map is only alive for the length of one
//  bake, so this wants to be cheap rather than strong.
//
////////////////////////////////////////////////////////////////////////////////

size_t MeshBlob::VectorHash::operator() (const VectorKey & key) const noexcept
{
    size_t   hash = 1469598103934665603ull;



    for (uint32_t word : key)
    {
        hash = (hash ^ word) * 1099511628211ull;
    }

    return hash;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeshBlob::MakeKey
//
////////////////////////////////////////////////////////////////////////////////

MeshBlob::VectorKey MeshBlob::MakeKey (const float * value)
{
    VectorKey   key = {};



    memcpy (key.data(), value, sizeof (float) * 3);

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeshBlob::AppendBytes
//
////////////////////////////////////////////////////////////////////////////////

void MeshBlob::AppendBytes (std::vector<uint8_t> & out, const void * data, size_t count)
{
    const uint8_t *   first = static_cast<const uint8_t *> (data);



    out.insert (out.end(), first, first + count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeshBlob::Write
//
//  Shares positions and normals, then writes the header, the per-material
//  colors, the name table, the positions, the normals, and the faces, in that
//  order.
//
//  A material's color is taken from the LAST triangle that names it, and any
//  triangle would do. The parser resolves a material name to a color once and
//  stamps that same color on every triangle in the run, so the material has
//  one color rather than a set of them to choose from.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MeshBlob::Write (const std::vector<ObjTriangle>          & triangles,
                         const std::vector<std::string>          & materialNames,
                         std::span<const std::array<float, 3>>     normals,
                         std::vector<uint8_t>                    & outBytes)
{
    HRESULT                                              hr           = S_OK;
    Header                                               header       = {};
    std::vector<float>                                   positions;
    std::vector<float>                                   normalTable;
    std::vector<uint32_t>                                faces;
    std::vector<float>                                   colors;
    std::vector<uint8_t>                                 names;
    bool                                                 haveNormals  = false;
    bool                                                 normalsMatch = false;
    bool                                                 fits         = false;
    std::unordered_map<VectorKey, uint32_t, VectorHash>  sharedPos;
    std::unordered_map<VectorKey, uint32_t, VectorHash>  sharedNrm;



    outBytes.clear();

    // Either every triangle has three normals or none does. A partial set is
    // a caller bug rather than a shape this format should try to describe.
    haveNormals  = !normals.empty();
    normalsMatch = !haveNormals || normals.size() == triangles.size() * 3;
    CBRA (normalsMatch);

    // One color per material, seeded white so a material no triangle
    // references still round-trips to the white the parser hands back for an
    // unknown one.
    colors.assign (materialNames.size() * 3, 1.0f);

    for (const ObjTriangle & tri : triangles)
    {
        if (tri.material >= 0 && (size_t) tri.material < materialNames.size())
        {
            colors[(size_t) tri.material * 3 + 0] = tri.r;
            colors[(size_t) tri.material * 3 + 1] = tri.g;
            colors[(size_t) tri.material * 3 + 2] = tri.b;
        }
    }

    positions.reserve (triangles.size() * 3);
    faces.reserve (triangles.size() * kFaceWords);

    for (size_t t = 0; t < triangles.size(); t++)
    {
        const ObjTriangle &   tri       = triangles[t];
        const float *         corner[3] = { tri.p0, tri.p1, tri.p2 };

        for (size_t c = 0; c < 3; c++)
        {
            VectorKey   key   = MakeKey (corner[c]);
            auto        found = sharedPos.find (key);

            if (found == sharedPos.end())
            {
                uint32_t   index = (uint32_t) (positions.size() / 3);

                positions.push_back (corner[c][0]);
                positions.push_back (corner[c][1]);
                positions.push_back (corner[c][2]);

                found = sharedPos.emplace (key, index).first;
            }

            faces.push_back (found->second);
        }

        for (size_t c = 0; c < 3; c++)
        {
            uint32_t   index = 0;

            if (haveNormals)
            {
                const float *   n     = normals[t * 3 + c].data();
                VectorKey       key   = MakeKey (n);
                auto            found = sharedNrm.find (key);

                if (found == sharedNrm.end())
                {
                    index = (uint32_t) (normalTable.size() / 3);

                    normalTable.push_back (n[0]);
                    normalTable.push_back (n[1]);
                    normalTable.push_back (n[2]);

                    sharedNrm.emplace (key, index);
                }
                else
                {
                    index = found->second;
                }
            }

            faces.push_back (index);
        }

        faces.push_back ((tri.material < 0) ? kNoMaterial : (uint32_t) tri.material);
    }

    for (const std::string & name : materialNames)
    {
        AppendBytes (names, name.c_str(), name.size() + 1);
    }

    // Pad the name table so the positions after it start on a float boundary;
    // the reader copies those out as words, not as bytes.
    while ((names.size() % 4) != 0)
    {
        names.push_back (0);
    }

    // Every count is written as 32 bits, so a model past four billion of
    // anything is one this format cannot describe. Nothing comes close, the
    // largest shipping mesh being under a million faces, but truncating
    // silently would corrupt the blob where refusing it says so.
    fits = materialNames.size()     <= UINT32_MAX
        && (positions.size() / 3)   <= UINT32_MAX
        && (normalTable.size() / 3) <= UINT32_MAX
        && triangles.size()         <= UINT32_MAX
        && names.size()             <= UINT32_MAX;
    CBRA (fits);

    memcpy (header.magic, kMagic, sizeof (header.magic));

    header.version       = kVersion;
    header.materialCount = (uint32_t) materialNames.size();
    header.positionCount = (uint32_t) (positions.size() / 3);
    header.faceCount     = (uint32_t) triangles.size();
    header.nameBytes     = (uint32_t) names.size();
    header.normalCount   = (uint32_t) (normalTable.size() / 3);

    outBytes.reserve (sizeof (header)
                      + colors.size()      * sizeof (float)
                      + names.size()
                      + positions.size()   * sizeof (float)
                      + normalTable.size() * sizeof (float)
                      + faces.size()       * sizeof (uint32_t));

    AppendBytes (outBytes, &header,            sizeof (header));
    AppendBytes (outBytes, colors.data(),      colors.size()      * sizeof (float));
    AppendBytes (outBytes, names.data(),       names.size());
    AppendBytes (outBytes, positions.data(),   positions.size()   * sizeof (float));
    AppendBytes (outBytes, normalTable.data(), normalTable.size() * sizeof (float));
    AppendBytes (outBytes, faces.data(),       faces.size()       * sizeof (uint32_t));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeshBlob::Read
//
//  Rebuilds the parser's own output: a flat triangle list with each
//  triangle's color already resolved, the material name table those triangles
//  index, and the per-corner normals the baker averaged.
//
//  The length check comes first and covers the whole body, so the expansion
//  loop can index the arrays without re-testing a bound the header already
//  settled. Each face's indices are still checked, because those are data
//  rather than a size, and one past the end would read off an array whose
//  length no header field constrains.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MeshBlob::Read (std::span<const uint8_t>            bytes,
                        std::vector<ObjTriangle>          & outTriangles,
                        std::vector<std::string>          & outMaterialNames,
                        std::vector<std::array<float, 3>> & outNormals)
{
    HRESULT                 hr          = S_OK;
    Header                  header      = {};
    std::vector<float>      colors;
    std::vector<float>      positions;
    std::vector<float>      normalTable;
    std::vector<uint32_t>   faces;
    size_t                  offset      = 0;
    size_t                  colorBytes  = 0;
    size_t                  posBytes    = 0;
    size_t                  normalBytes = 0;
    size_t                  faceBytes   = 0;
    bool                    headerFits  = false;
    bool                    recognized  = false;
    bool                    bodyFits    = false;
    bool                    terminated  = false;
    bool                    indexOk     = false;



    outTriangles.clear();
    outMaterialNames.clear();
    outNormals.clear();

    headerFits = bytes.size() >= sizeof (Header);
    CBRA (headerFits);

    memcpy (&header, bytes.data(), sizeof (Header));

    recognized = memcmp (header.magic, kMagic, sizeof (kMagic)) == 0
              && header.version == kVersion;
    CBRA (recognized);

    colorBytes  = (size_t) header.materialCount * 3 * sizeof (float);
    posBytes    = (size_t) header.positionCount * 3 * sizeof (float);
    normalBytes = (size_t) header.normalCount   * 3 * sizeof (float);
    faceBytes   = (size_t) header.faceCount * kFaceWords * sizeof (uint32_t);

    bodyFits = bytes.size() == sizeof (Header) + colorBytes + header.nameBytes
                             + posBytes + normalBytes + faceBytes;
    CBRA (bodyFits);

    offset = sizeof (Header);

    colors.resize ((size_t) header.materialCount * 3);
    memcpy (colors.data(), bytes.data() + offset, colorBytes);
    offset += colorBytes;

    // The name table is a run of NUL-terminated strings and the header says
    // how many. A table that runs out before the count does is a blob whose
    // header and body disagree.
    for (uint32_t i = 0; i < header.materialCount; i++)
    {
        const char *   first  = reinterpret_cast<const char *> (bytes.data() + offset);
        size_t         room   = sizeof (Header) + colorBytes + header.nameBytes - offset;
        size_t         length = strnlen (first, room);

        terminated = length < room;
        CBRA (terminated);

        outMaterialNames.emplace_back (first, length);
        offset += length + 1;
    }

    offset = sizeof (Header) + colorBytes + header.nameBytes;

    positions.resize ((size_t) header.positionCount * 3);
    memcpy (positions.data(), bytes.data() + offset, posBytes);
    offset += posBytes;

    normalTable.resize ((size_t) header.normalCount * 3);
    memcpy (normalTable.data(), bytes.data() + offset, normalBytes);
    offset += normalBytes;

    faces.resize ((size_t) header.faceCount * kFaceWords);
    memcpy (faces.data(), bytes.data() + offset, faceBytes);

    outTriangles.resize (header.faceCount);

    if (header.normalCount > 0)
    {
        outNormals.resize ((size_t) header.faceCount * 3);
    }

    for (size_t f = 0; f < header.faceCount; f++)
    {
        ObjTriangle     & tri       = outTriangles[f];
        const uint32_t  * face      = faces.data() + f * kFaceWords;
        uint32_t          material  = face[6];
        float           * corner[3] = { tri.p0, tri.p1, tri.p2 };

        for (size_t c = 0; c < 3; c++)
        {
            uint32_t   index = face[c];

            indexOk = index < header.positionCount;
            CBRA (indexOk);

            memcpy (corner[c], positions.data() + (size_t) index * 3, sizeof (float) * 3);
        }

        if (header.normalCount > 0)
        {
            for (size_t c = 0; c < 3; c++)
            {
                uint32_t   index = face[3 + c];

                indexOk = index < header.normalCount;
                CBRA (indexOk);

                memcpy (outNormals[f * 3 + c].data(),
                        normalTable.data() + (size_t) index * 3, sizeof (float) * 3);
            }
        }

        if (material == kNoMaterial)
        {
            tri.material = -1;
        }
        else
        {
            indexOk = material < header.materialCount;
            CBRA (indexOk);

            tri.material = (int) material;
            tri.r        = colors[(size_t) material * 3 + 0];
            tri.g        = colors[(size_t) material * 3 + 1];
            tri.b        = colors[(size_t) material * 3 + 2];
        }
    }

Error:
    if (FAILED (hr))
    {
        outTriangles.clear();
        outMaterialNames.clear();
        outNormals.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeshBlob::Read
//
//  The shape-only overload.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MeshBlob::Read (std::span<const uint8_t>     bytes,
                        std::vector<ObjTriangle>   & outTriangles,
                        std::vector<std::string>   & outMaterialNames)
{
    std::vector<std::array<float, 3>>   ignored;



    return Read (bytes, outTriangles, outMaterialNames, ignored);
}
