#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusBar
//
//  The disk inspector's band along the window's bottom, at the window's
//  scale rather than its zoom: lead items at its left end (a result, the
//  summary chips, a note such as "Analyzing", and the selection), the hint
//  for the view under the pointer, the "?" button, and the window's zoom.
//  The band is a DxuiStatusBar: the hint is the text of the bar's stretch
//  field, and the rest is drawn over the bar's other fields. The track
//  inspector's StatusBand and block mode both draw through it, each with its
//  lead items.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorStatusBar
{
public:
    enum class LeadKind
    {
        Result,
        Chip,
        BadChip,
        Note,
        Selection
    };

    //  One thing drawn at the band's left end, and where it goes. A linked
    //  chip is drawn in the accent, unless it's bad, and is tinted while the
    //  pointer is on it.
    struct LeadItem
    {
        std::wstring  text;
        LeadKind      kind     = LeadKind::Chip;
        float         x        = 0;
        float         w        = 0;
        bool          isPlaced = false;
        bool          isLinked = false;
    };

    using ZoomFn = std::function<void (double zoom)>;

    //  The zoom control is created in `parent`; a change of zoom calls
    //  `onZoom`. With `hasHelp` false there is no "?" button.
    void  Create (DxuiPanel & parent, ZoomFn onZoom, bool hasHelp);

    //  The zoom's slider follows the cursor in `owner`'s client area.
    void  Tick (HWND owner);

    void  Layout      (const RECT & area, const DxuiDpiScaler & scaler);
    int   GetHeightPx (const DxuiDpiScaler & scaler) const;
    void  Paint       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, vector<LeadItem> items, uint32_t badColor);

    void  SetHint        (const std::wstring & hint) { m_hint = hint; }
    void  SetZoom        (double zoom);
    bool  IsZoomDragging () const { return m_zoomControl != nullptr && m_zoomControl->IsDragging(); }
    bool  IsOverZoom     (POINT p) const { return m_zoomControl != nullptr && m_zoomControl->IsOverControls (p); }
    bool  IsOverHelp     (POINT p) const { return m_hasHelp && PtInRect (&m_helpPx, p) != FALSE; }
    RECT  GetHelpRect    () const { return m_helpPx; }

    //  The linked chip under a point, as the band was last drawn, or null;
    //  and whether the pointer is on one.
    const LeadItem  * GetLinkedChipAt (POINT p) const;
    void              SetLinkHot      (bool isHot) { m_isLinkHot = isHot; }

private:
    enum Field : size_t
    {
        kFieldLead,
        kFieldHint,
        kFieldHelp,
        kFieldZoom
    };

    void   LayOutBar (int leadPx);
    float  PlaceLead (IDxuiTextRenderer & text, vector<LeadItem> & inOutItems) const;
    void   PaintLead (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const vector<LeadItem> & items, uint32_t badColor) const;
    void   PaintHelp (IDxuiTextRenderer & text, const IDxuiTheme & theme) const;

    DxuiZoomControl  * m_zoomControl = nullptr;
    DxuiStatusBar      m_bar;
    DxuiDpiScaler      m_scaler;
    RECT               m_bandPx      = {};
    RECT               m_helpPx      = {};
    RECT               m_chipsPx     = {};
    std::wstring       m_hint;
    vector<LeadItem>   m_placed;
    bool               m_hasHelp     = true;
    bool               m_isLinkHot   = false;
};
