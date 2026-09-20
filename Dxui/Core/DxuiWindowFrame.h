#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowFrame
//
//  A window rect and the frame the user actually sees are not the same
//  rectangle. A window with a resize border carries an invisible margin --
//  9px a side at 150% -- that the OS includes in the window rect and does not
//  draw: a window snapped to a screen edge overhangs the region by exactly
//  that, so the frame that IS drawn fills it.
//
//  Placement went wrong at every turn while it was stored as window rects.
//  The same saved numbers mean a filled half on one window and a 9px margin
//  on another, the difference shows up only on screen, and no amount of
//  arithmetic over work areas recovers which was meant.
//
//  So placement is stored and compared as the VISIBLE rect, which is what the
//  user arranged, and turned back into a window rect against the live window
//  that is about to be placed. Both directions are here, and nothing else
//  needs to know that the two rectangles differ.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiWindowFrame
{
public:

    //  What the user sees: the DWM frame bounds, or the window rect where DWM
    //  has no answer (a window that is not composited, or one still being
    //  created).
    static RECT  GetVisibleRect (HWND hwnd);

    //  The invisible margin this window carries on each side: window rect
    //  minus visible rect, never negative.
    static RECT  GetBorder      (HWND hwnd);

    //  The window rect that puts this window's VISIBLE frame at `visible`.
    static RECT  ToWindowRect   (HWND hwnd, const RECT & visible);
};
