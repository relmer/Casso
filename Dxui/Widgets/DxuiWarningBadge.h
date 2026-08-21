#pragma once

#include "Pch.h"

#include "Render/IDxuiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWarningBadge
//
//  The warning mark: a filled triangle with an outlined edge and an
//  exclamation inside it. Drawn from primitives rather than an icon font, so
//  it scales exactly and needs no font dependency.
//
//  It lives in Dxui rather than beside its first caller because two places
//  now show it -- the drive widget's damaged-disk badge and the info banner's
//  warning severity -- and two lookalike triangles drawn by two pieces of
//  code would drift. The same mark should mean the same thing.
//
//  Colors are the caller's: the mark is used against a wooden faceplate in
//  one place and a tinted banner in the other, and each needs its own edge
//  treatment to stay legible.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiWarningBadge
{
public:
    //  Draws into the box (left, top, w, h). The triangle fills the box; the
    //  exclamation is centered on its optical middle rather than the box's,
    //  so the mark reads as balanced.
    static void  Draw (IDxuiPainter & painter,
                       float left, float top, float w, float h,
                       uint32_t fill, uint32_t edge, uint32_t mark);
};
