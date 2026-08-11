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
////////////////////////////////////////////////////////////////////////////////

//
//  The glass sheet in model space: X right, Z up, sag along -Y from the front
//  plane at `baseY`. The sag is a section of a sphere whose radius the model
//  generator derives from the glass diagonal.
//
struct CurvedDisplaySurface
{
    float  x0      = 0.0f;   // glass bounding rect, model space
    float  x1      = 0.0f;
    float  z0      = 0.0f;
    float  z1      = 0.0f;
    float  baseY   = 0.0f;   // front plane the sag is relative to
    float  radius  = 0.0f;   // sag sphere radius
};


class CurvedDisplayMath
{
public:
};
