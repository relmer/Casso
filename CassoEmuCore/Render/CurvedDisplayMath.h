#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CurvedDisplayMath
//
//  The monitor glass and its mappings: a spherical-sag sheet described in model
//  space, with pure functions between glass UV, model points, world rays, and
//  emulated pixels -- both directions, so the input path (screen px -> emulated
//  pixel) and its test oracle (the exact forward transform) share one source of
//  truth. No GPU, no window types.
//
//  Model-space conventions match the generated models: X right, Z up, the
//  front face toward -Y. Glass vertices are displaced along -Y from the front
//  plane at `baseY`, bulging toward the viewer, on a section of a sphere: the
//  rect corners sit exactly at `baseY` and the rect center sits proudest, at
//  `baseY - maxSag`. The sag sphere's center is therefore BEHIND the glass at
//  (cx, baseY - maxSag + radius, cz), which is what makes ray intersection a
//  single quadratic.
//
////////////////////////////////////////////////////////////////////////////////

//
//  The glass sheet in model space. `radius` must be at least the rect's
//  half-diagonal (the generator uses halfDiag * 2.2); everything else is
//  derived on demand.
//
struct CurvedDisplaySurface
{
    float  x0      = 0.0f;   // glass bounding rect, model space
    float  x1      = 0.0f;
    float  z0      = 0.0f;
    float  z1      = 0.0f;
    float  baseY   = 0.0f;   // front plane the sag hangs from (rect corners)
    float  radius  = 0.0f;   // sag sphere radius
};


class CurvedDisplayMath
{
public:
    static bool  IsValid          (const CurvedDisplaySurface & surface);

    static float MaxSag           (const CurvedDisplaySurface & surface);

    // Where the picture lands on the glass, in glass-UV space: the display
    // grid aspect-fits inside the glass rect inset by kBandInsetMm, centered
    // -- like the image on a real tube, whose raster never quite fills the
    // faceplate. Shared by the render path (glass UV remap) and both input
    // directions, so what a texel shows and what a click hits cannot
    // disagree.
    static void  ComputePictureBand (const CurvedDisplaySurface & surface,
                                     int                          displayW,
                                     int                          displayH,
                                     float                      & outU0,
                                     float                      & outV0,
                                     float                      & outU1,
                                     float                      & outV1);

    // Planar XZ mapping between the glass rect and UV space: u runs x0 -> x1,
    // v runs z1 (top) -> z0 (bottom), texture convention. Exact for a
    // spherical-sag sheet, whose vertices are displaced only along Y.
    static void  UvFromModelPoint (const CurvedDisplaySurface & surface,
                                   const float                  modelPt[3],
                                   float                      & outU,
                                   float                      & outV);

    // Inverse of UvFromModelPoint, including the sag: the returned point lies
    // ON the glass surface.
    static void  ModelPointFromUv (const CurvedDisplaySurface & surface,
                                   float                        u,
                                   float                        v,
                                   float                        outModelPt[3]);

    // Intersects a model-space ray with the glass. Solves the sag sphere's
    // quadratic, keeps the front-hemisphere hit, and accepts it when it lands
    // on the glass rect (with a hair of tolerance for float error at the
    // edges, clamped back onto the rect so edge hits map to edge UVs).
    static bool  IntersectRay     (const CurvedDisplaySurface & surface,
                                   const float                  rayOrigin[3],
                                   const float                  rayDir[3],
                                   float                        outModelPt[3]);

    // Screen px -> emulated pixel through the full chain: pixel ray (inverse
    // view-proj), world -> model (inverse world), sphere intersection, UV,
    // pixel grid. Returns false when the pixel's ray misses the glass.
    static bool  EmulatedPixelFromScreenPx (const CurvedDisplaySurface & surface,
                                            const float                  world[16],
                                            const float                  viewProj[16],
                                            const RECT                 & viewportPx,
                                            float                        screenX,
                                            float                        screenY,
                                            int                          displayW,
                                            int                          displayH,
                                            POINT                      & outPixel);

    // The exact forward transform: emulated pixel center -> glass point ->
    // world -> screen px. The oracle for the inverse's round-trip tests.
    static bool  ScreenPxFromEmulatedPixel (const CurvedDisplaySurface & surface,
                                            const float                  world[16],
                                            const float                  viewProj[16],
                                            const RECT                 & viewportPx,
                                            int                          pixelX,
                                            int                          pixelY,
                                            int                          displayW,
                                            int                          displayH,
                                            float                        outScreenPx[2]);

    // The faceplate border kept around the raster: the band aspect-fits into
    // the glass rect inset by this much per side (capped for tiny synthetic
    // surfaces by kBandInsetMaxFrac). Overlays hugging the band from outside
    // must stand off by less than this, or they reach the picture.
    static constexpr float  kBandInsetMm      = 6.0f;
    static constexpr float  kBandInsetMaxFrac = 0.1f;

private:
    // Fraction of each rect extent accepted (then clamped) beyond the exact
    // edge, absorbing float error so a click on the visible glass edge never
    // reports a miss.
    static constexpr float  kEdgeSlopFrac = 1e-4f;

    static float SphereCenterY    (const CurvedDisplaySurface & surface);
};
