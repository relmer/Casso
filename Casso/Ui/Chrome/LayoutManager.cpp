#include "Pch.h"

#include "LayoutManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager::LayoutManager
//
//  Binds to an external DpiScaler -- typically the one owned by the
//  Window the layout applies to. Stores a pointer so live DPI changes
//  (WM_DPICHANGED updating the scaler) are picked up automatically on
//  the next layout query without a per-frame re-bind.
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
//  DPI-scaling primitive. Returns dp scaled to physical pixels via the
//  bound DpiScaler. Falls back to base DPI (1:1) if no scaler was ever
//  bound, which only happens in malformed test setups.
//
////////////////////////////////////////////////////////////////////////////////

int LayoutManager::ScaleForDpi (int dp) const
{
    if (m_scaler == nullptr)
    {
        return dp;
    }
    return m_scaler->Px (dp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager::RegisterEdge
//
//  Append an edge contributor. Lifetime is owned by the caller;
//  LayoutManager does not delete anything on shutdown. Null contributors
//  are silently ignored so callers can pass conditional pointers without
//  guarding at every site.
//
////////////////////////////////////////////////////////////////////////////////

void LayoutManager::RegisterEdge (IEdgeContributor * contributor)
{
    if (contributor == nullptr)
    {
        return;
    }
    m_edges.push_back (contributor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager::UnregisterEdge
//
//  Remove an edge contributor. Linear scan, but the contributor list is
//  small (currently <= 4 edges) so the simplicity wins. No-op if the
//  contributor was never registered.
//
////////////////////////////////////////////////////////////////////////////////

void LayoutManager::UnregisterEdge (IEdgeContributor * contributor)
{
    auto  it = std::remove (m_edges.begin(), m_edges.end(), contributor);

    m_edges.erase (it, m_edges.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager::RegisterCenterLayer
//
//  Append a center-layer contributor (a future monitor-frame border, for
//  example). Same lifetime/null-handling contract as RegisterEdge.
//
////////////////////////////////////////////////////////////////////////////////

void LayoutManager::RegisterCenterLayer (ICenterLayer * layer)
{
    if (layer == nullptr)
    {
        return;
    }
    m_centerLayers.push_back (layer);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager::UnregisterCenterLayer
//
//  Remove a center-layer contributor. Same contract as UnregisterEdge.
//
////////////////////////////////////////////////////////////////////////////////

void LayoutManager::UnregisterCenterLayer (ICenterLayer * layer)
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
    int  scaledFbWidthPx  = ScaleForDpi (framebufferWidthPx);
    int  scaledFbHeightPx = ScaleForDpi (framebufferHeightPx);

    return ClientSizeForCenter (scaledFbWidthPx, scaledFbHeightPx);
}
