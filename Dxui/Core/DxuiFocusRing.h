#pragma once

#include "Pch.h"


class IDxuiPainter;
class IDxuiTextRenderer;
class DxuiDpiScaler;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFocusRing
//
//  The keyboard focus ring for controls that are NOT a filled rectangle.
//
//  A button, a dropdown, a text input and a tab are boxes, and a box gets a
//  box: those keep drawing their own square outline on their own bounds, which
//  traces the shape the user already sees.
//
//  Everything else is a small glyph beside a run of words -- a checkbox, a
//  radio option, a toggle, a link. Two things went wrong there. The ring
//  enclosed only the glyph, so the control that got focus and the control the
//  user was reading looked like different things; and it was square, which
//  around text reads as a text field the words sit inside rather than as a
//  mark of focus. So the ring here is ROUNDED and encloses the whole control,
//  glyph and label together.
//
//  It has to be MEASURED, not taken from the bounds: a checkbox row or a link
//  row is as wide as the layout's column, which is nothing to do with how wide
//  its label is. Fitting the ring to the box would put a 400 DIP frame around
//  four words, and reach into whatever is on the next row.
//
//  The padding, corner radius and stroke live here and nowhere else, so the
//  rings on four different widgets stay the same ring.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiFocusRing
{
public:
    // A ring just outside the given content box, in pixels.
    static void  Around    (IDxuiPainter        & painter,
                            float                 contentLeftPx,
                            float                 contentTopPx,
                            float                 contentWidthPx,
                            float                 contentHeightPx,
                            const DxuiDpiScaler & scaler,
                            uint32_t              argbColor);

    // A ring around a glyph-plus-label control. `contentLeftPx` is where the
    // control starts (the checkbox's box, the radio's circle, the toggle's
    // pill) and `runLeftPx` is where its text starts; pass the same value for
    // both when the control is only text, as a link is.
    //
    // `minHeightPx` is the glyph's own height, so the ring still clears the
    // glyph when the label measures shorter than it -- or when there is no
    // label at all, in which case the ring is the glyph.
    //
    // The ring centers on [bandTopPx, bandTopPx + bandHeightPx), which is the
    // control's first line rather than its whole box: a described radio option
    // is two lines tall and the ring belongs on the line with the circle.
    static void  AroundRun (IDxuiPainter        & painter,
                            IDxuiTextRenderer   & text,
                            const std::wstring  & run,
                            float                 fontDip,
                            const wchar_t       * fontFamily,
                            float                 contentLeftPx,
                            float                 runLeftPx,
                            float                 bandTopPx,
                            float                 bandHeightPx,
                            float                 minHeightPx,
                            const DxuiDpiScaler & scaler,
                            uint32_t              argbColor);
};
