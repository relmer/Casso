#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Theme/DxuiTheme.h"




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner
//
//  A bounded, bordered notice strip: a leading info glyph and a run of body text
//  that wraps within the banner. Distinct from a button (not clickable, no raised
//  surface) and from a tooltip (inline, not a popup). Colours come from the
//  theme's InfoBanner* accessors -- a subtle accent-tinted fill, an accent border
//  that stands out, and readable body text -- so it stays themed everywhere.
//
//  Height is content-driven: the caller sizes the banner to the width it wants
//  and asks PreferredHeightPx for the height the wrapped text needs, then reflows
//  the controls beneath it (Layout carries no text renderer, so the height is
//  estimated from the glyph metrics, rounded up so text never clips).
//
////////////////////////////////////////////////////////////////////////////////

class DxuiInfoBanner : public IDxuiControl
{
public:
    DxuiInfoBanner  () = default;
    explicit DxuiInfoBanner  (std::wstring text) : m_text (std::move (text)) {}
    ~DxuiInfoBanner () override = default;

    void  SetText (const std::wstring & text) { m_text = text; }
    const std::wstring & Text () const { return m_text; }

    void  SetRect (const RECT & rect) { SetBounds (rect); }
    void  SetDpi  (UINT dpi) { m_scaler.SetDpi (dpi); }

    // The height (in the caller's layout space) the banner needs to show its text
    // wrapped to `widthPx`: the estimated wrapped-line count times the line
    // height, plus vertical padding, never shorter than the icon. Estimated from
    // an average glyph width (Layout has no text renderer) and rounded up so text
    // never clips.
    float  PreferredHeightPx (float widthPx, const DxuiDpiScaler & scaler) const;

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        SetBounds (boundsDip);
        m_scaler.SetDpi (scaler.Dpi());
    }

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    std::wstring        AccessibleName () const override { return m_text; }
    DxuiAccessibleRole  AccessibleRole () const override { return DxuiAccessibleRole::Label; }

private:
    // Geometry (DIP; scaled into the layout space via the scaler).
    static constexpr float  s_kPadXDip      = 12.0f;   // left / right inner padding
    static constexpr float  s_kPadYDip      = 9.0f;    // top / bottom inner padding
    static constexpr float  s_kIconBoxDip   = 16.0f;   // info-glyph column width
    static constexpr float  s_kIconGapDip   = 9.0f;    // icon -> text gap
    static constexpr float  s_kBorderDip    = 1.0f;
    static constexpr float  s_kFontDip      = 13.0f;
    static constexpr float  s_kLineHeightEm = 1.38f;   // body line height, in ems
    static constexpr float  s_kEstGlyphEm   = 0.55f;   // avg glyph width (generous -> no clip)

    // Estimated wrapped-line count for the text laid out at `textWidthPx`.
    int    EstimateLines (float textWidthPx, const DxuiDpiScaler & scaler) const;

    std::wstring    m_text;
    DxuiDpiScaler   m_scaler;
};
