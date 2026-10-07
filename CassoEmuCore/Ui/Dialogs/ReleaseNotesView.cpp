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
    constexpr float  kEstGlyphEm     = 0.55f;
    constexpr float  kIndentDip      = 18.0f;
    constexpr float  kBulletGapDip   = 12.0f;
    constexpr float  kBlankGapDip    = 8.0f;
    constexpr float  kHeadingGapDip  = 6.0f;
    constexpr float  kH1Scale        = 1.4f;
    constexpr float  kH2Scale        = 1.25f;
    constexpr float  kH3Scale        = 1.1f;
    constexpr float  kCaptionDip     = 11.0f;
    constexpr float  kImageGapDip    = 8.0f;
    constexpr float  kPlaceholderDip = 80.0f;



    NotesLayoutMetrics  metrics;
    DxuiFontHandle      body      = theme.BodyFont();
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
        const wchar_t  * face   = style.code ? mono.face : body.face;
        DxuiFontWeight   weight = style.bold ? DxuiFontWeight::Bold : DxuiFontWeight::Normal;
        float            width  = 0.0f;
        float            height = 0.0f;
        HRESULT          hr     = text.MeasureStringWeighted (word.c_str(), style.sizePx, face, weight, width, height);

        if (FAILED (hr))
        {
            width = (float) word.size() * style.sizePx * kEstGlyphEm;
        }

        return width;
    };

    metrics.captionSizePx = m_scaler.ToPxf (kCaptionDip);
    metrics.imageGapPx    = m_scaler.ToPxf (kImageGapDip);
    metrics.placeholderPx = m_scaler.ToPxf (kPlaceholderDip);
    metrics.imageScale    = (float) m_scaler.GetDpi() / (float) USER_DEFAULT_SCREEN_DPI;

    auto  imageState = [this] (const std::string & src) -> NotesImageState
    {
        NotesImageState  state;
        auto             it    = m_images.find (src);

        state.isFailed = m_failedImages.contains (src);
        state.isLoaded = it != m_images.end() && it->second != nullptr;
        state.widthPx  = state.isLoaded ? it->second->width  : 0;
        state.heightPx = state.isLoaded ? it->second->height : 0;
        return state;
    };

    m_measuredHeightPx = (int) std::ceil (ReleaseNotesLayout::Flow (m_lines, widthPx, metrics, measure, imageState, m_runs, m_placedImages));
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



    if (width != m_flowWidthPx || m_scaler.GetDpi() != m_flowDpi)
    {
        Reflow (text, theme, width);
    }

    PaintImages (painter, text, theme, bounds);

    for (const PlacedNotesRun & run : m_runs)
    {
        color = run.style.isLink ? theme.Accent() : (run.style.muted ? theme.ForegroundMuted() : (run.style.bold ? theme.HeadingForeground() : theme.Foreground()));

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





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesView::SetImage
//
//  A fetched image arrived, or (null) could not be had. Either changes the
//  layout, so the next paint flows again.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesView::SetImage (const std::string & src, std::shared_ptr<const NotesImage> image)
{
    if (image != nullptr)
    {
        m_images[src] = std::move (image);
        m_failedImages.erase (src);
    }
    else
    {
        m_failedImages.insert (src);
    }

    m_flowWidthPx = -1.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesView::GetImageSources
//
//  Every image source in the notes, in order, without repeats.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ReleaseNotesView::GetImageSources() const
{
    std::vector<std::string>  sources;



    for (const FormattedLine & line : m_lines)
    {
        if (line.kind == FormattedLineKind::Image &&
            std::find (sources.begin(), sources.end(), line.image.src) == sources.end())
        {
            sources.push_back (line.image.src);
        }
    }

    return sources;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesView::PaintImages
//
//  A loaded image is drawn scaled into its box. A placeholder is an outlined
//  box with the alt text inside, muted, which is also what a failed image
//  keeps.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesView::PaintImages (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & bounds)
{
    constexpr float  kInsetDip   = 6.0f;
    constexpr float  kOutlineDip = 1.0f;



    HRESULT         hr    = S_OK;
    DxuiFontHandle  body  = theme.BodyFont();
    float           inset = m_scaler.ToPxf (kInsetDip);
    float           x     = 0.0f;
    float           y     = 0.0f;



    for (const PlacedNotesImage & placed : m_placedImages)
    {
        auto  it = m_images.find (placed.src);

        x = (float) bounds.left + placed.x;
        y = (float) bounds.top  + placed.y;

        if (placed.isLoaded && it != m_images.end() && it->second != nullptr)
        {
            hr = text.DrawIconBitmap (it->second->bgraPremul.data(), it->second->width, it->second->height,
                                      x, y, placed.width, placed.height);
            IGNORE_RETURN_VALUE (hr, S_OK);
            continue;
        }

        painter.OutlineRect (x, y, placed.width, placed.height, m_scaler.ToPxf (kOutlineDip), theme.Border());

        hr = text.DrawString (placed.alt.c_str(), x + inset, y + inset,
                              std::max (placed.width - inset * 2.0f, 0.0f), std::max (placed.height - inset * 2.0f, 0.0f),
                              theme.ForegroundMuted(), m_scaler.ToPxf (body.sizeDip), body.face,
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}