#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"

class IInspectorComponentHost;
class InspectorSession;





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand
//
//  The band along the window's bottom, at the window's scale rather than
//  its zoom: the summary chips at its left end, the hint for the view under
//  the pointer or the latest note, the "?" button that opens the
//  walkthrough, and the window's zoom (FR-007, FR-153, FR-154).
//
////////////////////////////////////////////////////////////////////////////////

class StatusBand : public IInspectorComponent
{
public:
    StatusBand (InspectorSession & session, IInspectorComponentHost & host);

    void          Create     () override;
    void          PaintUnder (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool          OnMouse    (const DxuiMouseEvent & ev) override;
    std::wstring  GetTip     (POINT pointPx, RECT & outAnchorPx) const override;

    void  Layout      (const RECT & area, const DxuiDpiScaler & scaler);
    int   GetHeightPx (const DxuiDpiScaler & scaler) const;
    void  SetHint     (const std::wstring & hint) { m_hint = hint; }
    void  SetZoom     (double zoom);

private:
    void          PaintChips       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float right, float & inOutX);
    void          PaintHelp        (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void          PaintSelection   (IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & line, float & inOutX) const;
    std::wstring  GetSelectionLine () const;

    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    DxuiZoomControl          * m_zoomControl = nullptr;
    DxuiDpiScaler              m_scaler;
    RECT                       m_bandPx      = {};
    RECT                       m_helpPx      = {};
    RECT                       m_chipsPx     = {};
    std::wstring               m_hint;
};
