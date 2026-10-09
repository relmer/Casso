#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesTab
//
//  Every nibble of the selected track (FR-041): rows of a multiple of 8
//  nibbles sized to the pane, each under a hex offset, in its kind's color,
//  with each sync nibble's width and any extra zero cells after a nibble in
//  small figures beside it, invalid nibbles and random-bit regions marked,
//  and the selected sector's nibbles highlighted. A click selects a nibble;
//  a selection made elsewhere scrolls its row a third of the way down.
//
////////////////////////////////////////////////////////////////////////////////

class NibblesTab : public InspectorView
{
public:
    static constexpr int  kRowDip       = 20;
    static constexpr int  kCellDip      = 34;
    static constexpr int  kOffsetDip    = 44;
    static constexpr int  kNibbleStep   = 8;

    explicit NibblesTab (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse    (const DxuiMouseEvent & ev) override;
    bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;

    //  Scrolls so the given nibble's row is about a third of the way down.
    void  ScrollTo (int nibble);

    static int  GetNibblesPerRow (float widthPx, float cellPx, float offsetPx);

private:
    int   GetPerRow     () const;
    int   GetVisibleRows () const;
    int   HitTest       (POINT pointPx) const;
    bool  IsInSelection (const TrackAnalysis & track, int nibble) const;
    static bool  IsOutsideTable (const TrackAnalysis & track, int nibble);

    int   m_firstRow = 0;
};
