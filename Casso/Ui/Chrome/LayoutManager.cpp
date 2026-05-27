#include "Pch.h"

#include "LayoutManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager::ScaleForDpi
//
//  DPI-scaling primitive used by Resolve. Mirrors ChromeMetrics::ScaleForDpi
//  (which will be deleted once the migration is complete). Caps DPI at
//  kBaseDpi when zero is supplied so callers can pass GetDpiForWindow's
//  return value without guarding.
//
////////////////////////////////////////////////////////////////////////////////

int LayoutManager::ScaleForDpi (int dp, UINT dpi)
{
    UINT  effectiveDpi = (dpi == 0) ? (UINT) kBaseDpi : dpi;



    return MulDiv (dp, (int) effectiveDpi, kBaseDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager::Register / Unregister
//
//  Append/remove contributor pointers. Lifetime is owned by the caller;
//  LayoutManager does not delete anything on shutdown. Unregister is a
//  linear scan but the contributor list is small (currently <= 4 edges
//  + a handful of future center layers) so the simplicity wins.
//
////////////////////////////////////////////////////////////////////////////////

void LayoutManager::Register (IEdgeContributor * contributor)
{
    if (contributor == nullptr)
    {
        return;
    }
    m_edges.push_back (contributor);
}


void LayoutManager::Register (ICenterLayer * layer)
{
    if (layer == nullptr)
    {
        return;
    }
    m_centerLayers.push_back (layer);
}


void LayoutManager::Unregister (IEdgeContributor * contributor)
{
    auto  it = std::remove (m_edges.begin(), m_edges.end(), contributor);

    m_edges.erase (it, m_edges.end());
}


void LayoutManager::Unregister (ICenterLayer * layer)
{
    auto  it = std::remove (m_centerLayers.begin(), m_centerLayers.end(), layer);

    m_centerLayers.erase (it, m_centerLayers.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager::Resolve
//
//  Pure-function reduction of contributor state into a canonical inset
//  snapshot. Sums edge contributors per-edge, sums center layer paddings
//  per-side, and produces the centerRect = client rect minus the totals.
//  Center layers' paddings are conceptually INSIDE the chrome insets,
//  so totalTop = topInset + topCenterPad and similarly for the other
//  edges; the centerRect contracts by the sum.
//
//  Negative center rects (when contributors over-allocate vs the client
//  area) are clamped to zero-area at the centered position to avoid
//  inverted RECT bugs downstream; the caller is expected to ensure the
//  window is large enough.
//
////////////////////////////////////////////////////////////////////////////////

LayoutManagerResult LayoutManager::Resolve (int clientWidthPx, int clientHeightPx, UINT dpi) const
{
    LayoutManagerResult  r = {};



    for (IEdgeContributor * e : m_edges)
    {
        int  px = ScaleForDpi (e->DesiredThicknessDp(), dpi);

        switch (e->Edge())
        {
            case ChromeEdge::Top:    r.topInsetPx    += px; break;
            case ChromeEdge::Bottom: r.bottomInsetPx += px; break;
            case ChromeEdge::Left:   r.leftInsetPx   += px; break;
            case ChromeEdge::Right:  r.rightInsetPx  += px; break;
        }
    }

    for (ICenterLayer * c : m_centerLayers)
    {
        r.topCenterPadPx    += ScaleForDpi (c->TopPadDp(),    dpi);
        r.bottomCenterPadPx += ScaleForDpi (c->BottomPadDp(), dpi);
        r.leftCenterPadPx   += ScaleForDpi (c->LeftPadDp(),   dpi);
        r.rightCenterPadPx  += ScaleForDpi (c->RightPadDp(),  dpi);
    }

    r.centerRect.left   = r.TotalLeftPx();
    r.centerRect.top    = r.TotalTopPx();
    r.centerRect.right  = std::max ((LONG) r.centerRect.left, (LONG) (clientWidthPx  - r.TotalRightPx()));
    r.centerRect.bottom = std::max ((LONG) r.centerRect.top,  (LONG) (clientHeightPx - r.TotalBottomPx()));

    return r;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager::ClientSizeForCenter
//
//  Inverse of Resolve: given a desired emulator pixel grid, return the
//  client size that hosts it with all current contributors. This is the
//  single source of truth for window-sizing math at:
//      * EmulatorShell::Create (initial window size)
//      * WindowCommandManager Ctrl+0 (snap to integer scale)
//
//  Both paths previously inlined the formula `framebuffer*scale +
//  ChromeTopInset + ChromeBottomInset`. Centralizing here means a new
//  contributor (e.g. a left-docked drive bar in spec 009) automatically
//  participates in both call sites without further edits.
//
////////////////////////////////////////////////////////////////////////////////

SIZE LayoutManager::ClientSizeForCenter (int centerWidthPx, int centerHeightPx, UINT dpi) const
{
    LayoutManagerResult  totals = Resolve (0, 0, dpi);
    SIZE                size;



    size.cx = centerWidthPx  + totals.TotalLeftPx() + totals.TotalRightPx();
    size.cy = centerHeightPx + totals.TotalTopPx()  + totals.TotalBottomPx();
    return size;
}
