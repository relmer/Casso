#pragma once

#include "Pch.h"

#include "Widgets/ListView/DxuiListViewTypes.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiIconImage.h"
#include "Render/IDxuiTextRenderer.h"
#include "Widgets/DxuiScrollbar.h"

class DxuiListView;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListPainter
//
//  Painting the rows and items: the details rows, the items of the other
//  views with their art, badges, content lines and meters, the selection
//  band, and the palette they share.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListPainter
{
public:
    explicit DxuiListPainter (DxuiListView & list);

    static constexpr int    s_kRowBoxEndInsetDip = 4;

    //  How opaque a ghosted icon is: Explorer's hidden items, measured.
    static constexpr float     s_kGhostedIconAlpha = 0.5f;
    static constexpr uint32_t  s_kBandFillAlpha    = 0x50000000u;   // the band's fill, about a third, as measured off Explorer

    //  Explorer's drive usage bar, measured: its size, its fill, track and
    //  edge, and the red it turns once the drive is nearly full.
    static constexpr int       s_kMeterWidthDip  = 190;
    static constexpr int       s_kMeterHeightDip = 14;
    static constexpr float     s_kMeterFullAt    = 0.9f;
    static constexpr uint32_t  s_kMeterFillArgb  = 0xFF0070CBu;
    static constexpr uint32_t  s_kMeterFullArgb  = 0xFFDA2626u;
    static constexpr uint32_t  s_kMeterTrackArgb = 0xFFE6E6E6u;
    static constexpr uint32_t  s_kMeterEdgeArgb  = 0xFFBCBCBCu;

    //  The badge on an item that cannot be opened, over its icon's lower-left
    //  corner, for the tree's icons as well as the list's. On the text layer,
    //  which icons are drawn on, so it goes over its icon.
    static void         PaintBrokenBadge (IDxuiTextRenderer & text, float iconX, float iconY, float iconPx, float minPx);
    static constexpr float  s_kContentRightShare     = 0.6f;
    static constexpr float  s_kContentRightNudgeDip  = 32.67f;
    static constexpr float  s_kContentRightRoomDip   = 303.33f;
    static constexpr float  s_kContentRightMaxDip    = 685.0f;
    static constexpr float  s_kContentMiddleDip      = 298.67f;   // from the name's left to the middle column
    static constexpr float  s_kContentEdgeDip        = 9.33f;   // from the row's end to where those widths are measured
    static constexpr float  s_kContentRuleInsetDip   = 4.0f;
    static constexpr float  s_kContentRuleEndDip     = 6.67f;

    static constexpr uint32_t  s_kBrokenBadgeArgb   = 0xFFD13438;   // Windows' error red
    void          PaintItems              (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, float x, float y) const;
    void          PaintMeter              (IDxuiPainter & painter, float x, float y, float w, float lineH, float fraction) const;
    static float  MeasureContentLine      (IDxuiTextRenderer & text, const DxuiListViewTypes::Cell & line, float fontPx);
    static void   PaintContentLine        (IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, const DxuiListViewTypes::Cell & line, float x, float y, float w, float lineH, float fontPx);
    static constexpr float  s_kContentNameScale  = 11.0f / 9.0f;
    static constexpr float  s_kContentPathGapDip = 14.0f;   // from a result's folder to its type
    static constexpr float  s_kTileBoxShortDip   = 6.0f;
    static void   PaintSmallArtFrame      (IDxuiPainter & painter, float x, float y, float sizePx, uint32_t bg);
    static uint32_t ShiftGray             (uint32_t argb, int delta);
    void          PaintSelectionBand      (IDxuiPainter & painter, const DxuiListViewTypes::Palette & pal, float x, float y) const;

    DxuiListViewTypes::Palette      MakePalette         () const;
    void    PaintDataRows           (IDxuiPainter           & painter,
                                     IDxuiTextRenderer    & text,
                                     const DxuiListViewTypes::Palette          & pal,
                                     float                    x,
                                     float                    y,
                                     float                    layoutW,
                                     int                      firstRow,
                                     int                      lastRow,
                                     const std::vector<int> & colXPx,
                                     const std::vector<int> & colWPx) const;

    static constexpr uint32_t  s_kSecondaryColumnAlpha = 0xCC000000u;   // Explorer's columns after the name, against its full white

private:
    friend class DxuiListView;

    DxuiListView  & m_list;

    int                       m_hovered             = -1;
    int                       m_dropRow             = -1;
    mutable std::vector<int>  m_nameCut;   // the items drawn cut short in the last paint
    bool                      m_alwaysShowSelection = false;
    bool                      m_textSelectionColors = false;
};
