#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiVectorIcon
//
//  An icon drawn from SVG path data in two tones, the way File Explorer's
//  command bar draws its icons: an outline in the foreground ink with one
//  part in the accent -- the plus inside New's ring, the arrow Sort points
//  down with.
//
//  The path is one `d` string whose sub-shapes arrive in order. A layer
//  picks some of them by index and fills them together, which matters: a
//  ring is an outer and an inner sub-shape cut by the winding rule, and
//  filled apart the inner one would be a solid disc. A layer may also keep
//  to a horizontal band of the icon, for an icon whose accent part shares a
//  sub-shape with the rest, like scissors whose handle loops and blades are
//  one outline.
//
//  Everything is constant data, so an icon set is a header of these.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiVectorIconLayer
{
    uint32_t  subpaths   = 0;       // bit n set: sub-shape n is in this layer
    bool      accent     = false;   // accent ink, else the foreground's
    float     bandTop    = 0.0f;    // in icon units; top == bottom means the whole icon
    float     bandBottom = 0.0f;
};



struct DxuiVectorIcon
{
    const wchar_t              * pathData   = nullptr;   // SVG `d`
    float                        box        = 20.0f;     // the square the path is drawn in
    const DxuiVectorIconLayer  * layers     = nullptr;
    size_t                       layerCount = 0;
};
