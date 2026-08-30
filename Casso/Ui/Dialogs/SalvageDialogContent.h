#pragma once

#include "Pch.h"

#include "Core/DxuiPanel.h"
#include "Widgets/DxuiInfoBanner.h"
#include "Devices/Disk/DiskImageStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SalvageDialogContent
//
//  The body of the salvage dialog: the disk being acted on, prose, a figures
//  table, the destination filename, and a warning banner.
//
//  The table is laid out rather than spaced with padding characters. Columns
//  aligned by counting spaces come apart the moment the font or the DPI is
//  not what the author had, and these are numbers a user is comparing down a
//  column -- so labels get one column, figures a right-aligned second, and
//  the qualifying notes a third.
//
////////////////////////////////////////////////////////////////////////////////

class SalvageDialogContent : public DxuiPanel
{
public:
    //  Fills the panel from an assessment. Everything shown is derived from
    //  it, so the dialog cannot drift from the numbers salvage will act on.
    void  SetAssessment (const std::wstring     &  sourcePath,
                         const std::wstring     &  destName,
                         const SalvageAssessment & assessment);

    int   GetPreferredHeightDip () const { return m_preferredHeightDip; }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;

private:
    //  Every metric below is in DIPs and MUST be scaled before use. Drawing
    //  at raw DIP values renders correctly only at 100% -- at 125% the text
    //  came out a fifth smaller than every other dialog on screen, which is
    //  exactly how this was found.
    DxuiDpiScaler     m_scaler;

public:
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    //  Draws wrapped text, returns the height used (clamped to maxHeight).
    static float  DrawWrapped (IDxuiTextRenderer   &  text,
                               const IDxuiTheme    &  theme,
                               const std::wstring  &  body,
                               float                  left,
                               float                  top,
                               float                  width,
                               float                  maxHeight,
                               float                  fontPx,
                               const wchar_t       *  face,
                               DxuiFontWeight         weight);

public:

private:
    struct Row
    {
        std::wstring  label;
        std::wstring  figure;
        std::wstring  note;
    };

    static constexpr int  s_kPathLines     = 3;    // a full path routinely wraps
    static constexpr int  s_kProseLines    = 3;
    static constexpr int  s_kBannerEstDip  = 64;   // corrected once Layout runs
    static constexpr int  s_kGapDip        = 10;
    static constexpr int  s_kLineDip       = 19;
    static constexpr int  s_kFigureColDip  = 150;   // label column width
    static constexpr int  s_kNoteColDip    = 215;   // figures right-align here

    static constexpr float  s_kFontDip = 13.0f;

    std::wstring      m_sourcePath;
    std::wstring      m_prose;
    std::wstring      m_destLine;
    std::vector<Row>  m_rows;
    DxuiInfoBanner    m_warning;
    RECT              m_warningRect        = {};
    int               m_preferredHeightDip = 0;
};
