#include "Pch.h"

#include "HitTester.h"





namespace
{
    constexpr int  s_kDefaultResizeBorderPx = 6;


    inline bool RectContains (const RECT & r, int x, int y)
    {
        return (x >= r.left) && (x < r.right) && (y >= r.top) && (y < r.bottom);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
////////////////////////////////////////////////////////////////////////////////

void HitTester::Clear ()
{
    m_rects.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Register
//
////////////////////////////////////////////////////////////////////////////////

void HitTester::Register (const HitRect & rect)
{
    m_rects.push_back (rect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Pick
//
//  Walks registrations in reverse-insert order so later registrations
//  (which render on top) take precedence on hit ties.
//
////////////////////////////////////////////////////////////////////////////////

const HitRect * HitTester::Pick (int xClient, int yClient) const
{
    for (auto it = m_rects.rbegin(); it != m_rects.rend(); ++it)
    {
        if (RectContains (it->rect, xClient, yClient))
        {
            return &(*it);
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyNcHit
//
//  Pure mapping from a screen-space mouse point + window rect to an
//  HT* code. Resize edges take precedence over the system-button rects,
//  which take precedence over the caption rect, which takes precedence
//  over the client area.
//
////////////////////////////////////////////////////////////////////////////////

LRESULT HitTester::ClassifyNcHit (const NcHitTestInput & in)
{
    int    border = (in.resizeBorderPx > 0) ? in.resizeBorderPx : s_kDefaultResizeBorderPx;
    int    xWin   = in.mouseXScreen - in.windowRectScreen.left;
    int    yWin   = in.mouseYScreen - in.windowRectScreen.top;
    int    width  = in.windowRectScreen.right  - in.windowRectScreen.left;
    int    height = in.windowRectScreen.bottom - in.windowRectScreen.top;
    bool   left   = xWin < border;
    bool   right  = xWin >= (width  - border);
    bool   top    = yWin < border;
    bool   bottom = yWin >= (height - border);



    if (top && left)     { return HTTOPLEFT;     }
    if (top && right)    { return HTTOPRIGHT;    }
    if (bottom && left)  { return HTBOTTOMLEFT;  }
    if (bottom && right) { return HTBOTTOMRIGHT; }
    if (left)            { return HTLEFT;        }
    if (right)           { return HTRIGHT;       }
    if (top)             { return HTTOP;         }
    if (bottom)          { return HTBOTTOM;      }

    if (RectContains (in.minButtonRect,   xWin, yWin)) { return HTMINBUTTON; }
    if (RectContains (in.maxButtonRect,   xWin, yWin)) { return HTMAXBUTTON; }
    if (RectContains (in.closeButtonRect, xWin, yWin)) { return HTCLOSE;     }
    if (RectContains (in.captionRect,     xWin, yWin)) { return HTCAPTION;   }

    return HTCLIENT;
}
