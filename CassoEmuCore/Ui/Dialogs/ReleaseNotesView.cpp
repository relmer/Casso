#include "Pch.h"

#include "Ui/Dialogs/ReleaseNotesView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesView::SetLines
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesView::SetLines (std::vector<FormattedLine> lines)
{
    m_lines            = std::move (lines);
    m_runs.clear();
    m_flowWidthPx      = -1.0f;
    m_measuredHeightPx = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesView::GetEstimatedHeightPx
//
//  The height before the first paint has measured anything: one body line
//  per formatted line. The real height replaces it once Paint has run.
//
////////////////////////////////////////////////////////////////////////////////

int ReleaseNotesView::GetEstimatedHeightPx (const DxuiDpiScaler & scaler) const
{
    constexpr int  kLineHeightDip = 18;



    return m_measuredHeightPx > 0 ? m_measuredHeightPx
                                  : (int) m_lines.size() * scaler.ToPx (kLineHeightDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsPx);
    m_scaler = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesView::Reflow
//
//  Measures with the renderer the words will be drawn by, so the flow and
//  the drawing agree. A renderer that cannot measure gets an estimate from
//  the character count instead of a column of overlapping words.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesView::Reflow (IDxuiTextRenderer & text, const IDxuiTheme & theme, float widthPx)
{
    constexpr float  kEstGlyphEm    = 0.55f;
    constexpr float  kIndentDip     = 18.0f;
    constexpr float  kBulletGapDip  = 12.0f;
    constexpr float  kBlankGapDip   = 8.0f;
    constexpr float  kHeadingGapDip = 6.0f;
    constexpr float  kH1Scale       = 1.4f;
    constexpr float  kH2Scale       = 1.25f;
    constexpr float  kH3Scale       = 1.1f;



    NotesLayoutMetrics  metrics;
    DxuiFontHandle      body      = theme.BodyFont();
    DxuiFontHandle      bold      = theme.BodyBoldFont();
    DxuiFontHandle      mono      = theme.MonospaceFont();
    float               bodyPx    = m_scaler.ToPxf (body.sizeDip);



    metrics.bodySizePx    = bodyPx;
    metrics.codeSizePx    = m_scaler.ToPxf (mono.sizeDip);
    metrics.headingSizePx = { bodyPx * kH1Scale, bodyPx * kH2Scale, bodyPx * kH3Scale, bodyPx };
    metrics.lineHeightPx  = m_scaler.ToPxf (theme.BodyLineHeightDip());
    metrics.indentPx      = m_scaler.ToPxf (kIndentDip);
    metrics.bulletGapPx   = m_scaler.ToPxf (kBulletGapDip);
    metrics.blankGapPx    = m_scaler.ToPxf (kBlankGapDip);
    metrics.headingGapPx  = m_scaler.ToPxf (kHeadingGapDip);

    auto  measure = [&] (const std::wstring & word, const NotesRunStyle & style) -> float
    {
        const wchar_t  * face   = style.code ? mono.face : (style.bold ? bold.face : body.face);
        float            width  = 0.0f;
        float            height = 0.0f;
        HRESULT          hr     = text.MeasureString (word.c_str(), style.sizePx, face, width, height);

        if (FAILED (hr))
        {
            width = (float) word.size() * style.sizePx * kEstGlyphEm;
        }

        return width;
    };

    m_measuredHeightPx = (int) std::ceil (ReleaseNotesLayout::Flow (m_lines, widthPx, metrics, measure, m_runs));
    m_flowWidthPx      = widthPx;
    m_flowDpi          = m_scaler.GetDpi();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesView::Paint
//
//  Flows again only when the width or the DPI changed, then draws each
//  placed word. Bold and heading words use the bold face; a measured word
//  is given a pixel of slack so rounding cannot clip its last glyph.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float  kSlackPx = 2.0f;



    RECT            bounds = GetBounds();
    float           width  = (float) (bounds.right - bounds.left);
    DxuiFontHandle  body   = theme.BodyFont();
    DxuiFontHandle  mono   = theme.MonospaceFont();
    HRESULT         hr     = S_OK;
    uint32_t        color  = 0;



    (void) painter;

    if (width != m_flowWidthPx || m_scaler.GetDpi() != m_flowDpi)
    {
        Reflow (text, theme, width);
    }

    for (const PlacedNotesRun & run : m_runs)
    {
        color = run.style.isLink ? theme.Accent() : (run.style.bold ? theme.HeadingForeground() : theme.Foreground());

        hr = text.DrawString (run.text.c_str(),
                              (float) bounds.left + run.x,
                              (float) bounds.top  + run.y,
                              run.width + kSlackPx,
                              run.height,
                              color,
                              run.style.sizePx,
                              run.style.code ? mono.face : body.face,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Top,
                              run.style.bold ? DxuiFontWeight::Bold : DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesView::OnMouse
//
//  A press and release on a link opens it. Anything else is not claimed,
//  so the scroll panel still gets its wheel and drag.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesView::OnMouse (const DxuiMouseEvent & ev)
{
    RECT                    bounds  = GetBounds();
    float                   x       = (float) (ev.positionDip.x - bounds.left);
    float                   y       = (float) (ev.positionDip.y - bounds.top);
    const PlacedNotesRun  * link    = ReleaseNotesLayout::FindLinkAt (m_runs, x, y);
    bool                    claimed = false;



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        m_isPressedOnLink = link != nullptr;
        claimed           = m_isPressedOnLink;
    }
    else if (ev.kind == DxuiMouseEventKind::Up && ev.button == DxuiMouseButton::Left)
    {
        claimed = m_isPressedOnLink && link != nullptr;

        if (claimed && m_onOpenLink)
        {
            m_onOpenLink (link->linkUrl);
        }

        m_isPressedOnLink = false;
    }

    return claimed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesView::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR ReleaseNotesView::GetCursorForPoint (POINT clientPx) const
{
    RECT  bounds = GetBounds();



    return ReleaseNotesLayout::FindLinkAt (m_runs,
                                           (float) (clientPx.x - bounds.left),
                                           (float) (clientPx.y - bounds.top)) != nullptr ? IDC_HAND : nullptr;
}
