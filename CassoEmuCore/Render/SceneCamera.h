#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SceneCamera
//
//  Row-vector matrix math for the desk scene (clip = v * view * proj), matching
//  Dxui3DRenderer's row_major cbuffer convention. Pure and system-free so every
//  transform the scene relies on -- including the input inverse-projection path
//  -- is unit-testable with no GPU.
//
//  LookAtRH / PerspectiveFovRH / Mul44 are hoisted from Printer3DScene, which
//  now delegates here; the handedness rationale lives with the implementations.
//  Inverse44 and the screen ray/projection helpers are what the curved-display
//  input path adds: the forward transform (world -> screen px) and its inverse
//  (screen px -> world ray) share these matrices, so the two directions cannot
//  drift apart.
//
////////////////////////////////////////////////////////////////////////////////

class SceneCamera
{
public:
    static void  Identity44       (float out[16]);
    static void  Mul44            (const float a[16], const float b[16], float out[16]);
    static void  LookAtRH         (const float eye[3], const float at[3], float out[16]);
    static void  PerspectiveFovRH (float fovY, float aspect, float zn, float zf, float out[16]);

    // General 4x4 inverse by cofactor expansion. Returns false (and identity)
    // for a singular matrix; view and projection matrices built by this class
    // are always invertible.
    static bool  Inverse44        (const float m[16], float out[16]);

    // Widens a vertical field of view so content with `contentAspect` (w/h)
    // stays fully visible in a viewport with `viewportAspect` -- contain, not
    // crop. A viewport at least as wide as the content keeps the fov as-is.
    static float FitContainFovY   (float fovY, float contentAspect, float viewportAspect);

    // world point * viewProj -> pixel position inside `viewportPx`. Returns
    // false when the point is at or behind the eye plane (w <= 0).
    static bool  ProjectToScreen  (const float viewProj[16],
                                   const float world[3],
                                   const RECT & viewportPx,
                                   float        outScreenPx[2]);

    // Pixel position inside `viewportPx` -> world-space ray, using the
    // inverse of the same viewProj that ProjectToScreen consumes. The ray
    // origin is the near-plane unprojection; the direction is normalized.
    static bool  ScreenRayFromPx  (const float invViewProj[16],
                                   const RECT & viewportPx,
                                   float        screenX,
                                   float        screenY,
                                   float        outOrigin[3],
                                   float        outDir[3]);

    // Transforms (x,y,z,1) by row-vector m and divides through by w. Returns
    // false when |w| is degenerate.
    static bool  TransformPoint   (const float m[16], const float in[3], float out[3]);

    // Transforms a direction by the 3x3 part of row-vector m -- no
    // translation, no divide, no normalization.
    static void  TransformVector  (const float m[16], const float in[3], float out[3]);
};
