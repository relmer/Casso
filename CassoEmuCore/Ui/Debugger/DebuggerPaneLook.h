#pragma once

#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/Panes/HeatMapView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneLook
//
//  The theme in force, and the colors and gutter marks the panes draw in it.
//  Each mark is made once for its color and made again when a theme changes
//  the color, so the caches are mutable and a pane reads them through a
//  const reference.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerPaneLook
{
public:
    void               SetTheme (const DxuiTheme * theme) { m_theme = theme; }
    const DxuiTheme  * GetTheme () const { return m_theme; }
    bool               IsDark   () const;

    //  Every text color the panes draw, made readable against the theme's
    //  content background; the heat map's colors; and every color in the
    //  color key.
    DebuggerTextColors::Set  GetTextColors   () const;
    HeatMapView::Palette     GetHeatPalette  () const;
    ColorLegend::Palette     GetColorPalette () const;

    //  A breakpoint's red, the PC marker's yellow, and the PC's marker on a
    //  tab.
    uint32_t  GetBreakpointArgb () const;
    uint32_t  GetPcMarkerArgb   () const;
    uint32_t  GetPcTabMarkArgb  () const;

    //  A breakpoint's dot, on or off; the gray one where a click would set
    //  one; and the PC's arrow and a bookmark's outline, over `dot` when the
    //  line has one.
    std::shared_ptr<const DxuiIconImage>  GetBreakpointIcon      (bool enabled) const;
    std::shared_ptr<const DxuiIconImage>  GetHoverBreakpointIcon () const;
    std::shared_ptr<const DxuiIconImage>  GetPcIcon              (const std::shared_ptr<const DxuiIconImage> & dot) const;
    std::shared_ptr<const DxuiIconImage>  GetBookmarkMark        (const std::shared_ptr<const DxuiIconImage> & dot) const;

private:
    //  A mark and the dot it was drawn over. The cache has a reference to
    //  each dot, so no dot made later can reuse a cached dot's address.
    struct MarkedIcon
    {
        std::shared_ptr<const DxuiIconImage>  dot;
        std::shared_ptr<const DxuiIconImage>  icon;
    };

    const DxuiTheme                               * m_theme               = nullptr;
    mutable DebuggerTextColors::Cache               m_textColors;
    mutable std::shared_ptr<const DxuiIconImage>    m_breakpointIcons[2];
    mutable uint32_t                                m_breakpointIconArgb  = 0;
    mutable std::shared_ptr<const DxuiIconImage>    m_hoverBreakpointIcon;
    mutable uint32_t                                m_hoverBreakpointArgb = 0;
    mutable std::vector<MarkedIcon>                 m_pcIcons;
    mutable uint32_t                                m_pcIconArgb          = 0;
    mutable std::vector<MarkedIcon>                 m_bookmarkIcons;
    mutable uint32_t                                m_bookmarkIconArgb    = 0;
};
