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
//  walkthrough, and the window's zoom (FR-007, FR-153, FR-154). The band is
//  a DxuiStatusBar: the hint is the text of the bar's stretch field, and the
//  chips, the selection, the "?" button and the zoom are drawn over the
//  bar's other fields.
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

    void  Layout         (const RECT & area, const DxuiDpiScaler & scaler);
    int   GetHeightPx    (const DxuiDpiScaler & scaler) const;
    void  SetHint        (const std::wstring & hint) { m_hint = hint; }
    void  SetZoom        (double zoom);
    bool  IsZoomDragging () const { return m_zoomControl != nullptr && m_zoomControl->IsDragging(); }
    bool  IsOverZoom     (POINT p) const { return m_zoomControl != nullptr && m_zoomControl->IsOverControls (p); }

private:
    enum Field : size_t
    {
        kFieldLead,
        kFieldHint,
        kFieldHelp,
        kFieldZoom
    };

    enum class LeadKind
    {
        Result,
        Chip,
        BadChip,
        Analyzing,
        Selection
    };

    //  One thing drawn at the band's left end, and where it goes.
    struct LeadItem
    {
        std::wstring  text;
        LeadKind      kind     = LeadKind::Chip;
        float         x        = 0;
        float         w        = 0;
        bool          isPlaced = false;
    };

    void                   LayOutBar        (int leadPx);
    std::vector<LeadItem>  BuildLead        () const;
    float                  PlaceLead        (IDxuiTextRenderer & text, std::vector<LeadItem> & inOutItems) const;
    void                   PaintLead        (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::vector<LeadItem> & items) const;
    void                   PaintHelp        (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    std::wstring           GetSelectionLine () const;

    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    DxuiZoomControl          * m_zoomControl = nullptr;
    DxuiStatusBar              m_bar;
    DxuiDpiScaler              m_scaler;
    RECT                       m_bandPx      = {};
    RECT                       m_helpPx      = {};
    RECT                       m_chipsPx     = {};
    std::wstring               m_hint;
};
