#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/DxuiTheme.h"
#include "Widgets/DxuiButton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner
//
//  A bounded, bordered notice strip: a leading info glyph and a run of body text
//  that wraps within the banner. Distinct from a button (the strip itself is not
//  clickable, no raised surface) and from a tooltip (inline, not a popup).
//
//  ONE WIDGET FOR EVERY NOTICE, SET UP BY FLAGS. A notice that offers something
//  to do has buttons along its trailing edge (SetActions), and one the user may
//  put away has a close box there (SetDismissible). Without either it is the
//  plain strip. A close box rather than a Dismiss button: a labeled button is
//  for an action, and putting a notice away is not one. Colors come from the
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

    using ActionFn  = std::function<void (size_t index)>;
    using DismissFn = std::function<void ()>;

    DxuiInfoBanner  () = default;
    explicit DxuiInfoBanner  (std::wstring text) : m_text (std::move (text)) {}
    ~DxuiInfoBanner () override = default;

    void  SetText (const std::wstring & text) { m_text = text; m_fitValid = false; }

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
    void  SetCentered (bool centered) { m_centered = centered; m_fitValid = false; }

    //  Inner padding above and below the text, in DIPs. For a banner that sits
    //  in a form among rows of controls, where the default padding makes a
    //  one-line notice taller than the rows around it.
    //
    //  The default padding unless set, so every existing banner measures and
    //  paints exactly as it did.
    void  SetVerticalPaddingDip (float padDip) { m_padYDip = padDip; }

    //  A Segoe MDL2 Assets glyph drawn in the icon box in place of the badge,
    //  in the severity's color. Must outlive the banner (a UnicodeSymbols
    //  constant). Null by default, so every existing banner keeps its badge.
    void  SetIconGlyph (const wchar_t * glyph) { m_iconGlyph = glyph; }

    const wchar_t * GetIconGlyph () const { return m_iconGlyph; }

    //  Buttons along the trailing edge, for a notice that offers something to
    //  do. Replaced rather than added to, so a notice set again has one of
    //  each. None by default.
    void          SetActions     (const std::vector<std::wstring> & labels);
    size_t        GetActionCount () const { return m_actions.size(); }
    DxuiButton *  GetAction      (size_t index);

    //  Called with the index of the action that was pressed.
    void  SetOnAction (ActionFn action) { m_onAction = std::move (action); }

    //  A close box at the trailing edge, an X as on a tab, for a notice the
    //  user may put away. A click on it calls SetOnDismiss's function, and the
    //  caller hides the banner, since hiding a banner in a band changes the
    //  band's layout. Off by default.
    void  SetDismissible   (bool dismissible) { m_dismissible = dismissible; }
    bool  IsDismissible    () const           { return m_dismissible; }
    void  SetOnDismiss     (DismissFn dismiss) { m_onDismiss = std::move (dismiss); }
    RECT  GetDismissBounds () const           { return m_dismissRect; }

    //  How much of the trailing edge the actions and the close box take, the
    //  gap to the text included; zero with neither.
    float  GetControlColumnPx (const DxuiDpiScaler & scaler) const;

    void  SetRect (const RECT & rect) { SetBounds (rect); }
    void  SetDpi  (UINT dpi) { m_scaler.SetDpi (dpi); }   // the fit cache keys off the dpi itself

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

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

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

    // The trailing controls: buttons, the close box, and the X drawn in it.
    static constexpr float  s_kActionWidthDip  = 96.0f;
    static constexpr float  s_kActionHeightDip = 26.0f;
    static constexpr float  s_kActionGapDip    = 8.0f;
    static constexpr float  s_kEdgePadDip      = 10.0f;
    static constexpr float  s_kCloseBoxDip     = 26.0f;
    static constexpr float  s_kCloseGlyphDip   = 10.0f;

    // The info badge's pen, as a fraction of the icon box: one weight for the
    // ring and the "i", which is what makes the mark read as monoline.
    static constexpr float  s_kBadgeStrokeEm = 0.085f;

    // The widest a centered line is allowed to get, whatever the bar's width.
    // A message bar is as wide as the window, and a single line run out to
    // 2000 px is read by sweeping the head, not the eye. Past this the text
    // wraps -- see ResolveCenteredLinePx.
    static constexpr float  s_kMaxLineDip = 1024.0f;


    // Estimated wrapped-line count for the text laid out at `textWidthPx`.
    int    EstimateLines (float textWidthPx, const DxuiDpiScaler & scaler) const;

    // The box a CENTERED banner lays its text in, MEASURED -- the one number
    // the height and the paint must agree on. Cached against the text, the
    // width it was resolved for and the DPI, because it changes only when one
    // of those does and Paint runs every frame.
    float  ResolveCenteredBoxPx (IDxuiTextRenderer   &  text,
                                 float                  availableTextPx,
                                 const DxuiDpiScaler &  scaler) const;

    // Widens the even split until the text really wraps into the line count
    // the split was worked out for. The split is an ideal width; the renderer
    // breaks at words, and a box a hair too narrow strands one on a line of
    // its own. See FitCenteredBoxPx.
    float  FitCenteredBoxPx (IDxuiTextRenderer   &  text,
                             float                  availableTextPx,
                             float                  splitPx,
                             float                  measuredWidthPx,
                             const DxuiDpiScaler &  scaler) const;

    // The same box from the renderer-free estimate, for a caller that has no
    // renderer to ask. See ResolveCenteredLinePx.

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

    // The text's trailing reserve: the caller's, and the controls' column.
    float  GetTextReservePx (const DxuiDpiScaler & scaler) const;

    // The controls' height, edge padding included; zero with none.
    float  GetControlHeightPx (const DxuiDpiScaler & scaler) const;

    // The close box's X, over a wash while the pointer is on it.
    void  PaintCloseBox (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;

    // Draws the icon-font glyph set by SetIconGlyph, filling the icon box.
    void  PaintIconGlyph (IDxuiTextRenderer & text, const IDxuiTheme & theme,
                          float iconLeft, float iconTop, float iconBox) const;

    //  How much of the trailing edge belongs to something else. See
    //  SetTrailingReservePx.
    float           m_trailingReservePx = 0.0f;

    //  See SetCentered.
    bool            m_centered = false;

    //  See SetVerticalPaddingDip.
    float           m_padYDip = s_kPadYDip;

    //  See SetIconGlyph.
    const wchar_t * m_iconGlyph = nullptr;

    //  See SetActions and SetDismissible.
    std::vector<std::unique_ptr<DxuiButton>>  m_actions;
    ActionFn                                  m_onAction;
    bool                                      m_dismissible    = false;
    DismissFn                                 m_onDismiss;
    RECT                                      m_dismissRect    = {};
    bool                                      m_dismissHover   = false;
    bool                                      m_dismissPressed = false;

    //  What ResolveCenteredBoxPx last worked out, and the three inputs it
    //  depends on. Mutable because the height queries are const and are the
    //  ones that warm it -- Paint then costs nothing.
    mutable std::wstring  m_fitText;
    mutable float         m_fitAvailPx = 0.0f;
    mutable UINT          m_fitDpi     = 0;
    mutable float         m_fitBoxPx   = 0.0f;
    mutable bool          m_fitValid   = false;

    std::wstring    m_text;
    DxuiDpiScaler   m_scaler;
};
