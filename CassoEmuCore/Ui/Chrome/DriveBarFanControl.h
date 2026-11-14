#pragma once

#include "Pch.h"

#include "CassoTheme.h"
#include "Shell/Layout/DriveBarFan.h"





//  The contents of one drive's slot on the bar. A Disk II is drawn by its DriveWidget,
//  placed in the drive's slot, so the control draws only the slot's panel.
struct DriveBarEntryView
{
    std::wstring  caption;
    std::wstring  name;
    bool          busy   = false;
    bool          diskII = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFanControl
//
//  The drive bar's 3.5-inch drives and hard disks under the desk scene, in
//  the compact drive widget's layout: the caption in a column on the left,
//  the image's name, and a rail under it that lights while the drive is busy.
//  Over a class's widget with more drives, an up arrow at the right of the
//  name row marks the fan, and the open fan is drawn on a panel above it.
//
//  Adopted ahead of the Disk II widgets, so the panel is drawn behind the
//  ones placed on it.
//
////////////////////////////////////////////////////////////////////////////////

class DriveBarFanControl : public IDxuiControl
{
public:
    DriveBarFan        &  GetFan ()       { return m_fan; }
    const DriveBarFan  &  GetFan () const { return m_fan; }

    void  SetViews   (std::vector<std::vector<DriveBarEntryView>> views) { m_views = std::move (views); }
    void  SetDpi     (UINT dpi)                                          { m_dpi = (dpi == 0) ? kBaseDpi : dpi; }
    void  SetHovered (int classIndex, int entry)                         { m_hoverClass = classIndex; m_hoverEntry = entry; }

    //  Cuts a name longer than `maxChars` and ends it with an ellipsis.
    static std::wstring  CutName (const std::wstring & name, size_t maxChars);

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    static constexpr UINT     kBaseDpi           = 96;
    static constexpr int      kBodyWidthPx       = 140;
    static constexpr int      kNameHeightPx      = 20;
    static constexpr int      kBarGapPx          = 3;
    static constexpr int      kBarHeightPx       = 5;
    static constexpr int      kBottomPadPx       = 8;
    static constexpr int      kCaptionWidthPx    = 44;
    static constexpr int      kCaptionGapXPx     = 6;
    static constexpr int      kCaptionHeightPx   = 14;
    static constexpr int      kPanelRadiusPx     = 4;
    static constexpr int      kArrowHalfPx       = 4;
    static constexpr float    kNameFontDip       = 11.0f;
    static constexpr float    kCaptionFontDip    = 9.0f;
    static constexpr float    kIdleRailAlpha     = 0.14f;
    static constexpr size_t   kMaxNameChars      = 22;

    static int  Scale (int value, UINT dpi);

    void  PaintEntry (IDxuiPainter & painter, IDxuiTextRenderer & text, const CassoTheme & theme,
                      const RECT & slot, const DriveBarEntryView & view, bool hovered);
    void  PaintArrow (IDxuiPainter & painter, const CassoTheme & theme, const RECT & slot);

    DriveBarFan                                   m_fan;
    std::vector<std::vector<DriveBarEntryView>>   m_views;
    UINT                                          m_dpi        = kBaseDpi;
    int                                           m_hoverClass = -1;
    int                                           m_hoverEntry = -1;
};
