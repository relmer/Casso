#include "Pch.h"

#include "Ui/DiskInspector/TrackHeaderView.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/DiskInspector/InspectorText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackHeaderView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void TrackHeaderView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const TrackAnalysis *  track  = m_context.GetTrack();
    float                  x      = static_cast<float> (m_boundsDip.left);
    float                  y      = static_cast<float> (m_boundsDip.top);
    float                  line   = m_scaler.ToPxf (static_cast<float> (kLineDip));
    float                  w      = GetWidth();
    int                    qt     = m_context.model->GetQuarterTrack();
    std::wstring           title;
    std::wstring           note;



    (void) painter;

    if (m_context.hasDisk && m_context.analysis != nullptr)
    {
        title = InspectorText::FormatTrackTitle (qt) + L"  " + InspectorText::FormatTrackClass (m_context.analysis->entries[qt].trackClass);

        if (m_context.analysis->entries[qt].isBeyondHeadReach)
        {
            title += L"  (beyond the head's reach)";
        }

        note = (m_context.analysis->copy != nullptr) ? GetFormatNote (m_context.analysis->copy->format) : std::wstring();

        text.DrawString (title.c_str(), x, y, w, line, theme.HeadingForeground(), m_scaler.ToPxf (kTextDip + 1.0f), DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);

        text.DrawString (InspectorText::FormatTrackLine (track).c_str(), x, y + line, w, line, theme.Foreground(), m_scaler.ToPxf (kTextDip),
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

        if (track != nullptr)
        {
            text.DrawString (InspectorText::FormatMeasureLine (*track).c_str(), x, y + 2 * line, w, line, theme.ForegroundMuted(), m_scaler.ToPxf (kTextDip),
                             DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }

        if (!note.empty())
        {
            text.DrawString (note.c_str(), x, y + 3 * line, w, line, theme.ForegroundMuted(), m_scaler.ToPxf (kSmallDip),
                             DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackHeaderView::GetFormatNote
//
//  What a sector or nibble image cannot hold (FR-003).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TrackHeaderView::GetFormatNote (DiskFormat format)
{
    std::wstring  note;



    switch (format)
    {
        case DiskFormat::Dsk:
        case DiskFormat::Do:
        case DiskFormat::Po:
            note = L"Built from sector data, so it holds no timing or copy protection";
            break;

        case DiskFormat::Nib:
            note = L"A nibble image stores no timing bits between nibbles";
            break;

        default:
            break;
    }

    return note;
}
