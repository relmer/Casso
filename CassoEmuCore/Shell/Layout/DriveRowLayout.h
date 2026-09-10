#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveRowLayout
//
//  Where the drive widgets sit along the bottom of the window.
//
//  The arithmetic is small and every part of it has been wrong at least once,
//  in ways that read as a row hanging off to one side rather than as anything
//  obviously broken. It is here, taking its inputs, so a test can say where a
//  single drive lands instead of someone having to look at a //c and decide
//  whether it seems centered.
//
////////////////////////////////////////////////////////////////////////////////

class DriveRowLayout
{
public:

    //  Left edge of the row.
    //
    //  Centered on the drives that will actually SHOW, not on the array. A //c
    //  with its external drive unconnected lays out both and hides the second
    //  immediately after, so centering on two left the one visible drive
    //  sitting left of center by half a widget and a gap.
    static int  ComputeRowOriginX (int clientWidthPx,
                                   int widgetWidthPx,
                                   int gapPx,
                                   int visibleCount);

    //  A LONE drive centers on the part that carries the weight -- the disk
    //  name and its head bar -- rather than on the whole widget. The 2D widget
    //  hangs its "DRIVE 1" caption off to the left, so centering the outer box
    //  put the name and bar half a caption column right of center.
    //
    //  Two drives keep centering on the pair: the caption then reads as part of
    //  a repeating unit rather than as a tail on a single object. A widget
    //  whose body starts at its own left edge has no lead and is unaffected.
    static int  ApplyLoneDriveCaptionOffset (int originX,
                                             int captionLeadPx,
                                             int visibleCount);

    //  Left edge of one widget in the row.
    static int  ComputeWidgetX (int originX, int index, int widgetWidthPx, int gapPx);

    //  How far a widget is nudged toward the shared vanishing point at the
    //  client's horizontal center, so drives side by side read as sitting on
    //  one surface under one monitor rather than as repeated sprites.
    static int  ComputePerspectiveSkewPx (int clientWidthPx, int widgetX, int widgetWidthPx);
};
