#include "Pch.h"

#include "Ui/Dialogs/SalvageDialogContent.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SalvageDialogContent::SetAssessment
//
////////////////////////////////////////////////////////////////////////////////

void SalvageDialogContent::SetAssessment (
    const std::wstring      &  sourcePath,
    const std::wstring      &  destName,
    const SalvageAssessment &  assessment)
{
    int   total     = assessment.totalSectors;
    int   verified  = assessment.report.sectorsVerified;
    int   recovered = assessment.report.sectorsRecovered;
    int   lost      = assessment.report.sectorsLost;



    m_sourcePath = sourcePath;

    m_prose = L"This disk is damaged, so Casso will not write to it. "
              L"Casso can build a salvaged copy you can work on. "
              L"The original is left untouched.";

    m_rows.clear();
    m_rows.push_back (Row { L"Total sectors:",      std::to_wstring (total),     L"" });
    m_rows.push_back (Row { L"Verified sectors:",   std::to_wstring (verified),  L"(checksums matched)" });
    m_rows.push_back (Row { L"Recovered sectors:",  std::to_wstring (recovered), L"(readable, may contain errors)" });
    m_rows.push_back (Row { L"Lost sectors:",       std::to_wstring (lost),      L"(zeroed - nothing to recover)" });

    m_destLine = L"Save as: " + destName + L" (in the same directory)";

    m_warning.SetSeverity (DxuiInfoBanner::Severity::Warning);
    m_warning.SetText (L"Repairing the checksums makes the disk structurally sound, "
                       L"but this masks any data corruption remaining in the "
                       L"recovered sectors.");

    // The host needs a height BEFORE the panel is laid out -- it sizes the
    // window, and layout happens inside it. Computed here from the content
    // rather than in Layout, where it would arrive one frame too late and
    // leave the dialog clamped to its minimum, clipping the table.
    m_preferredHeightDip = s_kPathLines * s_kLineDip + s_kGapDip
                         + s_kProseLines * s_kLineDip + s_kGapDip
                         + static_cast<int> (m_rows.size()) * s_kLineDip + s_kGapDip
                         + s_kLineDip + s_kGapDip
                         + s_kBannerEstDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SalvageDialogContent::Layout
//
////////////////////////////////////////////////////////////////////////////////

void SalvageDialogContent::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int    width      = boundsDip.right - boundsDip.left;
    int    y          = boundsDip.top;
    float  bannerH    = 0.0f;
    RECT   bannerRect = {};



    SetBounds (boundsDip);

    // path, gap, prose, gap, table, gap, destination, gap
    y += s_kPathLines * s_kLineDip + s_kGapDip;
    y += s_kProseLines * s_kLineDip + s_kGapDip;
    y += s_kLineDip * static_cast<int> (m_rows.size()) + s_kGapDip;
    y += s_kLineDip + s_kGapDip;

    bannerRect.left   = boundsDip.left;
    bannerRect.top    = y;
    bannerRect.right  = boundsDip.right;
    bannerH           = m_warning.PreferredHeightPx (static_cast<float> (width), scaler);
    bannerRect.bottom = y + static_cast<int> (bannerH);

    m_warning.Layout (bannerRect, scaler);
    m_warningRect = bannerRect;

    // Layout knows better than the estimate, so let it correct the value.
    m_preferredHeightDip = (bannerRect.bottom - boundsDip.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SalvageDialogContent::Paint
//
////////////////////////////////////////////////////////////////////////////////

void SalvageDialogContent::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text,
                                  const IDxuiTheme & theme)
{
    HRESULT  hr    = S_OK;
    float    left  = static_cast<float> (m_boundsDip.left);
    float    width = static_cast<float> (m_boundsDip.right - m_boundsDip.left);
    float    y     = static_cast<float> (m_boundsDip.top);
    size_t   i     = 0;



    // The disk being acted on, set apart from the prose by a blank line so it
    // reads as the subject rather than as part of a sentence.
    hr = text.DrawString (m_sourcePath.c_str(), left, y, width,
                          (float) (s_kPathLines * s_kLineDip),
                          theme.Foreground(), s_kFontDip, DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Top,
                          DxuiFontWeight::SemiBold, true);
    IGNORE_RETURN_VALUE (hr, S_OK);
    y += s_kPathLines * s_kLineDip + s_kGapDip;

    hr = text.DrawString (m_prose.c_str(), left, y, width,
                          (float) (s_kProseLines * s_kLineDip),
                          theme.Foreground(), s_kFontDip, DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Top,
                          DxuiFontWeight::Normal, true);
    IGNORE_RETURN_VALUE (hr, S_OK);
    y += s_kProseLines * s_kLineDip + s_kGapDip;

    for (i = 0; i < m_rows.size(); i++)
    {
        const Row &  row = m_rows[i];

        hr = text.DrawString (row.label.c_str(), left, y, (float) s_kFigureColDip,
                              (float) s_kLineDip, theme.Foreground(), s_kFontDip,
                              DxuiTheme::kBodyFace, DxuiTextHAlign::Left,
                              DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        // Right-aligned so the digits line up regardless of how many there
        // are -- the whole reason this is a layout and not padded text.
        hr = text.DrawString (row.figure.c_str(), left, y, (float) s_kNoteColDip,
                              (float) s_kLineDip, theme.Foreground(), s_kFontDip,
                              DxuiTheme::kBodyFace, DxuiTextHAlign::Right,
                              DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (!row.note.empty())
        {
            hr = text.DrawString (row.note.c_str(), left + s_kNoteColDip + s_kGapDip, y,
                                  width - s_kNoteColDip - s_kGapDip, (float) s_kLineDip,
                                  theme.ForegroundMuted(), s_kFontDip,
                                  DxuiTheme::kBodyFace, DxuiTextHAlign::Left,
                                  DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        y += s_kLineDip;
    }

    y += s_kGapDip;

    hr = text.DrawString (m_destLine.c_str(), left, y, width, (float) s_kLineDip,
                          theme.Foreground(), s_kFontDip, DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Top,
                          DxuiFontWeight::SemiBold, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_warning.Paint (painter, text, theme);
}
