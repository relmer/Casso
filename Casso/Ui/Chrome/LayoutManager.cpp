#include "Pch.h"

#include "LayoutManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager::LayoutManager
//
////////////////////////////////////////////////////////////////////////////////

LayoutManager::LayoutManager (const DpiScaler & scaler)
    : m_scaler (&scaler)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager::ScaleForDpi
//
//  DPI-scaling primitive. Two flavors: the instance method queries
//  the bound DpiScaler (the normal path); the static overload is for
//  the rare pre-window caller that has a raw DPI but no scaler yet.
//  Caps DPI at kBaseDpi when zero is supplied so callers can pass
//  GetDpiForWindow's return value without guarding.
//
////////////////////////////////////////////////////////////////////////////////

int LayoutManager::ScaleForDpi (int dp) const
{
    return (m_scaler != nullptr) ? m_scaler->Px (dp)
                                 : ScaleForDpi (dp, (UINT) kBaseDpi);
}


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

LayoutManagerResult LayoutManager::Resolve (int clientWidthPx, int clientHeightPx) const
{
    LayoutManagerResult  r = {};



    for (IEdgeContributor * e : m_edges)
    {
        int  px = ScaleForDpi (e->DesiredThicknessDp());

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
        r.topCenterPadPx    += ScaleForDpi (c->TopPadDp());
        r.bottomCenterPadPx += ScaleForDpi (c->BottomPadDp());
        r.leftCenterPadPx   += ScaleForDpi (c->LeftPadDp());
        r.rightCenterPadPx  += ScaleForDpi (c->RightPadDp());
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
//  Inverse of Resolve: given a desired emulator pixel grid (already
//  scaled to physical pixels), return the client size that hosts it
//  with all current contributors. Used by the theme-resize path
//  which wants to preserve the user's current emu viewport size as
//  the chrome thickness changes.
//
////////////////////////////////////////////////////////////////////////////////

SIZE LayoutManager::ClientSizeForCenter (int centerWidthPx, int centerHeightPx) const
{
    LayoutManagerResult  totals = Resolve (0, 0);
    SIZE                 size;



    size.cx = centerWidthPx  + totals.TotalLeftPx() + totals.TotalRightPx();
    size.cy = centerHeightPx + totals.TotalTopPx()  + totals.TotalBottomPx();
    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager::ClientSizeForFramebuffer
//
//  Owns the framebuffer scale policy. Today: linear DPI scaling --
//  emu pixels scale at the same rate as chrome dp, so a 144 DPI
//  monitor shows the framebuffer at 1.5x and chrome insets at 1.5x,
//  and the two stay in proportion at every DPI. If we ever want to
//  switch to integer-only scaling for pixel-art crispness, this is
//  the one function to change.
//
//  Both EmulatorShell::CreateEmulatorWindow (initial window size)
//  and WindowCommandManager Ctrl+0 (reset size) go through here.
//  Previously each inlined its own (and disagreeing) formula.
//
////////////////////////////////////////////////////////////////////////////////

SIZE LayoutManager::ClientSizeForFramebuffer (int framebufferWidthPx, int framebufferHeightPx) const
{
    int   scaledFbWidthPx  = ScaleForDpi (framebufferWidthPx);
    int   scaledFbHeightPx = ScaleForDpi (framebufferHeightPx);

    return ClientSizeForCenter (scaledFbWidthPx, scaledFbHeightPx);
}
