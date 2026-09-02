#include "Pch.h"

#include "Devices/Printer/MeshNormals.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MeshNormals::CornerKey::operator==
//
////////////////////////////////////////////////////////////////////////////////

bool MeshNormals::CornerKey::operator== (const CornerKey & other) const noexcept
{
    return x == other.x && y == other.y && z == other.z && material == other.material;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeshNormals::CornerHash::operator()
//
//  FNV-1a over the position words and the material. The map lives only for
//  one bake, so this wants to be cheap rather than strong.
//
////////////////////////////////////////////////////////////////////////////////

size_t MeshNormals::CornerHash::operator() (const CornerKey & key) const noexcept
{
    size_t   hash = 1469598103934665603ull;



    for (uint32_t word : { key.x, key.y, key.z, (uint32_t) key.material })
    {
        hash = (hash ^ word) * 1099511628211ull;
    }

    return hash;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeshNormals::MakeKey
//
////////////////////////////////////////////////////////////////////////////////

MeshNormals::CornerKey MeshNormals::MakeKey (const float * position, int material)
{
    CornerKey   key = {};



    memcpy (&key.x, position + 0, sizeof (uint32_t));
    memcpy (&key.y, position + 1, sizeof (uint32_t));
    memcpy (&key.z, position + 2, sizeof (uint32_t));
    key.material = material;

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeshNormals::FaceNormal
//
////////////////////////////////////////////////////////////////////////////////

std::array<float, 3> MeshNormals::FaceNormal (const ObjTriangle & tri)
{
    float   e1[3] = { tri.p1[0] - tri.p0[0], tri.p1[1] - tri.p0[1], tri.p1[2] - tri.p0[2] };
    float   e2[3] = { tri.p2[0] - tri.p0[0], tri.p2[1] - tri.p0[1], tri.p2[2] - tri.p0[2] };



    return std::array<float, 3> { e1[1] * e2[2] - e1[2] * e2[1],
                                  e1[2] * e2[0] - e1[0] * e2[2],
                                  e1[0] * e2[1] - e1[1] * e2[0] };
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeshNormals::Compute
//
//  Two passes. The first buckets every corner by position and material, which
//  is what makes a vertex "shared". The second walks each corner again and
//  averages the faces in its bucket that lie within the smoothing angle OF
//  THIS FACE.
//
//  The test is per FACE, not per bucket. A bucket at the apex of a cone holds
//  faces spanning every direction, and asking "is this bucket smooth" would
//  have to answer once for all of them; asking instead which of its neighbors
//  each face agrees with lets one corner be smooth along the cone and hard
//  across the seam where it closes.
//
//  Area weighting falls out of leaving the face normals unnormalized -- a
//  cross product's length is twice the triangle's area -- so a long thin
//  sliver does not pull a shared normal as hard as the broad face beside it.
//  The comparison against the smoothing angle IS normalized, because that is
//  a question about direction alone.
//
////////////////////////////////////////////////////////////////////////////////

void MeshNormals::Compute (const std::vector<ObjTriangle>     & triangles,
                           float                               smoothingDeg,
                           std::vector<std::array<float, 3>> & outNormals)
{
    std::vector<std::array<float, 3>>                        faceNormals;
    std::vector<std::array<float, 3>>                        faceUnit;
    std::unordered_map<CornerKey, std::vector<uint32_t>, CornerHash>  buckets;
    float                                                    cosLimit = 0.0f;



    outNormals.assign (triangles.size() * 3, std::array<float, 3> { 0.0f, 0.0f, 0.0f });

    cosLimit = std::cos (smoothingDeg * 3.14159265358979323846f / 180.0f);

    faceNormals.reserve (triangles.size());
    faceUnit.reserve (triangles.size());

    for (const ObjTriangle & tri : triangles)
    {
        std::array<float, 3>   n      = FaceNormal (tri);
        float                  length = std::sqrt (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);

        faceNormals.push_back (n);

        if (length > 0.0f)
        {
            faceUnit.push_back (std::array<float, 3> { n[0] / length, n[1] / length, n[2] / length });
        }
        else
        {
            faceUnit.push_back (std::array<float, 3> { 0.0f, 0.0f, 0.0f });
        }
    }

    for (size_t t = 0; t < triangles.size(); t++)
    {
        const ObjTriangle &   tri       = triangles[t];
        const float *         corner[3] = { tri.p0, tri.p1, tri.p2 };

        for (size_t c = 0; c < 3; c++)
        {
            buckets[MakeKey (corner[c], tri.material)].push_back ((uint32_t) t);
        }
    }

    for (size_t t = 0; t < triangles.size(); t++)
    {
        const ObjTriangle &   tri       = triangles[t];
        const float *         corner[3] = { tri.p0, tri.p1, tri.p2 };

        for (size_t c = 0; c < 3; c++)
        {
            const std::vector<uint32_t>  & share  = buckets[MakeKey (corner[c], tri.material)];
            std::array<float, 3>           sum    = { 0.0f, 0.0f, 0.0f };
            float                          length = 0.0f;

            for (uint32_t other : share)
            {
                float   dot = faceUnit[t][0] * faceUnit[other][0]
                            + faceUnit[t][1] * faceUnit[other][1]
                            + faceUnit[t][2] * faceUnit[other][2];

                if (dot >= cosLimit)
                {
                    sum[0] += faceNormals[other][0];
                    sum[1] += faceNormals[other][1];
                    sum[2] += faceNormals[other][2];
                }
            }

            length = std::sqrt (sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);

            // Opposed neighbors can cancel to nothing; that corner keeps its
            // own face normal rather than going unlit.
            if (length <= 0.0f)
            {
                sum    = faceNormals[t];
                length = std::sqrt (sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
            }

            if (length > 0.0f)
            {
                outNormals[t * 3 + c] = std::array<float, 3> { sum[0] / length,
                                                               sum[1] / length,
                                                               sum[2] / length };
            }
        }
    }
}
