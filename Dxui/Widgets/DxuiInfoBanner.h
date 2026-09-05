#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/DxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner
//
//  A bounded, bordered notice strip: a leading info glyph and a run of body text
//  that wraps within the banner. Distinct from a button (not clickable, no raised
//  surface) and from a tooltip (inline, not a popup). Colors come from the
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
    //  Info states something; Warning cautions about a consequence. The
    //  difference is the badge and the tint, not the layout.
    enum class Severity
    {
        Info,
        Warning
    };

    DxuiInfoBanner  () = default;
    explicit DxuiInfoBanner  (std::wstring text) : m_text (std::move (text)) {}
    ~DxuiInfoBanner () override = default;

    void  SetText (const std::wstring & text) { m_text = text; }

    void  SetSeverity (Severity severity) { m_severity = severity; }

    Severity  GetSeverity () const { return m_severity; }
    const std::wstring & GetText () const { return m_text; }

    //  Width at the trailing edge the text must not run into, in PIXELS of
    //  the caller's layout space.
    //
    //  FOR A BANNER THAT CONTAINS SOMETHING. A message bar with a button in it
    //  is one bordered strip, not a strip beside a button: the border spans the
    //  whole bar and the text stops short of what sits inside it. Without this
    //  the only way to keep the two apart is to shrink the banner itself, which
    //  puts the button outside the border and stops it being a message bar.
    //
    //  Zero by default, so every existing banner measures and paints exactly as
    //  it did.
    void  SetTrailingReservePx (float reservePx) { m_trailingReservePx = reservePx; }

    //  Centers the badge and the text AS ONE GROUP within the banner, rather
    //  than starting them at the leading edge. For a banner that is a strip
    //  across a window rather than a box in a dialog: left-aligned, one short
    //  line stranded itself against the far edge of a very wide bar.
    //
    //  The group, not the text alone -- the badge belongs to the words, and
    //  parking it at the leading edge with the text adrift in the middle
    //  reads as two things that happen to share a strip.
    //
    //  Off by default, so every existing banner paints exactly as it did.
    void  SetCentered (bool centered) { m_centered = centered; }

    void  SetRect (const RECT & rect) { SetBounds (rect); }
    void  SetDpi  (UINT dpi) { m_scaler.SetDpi (dpi); }

    // The height (in the caller's layout space) the banner needs to show its text
    // wrapped to `widthPx`: the estimated wrapped-line count times the line
    // height, plus vertical padding, never shorter than the icon. Estimated from
    // an average glyph width (Layout has no text renderer) and rounded up so text
    // never clips.
    float  GetPreferredHeightPx (float widthPx, const DxuiDpiScaler & scaler) const;

    // The height the banner ACTUALLY needs, measured rather than estimated.
    // PreferredHeightPx is deliberately generous -- it has no text renderer,
    // so it rounds up and never clips -- but on a fixed-size dialog that
    // slack shows as an empty line inside the box. A caller that has a
    // renderer can ask for the real height instead.
    float  GetMeasuredHeightPx (IDxuiTextRenderer   &  text,
                                float                  widthPx,
                                const DxuiDpiScaler &  scaler) const;

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        SetBounds (boundsDip);
        m_scaler.SetDpi (scaler.GetDpi());
    }

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    std::wstring        GetAccessibleName () const override { return m_text; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Label; }

private:
    Severity  m_severity = Severity::Info;

    // Geometry (DIP; scaled into the layout space via the scaler).
    static constexpr float  s_kPadXDip      = 12.0f;   // left / right inner padding
    static constexpr float  s_kPadYDip      = 9.0f;    // top / bottom inner padding
    static constexpr float  s_kIconBoxDip   = 16.0f;   // info-glyph column width
    static constexpr float  s_kIconGapDip   = 9.0f;    // icon -> text gap
    static constexpr float  s_kBorderDip    = 1.0f;
    static constexpr float  s_kFontDip      = 13.0f;
    static constexpr float  s_kLineHeightEm = 1.38f;   // body line height, in ems
    static constexpr float  s_kEstGlyphEm   = 0.55f;   // avg glyph width (generous -> no clip)

    // The info badge's pen, as a fraction of the icon box: one weight for the
    // ring and the "i", which is what makes the mark read as monoline.
    static constexpr float  s_kBadgeStrokeEm = 0.085f;

    // The widest a centered line is allowed to get, whatever the bar's width.
    // A message bar is as wide as the window, and a single line run out to
    // 2000 px is read by sweeping the head, not the eye. Past this the text
    // wraps -- see ResolveCenteredLinePx.
    static constexpr float  s_kMaxLineDip = 1024.0f;

    // Widening the wrapped box when the real word breaks need one line more
    // than the even split allowed: how far each step goes, and how many are
    // tried before the paint gives up and uses the full width instead.
    static constexpr float  s_kCenterFitWiden = 1.12f;
    static constexpr int    s_kCenterFitSteps = 6;

    // Estimated wrapped-line count for the text laid out at `textWidthPx`.
    int    EstimateLines (float textWidthPx, const DxuiDpiScaler & scaler) const;

    // The box a CENTERED banner lays its text in, given the width available to
    // it and how wide that text wants to be on one line.
    //
    // EVENLY SPLIT WHEN IT WRAPS. Filling each line to the cap and letting the
    // remainder fall onto the last one leaves a word or two stranded under a
    // full-width block, which reads as a mistake. Dividing the text's own
    // width by the number of lines it needs gives every line the same share,
    // so the last one is as full as the rest.
    float  ResolveCenteredLinePx (float availableTextPx, float wantedWidthPx,
                                  const DxuiDpiScaler & scaler) const;

    // A circle drawn as a ring rather than filled: short chords around the
    // circumference, the way the toolbar's monoline glyphs are stroked. The
    // painter has no arc primitive and does not need one for a mark this
    // small.
    static void  StrokeCircle (IDxuiPainter & painter, float cx, float cy,
                               float radiusPx, float strokePx, uint32_t argb);

    //  How much of the trailing edge belongs to something else. See
    //  SetTrailingReservePx.
    float           m_trailingReservePx = 0.0f;

    //  See SetCentered.
    bool            m_centered = false;

    std::wstring    m_text;
    DxuiDpiScaler   m_scaler;
};
