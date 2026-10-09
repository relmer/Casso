#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/InspectorView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorRowView
//
//  Under "Sectors, in the order they pass the head", a button per address
//  field in passing order, showing the sector's hex number with its state's
//  symbol and color; the selected one highlighted; a click selects it
//  (FR-039). With no sectors it says why.
//
////////////////////////////////////////////////////////////////////////////////

class SectorRowView : public InspectorView
{
public:
    static constexpr int  kLabelDip  = 18;
    static constexpr int  kButtonDip = 40;
    static constexpr int  kRowDip    = 26;
    static constexpr int  kGapDip    = 4;

    explicit SectorRowView (InspectorViewContext & context) : InspectorView (context) {}

    void  Paint      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse    (const DxuiMouseEvent & ev) override;
    bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;

    //  The sector indices in passing order.
    static vector<int>  GetPassingOrder (const TrackAnalysis & track);

private:
    RECT  GetButtonRect (int position) const;
    int   HitTest       (POINT pointPx, const TrackAnalysis & track) const;

    int   m_hover = -1;
};
