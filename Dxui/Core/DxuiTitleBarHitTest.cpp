#include "Pch.h"

#include "DxuiTitleBarHitTest.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    bool  PointInRect (int x, int y, int l, int t, int r, int b)
    {
        // Empty (collapsed) rect contains nothing.
        if (r <= l || b <= t)
        {
            return false;
        }
        return x >= l && x < r && y >= t && y < b;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTitleBarHitTest::Test
//
//  Returns an HT* constant (defined in <winuser.h>):
//      HTCAPTION, HTCLIENT, HTMINBUTTON, HTMAXBUTTON, HTCLOSE,
//      HTLEFT, HTRIGHT, HTTOP, HTBOTTOM,
//      HTTOPLEFT, HTTOPRIGHT, HTBOTTOMLEFT, HTBOTTOMRIGHT.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiTitleBarHitTest::Test (const DxuiTitleBarHitTestInput & in)
{
    bool    fLeftEdge   = false;
    bool    fRightEdge  = false;
    bool    fTopEdge    = false;
    bool    fBottomEdge = false;
    bool    fInTitle    = false;


    // ---- 1. Resize-edge zones (highest priority) ----------------------
    fLeftEdge   = in.mouseX >= 0                               && in.mouseX <  in.resizeBorderPx;
    fRightEdge  = in.mouseX >= in.clientWidth - in.resizeBorderPx && in.mouseX <  in.clientWidth;
    fTopEdge    = in.mouseY >= 0                               && in.mouseY <  in.resizeBorderPx;
    fBottomEdge = in.mouseY >= in.clientHeight - in.resizeBorderPx && in.mouseY <  in.clientHeight;

    if (fTopEdge    && fLeftEdge)  return HTTOPLEFT;
    if (fTopEdge    && fRightEdge) return HTTOPRIGHT;
    if (fBottomEdge && fLeftEdge)  return HTBOTTOMLEFT;
    if (fBottomEdge && fRightEdge) return HTBOTTOMRIGHT;
    if (fTopEdge)                  return HTTOP;
    if (fBottomEdge)               return HTBOTTOM;
    if (fLeftEdge)                 return HTLEFT;
    if (fRightEdge)                return HTRIGHT;

    // ---- 2. System buttons (priority over caption) --------------------
    if (PointInRect (in.mouseX, in.mouseY,
                     in.closeLeft, in.closeTop, in.closeRight, in.closeBottom))
    {
        return HTCLOSE;
    }
    if (PointInRect (in.mouseX, in.mouseY,
                     in.maxLeft, in.maxTop, in.maxRight, in.maxBottom))
    {
        return HTMAXBUTTON;
    }
    if (PointInRect (in.mouseX, in.mouseY,
                     in.minLeft, in.minTop, in.minRight, in.minBottom))
    {
        return HTMINBUTTON;
    }

    // ---- 3. Title-bar drag region -------------------------------------
    fInTitle = PointInRect (in.mouseX, in.mouseY,
                            in.titleLeft, in.titleTop,
                            in.titleRight, in.titleBottom);
    if (fInTitle)
    {
        return HTCAPTION;
    }

    // ---- 4. Default ----------------------------------------------------
    return HTCLIENT;
}
