#pragma once

#include "Pch.h"

#include "Widgets/ListView/DxuiListViewTypes.h"

class DxuiListView;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemPainter
//
//  Painting the items of the views other than Details: their icons and art,
//  names, content lines and meters.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListItemPainter
{
public:
    explicit DxuiListItemPainter (DxuiListView & list);

    //  Explorer's drive usage bar, measured: its size, its fill, track and
    //  edge, and the red it turns once the drive is nearly full.
    static constexpr int       s_kMeterWidthDip  = 190;
    static constexpr int       s_kMeterHeightDip = 14;
    static constexpr float     s_kMeterFullAt    = 0.9f;
    static constexpr uint32_t  s_kMeterFillArgb  = 0xFF0070CBu;
    static constexpr uint32_t  s_kMeterFullArgb  = 0xFFDA2626u;
    static constexpr uint32_t  s_kMeterTrackArgb = 0xFFE6E6E6u;
    static constexpr uint32_t  s_kMeterEdgeArgb  = 0xFFBCBCBCu;

    static constexpr float  s_kContentRightShare     = 0.6f;
    static constexpr float  s_kContentRightNudgeDip  = 32.67f;
    static constexpr float  s_kContentRightRoomDip   = 303.33f;
    static constexpr float  s_kContentRightMaxDip    = 685.0f;
    static constexpr float  s_kContentMiddleDip      = 298.67f;   // from the name's left to the middle column
    static constexpr float  s_kContentEdgeDip        = 9.33f;   // from the row's end to where those widths are measured
    static constexpr float  s_kContentRuleInsetDip   = 4.0f;
    static constexpr float  s_kContentRuleEndDip     = 6.67f;

    void          PaintItems         (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, float x, float y) const;
    void          PaintHomeItem      (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, int item, const RECT & cell, const std::vector<DxuiListViewTypes::Cell> & cells) const;
    void          PaintMeter         (IDxuiPainter & painter, float x, float y, float w, float lineH, float fraction) const;
    static float  MeasureContentLine (IDxuiTextRenderer & text, const DxuiListViewTypes::Cell & line, float fontPx);
    static void   PaintContentLine   (IDxuiTextRenderer & text, const DxuiListViewTypes::Palette & pal, const DxuiListViewTypes::Cell & line, float x, float y, float w, float lineH, float fontPx);
    static constexpr float  s_kContentNameScale  = 11.0f / 9.0f;
    static constexpr float  s_kContentPathGapDip = 14.0f;   // from a result's folder to its type
    static constexpr float  s_kTileBoxShortDip   = 6.0f;

private:
    friend class DxuiListView;

    DxuiListView  & m_list;

    mutable std::vector<int>  m_nameCut;   // the items drawn cut short in the last paint
};
