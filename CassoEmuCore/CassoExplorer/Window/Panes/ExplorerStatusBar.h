#pragma once

#include "Pch.h"

class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerStatusBar
//
//  The status bar: the item count and selection, the CassoLink line, free
//  space, the preview's detail and its zoom, the fields laid out to the
//  preview's edge, and their tips and clicks.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerStatusBar
{
public:
    explicit ExplorerStatusBar (CassoExplorerWindow & window);

    //  The fields, left to right: Explorer's item count and selection,
    //  flowing from the left; the space between; then free space, the
    //  preview's detail and its zoom.
    static constexpr size_t  kStatusCount    = 0;
    static constexpr size_t  kStatusSelected = 1;
    static constexpr size_t  kStatusFill     = 2;
    static constexpr size_t  kStatusFree     = 3;
    static constexpr size_t  kStatusDetail   = 4;
    static constexpr size_t  kStatusZoom     = 5;

    //  Field widths: free space, and the preview's detail and zoom when the
    //  preview is hidden and they cannot follow its edge.
    static constexpr int     kStatusBandDip   = 26;      // Explorer's status bar, measured at 150%
    static constexpr int     kStatusFreeDip   = 140;
    static constexpr int     kStatusDetailDip = 280;
    static constexpr int     kStatusZoomDip   = 64;
    static constexpr float   kStatusLeadDip   = 14.67f;  // the item count, from the band's left edge

    //  The CassoLink line's tip: the latest entries of the activity log.
    static constexpr size_t  kActivityTipLines    = 10;
    static constexpr float   kActivityTipWidthDip = 720.0f;   // a log line, time and link and all, on one line

    //  The bar, as a child of the window, and its fields.
    void  Create    ();
    void  Configure ();
    void  Layout    (const RECT & band);
    void  Fill      ();
    void  SetDetail   (const std::wstring & text) { m_status->SetText (kStatusDetail, text); }
    void  SetActivity (const std::wstring & text) { m_status->SetText (kStatusFill, text); }

    //  Pointer events: a click on the zoom is ahead of the window's other
    //  routing, one on the CassoLink line after the panes' captures.
    bool  HandleZoomClick     (const DxuiMouseEvent & ev);
    bool  HandleActivityClick (const DxuiMouseEvent & ev);
    void  UpdateTip       (const DxuiMouseEvent & ev, POINT point);

private:
    CassoExplorerWindow  & m_window;

    DxuiStatusBar        * m_status = nullptr;

    //  The text of the CassoLink line's tip, for as long as the tip is up.
    std::wstring           m_activityTip;
};