#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSvgPath
//
//  SVG path data -- the `d` attribute -- parsed into absolute segments, one
//  list per sub-shape. A sub-shape starts at each moveto, so an icon whose
//  pieces are separate shapes arrives as separate lists and each can be
//  filled in a color of its own.
//
//  Every command SVG defines is read, in either case: relative coordinates
//  become absolute, H and V become lines, S and T become the curves they
//  abbreviate, and a quadratic becomes the cubic it equals. What is left is
//  moves, lines, cubics, arcs and closes, which is what Direct2D draws.
//
//  Nothing here draws. The parse is plain data, so it is tested without a
//  device.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiSvgPoint
{
    float  x = 0.0f;
    float  y = 0.0f;
};



struct DxuiSvgSegment
{
    enum class Kind { Line, Cubic, Arc };

    Kind          kind = Kind::Line;

    //  Line: `to`. Cubic: `c1`, `c2`, `to`. Arc: radii, rotation, the two
    //  flags and `to`.
    DxuiSvgPoint  c1;
    DxuiSvgPoint  c2;
    DxuiSvgPoint  to;
    float         radiusX       = 0.0f;
    float         radiusY       = 0.0f;
    float         rotationDeg   = 0.0f;
    bool          largeArc      = false;
    bool          clockwise     = false;
};



struct DxuiSvgSubpath
{
    DxuiSvgPoint                 start;
    std::vector<DxuiSvgSegment>  segments;
    bool                         closed = false;
};



class DxuiSvgPath
{
public:
    //  The sub-shapes of `d`, in order. A malformed path gives what parsed
    //  before the fault and returns false.
    static bool  Parse (const wchar_t * d, std::vector<DxuiSvgSubpath> & outSubpaths);

private:
    static bool  IsCommand     (wchar_t c);
    static bool  ReadNumber    (const wchar_t *& p, float & outValue);
    static bool  ReadFlag      (const wchar_t *& p, bool & outValue);
    static void  SkipSeparators (const wchar_t *& p);
};
