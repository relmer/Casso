#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"
#include "Ui/DiskInspector/Components/InspectorStatusBar.h"

class IInspectorComponentHost;
class InspectorSession;





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand
//
//  The track inspector's InspectorStatusBar (FR-007, FR-153, FR-154): the
//  comparison's result, the summary chips, "Analyzing" and the selection at
//  its left end, from the session, and the "?" button that opens the
//  walkthrough.
//
////////////////////////////////////////////////////////////////////////////////

class StatusBand : public IInspectorComponent
{
public:
    StatusBand (InspectorSession & session, IInspectorComponentHost & host);

    void          Create     () override;
    void          Tick       (int64_t nowMs) override;
    void          PaintUnder (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool          OnMouse    (const DxuiMouseEvent & ev) override;
    std::wstring  GetTip     (POINT pointPx, RECT & outAnchorPx) const override;

    void  Layout         (const RECT & area, const DxuiDpiScaler & scaler) { m_bar.Layout (area, scaler); }
    int   GetHeightPx    (const DxuiDpiScaler & scaler) const              { return m_bar.GetHeightPx (scaler); }
    void  SetHint        (const std::wstring & hint)                       { m_bar.SetHint (hint); }
    void  SetZoom        (double zoom)                                     { m_bar.SetZoom (zoom); }
    bool  IsZoomDragging () const                                          { return m_bar.IsZoomDragging(); }
    bool  IsOverZoom     (POINT p) const                                   { return m_bar.IsOverZoom (p); }

    //  Whether the point is on a chip that is a link, for IDC_HAND.
    bool  IsOverLink     (POINT p) const                                   { return m_bar.GetLinkedChipAt (p) != nullptr; }

private:
    vector<InspectorStatusBar::LeadItem>  BuildLead        () const;
    std::wstring                          GetSelectionLine () const;

    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    InspectorStatusBar         m_bar;
};
