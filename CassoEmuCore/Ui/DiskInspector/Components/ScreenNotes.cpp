#include "Pch.h"

#include "Ui/DiskInspector/Components/ScreenNotes.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenNotes::PaintNot35
//
//  On a 3.5" disk, a note over every view but the Image tab (FR-054).
//
////////////////////////////////////////////////////////////////////////////////

void ScreenNotes::PaintNot35 (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    static constexpr LPCWSTR  kpszNote = L"Casso does not analyze 3.5\" disks. The Image tab lists what the file contains.";



    RECT  empty = {};



    for (const RECT & area : { m_session.layout.platterArea, m_session.layout.trackArea, m_session.layout.stripArea, IsImageShown() ? empty : m_session.layout.diskContent })
    {
        if (!IsRectEmpty (&area))
        {
            painter.FillRect (static_cast<float> (area.left), static_cast<float> (area.top), static_cast<float> (area.right - area.left),
                              static_cast<float> (area.bottom - area.top), theme.Background());
            text.DrawString (kpszNote, static_cast<float> (area.left), static_cast<float> (area.top), static_cast<float> (area.right - area.left),
                             static_cast<float> (area.bottom - area.top), theme.ForegroundMuted(), m_session.scaler.ToPxf (InspectorView::kTextDip),
                             DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, true);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenNotes::IsCovered
//
////////////////////////////////////////////////////////////////////////////////

bool ScreenNotes::IsCovered (POINT pointPx) const
{
    return m_session.is35 && (IsInside (m_session.layout.platterArea, pointPx) || IsInside (m_session.layout.trackArea, pointPx) || IsInside (m_session.layout.stripArea, pointPx) ||
                      (!IsImageShown() && IsInside (m_session.layout.diskContent, pointPx)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenNotes::IsImageShown
//
////////////////////////////////////////////////////////////////////////////////

bool ScreenNotes::IsImageShown() const
{
    return m_session.isDiskGroup && m_session.diskTab == kTabImage;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenNotes::PaintNoDisk
//
//  An empty drive: a dashed outline of the disk and its hub where the
//  platter would be, and over it the reason no disk is drawn (FR-061,
//  FR-070).
//
////////////////////////////////////////////////////////////////////////////////

void ScreenNotes::PaintNoDisk (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    static constexpr double  kTwoPi    = 6.283185307179586;
    static constexpr float   kInsetDip = 16.0f;
    static constexpr float   kDashDip  = 12.0f;



    float          cx      = (m_session.layout.platterArea.left + m_session.layout.platterArea.right) / 2.0f;
    float          cy      = (m_session.layout.platterArea.top + m_session.layout.platterArea.bottom) / 2.0f;
    float          rim     = std::min (m_session.layout.platterArea.right - m_session.layout.platterArea.left, m_session.layout.platterArea.bottom - m_session.layout.platterArea.top) / 2.0f - m_session.scaler.ToPxf (kInsetDip);
    float          line    = m_session.scaler.ToPxf (1.5f);
    bool           isNoCtl = m_session.headState != nullptr && !m_session.headState->hasController;
    bool           isOff   = m_session.host != nullptr && !m_session.host->IsDriveAttached (m_session.drive);
    std::wstring   why     = isNoCtl ? std::wstring (L"This machine has no Disk II controller in slot 6") :
                             isOff   ? std::format (L"Drive {} is not attached", m_session.drive + 1) : std::format (L"No disk in drive {}", m_session.drive + 1);
    double         a       = 0.0;
    double         b       = 0.0;
    int            k       = 0;
    int            dashes  = 0;



    //  The same dash length on both circles.
    for (float radius : { rim, rim * PlatterRenderer::kInnerFraction })
    {
        dashes = std::max (8, static_cast<int> (kTwoPi * radius / (2.0f * m_session.scaler.ToPxf (kDashDip))));

        for (k = 0; radius > 0.0f && k < dashes; k++)
        {
            a = kTwoPi * k / dashes;
            b = kTwoPi * (k + 0.5) / dashes;

            painter.DrawLine (cx + radius * static_cast<float> (std::sin (a)), cy - radius * static_cast<float> (std::cos (a)),
                              cx + radius * static_cast<float> (std::sin (b)), cy - radius * static_cast<float> (std::cos (b)), line, theme.ForegroundMuted());
        }
    }

    text.DrawString (why.c_str(), static_cast<float> (m_session.layout.platterArea.left), static_cast<float> (m_session.layout.platterArea.top),
                     static_cast<float> (m_session.layout.platterArea.right - m_session.layout.platterArea.left), static_cast<float> (m_session.layout.platterArea.bottom - m_session.layout.platterArea.top),
                     theme.ForegroundMuted(), m_session.scaler.ToPxf (InspectorView::kTextDip + 1.0f), DxuiTheme::kBodyFace, DxuiTextHAlign::Center,
                     DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenNotes::IsInside
//
////////////////////////////////////////////////////////////////////////////////

bool ScreenNotes::IsInside (const RECT & rect, POINT pointPx)
{
    return PtInRect (&rect, pointPx) != FALSE;
}
