#include "Pch.h"

#include "Ui/DiskInspector/TrackHeaderView.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/DiskInspector/InspectorText.h"





static constexpr float  s_kPillPadDip  = 8.0f;
static constexpr float  s_kPillGapDip  = 6.0f;
static constexpr float  s_kPillTextDip = 12.0f; constexpr LPCWSTR s_kpszBeyondReach = L"Beyond the head's reach";





////////////////////////////////////////////////////////////////////////////////
//
//  TrackHeaderView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void TrackHeaderView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    std::wstring           separator = std::wstring (L" ") + s_kpszMiddleDot + L" ";
    const TrackAnalysis  * track      = m_context.GetTrack();
    float                  x          = static_cast<float> (m_boundsDip.left);
    float                  y          = static_cast<float> (m_boundsDip.top);
    float                  line       = m_scaler.ToPxf (static_cast<float> (kLineDip));
    float                  w          = GetWidth();
    float                  gap        = m_scaler.ToPxf (kTextDip);
    float                  titleW     = 0;
    float                  titleH     = 0;
    TrackClass             trackClass = TrackClass::NothingRecorded;
    int                    qt         = m_context.model->GetQuarterTrack();
    std::wstring           title;
    std::wstring           counts;
    std::wstring           second;
    std::wstring           rest;



    m_fullText.clear();

    if (m_context.hasDisk && m_context.analysis != nullptr)
    {
        title      = InspectorText::FormatTrackTitle (qt);
        trackClass = m_context.analysis->entries[qt].trackClass;

        counts = InspectorText::FormatTrackLine (track);
        second = (track != nullptr) ? InspectorText::FormatMeasureLine (*track) : std::wstring();

        m_fullText = title + separator + InspectorText::FormatTrackClass (trackClass) +
                     (m_context.analysis->entries[qt].isBeyondHeadReach ? separator + s_kpszBeyondReach : L"") +
                     L"\n" + counts + (second.empty() ? L"" : L"\n" + second);

        text.MeasureString (title.c_str(), m_scaler.ToPxf (kTextDip + 1.0f), DxuiTheme::kBodyFace, titleW, titleH);
        text.DrawString (title.c_str(), x, y, w, line, theme.HeadingForeground(), m_scaler.ToPxf (kTextDip + 1.0f), DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);

        //  The pills after the title; the counts follow them. The track's
        //  class is left out when it is the disk's format, already in the
        //  status band's first chip.
        if (InspectorText::FormatTrackClass (trackClass) != InspectorText::FormatDiskFormat (m_context.analysis->summary.format))
        {
            titleW += gap / 2 + PaintPill (painter, text, theme, InspectorText::FormatTrackClass (trackClass), trackClass == TrackClass::Damaged, x + titleW + gap / 2, y);
        }

        if (m_context.analysis->entries[qt].isBeyondHeadReach)
        {
            titleW += m_scaler.ToPxf (s_kPillGapDip) + PaintPill (painter, text, theme, s_kpszBeyondReach, false, x + titleW + m_scaler.ToPxf (s_kPillGapDip), y);
        }

        //  The counts beside the title, and what does not fit there at the
        //  start of the second line.
        rest   = SplitToFit (text, counts, w - titleW - gap);
        second = rest.empty() ? second : (second.empty() ? rest : rest + separator + second);

        text.DrawString (counts.c_str(), x + titleW + gap, y, w - titleW - gap, line, theme.Foreground(), m_scaler.ToPxf (kTextDip),
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

        (void) SplitToFit (text, second, w);

        text.DrawString (second.c_str(), x, y + line, w, line, theme.ForegroundMuted(), m_scaler.ToPxf (kTextDip),
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackHeaderView::GetTooltip
//
//  The whole of the header, including anything cut from its lines.
//
////////////////////////////////////////////////////////////////////////////////

bool TrackHeaderView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    bool  hasTip = !m_fullText.empty() && PtInRect (&m_boundsDip, pointPx) != FALSE;



    if (hasTip)
    {
        outText     = m_fullText;
        outAnchorPx = m_boundsDip;
    }

    return hasTip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackHeaderView::PaintPill
//
//  A label in a pill, drawn as the toolbar draws the disk's summary chips,
//  centered on the first line; returns its width.
//
////////////////////////////////////////////////////////////////////////////////

float TrackHeaderView::PaintPill (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & label, bool isBad, float x, float y) const
{
    float     pad    = m_scaler.ToPxf (s_kPillPadDip);
    float     textPx = m_scaler.ToPxf (s_kPillTextDip);
    float     line   = m_scaler.ToPxf (static_cast<float> (kLineDip));
    float     h      = line - 2.0f;
    float     w      = 0;
    float     textH  = 0;
    uint32_t  ink    = isBad ? m_context.palette.colors.sectorBad : theme.Foreground();



    text.MeasureString (label.c_str(), textPx, DxuiTheme::kBodyFace, w, textH);

    painter.FillRoundedRect    (x, y + 1.0f, w + 2 * pad, h, h / 2, theme.ButtonIdle());
    painter.OutlineRoundedRect (x, y + 1.0f, w + 2 * pad, h, h / 2, m_scaler.ToPxf (1.0f), isBad ? m_context.palette.colors.sectorBad : theme.ButtonBorder());
    text.DrawString (label.c_str(), x, y + 1.0f, w + 2 * pad, h, ink, textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center,
                     DxuiFontWeight::Normal, false);

    return w + 2 * pad;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackHeaderView::SplitToFit
//
//  Shortens the line to the items that fit the width and returns the rest,
//  breaking only between items.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TrackHeaderView::SplitToFit (IDxuiTextRenderer & text, std::wstring & inOutLine, float widthPx) const
{
    std::wstring  separator = std::wstring (L" ") + s_kpszMiddleDot + L" ";
    std::wstring  rest;
    size_t        at        = inOutLine.size();
    float         textW     = 0;
    float         textH     = 0;



    text.MeasureString (inOutLine.c_str(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, textW, textH);

    while (textW > widthPx && at != std::wstring::npos && at > 0)
    {
        at = inOutLine.rfind (separator, at - 1);

        if (at != std::wstring::npos)
        {
            text.MeasureString (inOutLine.substr (0, at).c_str(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, textW, textH);
        }
    }

    if (at != std::wstring::npos && at < inOutLine.size())
    {
        rest = inOutLine.substr (at + separator.size());
        inOutLine.resize (at);
    }

    return rest;
}
