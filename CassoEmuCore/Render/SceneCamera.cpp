#include "Pch.h"

#include "Render/SceneCamera.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCamera::Identity44
//
////////////////////////////////////////////////////////////////////////////////

void SceneCamera::Identity44 (float out[16])
{
    memset (out, 0, 16 * sizeof (float));
    out[0] = out[5] = out[10] = out[15] = 1.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCamera::Mul44
//
////////////////////////////////////////////////////////////////////////////////

void SceneCamera::Mul44 (const float a[16], const float b[16], float out[16])
{
    for (int r = 0; r < 4; r++)
    {
        for (int c = 0; c < 4; c++)
        {
            out[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c]
                           + a[r * 4 + 1] * b[1 * 4 + c]
                           + a[r * 4 + 2] * b[2 * 4 + c]
                           + a[r * 4 + 3] * b[3 * 4 + c];
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCamera::LookAtRH
//
//  Builds a right-handed view matrix from an eye position and a look-at point.
//
//  Written out rather than pulled from a math library so the scene has no
//  dependency beyond the standard library, and so the handedness is visible in
//  the source instead of implied by whichever library variant was linked --
//  a mismatch there mirrors the whole scene and is maddening to diagnose.
//
//  The up vector is FIXED at (0,1,0), which is why the cross product collapses
//  to two components: with up on the Y axis, cross(up, z) has no Y term at
//  all. This camera never rolls, so the general form would be arithmetic
//  nobody uses.
//
//  The translation row is the negated dot of the eye against each basis
//  vector, which is what makes this the INVERSE of the camera's transform --
//  a view matrix moves the world, not the camera.
//
//  A degenerate eye-equals-target input is not guarded; the scene's camera
//  positions are all computed with nonzero standoff, so it cannot arise here.
//
////////////////////////////////////////////////////////////////////////////////

void SceneCamera::LookAtRH (const float eye[3], const float at[3], float out[16])
{
    float   z[3] = { eye[0] - at[0], eye[1] - at[1], eye[2] - at[2] };
    float   zl   = std::sqrt (z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
    float   x[3] = {};
    float   xl   = 0.0f;
    float   y[3] = {};

    z[0] /= zl; z[1] /= zl; z[2] /= zl;

    // x = normalize(cross(up, z)) with up = (0,1,0)
    x[0] = z[2]; x[1] = 0.0f; x[2] = -z[0];
    xl   = std::sqrt (x[0] * x[0] + x[2] * x[2]);
    x[0] /= xl; x[2] /= xl;

    // y = cross(z, x)
    y[0] = z[1] * x[2] - z[2] * x[1];
    y[1] = z[2] * x[0] - z[0] * x[2];
    y[2] = z[0] * x[1] - z[1] * x[0];

    out[0]  = x[0]; out[1]  = y[0]; out[2]  = z[0]; out[3]  = 0.0f;
    out[4]  = x[1]; out[5]  = y[1]; out[6]  = z[1]; out[7]  = 0.0f;
    out[8]  = x[2]; out[9]  = y[2]; out[10] = z[2]; out[11] = 0.0f;
    out[12] = -(x[0] * eye[0] + x[1] * eye[1] + x[2] * eye[2]);
    out[13] = -(y[0] * eye[0] + y[1] * eye[1] + y[2] * eye[2]);
    out[14] = -(z[0] * eye[0] + z[1] * eye[1] + z[2] * eye[2]);
    out[15] = 1.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCamera::PerspectiveFovRH
//
////////////////////////////////////////////////////////////////////////////////

void SceneCamera::PerspectiveFovRH (float fovY, float aspect, float zn, float zf, float out[16])
{
    float   ys = 1.0f / std::tan (fovY * 0.5f);
    float   xs = ys / aspect;



    memset (out, 0, 16 * sizeof (float));
    out[0]  = xs;
    out[5]  = ys;
    out[10] = zf / (zn - zf);
    out[11] = -1.0f;
    out[14] = zn * zf / (zn - zf);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCamera::Inverse44
//
//  General 4x4 inverse by cofactor expansion (the standard adjugate form).
//  Chosen over Gaussian elimination because it is branch-free, allocation-free,
//  and exact enough for the well-conditioned view/projection matrices this
//  class builds; a singular input returns identity so a caller that ignores
//  the result degrades to a no-op transform instead of NaNs.
//
////////////////////////////////////////////////////////////////////////////////

bool SceneCamera::Inverse44 (const float m[16], float out[16])
{
    float   inv[16] = {};
    float   det     = 0.0f;



    inv[0]  =  m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15]
            +  m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4]  = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15]
            -  m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8]  =  m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15]
            +  m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14]
            -  m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1]  = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15]
            -  m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5]  =  m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15]
            +  m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9]  = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15]
            -  m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] =  m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14]
            +  m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2]  =  m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15]
            +  m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6]  = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15]
            -  m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] =  m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15]
            +  m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14]
            -  m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3]  = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11]
            -  m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7]  =  m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11]
            +  m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11]
            -  m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] =  m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10]
            +  m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0.0f)
    {
        Identity44 (out);
        return false;
    }

    det = 1.0f / det;

    for (int i = 0; i < 16; i++)
    {
        out[i] = inv[i] * det;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCamera::FitContainFovY
//
//  When the viewport is proportionally narrower than the content, the vertical
//  fov must widen so the content's WIDTH still fits: tan(fov'/2) scales by the
//  aspect deficit. A viewport at least as wide as the content contains it
//  already. Mirrors the "contain rather than crop" behavior the printer scene
//  established.
//
////////////////////////////////////////////////////////////////////////////////

float SceneCamera::FitContainFovY (float fovY, float contentAspect, float viewportAspect)
{
    if (viewportAspect >= contentAspect)
    {
        return fovY;
    }

    return 2.0f * std::atan (std::tan (fovY * 0.5f) * contentAspect / viewportAspect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCamera::TransformPoint
//
////////////////////////////////////////////////////////////////////////////////

bool SceneCamera::TransformPoint (const float m[16], const float in[3], float out[3])
{
    float   x = in[0] * m[0] + in[1] * m[4] + in[2] * m[8]  + m[12];
    float   y = in[0] * m[1] + in[1] * m[5] + in[2] * m[9]  + m[13];
    float   z = in[0] * m[2] + in[1] * m[6] + in[2] * m[10] + m[14];
    float   w = in[0] * m[3] + in[1] * m[7] + in[2] * m[11] + m[15];



    if (std::abs (w) < 1e-12f)
    {
        return false;
    }

    out[0] = x / w;
    out[1] = y / w;
    out[2] = z / w;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCamera::TransformVector
//
////////////////////////////////////////////////////////////////////////////////

void SceneCamera::TransformVector (const float m[16], const float in[3], float out[3])
{
    out[0] = in[0] * m[0] + in[1] * m[4] + in[2] * m[8];
    out[1] = in[0] * m[1] + in[1] * m[5] + in[2] * m[9];
    out[2] = in[0] * m[2] + in[1] * m[6] + in[2] * m[10];
}





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCamera::ProjectToScreen
//
//  NDC x runs left->right and y runs bottom->top, while pixel y runs top->
//  bottom -- the y flip below is that convention change, the same one the
//  rasterizer applies via the viewport transform.
//
//  Points at or behind the eye plane report false rather than a mirrored
//  garbage position; the w sign is checked BEFORE the divide because the
//  divided result looks plausible and lands on screen.
//
////////////////////////////////////////////////////////////////////////////////

bool SceneCamera::ProjectToScreen (const float viewProj[16],
                                   const float world[3],
                                   const RECT & viewportPx,
                                   float        outScreenPx[2])
{
    float  w      = world[0] * viewProj[3] + world[1] * viewProj[7] + world[2] * viewProj[11] + viewProj[15];
    float  ndc[3] = {};



    if (w <= 0.0f)
    {
        return false;
    }

    if (!TransformPoint (viewProj, world, ndc))
    {
        return false;
    }

    outScreenPx[0] = viewportPx.left + (ndc[0] * 0.5f + 0.5f) * (viewportPx.right - viewportPx.left);
    outScreenPx[1] = viewportPx.top  + (0.5f - ndc[1] * 0.5f) * (viewportPx.bottom - viewportPx.top);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCamera::ScreenRayFromPx
//
//  Unprojects the pixel at the near plane (NDC z = 0, D3D convention) and at
//  the far plane (z = 1), and forms the ray between them. Using the same
//  inverse matrix for both keeps the ray exactly consistent with
//  ProjectToScreen -- the round trip is what the curved-display accuracy tests
//  pin down.
//
////////////////////////////////////////////////////////////////////////////////

bool SceneCamera::ScreenRayFromPx (const float invViewProj[16],
                                   const RECT & viewportPx,
                                   float        screenX,
                                   float        screenY,
                                   float        outOrigin[3],
                                   float        outDir[3])
{
    float   viewportW = (float) (viewportPx.right - viewportPx.left);
    float   viewportH = (float) (viewportPx.bottom - viewportPx.top);
    float   ndcX      = 0.0f;
    float   ndcY      = 0.0f;
    float   nearPt[3] = {};
    float   farPt[3]  = {};
    float   len       = 0.0f;



    if (viewportW <= 0.0f || viewportH <= 0.0f)
    {
        return false;
    }

    ndcX = ((screenX - viewportPx.left) / viewportW) * 2.0f - 1.0f;
    ndcY = 1.0f - ((screenY - viewportPx.top) / viewportH) * 2.0f;

    {
        float   nearNdc[3] = { ndcX, ndcY, 0.0f };
        float   farNdc[3]  = { ndcX, ndcY, 1.0f };

        if (!TransformPoint (invViewProj, nearNdc, nearPt) ||
            !TransformPoint (invViewProj, farNdc, farPt))
        {
            return false;
        }
    }

    outDir[0] = farPt[0] - nearPt[0];
    outDir[1] = farPt[1] - nearPt[1];
    outDir[2] = farPt[2] - nearPt[2];

    len = std::sqrt (outDir[0] * outDir[0] + outDir[1] * outDir[1] + outDir[2] * outDir[2]);

    if (len < 1e-12f)
    {
        return false;
    }

    outDir[0] /= len;
    outDir[1] /= len;
    outDir[2] /= len;

    outOrigin[0] = nearPt[0];
    outOrigin[1] = nearPt[1];
    outOrigin[2] = nearPt[2];

    return true;
}
