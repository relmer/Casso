#pragma once

#include "Pch.h"

#include "Ui/Dialogs/ReleaseNotesLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesView
//
//  Draws formatted release notes: headings, bullets, and paragraphs with
//  bold, code and link words, flowed by ReleaseNotesLayout. Its height
//  depends on real text measurement, which only exists while painting, so
//  the flow runs in Paint and the owner reads GetMeasuredHeightPx afterward
//  to size the scroll area. Lays out and paints in physical pixels, as the
//  dialog content does.
//
////////////////////////////////////////////////////////////////////////////////

class ReleaseNotesView : public IDxuiControl
{
public:
    using OpenLinkFn = std::function<void (const std::string & url)>;

    void     SetLines             (std::vector<FormattedLine> lines);
    void     SetOnOpenLink        (OpenLinkFn fn) { m_onOpenLink = std::move (fn); }

    int      GetMeasuredHeightPx  () const { return m_measuredHeightPx; }
    int      GetEstimatedHeightPx (const DxuiDpiScaler & scaler) const;

    void     Layout               (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void     Paint                (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     OnMouse              (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint    (POINT clientPx) const override;

private:
    void     Reflow               (IDxuiTextRenderer & text, const IDxuiTheme & theme, float widthPx);

    std::vector<FormattedLine>   m_lines;
    std::vector<PlacedNotesRun>  m_runs;
    DxuiDpiScaler                m_scaler;
    OpenLinkFn                   m_onOpenLink;
    float                        m_flowWidthPx      = -1.0f;
    UINT                         m_flowDpi          = 0;
    int                          m_measuredHeightPx = 0;
    bool                         m_isPressedOnLink  = false;
};
