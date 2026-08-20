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

    // Prose first, then the file it refers to. Naming the disk before saying
    // anything about it makes the reader hold a path in mind with no reason
    // to yet.

    m_rows.clear();
    m_rows.push_back (Row { L"Total sectors:",      std::to_wstring (total),     L"" });
    m_rows.push_back (Row { L"Verified sectors:",   std::to_wstring (verified),  L"(checksums matched)" });
    m_rows.push_back (Row { L"Recovered sectors:",  std::to_wstring (recovered), L"(readable, may contain errors)" });
    m_rows.push_back (Row { L"Lost sectors:",       std::to_wstring (lost),      L"(zeroed - nothing to recover)" });

    m_destLine = L"Save as: " + destName + L" (in the same directory)";

    m_warning.SetSeverity (DxuiInfoBanner::Severity::Warning);
    m_warning.SetText (L"Repairing the checksums makes the disk structurally sound, "
                       L"but this masks any corrupted data in those recovered "
                       L"sectors.");

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
    m_scaler = scaler;

    // prose, gap, path, gap, table, gap, destination, gap
    y += m_scaler.Px (s_kProseLines * s_kLineDip + s_kGapDip);
    y += m_scaler.Px (s_kPathLines * s_kLineDip + s_kGapDip);
    y += m_scaler.Px (s_kLineDip * static_cast<int> (m_rows.size()) + s_kGapDip);
    y += m_scaler.Px (s_kLineDip + s_kGapDip);

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
//  SalvageDialogContent::DrawWrapped
//
//  Draws wrapped text and returns the height it used, clamped to maxHeight so
//  a pathological path cannot push the rest of the dialog off the bottom.
//  Measuring rather than reserving is what keeps the spacing right for a path
//  of any length.
//
////////////////////////////////////////////////////////////////////////////////

float SalvageDialogContent::DrawWrapped (IDxuiTextRenderer   &  text,
                                         const IDxuiTheme    &  theme,
                                         const std::wstring  &  body,
                                         float                  left,
                                         float                  top,
                                         float                  width,
                                         float                  maxHeight,
                                         float                  fontPx,
                                         const wchar_t       *  face,
                                         DxuiFontWeight         weight)
{
    HRESULT  hr     = S_OK;
    float    outW   = 0.0f;
    float    outH   = 0.0f;
    float    used   = maxHeight;



    hr = text.MeasureStringWrapped (body.c_str(), fontPx, face, width, outW, outH);

    if (SUCCEEDED (hr) && outH > 0.0f)
    {
        used = (outH < maxHeight) ? outH : maxHeight;
    }

    hr = text.DrawString (body.c_str(), left, top, width, used,
                          theme.Foreground(), fontPx, face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Top, weight, true);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return used;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SalvageDialogContent::Paint
//
////////////////////////////////////////////////////////////////////////////////

void SalvageDialogContent::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text,
                                  const IDxuiTheme & theme)
{
    HRESULT         hr        = S_OK;
    float           left      = static_cast<float> (m_boundsDip.left);
    float           width     = static_cast<float> (m_boundsDip.right - m_boundsDip.left);
    float           y         = static_cast<float> (m_boundsDip.top);
    size_t          i         = 0;

    // Match every other dialog: labels draw at the theme's body size, scaled
    // into pixels. Drawing at a raw DIP size renders correctly only at 100%.
    DxuiFontHandle    body       = theme.BodyFont();
    const wchar_t   * face       = (body.face != nullptr) ? body.face : DxuiTheme::kBodyFace;
    float             fontPx     = m_scaler.Pxf (body.sizeDip);
    float             lineH      = static_cast<float> (m_scaler.Px (s_kLineDip));
    float             gap        = static_cast<float> (m_scaler.Px (s_kGapDip));
    float             figureCol  = static_cast<float> (m_scaler.Px (s_kNoteColDip));
    float             noteLeft   = figureCol + gap;
    float             bannerH    = 0.0f;
    RECT              bannerRect = {};



    // Prose first: it says what this dialog is about. The path names the disk
    // it is about, set apart beneath it by a blank line.
    //
    // Both advance by the height they actually used rather than by a fixed
    // line allowance. A path can be one line or four depending entirely on
    // where the user keeps their disks, and a fixed reservation is wrong in
    // both directions -- it collides when the path is long and leaves a hole
    // when it is short.
    y += DrawWrapped (text, theme, m_prose, left, y, width, lineH * s_kProseLines,
                      fontPx, face, DxuiFontWeight::Normal) + gap;

    y += DrawWrapped (text, theme, m_sourcePath, left, y, width, lineH * s_kPathLines,
                      fontPx, face, DxuiFontWeight::SemiBold) + gap;

    for (i = 0; i < m_rows.size(); i++)
    {
        const Row &  row = m_rows[i];

        hr = text.DrawString (row.label.c_str(), left, y,
                              static_cast<float> (m_scaler.Px (s_kFigureColDip)), lineH,
                              theme.Foreground(), fontPx, face, DxuiTextHAlign::Left,
                              DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        // Right-aligned so the digits line up regardless of how many there
        // are -- the whole reason this is a layout and not padded text.
        hr = text.DrawString (row.figure.c_str(), left, y, figureCol, lineH,
                              theme.Foreground(), fontPx, face, DxuiTextHAlign::Right,
                              DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (!row.note.empty())
        {
            hr = text.DrawString (row.note.c_str(), left + noteLeft, y,
                                  width - noteLeft, lineH,
                                  theme.ForegroundMuted(), fontPx, face,
                                  DxuiTextHAlign::Left, DxuiTextVAlign::Top,
                                  DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        y += lineH;
    }

    y += gap;

    hr = text.DrawString (m_destLine.c_str(), left, y, width, lineH,
                          theme.Foreground(), fontPx, face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Top,
                          DxuiFontWeight::SemiBold, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
    y += lineH + gap;

    // Size the banner to the height its text actually needs. Layout had to
    // estimate (no text renderer there), and the estimate rounds up so text
    // never clips -- which showed as an empty line inside the box.
    bannerH           = m_warning.MeasuredHeightPx (text, width, m_scaler);
    bannerRect.left   = m_boundsDip.left;
    bannerRect.top    = static_cast<int> (y);
    bannerRect.right  = m_boundsDip.right;
    bannerRect.bottom = static_cast<int> (y + bannerH);

    m_warning.SetRect (bannerRect);
    m_warning.SetDpi (m_scaler.Dpi());
    m_warning.Paint (painter, text, theme);
}
