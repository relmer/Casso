#include "Pch.h"

#include "Widgets/DxuiInfoBanner.h"
#include "Widgets/DxuiWarningBadge.h"
#include "Core/DxuiUnicodeSymbols.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::EstimateLines
//
//  Renderer-free wrapped-line estimate: how many lines the text takes at
//  `textWidthPx`, from an average glyph width. Deliberately generous (a wide
//  average) so the count rounds up and the caller never sizes the banner too
//  short to show every line.
//
//  MEASURED PER PARAGRAPH, BECAUSE A FORCED BREAK ENDS A LINE WHEREVER IT
//  FALLS. Dividing the whole string by the characters that fit assumes every
//  line is full, which undercounts by one for each partly-filled line a break
//  leaves behind -- so a two-paragraph notice was sized as though it were one
//  flowing sentence and lost its last lines off the bottom. GetMeasuredHeightPx
//  does not share the fault, having a real renderer to ask, but the change
//  banner over the machine is laid out without one.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiInfoBanner::EstimateLines (float textWidthPx, const DxuiDpiScaler & scaler) const
{
    float   glyphPx = scaler.ToPxf (s_kFontDip) * s_kEstGlyphEm;
    float   perLine = (glyphPx > 0.0f) ? (textWidthPx / glyphPx) : 1.0f;
    size_t  start   = 0;
    size_t  breakAt = 0;
    int     lines   = 0;



    //  NARROWER THAN A SINGLE GLYPH ESTIMATES NOTHING, and must not fall
    //  through to one line per character: the printer page sizes its banner
    //  from a page rect that is briefly tiny, and a 250-character notice
    //  would ask for 250 lines of height. One line, which is what this
    //  returned before it counted paragraphs.
    if (perLine < 1.0f)
    {
        return 1;
    }

    for (start = 0; start <= m_text.size(); start = breakAt + 1)
    {
        breakAt = m_text.find (L'\n', start);

        if (breakAt == std::wstring::npos)
        {
            breakAt = m_text.size();
        }

        //  An empty paragraph is the blank line between two others, and takes
        //  a line of its own.
        lines += (breakAt > start)
                     ? (int) std::ceil ((float) (breakAt - start) / perLine)
                     : 1;

        if (breakAt == m_text.size())
        {
            break;
        }
    }

    if (lines < 1)
    {
        lines = 1;
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::ResolveCenteredLinePx
//
//  How wide one line of a centered banner is allowed to be.
//
//  TWO REASONS TO WRAP, and the cap is the one a bar has that a dialog box
//  does not: the banner is as wide as the window, so text that fits on one
//  line can still be a line nobody wants to read. Past s_kMaxLineDip it wraps
//  even though there was room.
//
//  EVENLY, so the last line is as full as the others. Filling each line to the
//  cap and letting what is left fall onto the last one strands a word or two
//  under a full-width block; dividing the text's own width by the number of
//  lines it needs gives each the same share. The result is never wider than
//  the cap, because dividing by a count that came from the cap cannot be.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiInfoBanner::ResolveCenteredLinePx (float availableTextPx, float wantedWidthPx,
                                             const DxuiDpiScaler & scaler) const
{
    float  cap   = scaler.ToPxf (s_kMaxLineDip);
    int    lines = 0;



    if (cap > availableTextPx)
    {
        cap = availableTextPx;
    }

    if (cap < 1.0f)
    {
        cap = 1.0f;
    }

    if (wantedWidthPx <= cap)
    {
        return (wantedWidthPx > 1.0f) ? wantedWidthPx : 1.0f;
    }

    lines = (int) std::ceil (wantedWidthPx / cap);

    return wantedWidthPx / (float) lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::ResolveCenteredBoxPx
//
//  The same rule as ResolveCenteredLinePx, asked of the renderer instead of
//  the estimate -- and this is the answer that counts. The estimate exists for
//  a caller with no renderer to ask; it is an AVERAGE glyph width, so a wide
//  face or a line of capitals measures past it, and a width taken from it
//  would reserve a line fewer than the paint then needs.
//
//  CACHED, because Paint runs every frame and this changes only when the text,
//  the width it has to fit, or the DPI does. That is also what lets the height
//  query and the paint share one number rather than each work one out.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiInfoBanner::ResolveCenteredBoxPx (IDxuiTextRenderer   &  text,
                                            float                  availableTextPx,
                                            const DxuiDpiScaler &  scaler) const
{
    HRESULT  hr        = S_OK;
    float    measuredW = 0.0f;
    float    measuredH = 0.0f;



    if (m_fitValid && m_fitText == m_text && m_fitDpi == scaler.GetDpi()
        && m_fitAvailPx == availableTextPx)
    {
        return m_fitBoxPx;
    }

    hr = text.MeasureString (m_text.c_str(), scaler.ToPxf (s_kFontDip),
                             DxuiTheme::GetUiFace(), measuredW, measuredH);

    //  A measurement that failed says nothing about the text; the full width
    //  is what the banner used before it was centered, and it wraps to the
    //  fewest lines, which is the safe way to be wrong.
    if (FAILED (hr) || measuredW <= 0.0f)
    {
        return (availableTextPx > 1.0f) ? availableTextPx : 1.0f;
    }

    m_fitText    = m_text;
    m_fitDpi     = scaler.GetDpi();
    m_fitAvailPx = availableTextPx;
    m_fitBoxPx   = FitCenteredBoxPx (text, availableTextPx,
                                     ResolveCenteredLinePx (availableTextPx, measuredW, scaler),
                                     measuredW, scaler);
    m_fitValid   = true;

    return m_fitBoxPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::FitCenteredBoxPx
//
//  Widens the even-split box until the text actually wraps into the number of
//  lines that split was calculated for.
//
//  THE EVEN SPLIT IS AN IDEAL WIDTH, NOT AN ACHIEVABLE ONE. It divides the
//  text's measured width by the line count, but the renderer breaks at word
//  boundaries, and a box that is one character narrower than the break it
//  needs pushes a whole word to a line of its own. That is how a two-line
//  notice paints as two full lines and a stranded "1.", which is what a
//  narrow bar showed.
//
//  So the split is a starting point and the renderer has the last word:
//  measure the wrap, and while it needs more lines than were reserved, give
//  it a few more percent. Bounded, and it stops at the cap the split already
//  respected, so this can only widen a box within the width the height query
//  reserved -- never past it, and never onto a line that was not counted.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiInfoBanner::FitCenteredBoxPx (IDxuiTextRenderer   &  text,
                                        float                  availableTextPx,
                                        float                  splitPx,
                                        float                  measuredWidthPx,
                                        const DxuiDpiScaler &  scaler) const
{
    constexpr int    kMaxSteps  = 16;
    constexpr float  kGrowth    = 1.06f;
    HRESULT          hr         = S_OK;
    float            box        = splitPx;
    float            lineHeight = scaler.ToPxf (s_kFontDip) * s_kLineHeightEm;
    float            wrappedW   = 0.0f;
    float            wrappedH   = 0.0f;
    int              wanted     = 0;
    int              step       = 0;



    if (box >= measuredWidthPx || box >= availableTextPx || lineHeight < 1.0f)
    {
        return box;
    }

    wanted = (int) std::lround (measuredWidthPx / box);

    if (wanted < 1)
    {
        wanted = 1;
    }

    for (step = 0; step < kMaxSteps; step++)
    {
        hr = text.MeasureStringWrapped (m_text.c_str(), scaler.ToPxf (s_kFontDip),
                                        DxuiTheme::GetUiFace(), box, wrappedW, wrappedH);

        //  A renderer that cannot measure a wrap leaves the split as it was,
        //  which is what this widget did before.
        if (FAILED (hr) || wrappedH < 1.0f)
        {
            return box;
        }

        if ((int) std::lround (wrappedH / lineHeight) <= wanted)
        {
            return box;
        }

        box *= kGrowth;

        if (box >= availableTextPx)
        {
            return availableTextPx;
        }
    }

    return box;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::GetPreferredHeightPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiInfoBanner::GetPreferredHeightPx (float widthPx, const DxuiDpiScaler & scaler) const
{
    float   padX      = scaler.ToPxf (s_kPadXDip);
    float   padY      = scaler.ToPxf (m_padYDip);
    float   iconCol   = scaler.ToPxf (s_kIconBoxDip) + scaler.ToPxf (s_kIconGapDip);
    float   textWidth = widthPx - padX * 2.0f - iconCol - GetTextReservePx (scaler);
    float   lineH     = scaler.ToPxf (s_kFontDip) * s_kLineHeightEm;
    float   lineBox   = (textWidth > 1.0f) ? textWidth : 1.0f;
    int     lines     = 0;
    float   textH     = 0.0f;
    float   iconH     = scaler.ToPxf (s_kIconBoxDip);



    //  A CENTERED BANNER IS MEASURED IN THE BOX IT WILL ACTUALLY USE, not the
    //  whole width: it caps and evenly splits its lines (see
    //  ResolveCenteredLinePx), so a height taken from the full width would be
    //  a line short of what the paint needs. The estimate that feeds the split
    //  is the generous one this class already uses, which is what keeps the
    //  count at or above what the real text turns out to need.
    if (m_centered)
    {
        float  glyphPx = scaler.ToPxf (s_kFontDip) * s_kEstGlyphEm;

        lineBox = ResolveCenteredLinePx (lineBox, (float) m_text.size() * glyphPx, scaler);
    }

    lines = EstimateLines (lineBox, scaler);
    textH = lineH * (float) lines;



    // The content is the taller of the wrapped text and the icon, plus padding,
    // and never shorter than the controls, since a clipped button offers nothing.
    return (std::max) (((textH > iconH) ? textH : iconH) + padY * 2.0f, GetControlHeightPx (scaler));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::GetMeasuredHeightPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiInfoBanner::GetMeasuredHeightPx (IDxuiTextRenderer   &  text,
                                           float                  widthPx,
                                           const DxuiDpiScaler &  scaler) const
{
    HRESULT  hr        = S_OK;
    float    padX      = scaler.ToPxf (s_kPadXDip);
    float    padY      = scaler.ToPxf (m_padYDip);
    float    iconCol   = scaler.ToPxf (s_kIconBoxDip) + scaler.ToPxf (s_kIconGapDip);
    float    textWidth = widthPx - padX * 2.0f - iconCol - GetTextReservePx (scaler);
    float    iconH     = scaler.ToPxf (s_kIconBoxDip);
    float    outW      = 0.0f;
    float    outH      = 0.0f;



    if (textWidth < 1.0f)
    {
        textWidth = 1.0f;
    }

    //  THE BOX THE CENTERED PAINT WILL USE, not the full width: capped and
    //  evenly split, measured rather than estimated, and cached -- so the
    //  height reserved here and the lines the paint lays down are the same
    //  count by construction, with nothing left for a fit-up pass to rescue.
    if (m_centered)
    {
        textWidth = ResolveCenteredBoxPx (text, textWidth, scaler);
    }

    hr = text.MeasureStringWrapped (m_text.c_str(), scaler.ToPxf (s_kFontDip),
                                    DxuiTheme::GetUiFace(), textWidth, outW, outH);

    if (FAILED (hr) || outH <= 0.0f)
    {
        return GetPreferredHeightPx (widthPx, scaler);
    }

    return (std::max) (((outH > iconH) ? outH : iconH) + padY * 2.0f, GetControlHeightPx (scaler));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::StrokeCircle
//
//  A ring, walked as chords. Twenty segments is past the point where a mark
//  this size shows corners, and the painter has no arc of its own.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoBanner::StrokeCircle (IDxuiPainter & painter, float cx, float cy,
                                   float radiusPx, float strokePx, uint32_t argb)
{
    constexpr int  s_kSegments = 20;
    int            i           = 0;



    for (i = 0; i < s_kSegments; i++)
    {
        float  a0 = 6.2831853f * (float) i       / (float) s_kSegments;
        float  a1 = 6.2831853f * (float) (i + 1) / (float) s_kSegments;

        painter.DrawLine (cx + radiusPx * std::cos (a0), cy + radiusPx * std::sin (a0),
                          cx + radiusPx * std::cos (a1), cy + radiusPx * std::sin (a1),
                          strokePx, argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::PaintIconGlyph
//
//  Draws the caller's Segoe MDL2 Assets glyph in the icon box, in place of the
//  drawn badge: the same face the toolbar's icons use, sized so the em fills
//  the box, and centered both ways in it. The box is already centered on the
//  banner's height, which is where a one-line notice's text sits too.
//
//  The glyph takes the severity's color, so a glyph on a warning reads as a
//  warning and one on a notice reads as the accent.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoBanner::PaintIconGlyph (IDxuiTextRenderer & text, const IDxuiTheme & theme,
                                     float iconLeft, float iconTop, float iconBox) const
{
    constexpr wchar_t  kMdl2Family[] = L"Segoe MDL2 Assets";
    HRESULT            hr            = S_OK;
    uint32_t           argb          = theme.Accent();



    if (m_severity == Severity::Warning)
    {
        argb = theme.WarningAccent();
    }

    hr = text.DrawString (m_iconGlyph,
                          iconLeft,
                          iconTop,
                          iconBox,
                          iconBox,
                          argb,
                          iconBox,
                          kMdl2Family,
                          DxuiTextHAlign::Center,
                          DxuiTextVAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::Paint
//
//  Draws an informational notice: an accent-tinted surface, an info badge, and
//  wrapping body text.
//
//  A tinted fill inside a MUTED accent border, rather than a solid accent
//  panel, so the banner reads as a notice and not as a button. That
//  distinction is the widget's entire job -- it must be noticed without
//  inviting a click. The info badge is monoline for the same reason: a filled
//  disc is the heaviest mark the strip has.
//
//  The badge is drawn from PRIMITIVES rather than an icon-font glyph. That
//  avoids a font dependency for one symbol, and more importantly gives exact
//  centering: a glyph's optical center rarely coincides with its line-box
//  center, so a font-drawn "i" sits visibly off inside a disc.
//
//  The badge geometry is expressed as fractions of the disc radius, so it
//  scales with DPI and with any icon size without a second set of constants.
//
//  Text is inset past the badge and wraps, since a notice is prose of
//  unpredictable length -- unlike a label, it must not be clipped.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoBanner::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT         hr       = S_OK;
    float           left     = (float) m_boundsDip.left;
    float           top      = (float) m_boundsDip.top;
    float           width    = (float) (m_boundsDip.right  - m_boundsDip.left);
    float           height   = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float           padX     = m_scaler.ToPxf (s_kPadXDip);
    float           padY     = m_scaler.ToPxf (m_padYDip);
    float           borderPx = m_scaler.ToPxf (s_kBorderDip);
    float           iconBox  = m_scaler.ToPxf (s_kIconBoxDip);
    float           iconGap  = m_scaler.ToPxf (s_kIconGapDip);
    float           fontPx   = m_scaler.ToPxf (s_kFontDip);
    float           textX    = left + padX + iconBox + iconGap;
    float           reserve  = GetTextReservePx (m_scaler);
    float           textW    = width - padX * 2.0f - iconBox - iconGap - reserve;
    float           iconR    = iconBox * 0.5f;
    float           iconCx   = left + padX + iconR;
    float           iconCy   = top + height * 0.5f;   // vertically centered in the bordered area
    DxuiTextHAlign  hAlign   = DxuiTextHAlign::Left;



    if (!m_visible)
    {
        return;
    }

    //  CENTERED AS A GROUP when the caller asked for it: the badge, the gap and
    //  the text measured together and slid to the middle. Measured rather than
    //  estimated -- Paint has the renderer that Layout does not.
    //
    //  THE LINE BOX IS THE ONE THE HEIGHT WAS RESERVED FOR -- literally the
    //  same call, off the same cache: capped at s_kMaxLineDip and split evenly
    //  when the text has to wrap. Nothing is fitted up or retried here; a box
    //  the height query measured cannot need a line the height query did not
    //  reserve.
    if (m_centered && textW > 1.0f)
    {
        float  lineBox = ResolveCenteredBoxPx (text, textW, m_scaler);
        float  groupW  = iconBox + iconGap + lineBox;
        float  startX  = left + (width - reserve - groupW) * 0.5f;

        if (startX < left + padX)
        {
            startX = left + padX;
        }

        iconCx = startX + iconR;
        textX  = startX + iconBox + iconGap;
        textW  = lineBox;
        hAlign = DxuiTextHAlign::Center;
    }

    // Themed surface: a subtle tinted fill inside a muted border, so the banner
    // reads as a notice, not a button. A warning carries its own hue rather
    // than the theme accent -- caution should not depend on what the accent
    // happens to be.
    if (m_severity == Severity::Warning)
    {
        painter.FillRect    (left, top, width, height, theme.InfoBannerWarningBackground());
        painter.OutlineRect (left, top, width, height, borderPx, theme.InfoBannerWarningBorder());
    }
    else
    {
        painter.FillRect    (left, top, width, height, theme.InfoBannerBackground());
        painter.OutlineRect (left, top, width, height, borderPx, theme.InfoBannerBorder());
    }

    if (m_iconGlyph != nullptr)
    {
        PaintIconGlyph (text, theme, iconCx - iconR, iconCy - iconR, iconBox);
    }
    else if (m_severity == Severity::Warning)
    {
        // The same triangle the drive widget shows on a damaged disk, so the
        // two read as one idea rather than two similar-looking marks.
        DxuiWarningBadge::Draw (painter,
                                iconCx - iconR, iconCy - iconR,
                                iconBox, iconBox,
                                theme.WarningAccent(), theme.WarningEdge(), theme.WarningMark());
    }
    else
    {
        // Info badge, drawn from primitives (no icon-font dependency, exact
        // centering): a RING with an "i" inside it, one pen weight throughout.
        // Monoline, because a filled accent disc is the heaviest mark on a
        // strip whose whole job is to be noticed without shouting -- and it
        // sat next to line-drawn chrome that shares this weight.
        //
        // Inset by half the stroke so the pen's outer edge lands on the icon
        // box rather than straddling it, and the "i" is proportioned off the
        // ring's inner space so it stays centered at any size.
        //  The pen is clamped FIRST: the ring's inset and the dot are both
        //  proportions of it, and sizing them from a hairline the painter
        //  would then draw a pixel wide pushes the ring past the icon box.
        float  stroke = iconBox * s_kBadgeStrokeEm;
        float  ringR  = 0.0f;
        float  dotR   = 0.0f;
        float  stemH  = iconR * 0.58f;

        if (stroke < 1.0f)
        {
            stroke = 1.0f;
        }

        ringR = iconR - stroke * 0.5f;
        dotR  = stroke * 0.55f;

        StrokeCircle     (painter, iconCx, iconCy, ringR, stroke, theme.Accent());
        painter.FillCircle (iconCx, iconCy - iconR * 0.42f, dotR, theme.Accent());
        painter.FillRect         (iconCx - stroke * 0.5f, iconCy - iconR * 0.12f,
                                  stroke, stemH, theme.Accent());
    }

    // Wrapping body text, VERTICALLY CENTERED IN WHATEVER HEIGHT THE BANNER HAS.
    //
    // Top alignment is right only when the banner was sized to exactly the text
    // -- which is true in a dialog and false in a message bar, where the height
    // is the taller of the text and whatever the bar contains. A single line
    // then sat against the top edge with the whole button's worth of space
    // beneath it. Centering is identical in the sized-to-text case, so nothing
    // that used this before moves.
    hr = text.DrawString (m_text.c_str(),
                          textX,
                          top + padY,
                          (textW > 1.0f) ? textW : 1.0f,
                          height - padY * 2.0f,
                          theme.InfoBannerForeground(),
                          fontPx,
                          DxuiTheme::GetUiFace(),
                          hAlign,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          true);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (const std::unique_ptr<DxuiButton> & action : m_actions)
    {
        action->Paint (painter, text, theme);
    }

    if (m_dismissible)
    {
        PaintCloseBox (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::SetActions
//
//  REPLACED RATHER THAN ADDED TO. A standing notice absorbs further changes
//  instead of stacking, so setting its actions again must result in one
//  Restart button and not three.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoBanner::SetActions (const std::vector<std::wstring> & labels)
{
    size_t  i = 0;



    m_actions.clear();

    for (i = 0; i < labels.size(); i++)
    {
        auto    button = std::make_unique<DxuiButton> (labels[i]);
        size_t  index  = i;

        //  PRIMARY, NOT DEFAULT. A neutral button sits on the banner's tinted
        //  fill at almost the same value and disappears -- measured on the dark
        //  themes, where the notice's text was visible and its action could not
        //  be found at all. The accent fill is what makes the one thing here that
        //  can be pressed look pressable.
        button->SetVariant (DxuiButton::Variant::Primary);
        button->SetOnClick ([this, index] ()
        {
            if (m_onAction)
            {
                m_onAction (index);
            }
        });

        m_actions.push_back (std::move (button));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::GetAction
//
////////////////////////////////////////////////////////////////////////////////

DxuiButton * DxuiInfoBanner::GetAction (size_t index)
{
    return (index < m_actions.size()) ? m_actions[index].get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::GetControlColumnPx
//
//  The buttons and the close box, a gap after each, and the trailing edge pad
//  the last of them is inset by. Leaving the pad out reserves less than Layout
//  then uses, so the text runs the last few pixels underneath a button.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiInfoBanner::GetControlColumnPx (const DxuiDpiScaler & scaler) const
{
    float  width = 0.0f;



    if (!m_actions.empty() || m_dismissible)
    {
        width = (scaler.ToPxf (s_kActionWidthDip) + scaler.ToPxf (s_kActionGapDip)) * (float) m_actions.size()
              + scaler.ToPxf (s_kEdgePadDip);
    }

    if (m_dismissible)
    {
        width += scaler.ToPxf (s_kCloseBoxDip) + scaler.ToPxf (s_kActionGapDip);
    }

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::GetTextReservePx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiInfoBanner::GetTextReservePx (const DxuiDpiScaler & scaler) const
{
    return m_trailingReservePx + GetControlColumnPx (scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::GetControlHeightPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiInfoBanner::GetControlHeightPx (const DxuiDpiScaler & scaler) const
{
    float  height = 0.0f;



    if (!m_actions.empty())
    {
        height = scaler.ToPxf (s_kActionHeightDip);
    }

    if (m_dismissible)
    {
        height = (std::max) (height, scaler.ToPxf (s_kCloseBoxDip));
    }

    if (height > 0.0f)
    {
        height += scaler.ToPxf (s_kEdgePadDip) * 2.0f;
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::Layout
//
//  The strip takes the whole bounds, and the controls sit inside it against
//  the trailing edge, vertically centered: the close box last, the buttons
//  before it in the order given. The border spans everything; the text
//  reserve, not a narrower box, separates the text from the controls.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoBanner::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    float  actionW = scaler.ToPxf (s_kActionWidthDip);
    float  actionH = scaler.ToPxf (s_kActionHeightDip);
    float  gap     = scaler.ToPxf (s_kActionGapDip);
    float  closeW  = scaler.ToPxf (s_kCloseBoxDip);
    float  right   = (float) boundsDip.right - scaler.ToPxf (s_kEdgePadDip);
    float  midY    = (float) (boundsDip.top + boundsDip.bottom) * 0.5f;
    size_t i       = 0;



    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    m_dismissRect = {};

    if (m_dismissible)
    {
        m_dismissRect = RECT { (LONG) (right - closeW), (LONG) (midY - closeW * 0.5f), (LONG) right, (LONG) (midY + closeW * 0.5f) };
        right        -= closeW + gap;
    }

    //  From the trailing edge back, so the first action is the one furthest
    //  from the edge and the buttons read left to right in the order given.
    for (i = m_actions.size(); i > 0; i--)
    {
        RECT  slot = { (LONG) (right - actionW), (LONG) (midY - actionH * 0.5f), (LONG) right, (LONG) (midY + actionH * 0.5f) };

        m_actions[i - 1]->Layout (slot, scaler);

        right -= actionW + gap;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::OnMouse
//
//  Offered to each action, then to the close box, which closes on a release
//  inside it after a press inside it, as a button does. The strip itself
//  takes nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiInfoBanner::OnMouse (const DxuiMouseEvent & ev)
{
    bool    handled = false;
    bool    inClose = m_dismissible && PtInRect (&m_dismissRect, ev.positionDip);
    size_t  i       = 0;



    for (i = 0; i < m_actions.size() && !handled; i++)
    {
        handled = m_actions[i]->OnMouse (ev);
    }

    if (!handled && m_dismissible)
    {
        if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
        {
            m_dismissHover = inClose && ev.kind == DxuiMouseEventKind::Move;
        }
        else if (ev.kind == DxuiMouseEventKind::Down && inClose)
        {
            m_dismissPressed = true;
            handled          = true;
        }
        else if (ev.kind == DxuiMouseEventKind::Up && m_dismissPressed)
        {
            m_dismissPressed = false;
            handled          = true;

            if (inClose && m_onDismiss)
            {
                m_onDismiss();
            }
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::PaintCloseBox
//
//  The X a tab closes with, in the notice's text color, over a faint wash of
//  that color while the pointer is on it and a stronger one while pressed.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoBanner::PaintCloseBox (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr uint32_t  kHoverAlpha   = 0x1F000000u;
    constexpr uint32_t  kPressedAlpha = 0x33000000u;
    constexpr wchar_t   kMdl2Family[] = L"Segoe MDL2 Assets";
    HRESULT             hr            = S_OK;
    float               left          = (float) m_dismissRect.left;
    float               top           = (float) m_dismissRect.top;
    float               width         = (float) (m_dismissRect.right  - m_dismissRect.left);
    float               height        = (float) (m_dismissRect.bottom - m_dismissRect.top);
    uint32_t            glyph         = theme.InfoBannerForeground();



    if (m_dismissHover || m_dismissPressed)
    {
        painter.FillRoundedRect (left, top, width, height, m_scaler.ToPxf (4.0f),
                                 (glyph & 0x00FFFFFFu) | (m_dismissPressed ? kPressedAlpha : kHoverAlpha));
    }

    hr = text.DrawString (s_kpszMdl2ChromeClose, left, top, width, height, glyph,
                          m_scaler.ToPxf (s_kCloseGlyphDip), kMdl2Family,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
