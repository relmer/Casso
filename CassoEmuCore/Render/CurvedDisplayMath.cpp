#include "Pch.h"

#include "Render/CurvedDisplayMath.h"

#include "Render/SceneCamera.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CurvedDisplayMath::IsValid
//
////////////////////////////////////////////////////////////////////////////////

bool CurvedDisplayMath::IsValid (const CurvedDisplaySurface & surface)
{
    float   halfW    = (surface.x1 - surface.x0) * 0.5f;
    float   halfH    = (surface.z1 - surface.z0) * 0.5f;
    float   halfDiag = std::sqrt (halfW * halfW + halfH * halfH);



    return halfW > 0.0f && halfH > 0.0f && surface.radius >= halfDiag;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CurvedDisplayMath::MaxSag
//
//  The sag depth at the rect center relative to the corners: the corners sit
//  on the front plane (`baseY`) at half-diagonal distance from center, so the
//  center is proud by R - sqrt(R^2 - halfDiag^2).
//
////////////////////////////////////////////////////////////////////////////////

float CurvedDisplayMath::MaxSag (const CurvedDisplaySurface & surface)
{
    float   halfW    = (surface.x1 - surface.x0) * 0.5f;
    float   halfH    = (surface.z1 - surface.z0) * 0.5f;
    float   diagSq   = halfW * halfW + halfH * halfH;
    float   radiusSq = surface.radius * surface.radius;



    return surface.radius - std::sqrt (std::max (radiusSq - diagSq, 0.0f));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CurvedDisplayMath::SphereCenterY
//
//  The center point of the glass sits at baseY - maxSag and is the sphere's
//  nearest point to the viewer, so the center lies exactly one radius behind
//  it along +Y.
//
////////////////////////////////////////////////////////////////////////////////

float CurvedDisplayMath::SphereCenterY (const CurvedDisplaySurface & surface)
{
    return surface.baseY - MaxSag (surface) + surface.radius;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CurvedDisplayMath::ComputePictureBand
//
//  Aspect-fit of the display grid into the glass rect, centered. A glass
//  wider than the picture pillarboxes (u band inset, v full); a taller one
//  letterboxes.
//
////////////////////////////////////////////////////////////////////////////////

void CurvedDisplayMath::ComputePictureBand (const CurvedDisplaySurface & surface,
                                            int                          displayW,
                                            int                          displayH,
                                            float                      & outU0,
                                            float                      & outV0,
                                            float                      & outU1,
                                            float                      & outV1)
{
    float   glassAspect = (surface.x1 - surface.x0) / (surface.z1 - surface.z0);
    float   picAspect   = (float) displayW / (float) displayH;



    outU0 = 0.0f;
    outV0 = 0.0f;
    outU1 = 1.0f;
    outV1 = 1.0f;

    if (picAspect <= glassAspect)
    {
        float   bandW = picAspect / glassAspect;

        outU0 = (1.0f - bandW) * 0.5f;
        outU1 = outU0 + bandW;
    }
    else
    {
        float   bandH = glassAspect / picAspect;

        outV0 = (1.0f - bandH) * 0.5f;
        outV1 = outV0 + bandH;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CurvedDisplayMath::UvFromModelPoint
//
////////////////////////////////////////////////////////////////////////////////

void CurvedDisplayMath::UvFromModelPoint (const CurvedDisplaySurface & surface,
                                          const float                  modelPt[3],
                                          float                      & outU,
                                          float                      & outV)
{
    outU = (modelPt[0] - surface.x0) / (surface.x1 - surface.x0);
    outV = (surface.z1 - modelPt[2]) / (surface.z1 - surface.z0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CurvedDisplayMath::ModelPointFromUv
//
//  The y coordinate is recomputed from the sphere rather than interpolated,
//  so a point built from any UV lies exactly ON the sag surface -- that is
//  what lets the forward transform serve as the oracle for the ray inverse.
//
////////////////////////////////////////////////////////////////////////////////

void CurvedDisplayMath::ModelPointFromUv (const CurvedDisplaySurface & surface,
                                          float                        u,
                                          float                        v,
                                          float                        outModelPt[3])
{
    float   cx       = (surface.x0 + surface.x1) * 0.5f;
    float   cz       = (surface.z0 + surface.z1) * 0.5f;
    float   x        = surface.x0 + u * (surface.x1 - surface.x0);
    float   z        = surface.z1 - v * (surface.z1 - surface.z0);
    float   dx       = x - cx;
    float   dz       = z - cz;
    float   radiusSq = surface.radius * surface.radius;
    float   under    = std::max (radiusSq - dx * dx - dz * dz, 0.0f);



    outModelPt[0] = x;
    outModelPt[1] = SphereCenterY (surface) - std::sqrt (under);
    outModelPt[2] = z;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CurvedDisplayMath::IntersectRay
//
//  Standard ray-sphere quadratic against the sag sphere, keeping the nearer
//  root first: for a viewer in front of the glass that root IS the front
//  surface, and the far root only matters for rays that graze past the sheet
//  (which the rect test rejects anyway). The accepted hit is clamped onto the
//  rect so an edge hit lands on an edge UV rather than a hair outside it.
//
////////////////////////////////////////////////////////////////////////////////

bool CurvedDisplayMath::IntersectRay (const CurvedDisplaySurface & surface,
                                      const float                  rayOrigin[3],
                                      const float                  rayDir[3],
                                      float                        outModelPt[3])
{
    float   cx      = (surface.x0 + surface.x1) * 0.5f;
    float   cy      = SphereCenterY (surface);
    float   cz      = (surface.z0 + surface.z1) * 0.5f;
    float   oc[3]   = { rayOrigin[0] - cx, rayOrigin[1] - cy, rayOrigin[2] - cz };
    float   a       = rayDir[0] * rayDir[0] + rayDir[1] * rayDir[1] + rayDir[2] * rayDir[2];
    float   b       = 2.0f * (oc[0] * rayDir[0] + oc[1] * rayDir[1] + oc[2] * rayDir[2]);
    float   c       = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - surface.radius * surface.radius;
    float   disc    = b * b - 4.0f * a * c;
    float   sqrtD   = 0.0f;
    float   slopX   = (surface.x1 - surface.x0) * kEdgeSlopFrac;
    float   slopZ   = (surface.z1 - surface.z0) * kEdgeSlopFrac;



    if (a <= 0.0f || disc < 0.0f)
    {
        return false;
    }

    sqrtD = std::sqrt (disc);

    // Near root, then far root: the first positive hit that lands on the
    // glass rect (front hemisphere only -- the sheet is the part of the
    // sphere facing the viewer) wins.
    for (int root = 0; root < 2; root++)
    {
        float  t     = (root == 0) ? (-b - sqrtD) / (2.0f * a) : (-b + sqrtD) / (2.0f * a);
        float  pt[3] = {};

        if (t <= 0.0f)
        {
            continue;
        }

        pt[0] = rayOrigin[0] + t * rayDir[0];
        pt[1] = rayOrigin[1] + t * rayDir[1];
        pt[2] = rayOrigin[2] + t * rayDir[2];

        // Front hemisphere only: the glass is the part of the sphere nearer
        // the viewer than its center.
        if (pt[1] >= cy)
        {
            continue;
        }

        if (pt[0] < surface.x0 - slopX || pt[0] > surface.x1 + slopX ||
            pt[2] < surface.z0 - slopZ || pt[2] > surface.z1 + slopZ)
        {
            continue;
        }

        outModelPt[0] = std::clamp (pt[0], surface.x0, surface.x1);
        outModelPt[1] = pt[1];
        outModelPt[2] = std::clamp (pt[2], surface.z0, surface.z1);

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CurvedDisplayMath::EmulatedPixelFromScreenPx
//
//  The full input chain, every step shared with the forward transform: pixel
//  -> world ray (inverse view-proj), world -> model (inverse world -- rigid +
//  uniform scale, so a general 4x4 inverse is exact), sphere intersection, UV,
//  pixel grid. UV is clamped before the grid quantize so a hit at the visible
//  glass edge maps to the outermost pixel instead of one past it.
//
////////////////////////////////////////////////////////////////////////////////

bool CurvedDisplayMath::EmulatedPixelFromScreenPx (const CurvedDisplaySurface & surface,
                                                   const float                  world[16],
                                                   const float                  viewProj[16],
                                                   const RECT                 & viewportPx,
                                                   float                        screenX,
                                                   float                        screenY,
                                                   int                          displayW,
                                                   int                          displayH,
                                                   POINT                      & outPixel)
{
    float   invViewProj[16] = {};
    float   invWorld[16]    = {};
    float   origin[3]       = {};
    float   dir[3]          = {};
    float   modelOrigin[3]  = {};
    float   modelDir[3]     = {};
    float   modelHit[3]     = {};
    float   u               = 0.0f;
    float   v               = 0.0f;



    if (displayW <= 0 || displayH <= 0 || !IsValid (surface))
    {
        return false;
    }

    if (!SceneCamera::Inverse44 (viewProj, invViewProj) ||
        !SceneCamera::Inverse44 (world, invWorld))
    {
        return false;
    }

    if (!SceneCamera::ScreenRayFromPx (invViewProj, viewportPx, screenX, screenY, origin, dir))
    {
        return false;
    }

    if (!SceneCamera::TransformPoint (invWorld, origin, modelOrigin))
    {
        return false;
    }

    SceneCamera::TransformVector (invWorld, dir, modelDir);

    if (!IntersectRay (surface, modelOrigin, modelDir, modelHit))
    {
        return false;
    }

    UvFromModelPoint (surface, modelHit, u, v);

    // Map through the picture band: only the raster area counts as display.
    // Glass outside the band (the tube's dark margins) reports a miss, with
    // a hair of slop so a hit on the visible picture edge lands on the
    // outermost pixel instead of missing.
    {
        float   bandU0 = 0.0f;
        float   bandV0 = 0.0f;
        float   bandU1 = 1.0f;
        float   bandV1 = 1.0f;

        ComputePictureBand (surface, displayW, displayH, bandU0, bandV0, bandU1, bandV1);

        u = (u - bandU0) / (bandU1 - bandU0);
        v = (v - bandV0) / (bandV1 - bandV0);

        if (u < -kEdgeSlopFrac || u > 1.0f + kEdgeSlopFrac ||
            v < -kEdgeSlopFrac || v > 1.0f + kEdgeSlopFrac)
        {
            return false;
        }
    }

    u = std::clamp (u, 0.0f, 1.0f);
    v = std::clamp (v, 0.0f, 1.0f);

    outPixel.x = std::clamp ((LONG) (u * displayW), (LONG) 0, (LONG) (displayW - 1));
    outPixel.y = std::clamp ((LONG) (v * displayH), (LONG) 0, (LONG) (displayH - 1));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CurvedDisplayMath::ScreenPxFromEmulatedPixel
//
//  The exact forward transform through the pixel CENTER, so quantizing the
//  round trip lands back on the same pixel -- the oracle the inverse's
//  accuracy tests are written against.
//
////////////////////////////////////////////////////////////////////////////////

bool CurvedDisplayMath::ScreenPxFromEmulatedPixel (const CurvedDisplaySurface & surface,
                                                   const float                  world[16],
                                                   const float                  viewProj[16],
                                                   const RECT                 & viewportPx,
                                                   int                          pixelX,
                                                   int                          pixelY,
                                                   int                          displayW,
                                                   int                          displayH,
                                                   float                        outScreenPx[2])
{
    float   u          = 0.0f;
    float   v          = 0.0f;
    float   modelPt[3] = {};
    float   worldPt[3] = {};



    if (displayW <= 0 || displayH <= 0 || !IsValid (surface))
    {
        return false;
    }

    {
        float   bandU0 = 0.0f;
        float   bandV0 = 0.0f;
        float   bandU1 = 1.0f;
        float   bandV1 = 1.0f;

        ComputePictureBand (surface, displayW, displayH, bandU0, bandV0, bandU1, bandV1);

        u = bandU0 + (((float) pixelX + 0.5f) / (float) displayW) * (bandU1 - bandU0);
        v = bandV0 + (((float) pixelY + 0.5f) / (float) displayH) * (bandV1 - bandV0);
    }

    ModelPointFromUv (surface, u, v, modelPt);

    if (!SceneCamera::TransformPoint (world, modelPt, worldPt))
    {
        return false;
    }

    return SceneCamera::ProjectToScreen (viewProj, worldPt, viewportPx, outScreenPx);
}
