#pragma once

#include "Pch.h"

#include "../DpiScaler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutManager
//
//  Single source of truth for chrome inset math. Replaces the scattered
//  `ChromeMetrics::*Px()` callers that historically drifted out of sync
//  (the Ctrl+0 pillarbox lived in WindowCommandManager forgetting to
//  reserve the command-bar inset).
//
//  Model:
//      * Edge contributors  (`IEdgeContributor`)  reserve thickness on
//        one of the four window edges. The chrome insets are the sum of
//        contributors on each edge.
//      * Center layers      (`ICenterLayer`)      reserve non-uniform
//        padding around the center rect on all four sides. Conceptually
//        they live INSIDE the emulator viewport region (e.g. a monitor
//        frame that wraps the framebuffer), distinct from outer chrome.
//
//      window client rect
//      = NC overhead (handled by Windows / NCCALCSIZE)
//        + Σ edge contributors (top, bottom, left, right)
//        + Σ center layer paddings (top, bottom, left, right)
//        + emulator pixel grid (the governing input)
//
//  `Resolve` is a pure function of contributor state, client size, and
//  DPI; it does not mutate the layout. `ClientSizeForCenter` runs the
//  inverse: given a desired emulator pixel grid, return the client
//  rect needed to host it. Together they give EmulatorShell and
//  WindowCommandManager a single canonical place to read sizing from.
//
//  Center layer support is plumbed but no implementations exist in 007;
//  the future monitor-frame spec (009) plugs into this interface.
//
////////////////////////////////////////////////////////////////////////////////

enum class ChromeEdge
{
    Top,
    Bottom,
    Left,
    Right,
};


class IEdgeContributor
{
public:
    virtual                  ~IEdgeContributor    () = default;
    virtual ChromeEdge        Edge                () const = 0;
    virtual int               DesiredThicknessDp  () const = 0;
};


class ICenterLayer
{
public:
    virtual            ~ICenterLayer () = default;
    virtual int         TopPadDp     () const = 0;
    virtual int         BottomPadDp  () const = 0;
    virtual int         LeftPadDp    () const = 0;
    virtual int         RightPadDp   () const = 0;
};


struct LayoutManagerResult
{
    int   topInsetPx        = 0;
    int   bottomInsetPx     = 0;
    int   leftInsetPx       = 0;
    int   rightInsetPx      = 0;
    int   topCenterPadPx    = 0;
    int   bottomCenterPadPx = 0;
    int   leftCenterPadPx   = 0;
    int   rightCenterPadPx  = 0;
    RECT  centerRect        = {};

    int  TotalTopPx    () const { return topInsetPx    + topCenterPadPx;    }
    int  TotalBottomPx () const { return bottomInsetPx + bottomCenterPadPx; }
    int  TotalLeftPx   () const { return leftInsetPx   + leftCenterPadPx;   }
    int  TotalRightPx  () const { return rightInsetPx  + rightCenterPadPx;  }
};


class LayoutManager
{
public:
    static constexpr int  kBaseDpi = 96;


    explicit LayoutManager (const DpiScaler & scaler);

    void  RegisterEdge          (IEdgeContributor * contributor);
    void  UnregisterEdge        (IEdgeContributor * contributor);
    void  RegisterCenterLayer   (ICenterLayer     * layer);
    void  UnregisterCenterLayer (ICenterLayer     * layer);

    LayoutManagerResult  Resolve                  (int clientWidthPx,
                                                   int clientHeightPx) const;
    SIZE                 ClientSizeForCenter      (int centerWidthPx,
                                                   int centerHeightPx) const;
    SIZE                 ClientSizeForFramebuffer (int framebufferWidthPx,
                                                   int framebufferHeightPx) const;

    int  ScaleForDpi (int dp) const;


    const std::vector<IEdgeContributor *> & Edges        () const { return m_edges;        }
    const std::vector<ICenterLayer     *> & CenterLayers () const { return m_centerLayers; }

private:
    const DpiScaler                * m_scaler = nullptr;
    std::vector<IEdgeContributor *>  m_edges;
    std::vector<ICenterLayer     *>  m_centerLayers;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SimpleEdgeContributor
//
//  Trivial value-type contributor for callers that don't have a natural
//  class to attach the edge data to. Chrome regions with their own
//  identity (TitleBar, DriveBar) should implement `IEdgeContributor`
//  directly so the contributor's thickness reflects live region state.
//
////////////////////////////////////////////////////////////////////////////////

class SimpleEdgeContributor : public IEdgeContributor
{
public:
    SimpleEdgeContributor (ChromeEdge edge, int thicknessDp)
      : m_edge        (edge)
      , m_thicknessDp (thicknessDp)
    {
    }

    ChromeEdge  Edge               () const override { return m_edge;        }
    int         DesiredThicknessDp () const override { return m_thicknessDp; }

    void        SetEdge        (ChromeEdge edge)        { m_edge        = edge;        }
    void        SetThicknessDp (int        thicknessDp) { m_thicknessDp = thicknessDp; }

private:
    ChromeEdge  m_edge;
    int         m_thicknessDp;
};
