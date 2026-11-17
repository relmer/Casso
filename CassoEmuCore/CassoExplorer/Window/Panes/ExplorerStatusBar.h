#pragma once

#include "Pch.h"

struct ExplorerContext;





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
    explicit ExplorerStatusBar (ExplorerContext & context);

    //  The fields, left to right: Explorer's item count and selection, and
    //  the count of disk images waiting to be booted for a thumbnail, flowing
    //  from the left; the space between; then free space, the preview's
    //  detail and its zoom.
    static constexpr size_t  kStatusCount      = 0;
    static constexpr size_t  kStatusSelected   = 1;
    static constexpr size_t  kStatusThumbnails = 2;
    static constexpr size_t  kStatusFill       = 3;
    static constexpr size_t  kStatusFree       = 4;
    static constexpr size_t  kStatusDetail     = 5;
    static constexpr size_t  kStatusZoom       = 6;

    //  Field widths: free space, and the preview's detail and zoom when the
    //  preview is hidden and they cannot follow its edge.
    static constexpr int     kStatusBandDip   = 26;      // Explorer's status bar, measured at 150%
    static constexpr int     kStatusFreeDip   = 140;
    static constexpr int     kStatusDetailDip = 280;
    static constexpr int     kStatusZoomDip   = 64;

    //  The CassoLink line's tip: the latest entries of the activity log.
    static constexpr size_t  kActivityTipLines    = 10;
    static constexpr float   kActivityTipWidthDip = 720.0f;   // a log line, time and link and all, on one line

    void  Create      ();
    void  Configure   ();
    void  Layout      (const RECT & band);
    void  SetHidden   (bool hidden) { m_status->SetVisible (!hidden); }
    void  Fill        ();
    void  SetDetail   (const std::wstring & text) { m_status->SetText (kStatusDetail, text); }
    void  SetActivity (const std::wstring & text) { m_status->SetText (kStatusFill, text); }
    void  UpdateTip   (const DxuiMouseEvent & ev, POINT point);

    //  The thumbnails field: Capturing thumbnails and the count waiting, or
    //  empty with none.
    static std::wstring  DescribeThumbnails (size_t queued);

    //  A picker's count field: the count, then how many files the picker's
    //  scope left out. An empty Casso node's list has a line on where to
    //  browse from instead.
    static std::wstring  DescribePickerCount (const std::wstring & count, size_t listed, size_t scopedOut, bool isForDrive, bool isCassoList);

private:
    bool  HandleZoomClick     (const DxuiMouseEvent & ev);
    bool  HandleSummaryClick  (const DxuiMouseEvent & ev);
    bool  HandleActivityClick (const DxuiMouseEvent & ev);

    ExplorerContext  & m_context;
    DxuiStatusBar    * m_status  = nullptr;

    //  The text of the CassoLink line's tip, for as long as the tip is up.
    std::wstring           m_activityTip;
};